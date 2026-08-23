/*
 * QEMU MPSX simple audio device
 *
 * A minimal audio-out controller intended for the mps2-an505 board,
 * modelled after the mpsx-simple-lcd / mpsx-simple-touch devices.
 *
 * The guest writes PCM data into a buffer in system memory, configures
 * the format/sample rate via MMIO registers, and sets CTRL.ENABLE.
 * The device opens a QEMU audio voice and streams the buffer contents
 * out through the audio backend.  When one full buffer round has been
 * consumed it raises an interrupt so the guest can refill the buffer.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/audio/mpsx_simple_audio.h"
#include "hw/core/qdev.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"

#include "system/address-spaces.h"
#include "system/memory.h"

#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "qapi/error.h"

#define REG_CTRL                (0x00)
#define REG_STATUS              (0x04)
#define REG_FORMAT              (0x08)
#define REG_BUF_ADDR            (0x0C)
#define REG_BUF_LEN             (0x10)
#define REG_SAMPLE_RATE         (0x14)
#define REG_PLAY_POS            (0x18)
#define REG_INT_EN              (0x1C)
#define REG_INT_STATUS          (0x20)

/* Control register */
#define AUDIO_CTRL_ENABLE       (1 << 0)
#define AUDIO_CTRL_RESET        (1 << 1)
#define AUDIO_CTRL_UPDATE       (1 << 2)

/* Status register */
#define AUDIO_STATUS_BUSY       (1 << 0)
#define AUDIO_STATUS_DONE       (1 << 1)
#define AUDIO_STATUS_UNDERRUN   (1 << 2)

/* Interrupt status/enable register (bit aligned between the two) */
#define AUDIO_INT_DONE          (1 << 0)

/* Format register: bits[1:0] sample width, bit2 stereo.
 * NOTE: these are the DEVICE register bit values, NOT QEMU's AudioFormat
 * enum (AUDIO_FORMAT_U8=0, AUDIO_FORMAT_S16=3).  Prefix with MPSX_FMT_ so
 * they cannot shadow the QEMU enum in audio.h (which would turn S16 playback
 * into S8 => half speed, e.g. 1 kHz played as 500 Hz). */
#define MPSX_FMT_U8             (0)
#define MPSX_FMT_S16            (1)
#define MPSX_FMT_STEREO         (1 << 2)

#define AUDIO_DEFAULT_RATE      (8000)
#define AUDIO_CHUNK_SIZE        (4096)

static void mpsx_audio_update_irq(MPSXSimpleAudioState *s) {
    qemu_set_irq(s->irq, (s->int_status & s->int_enable) != 0);
}

/* Map the device FORMAT register bits to QEMU's AudioFormat enum. */
static AudioFormat mpsx_audio_fmt(MPSXSimpleAudioState *s) {
    return (s->format & 0x3) == MPSX_FMT_S16 ? AUDIO_FORMAT_S16 : AUDIO_FORMAT_U8;
}

static int mpsx_audio_nchannels(MPSXSimpleAudioState *s) {
    return (s->format & MPSX_FMT_STEREO) ? 2 : 1;
}

/*
 * The audio backend calls this when it wants more PCM data.
 * 'free' is the number of bytes it can accept, in the format of
 * the voice that was opened with audio_be_open_out().
 */
