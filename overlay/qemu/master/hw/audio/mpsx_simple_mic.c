/*
 * QEMU MPSX simple microphone (audio-in) device
 *
 * A minimal audio-in (recording) controller intended for the mps2-an505
 * board, modelled after the mpsx-simple-audio device.  It captures PCM
 * data from a QEMU audio backend and streams it into a guest buffer in
 * system memory.  When one full buffer round has been filled it raises
 * an interrupt so the guest can collect the samples.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/audio/mpsx_simple_mic.h"
#include "hw/core/qdev.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"

#include "system/address-spaces.h"
#include "system/memory.h"

#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "qemu/timer.h"
#include "qemu/bswap.h"
#include "qapi/error.h"

#define REG_CTRL                (0x00)
#define REG_STATUS              (0x04)
#define REG_FORMAT              (0x08)
#define REG_BUF_ADDR            (0x0C)
#define REG_BUF_LEN             (0x10)
#define REG_SAMPLE_RATE         (0x14)
#define REG_REC_POS             (0x18)
#define REG_INT_EN              (0x1C)
#define REG_INT_STATUS          (0x20)

/* Control register */
#define MIC_CTRL_ENABLE         (1 << 0)
#define MIC_CTRL_RESET          (1 << 1)
#define MIC_CTRL_UPDATE         (1 << 2)

/* Status register */
#define MIC_STATUS_BUSY         (1 << 0)
#define MIC_STATUS_DONE         (1 << 1)
#define MIC_STATUS_OVERRUN      (1 << 2)

/* Interrupt status/enable register (bit aligned between the two) */
#define MIC_INT_DONE            (1 << 0)
#define MIC_INT_OVERRUN         (1 << 1)

/* Format register: bits[1:0] sample width, bit2 stereo */
#define MIC_FORMAT_U8           (0)
#define MIC_FORMAT_S16          (1)
#define MIC_FORMAT_STEREO       (1 << 2)

#define MIC_DEFAULT_RATE        (8000)
#define MIC_CHUNK_SIZE          (4096)

static void mpsx_mic_update_irq(MPSXSimpleMicState *s) {
    qemu_set_irq(s->irq, (s->int_status & s->int_enable) != 0);
}

static AudioFormat mpsx_mic_fmt(MPSXSimpleMicState *s) {
    return (s->format & 0x3) == MIC_FORMAT_S16 ? AUDIO_FORMAT_S16 : AUDIO_FORMAT_U8;
}

static int mpsx_mic_nchannels(MPSXSimpleMicState *s) {
    return (s->format & MIC_FORMAT_STEREO) ? 2 : 1;
}

/*
 * Copy 'len' captured PCM bytes into the guest buffer, wrapping around
 * and raising the DONE interrupt each time a full buffer round is filled.
 */
static void mpsx_mic_dma_to_guest(MPSXSimpleMicState *s, const uint8_t *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        size_t avail_in_buf = MIN(len - off, (size_t)s->buf_len - s->rec_pos);
        hwaddr addr = s->buf_addr + s->rec_pos;
        MemTxResult ret = MEMTX_OK;
        ret = address_space_write(&address_space_memory, addr,
                                  MEMTXATTRS_UNSPECIFIED,
                                  buf + off, avail_in_buf);
        if (ret != MEMTX_OK) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "mpsx mic: DMA write failed at 0x%"HWADDR_PRIx"\n",
                          addr);
            break;
        }
        off += avail_in_buf;
        s->rec_pos += avail_in_buf;
        if (s->rec_pos >= s->buf_len) {
            /* one full buffer round captured: ask the guest to collect it */
            s->rec_pos = 0;
            s->status |= MIC_STATUS_DONE;
            s->int_status |= MIC_INT_DONE;
            mpsx_mic_update_irq(s);
        }
    }
}

/*
 * The audio backend calls this when captured PCM data is available.
 * 'avail' is the number of bytes it can hand over, in the format of
 * the voice that was opened with audio_be_open_in().  We read the
 * samples out and DMA them into the guest buffer.
 */
