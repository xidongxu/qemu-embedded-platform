#ifndef HW_INPUT_MPSX_SIMPLE_TOUCH_H
#define HW_INPUT_MPSX_SIMPLE_TOUCH_H

#include "hw/core/sysbus.h"
#include "ui/console.h"

#define TYPE_MPSX_SIMPLE_TOUCH "mpsx-simple-touch"

OBJECT_DECLARE_SIMPLE_TYPE(MPSXSimpleTouchState, MPSX_SIMPLE_TOUCH)

struct MPSXSimpleTouchState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    QemuInputHandlerState *input;
    qemu_irq irq;
    uint32_t status;
    uint32_t x;
    uint32_t y;
    bool pressed;
};

#endif
