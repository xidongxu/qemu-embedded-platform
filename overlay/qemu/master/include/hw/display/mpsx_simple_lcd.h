#ifndef HW_DISPLAY_MPSX_SIMPLE_LCD_H
#define HW_DISPLAY_MPSX_SIMPLE_LCD_H

#include "hw/core/sysbus.h"
#include "ui/console.h"
#include "ui/surface.h"
#include "system/memory.h"

#define TYPE_MPSX_SIMPLE_LCD "mpsx-simple-lcd"

OBJECT_DECLARE_SIMPLE_TYPE(MPSXSimpleLCDState, MPSX_SIMPLE_LCD)

struct MPSXSimpleLCDState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    QemuConsole *console;
    DisplaySurface *surface;
    uint32_t width;
    uint32_t height;
    uint32_t fb_addr;
    uint32_t update;
    uint32_t format;
};

#endif