static void mpsx_mic_in_cb(void *opaque, int avail) {
    MPSXSimpleMicState *s = opaque;
    uint8_t chunk[MIC_CHUNK_SIZE];
    bool recording = false;

    if (!s->voice) {
        return;
    }
    recording = (s->ctrl & MIC_CTRL_ENABLE) && s->buf_addr != 0 && s->buf_len > 0;
    while (avail > 0) {
        size_t n = MIN(avail, (int)sizeof(chunk));
        size_t got = audio_be_read(s->audio_be, s->voice, chunk, n);
        if (got == 0) {
            break;
        }
        if (recording) {
            mpsx_mic_dma_to_guest(s, chunk, got);
        } else {
            /* not recording: the samples are simply discarded */
            s->status |= MIC_STATUS_OVERRUN;
            s->int_status |= MIC_INT_OVERRUN;
            mpsx_mic_update_irq(s);
        }
        avail -= got;
    }
}

/*
 * Open 'path' as a RIFF/WAVE PCM capture source and remember the fmt /
 * data chunk layout.  On success s->wf and s->use_file are set and the
 * file position is left at the start of the PCM data.
 */
static int mpsx_mic_wav_open(MPSXSimpleMicState *s, const char *path) {
    uint8_t hdr[12];
    uint8_t ck[8];
    uint16_t fmt = 0, ch = 0, bits = 0;
    uint32_t rate = 0, cklen = 0;
    bool found_fmt = false, found_data = false;

    s->wf = fopen(path, "rb");
    if (!s->wf) {
        error_report("mpsx-simple-mic: cannot open '%s': %s",
                     path, strerror(errno));
        return -1;
    }
    if (fread(hdr, 1, sizeof(hdr), s->wf) != sizeof(hdr) ||
        memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        error_report("mpsx-simple-mic: '%s' is not a RIFF/WAVE file", path);
        goto fail;
    }
    while (fread(ck, 1, sizeof(ck), s->wf) == sizeof(ck)) {
        cklen = ldl_le_p(ck + 4);
        if (memcmp(ck, "fmt ", 4) == 0) {
            uint8_t fmtb[16];
            if (fread(fmtb, 1, sizeof(fmtb), s->wf) != sizeof(fmtb)) {
                goto fail;
            }
            fmt = lduw_le_p(fmtb + 0);
            ch = lduw_le_p(fmtb + 2);
            rate = ldl_le_p(fmtb + 4);
            bits = lduw_le_p(fmtb + 14);
            found_fmt = true;
            if (cklen > sizeof(fmtb)) {
                fseek(s->wf, cklen - sizeof(fmtb), SEEK_CUR);
            }
        } else if (memcmp(ck, "data", 4) == 0) {
            s->wav_data_off = ftell(s->wf);
            s->wav_data_len = cklen;
            found_data = true;
            break;
        } else {
            /* skip other chunks (they are word aligned) */
            fseek(s->wf, cklen + (cklen & 1), SEEK_CUR);
        }
    }
    if (!found_fmt || !found_data) {
        error_report("mpsx-simple-mic: '%s': missing fmt/data chunk", path);
        goto fail;
    }
    if (fmt != 1) {
        error_report("mpsx-simple-mic: '%s': only PCM WAV supported (format %u)",
                     path, fmt);
        goto fail;
    }
    s->wav_rate = rate;
    s->wav_channels = ch;
    s->wav_bits = bits;
    s->use_file = true;
    fseek(s->wf, s->wav_data_off, SEEK_SET);
    printf("mpsx simple mic: wav '%s' rate=%u ch=%u bits=%u data@%u len=%u\n",
           path, rate, ch, bits, s->wav_data_off, s->wav_data_len);
    return 0;

fail:
    fclose(s->wf);
    s->wf = NULL;
    return -1;
}

/*
 * Timer-driven capture source for infile mode: periodically read a chunk
 * of PCM from the WAV file and DMA it into the guest buffer.  Wraps back
 * to the start of the data chunk once the file is exhausted.
 */
