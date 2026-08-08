#ifndef HW_AUDIO_MPSX_SIMPLE_AUDIO_H
#define HW_AUDIO_MPSX_SIMPLE_AUDIO_H

#include "hw/core/sysbus.h"
#include "qemu/audio.h"
#include "system/memory.h"

#define TYPE_MPSX_SIMPLE_AUDIO "mpsx-simple-audio"

OBJECT_DECLARE_SIMPLE_TYPE(MPSXSimpleAudioState, MPSX_SIMPLE_AUDIO)

struct MPSXSimpleAudioState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq;
    /* QEMU audio backend, set via the "audiodev" property */
    AudioBackend *audio_be;
    SWVoiceOut *voice;
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
    /* current read offset within the buffer */
    uint32_t play_pos;
    uint32_t int_enable;
    uint32_t int_status;
    bool voice_active;
};

#endif