static void mpsx_audio_out_cb(void *opaque, int free) {
    MPSXSimpleAudioState *s = opaque;
    uint8_t chunk[AUDIO_CHUNK_SIZE] = { 0 };
    bool playing = false;
    uint8_t silence = 0;
    if (!s->voice) {
        return;
    }
    playing = (s->ctrl & AUDIO_CTRL_ENABLE) && s->buf_addr != 0 && s->buf_len > 0;
    silence = (mpsx_audio_fmt(s) == AUDIO_FORMAT_U8) ? 0x80 : 0x00;
    while (free > 0) {
        size_t n = MIN(free, (int)sizeof(chunk));
        size_t written = 0;

        if (playing) {
            size_t got = 0;
            while (got < n && s->buf_len > 0) {
                size_t avail = MIN(n - got, (size_t)s->buf_len - s->play_pos);
                hwaddr addr = s->buf_addr + s->play_pos;
                MemTxResult ret = MEMTX_OK;
                ret = address_space_read(&address_space_memory, addr, MEMTXATTRS_UNSPECIFIED, chunk + got, avail);
                if (ret != MEMTX_OK) {
                    qemu_log_mask(LOG_GUEST_ERROR, "mpsx audio: DMA read failed at 0x%"HWADDR_PRIx"\n", addr);
                    break;
                }
                got += avail;
                s->play_pos += avail;
                if (s->play_pos >= s->buf_len) {
                    /* one full buffer round consumed: ask the guest to refill */
                    s->play_pos = 0;
                    s->status |= AUDIO_STATUS_DONE;
                    s->int_status |= AUDIO_INT_DONE;
                    mpsx_audio_update_irq(s);
                }
            }
            if (got < n) {
                /* underrun: pad the rest with silence */
                memset(chunk + got, silence, n - got);
                s->status |= AUDIO_STATUS_UNDERRUN;
            }
            written = audio_be_write(s->audio_be, s->voice, chunk, n);
        } else {
            memset(chunk, silence, n);
            written = audio_be_write(s->audio_be, s->voice, chunk, n);
        }
        if (!written) {
            break;
        }
        free -= written;
    }
}

static void mpsx_audio_close_voice(MPSXSimpleAudioState *s) {
    if (s->voice) {
        audio_be_set_active_out(s->audio_be, s->voice, false);
        audio_be_close_out(s->audio_be, s->voice);
        s->voice = NULL;
    }
    s->voice_active = false;
}

static int mpsx_audio_open_voice(MPSXSimpleAudioState *s) {
    struct audsettings as = { 0 };
    if (s->voice) {
        return 0;
    }
    as.freq = s->sample_rate;
    as.nchannels = mpsx_audio_nchannels(s);
    as.fmt = mpsx_audio_fmt(s);
    as.big_endian = false;
    s->voice = audio_be_open_out(s->audio_be, s->voice, "mpsx-simple-audio", s, mpsx_audio_out_cb, &as);
    if (!s->voice) {
        error_report("mpsx-simple-audio: could not open audio voice");
        return -1;
    }
    return 0;
}

static void mpsx_audio_reopen_voice(MPSXSimpleAudioState *s) {
    bool was_active = s->voice_active;
    if (!s->voice) {
        return;
    }
    mpsx_audio_close_voice(s);
    if ((s->ctrl & AUDIO_CTRL_ENABLE) && was_active) {
        if (mpsx_audio_open_voice(s) == 0) {
            s->voice_active = true;
            audio_be_set_active_out(s->audio_be, s->voice, true);
        }
    }
}

static void mpsx_audio_reset(DeviceState *dev) {
    MPSXSimpleAudioState *s = MPSX_SIMPLE_AUDIO(dev);
    s->ctrl = 0;
    s->status = AUDIO_STATUS_DONE;
    s->format = MPSX_FMT_U8;
    s->sample_rate = AUDIO_DEFAULT_RATE;
    s->buf_addr = 0;
    s->buf_len = 0;
    s->play_pos = 0;
    s->int_enable = 0;
    s->int_status = 0;
    s->voice_active = false;
    if (s->voice) {
        audio_be_set_active_out(s->audio_be, s->voice, false);
    }
}

static uint64_t mpsx_audio_read(void *opaque, hwaddr addr, unsigned size) {
    MPSXSimpleAudioState *s = opaque;
    switch (addr) {
    case REG_CTRL:
        return s->ctrl;
    case REG_STATUS:
        return s->status;
    case REG_FORMAT:
        return s->format;
    case REG_BUF_ADDR:
        return s->buf_addr;
    case REG_BUF_LEN:
        return s->buf_len;
    case REG_SAMPLE_RATE:
        return s->sample_rate;
    case REG_PLAY_POS:
        return s->play_pos;
    case REG_INT_EN:
        return s->int_enable;
    case REG_INT_STATUS:
        return s->int_status;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "mpsx audio bad read offset=0x%"HWADDR_PRIx"\n", addr);
        return 0;
    }
}