static void mpsx_mic_file_cb(void *opaque) {
    MPSXSimpleMicState *s = opaque;
    uint8_t chunk[MIC_CHUNK_SIZE];
    size_t nread;
    int64_t ns;

    if (!s->wf) {
        return;
    }
    nread = fread(chunk, 1, sizeof(chunk), s->wf);
    if (nread == 0) {
        /* end of file: loop forever */
        fseek(s->wf, s->wav_data_off, SEEK_SET);
        nread = fread(chunk, 1, sizeof(chunk), s->wf);
    }
    if (nread > 0 &&
        (s->ctrl & MIC_CTRL_ENABLE) && s->buf_addr != 0 && s->buf_len > 0) {
        mpsx_mic_dma_to_guest(s, chunk, nread);
    }
    /* pace the feed at the WAV sample rate */
    if (s->wav_rate && s->wav_channels && s->wav_bits) {
        ns = (int64_t)sizeof(chunk) * NANOSECONDS_PER_SECOND /
             (s->wav_rate * s->wav_channels * (s->wav_bits / 8));
    } else {
        ns = 10 * (NANOSECONDS_PER_SECOND / 1000);
    }
    timer_mod(s->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + ns);
}

static void mpsx_mic_close_voice(MPSXSimpleMicState *s) {
    if (s->voice) {
        audio_be_set_active_in(s->audio_be, s->voice, false);
        audio_be_close_in(s->audio_be, s->voice);
        s->voice = NULL;
    }
    s->voice_active = false;
}

static int mpsx_mic_open_voice(MPSXSimpleMicState *s) {
    struct audsettings as = { 0 };
    if (s->voice) {
        return 0;
    }
    as.freq = s->sample_rate;
    as.nchannels = mpsx_mic_nchannels(s);
    as.fmt = mpsx_mic_fmt(s);
    as.big_endian = false;
    s->voice = audio_be_open_in(s->audio_be, s->voice, "mpsx-simple-mic",
                                s, mpsx_mic_in_cb, &as);
    if (!s->voice) {
        error_report("mpsx-simple-mic: could not open audio input voice");
        return -1;
    }
    return 0;
}

static void mpsx_mic_reopen_voice(MPSXSimpleMicState *s) {
    bool was_active = s->voice_active;
    if (s->use_file) {
        return;
    }
    if (!s->voice) {
        return;
    }
    mpsx_mic_close_voice(s);
    if ((s->ctrl & MIC_CTRL_ENABLE) && was_active) {
        if (mpsx_mic_open_voice(s) == 0) {
            s->voice_active = true;
            audio_be_set_active_in(s->audio_be, s->voice, true);
        }
    }
}

static void mpsx_mic_reset(DeviceState *dev) {
    MPSXSimpleMicState *s = MPSX_SIMPLE_MIC(dev);
    s->ctrl = 0;
    s->status = 0;
    s->format = MIC_FORMAT_U8;
    s->sample_rate = MIC_DEFAULT_RATE;
    s->buf_addr = 0;
    s->buf_len = 0;
    s->rec_pos = 0;
    s->int_enable = 0;
    s->int_status = 0;
    s->voice_active = false;
    if (s->voice) {
        audio_be_set_active_in(s->audio_be, s->voice, false);
    }
}

static uint64_t mpsx_mic_read(void *opaque, hwaddr addr, unsigned size) {
    MPSXSimpleMicState *s = opaque;
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
    case REG_REC_POS:
        return s->rec_pos;
    case REG_INT_EN:
        return s->int_enable;
    case REG_INT_STATUS:
        return s->int_status;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "mpsx mic bad read offset=0x%"HWADDR_PRIx"\n", addr);
        return 0;
    }
}

