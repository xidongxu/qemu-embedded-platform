#ifndef HW_AUDIO_MPSX_SIMPLE_MIC_H
#define HW_AUDIO_MPSX_SIMPLE_MIC_H

#include "qemu/osdep.h"
#include "hw/core/sysbus.h"
#include "qemu/audio.h"
#include "system/memory.h"

#define TYPE_MPSX_SIMPLE_MIC "mpsx-simple-mic"

OBJECT_DECLARE_SIMPLE_TYPE(MPSXSimpleMicState, MPSX_SIMPLE_MIC)

typedef struct QEMUTimer QEMUTimer;

struct MPSXSimpleMicState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq;
    /* QEMU audio backend, set via the "audiodev" property */
    AudioBackend *audio_be;
    SWVoiceIn *voice;
    /* control register */
    uint32_t ctrl;
    /* status register */
    uint32_t status;
    /* bits[1:0]: 0=U8, 1=S16; bit2: stereo */
    uint32_t format;
    /* Hz */
    uint32_t sample_rate;
    /* guest physical address of PCM buffer */
    hwaddr buf_addr;
    /* PCM buffer length in bytes */
    uint32_t buf_len;
    /* current write offset within the buffer */
    uint32_t rec_pos;
    uint32_t int_enable;
    uint32_t int_status;
    bool voice_active;

    /* Optional WAV file used as capture source instead of the audio backend */
    char *infile;
    FILE *wf;
    QEMUTimer *timer;
    uint32_t wav_rate;
    uint32_t wav_channels;
    uint32_t wav_bits;
    uint32_t wav_data_off;
    uint32_t wav_data_len;
    bool use_file;
};

#endif