static void mpsx_audio_write(void *opaque, hwaddr addr, uint64_t value, unsigned size) {
    MPSXSimpleAudioState *s = opaque;
    uint32_t old = 0;
    switch (addr) {
    case REG_CTRL:
        if (value & AUDIO_CTRL_RESET) {
            mpsx_audio_reset(DEVICE(s));
            return;
        }
        old = s->ctrl;
        s->ctrl = value & (AUDIO_CTRL_ENABLE | AUDIO_CTRL_UPDATE);
        if ((value & AUDIO_CTRL_ENABLE) && !(old & AUDIO_CTRL_ENABLE)) {
            if (mpsx_audio_open_voice(s) == 0) {
                s->voice_active = true;
                audio_be_set_active_out(s->audio_be, s->voice, true);
            }
        } else if (!(value & AUDIO_CTRL_ENABLE) && (old & AUDIO_CTRL_ENABLE)) {
            s->voice_active = false;
            if (s->voice) {
                audio_be_set_active_out(s->audio_be, s->voice, false);
            }
        }
        if (value & AUDIO_CTRL_UPDATE) {
            /* restart playback from the start of the buffer */
            s->play_pos = 0;
            s->status &= ~(AUDIO_STATUS_DONE | AUDIO_STATUS_UNDERRUN);
            s->ctrl &= ~AUDIO_CTRL_UPDATE;
        }
        break;
    case REG_FORMAT:
        if ((value & 0x3) <= MPSX_FMT_S16) {
            s->format = value & 0x7;
            mpsx_audio_reopen_voice(s);
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "mpsx audio invalid format=0x%"PRIx64"\n", value);
        }
        break;
    case REG_BUF_ADDR:
        s->buf_addr = value;
        s->play_pos = 0;
        break;
    case REG_BUF_LEN:
        s->buf_len = value;
        s->play_pos = 0;
        break;
    case REG_SAMPLE_RATE:
        if (value >= 1000 && value <= 192000) {
            s->sample_rate = value;
            mpsx_audio_reopen_voice(s);
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "mpsx audio invalid sample rate=%"PRIu64"\n", value);
        }
        break;
    case REG_INT_EN:
        s->int_enable = value;
        mpsx_audio_update_irq(s);
        break;
    case REG_INT_STATUS:
        s->int_status &= ~value;
        mpsx_audio_update_irq(s);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "mpsx audio bad write offset=0x%"HWADDR_PRIx"\n", addr);
        break;
    }
}

static const MemoryRegionOps mpsx_audio_ops = {
    .read = mpsx_audio_read,
    .write = mpsx_audio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void mpsx_audio_realize(DeviceState *dev, Error **errp) {
    MPSXSimpleAudioState *s = MPSX_SIMPLE_AUDIO(dev);
    Error *local_err = NULL;
    memory_region_init_io(&s->iomem, OBJECT(dev), &mpsx_audio_ops, s, "mpsx-simple-audio", 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);
    mpsx_audio_reset(dev);
    /*
     * Try to obtain an audio backend.  This can fail if no audiodev is
     * wired to the device (e.g. an explicit -audiodev was passed without
     * also setting the machine's audiodev, or no audio driver is compiled
     * in at all).  That is not fatal: the MMIO/IRQ side of the device
     * still works and playback is simply dropped.
     */
    if (!audio_be_check(&s->audio_be, &local_err)) {
        error_report_err(local_err);
    }
    printf("mpsx simple audio realize: rate=%u fmt=0x%x\n", s->sample_rate, s->format);
}

static void mpsx_audio_unrealize(DeviceState *dev) {
    MPSXSimpleAudioState *s = MPSX_SIMPLE_AUDIO(dev);
    mpsx_audio_close_voice(s);
}

static const Property mpsx_audio_properties[] = {
    DEFINE_AUDIO_PROPERTIES(MPSXSimpleAudioState, audio_be),
};

static void mpsx_audio_class_init(ObjectClass *klass, const void *data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = mpsx_audio_realize;
    dc->unrealize = mpsx_audio_unrealize;
    device_class_set_props(dc, mpsx_audio_properties);
}

static const TypeInfo mpsx_audio_type_info = {
    .name = TYPE_MPSX_SIMPLE_AUDIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MPSXSimpleAudioState),
    .class_init = mpsx_audio_class_init,
};

static void mpsx_audio_register_types(void) {
    type_register_static(&mpsx_audio_type_info);
}
type_init(mpsx_audio_register_types)