static void mpsx_mic_write(void *opaque, hwaddr addr, uint64_t value, unsigned size) {
    MPSXSimpleMicState *s = opaque;
    uint32_t old = 0;
    switch (addr) {
    case REG_CTRL:
        if (value & MIC_CTRL_RESET) {
            mpsx_mic_reset(DEVICE(s));
            return;
        }
        old = s->ctrl;
        s->ctrl = value & (MIC_CTRL_ENABLE | MIC_CTRL_UPDATE);
        if ((value & MIC_CTRL_ENABLE) && !(old & MIC_CTRL_ENABLE)) {
            if (s->use_file) {
                /* drive capture from the WAV file */
                s->voice_active = true;
                timer_mod(s->timer,
                          qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                          (NANOSECONDS_PER_SECOND / 1000));
            } else if (mpsx_mic_open_voice(s) == 0) {
                s->voice_active = true;
                audio_be_set_active_in(s->audio_be, s->voice, true);
            }
        } else if (!(value & MIC_CTRL_ENABLE) && (old & MIC_CTRL_ENABLE)) {
            s->voice_active = false;
            if (s->use_file) {
                timer_del(s->timer);
            } else if (s->voice) {
                audio_be_set_active_in(s->audio_be, s->voice, false);
            }
        }
        if (value & MIC_CTRL_UPDATE) {
            /* restart capture from the start of the buffer */
            s->rec_pos = 0;
            s->status &= ~(MIC_STATUS_DONE | MIC_STATUS_OVERRUN);
            s->ctrl &= ~MIC_CTRL_UPDATE;
        }
        break;
    case REG_FORMAT:
        if ((value & 0x3) <= MIC_FORMAT_S16) {
            s->format = value & 0x7;
            mpsx_mic_reopen_voice(s);
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "mpsx mic invalid format=0x%"PRIx64"\n", value);
        }
        break;
    case REG_BUF_ADDR:
        s->buf_addr = value;
        s->rec_pos = 0;
        break;
    case REG_BUF_LEN:
        s->buf_len = value;
        s->rec_pos = 0;
        break;
    case REG_SAMPLE_RATE:
        if (value >= 1000 && value <= 192000) {
            s->sample_rate = value;
            mpsx_mic_reopen_voice(s);
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "mpsx mic invalid sample rate=%"PRIu64"\n", value);
        }
        break;
    case REG_INT_EN:
        s->int_enable = value;
        mpsx_mic_update_irq(s);
        break;
    case REG_INT_STATUS:
        s->int_status &= ~value;
        mpsx_mic_update_irq(s);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "mpsx mic bad write offset=0x%"HWADDR_PRIx"\n", addr);
        break;
    }
}

static const MemoryRegionOps mpsx_mic_ops = {
    .read = mpsx_mic_read,
    .write = mpsx_mic_write,
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

static void mpsx_mic_realize(DeviceState *dev, Error **errp) {
    MPSXSimpleMicState *s = MPSX_SIMPLE_MIC(dev);
    Error *local_err = NULL;
    memory_region_init_io(&s->iomem, OBJECT(dev), &mpsx_mic_ops, s,
                          "mpsx-simple-mic", 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);
    mpsx_mic_reset(dev);
    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, mpsx_mic_file_cb, s);
    /*
     * If an input WAV file was provided, use it as the capture source
     * instead of the audio backend.  This makes recording testable on
     * hosts without a microphone.
     */
    if (s->infile) {
        if (mpsx_mic_wav_open(s, s->infile) != 0) {
            /* file could not be used; fall back to the backend (if any) */
            error_report("mpsx-simple-mic: disabling WAV capture source");
        }
    }
    /*
     * Try to obtain an audio backend.  This can fail if no audiodev is
     * wired to the device (e.g. no -audiodev with input support was
     * passed, or no audio driver is compiled in at all).  That is not
     * fatal: the MMIO/IRQ side of the device still works and capture is
     * simply dropped.
     */
    if (!s->use_file && !audio_be_check(&s->audio_be, &local_err)) {
        error_report_err(local_err);
    }
    printf("mpsx simple mic realize: rate=%u fmt=0x%x use_file=%d\n",
           s->sample_rate, s->format, s->use_file);
}

static void mpsx_mic_unrealize(DeviceState *dev) {
    MPSXSimpleMicState *s = MPSX_SIMPLE_MIC(dev);
    if (s->timer) {
        timer_del(s->timer);
        timer_free(s->timer);
        s->timer = NULL;
    }
    if (s->wf) {
        fclose(s->wf);
        s->wf = NULL;
    }
    g_free(s->infile);
    mpsx_mic_close_voice(s);
}

static const Property mpsx_mic_properties[] = {
    DEFINE_AUDIO_PROPERTIES(MPSXSimpleMicState, audio_be),
    DEFINE_PROP_STRING("infile", MPSXSimpleMicState, infile),
};

static void mpsx_mic_class_init(ObjectClass *klass, const void *data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = mpsx_mic_realize;
    dc->unrealize = mpsx_mic_unrealize;
    device_class_set_props(dc, mpsx_mic_properties);
}

static const TypeInfo mpsx_mic_type_info = {
    .name = TYPE_MPSX_SIMPLE_MIC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MPSXSimpleMicState),
    .class_init = mpsx_mic_class_init,
};

static void mpsx_mic_register_types(void) {
    type_register_static(&mpsx_mic_type_info);
}
type_init(mpsx_mic_register_types)
