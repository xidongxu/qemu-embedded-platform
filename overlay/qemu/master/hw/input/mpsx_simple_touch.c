#include "qemu/osdep.h"
#include "hw/input/mpsx_simple_touch.h"
#include "hw/core/qdev.h"
#include "hw/core/irq.h"
#include "ui/input.h"

#include "system/address-spaces.h"
#include "system/memory.h"

#include "qemu/log.h"

#define REG_STATUS          0x00
#define REG_X               0x04
#define REG_Y               0x08
#define REG_CTRL            0x0c
#define REG_ID              0x10
#define REG_RES_X           0x14
#define REG_RES_Y           0x18

#define STATUS_PRESSED      (1 << 0)
#define STATUS_READY        (1 << 1)

#define CTRL_CLEAR_INT      (1 << 0)

/* Touch ADC resolution (12-bit) */
#define TOUCH_X_RESOLUTION  4096
#define TOUCH_Y_RESOLUTION  4096

/* "MPSX" */
#define TOUCH_ID            0x4D505358

/* QEMU absolute input range */
#define QEMU_ABS_MAX        32767

static void mpsx_touch_update_irq(MPSXSimpleTouchState *s) {
    qemu_set_irq(s->irq, s->status ? 1 : 0);
}

static uint32_t mpsx_touch_scale(uint32_t value, uint32_t resolution) {
    if (value > QEMU_ABS_MAX) {
        value = QEMU_ABS_MAX;
    }
    return value * (resolution - 1) / QEMU_ABS_MAX;
}

static void mpsx_touch_event(DeviceState *dev, QemuConsole *src, QemuInputEvent *evt) {
    MPSXSimpleTouchState *s = MPSX_SIMPLE_TOUCH(dev);
    switch (evt->type) {
    case INPUT_EVENT_KIND_ABS:
        if (evt->abs.axis == INPUT_AXIS_X) {
            s->x = mpsx_touch_scale(evt->abs.value, TOUCH_X_RESOLUTION);
        } else if (evt->abs.axis == INPUT_AXIS_Y) {
            s->y = mpsx_touch_scale(evt->abs.value, TOUCH_Y_RESOLUTION);
        }
        s->status |= STATUS_READY;
        break;
    case INPUT_EVENT_KIND_BTN:
        if (evt->btn.button == INPUT_BUTTON_LEFT || evt->btn.button == INPUT_BUTTON_TOUCH) {
            s->pressed = evt->btn.down;
            if (evt->btn.down) {
                /* Touch Down */
                s->status |= STATUS_PRESSED;
                s->status |= STATUS_READY;
            } else {
                /* Touch Up */
                s->status &= ~STATUS_PRESSED;
            }
            mpsx_touch_update_irq(s);
        }
        break;
    default:
        break;
    }
}

static void mpsx_touch_sync(DeviceState *dev) {
    MPSXSimpleTouchState *s = MPSX_SIMPLE_TOUCH(dev);
    if (s->pressed) {
        qemu_log_mask(LOG_GUEST_ERROR, "Touch raw converted (%u,%u)\n", s->x, s->y);
    }
}

static const QemuInputHandler mpsx_touch_handler = {
    .name = "mpsx-touch",
    .mask = INPUT_EVENT_MASK_ABS | INPUT_EVENT_MASK_BTN,
    .event = mpsx_touch_event,
    .sync = mpsx_touch_sync,
};

static uint64_t mpsx_touch_read(void *opaque, hwaddr addr, unsigned size) {
    MPSXSimpleTouchState *s = opaque;
    switch (addr) {
    case REG_STATUS:
        return s->status;
    case REG_X:
        return s->x;
    case REG_Y:
        return s->y;
    case REG_ID:
        return TOUCH_ID;
    case REG_RES_X:
        return TOUCH_X_RESOLUTION;
    case REG_RES_Y:
        return TOUCH_Y_RESOLUTION;
    default:
        return 0;
    }
}

static void mpsx_touch_write(void *opaque, hwaddr addr, uint64_t value, unsigned size) {
    MPSXSimpleTouchState *s = opaque;
    switch (addr) {
    case REG_CTRL:
        if (value & CTRL_CLEAR_INT) {
            s->status &= ~STATUS_READY;
            mpsx_touch_update_irq(s);
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
    s->input = qemu_input_handler_register(dev, &mpsx_touch_handler);
    s->status = 0;
    s->x = 0;
    s->y = 0;
    s->pressed = false;
}

static void mpsx_touch_unrealize(DeviceState *dev) {
    MPSXSimpleTouchState *s = MPSX_SIMPLE_TOUCH(dev);
    if (s->input) {
        qemu_input_handler_unregister(s->input);
        s->input = NULL;
    }
}

static void mpsx_touch_class_init(ObjectClass *klass, const void *data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = mpsx_touch_realize;
    dc->unrealize = mpsx_touch_unrealize;
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
