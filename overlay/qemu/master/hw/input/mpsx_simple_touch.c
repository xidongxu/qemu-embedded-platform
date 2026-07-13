#include "qemu/osdep.h"
#include "hw/input/mpsx_simple_touch.h"
#include "hw/core/qdev.h"
#include "hw/core/irq.h"
#include "ui/input.h"

#include "system/address-spaces.h"
#include "system/memory.h"

#include "qemu/log.h"

#define REG_STATUS     0x00
#define REG_X          0x04
#define REG_Y          0x08
#define REG_CTRL       0x0c

static uint64_t mpsx_touch_read(void *opaque, hwaddr addr, unsigned size) {
    MPSXSimpleTouchState *s = opaque;
    switch (addr) {
    case REG_STATUS:
        return s->status;
    case REG_X:
        return s->x;
    case REG_Y:
        return s->y;
    default:
        return 0;
    }
}

static void mpsx_touch_write(void *opaque, hwaddr addr, uint64_t value, unsigned size) {
    MPSXSimpleTouchState *s = opaque;
    switch (addr) {
    case REG_CTRL:
        if (value == 1) {
            s->status = 0;
        }
        break;
    default:
        break;
    }
}

static const MemoryRegionOps mpsx_touch_ops = {
    .read = mpsx_touch_read,
    .write = mpsx_touch_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void mpsx_touch_realize(DeviceState *dev, Error **errp) {
    MPSXSimpleTouchState *s = MPSX_SIMPLE_TOUCH(dev);
    memory_region_init_io(&s->iomem, OBJECT(dev), &mpsx_touch_ops, s, "mpsx-simple-touch", 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);
    s->status = 0;
    s->x = 0;
    s->y = 0;
}

static void mpsx_touch_class_init(ObjectClass *klass, const void *data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = mpsx_touch_realize;
}

static const TypeInfo mpsx_touch_type_info = {
    .name = TYPE_MPSX_SIMPLE_TOUCH,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MPSXSimpleTouchState),
    .class_init = mpsx_touch_class_init,
};

static void mpsx_touch_register_types(void) {
    type_register_static(&mpsx_touch_type_info);
}

type_init(mpsx_touch_register_types)
