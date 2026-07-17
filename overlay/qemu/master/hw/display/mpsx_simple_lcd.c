#include "qemu/osdep.h"
#include "hw/display/mpsx_simple_lcd.h"
#include "hw/core/qdev.h"
#include "hw/core/irq.h"

#include "system/address-spaces.h"
#include "system/memory.h"

#include "qemu/log.h"

#define REG_WIDTH           0x00
#define REG_HEIGHT          0x04
#define REG_FB_ADDR         0x08
#define REG_CTRL            0x0C
#define REG_STATUS          0x10
#define REG_FORMAT          0x14
#define REG_STRIDE          0x18
#define REG_INT_EN          0x1c
#define REG_INT_STATUS      0x20
/* Lcd control */
#define LCD_CTRL_ENABLE     (1 << 0)
#define LCD_CTRL_UPDATE     (1 << 1)
#define LCD_CTRL_RESET      (1 << 2)
/* Lcd status */
#define LCD_STATUS_BUSY     (1 << 0)
#define LCD_STATUS_DONE     (1 << 1)
/* Lcd format */
#define LCD_FORMAT_ARGB8888 (0)
#define LCD_FORMAT_RGB888   (1)
#define LCD_FORMAT_RGB565   (2)

static uint32_t mpsx_rgb565_to_argb8888(uint16_t pixel) {
    uint32_t r = 0, g = 0, b = 0;
    r = (pixel >> 11) & 0x1f;
    g = (pixel >> 5)  & 0x3f;
    b = pixel & 0x1f;
    r = (r << 3);
    g = (g << 2);
    b = (b << 3);
    return 0xff000000 | (r << 16) | (g << 8) | b;
}

static void mpsx_lcd_refresh(MPSXSimpleLCDState *s) {
    uint32_t x;
    uint32_t y;
    uint32_t *dst;
    MemTxResult ret;
    if (!s->surface) {
        return;
    }
    if (s->fb_addr == 0) {
        return;
    }
    dst = surface_data(s->surface);
    for (y = 0; y < s->height; y++) {
        for (x = 0; x < s->width; x++) {
            hwaddr addr;
            addr = s->fb_addr + y * s->stride;
            switch (s->format)
            {
            case LCD_FORMAT_RGB565: {
                uint16_t pixel;
                ret = address_space_read(
                    &address_space_memory,
                    addr + x * 2,
                    MEMTXATTRS_UNSPECIFIED,
                    &pixel,
                    sizeof(pixel)
                );
                if (ret != MEMTX_OK) {
                    qemu_log_mask(LOG_GUEST_ERROR, "lcd framebuffer read failed\n");
                    return;
                }
                dst[y * s->width + x] = mpsx_rgb565_to_argb8888(pixel);
                break;
            }
            case LCD_FORMAT_ARGB8888: {
                uint32_t pixel;
                ret = address_space_read(
                    &address_space_memory,
                    addr + x * 4,
                    MEMTXATTRS_UNSPECIFIED,
                    &pixel,
                    sizeof(pixel)
                );
                if (ret != MEMTX_OK) {
                    qemu_log_mask(LOG_GUEST_ERROR, "lcd framebuffer read failed\n");
                    return;
                }
                dst[y * s->width + x] = pixel;
                break;
            }
            default:
                qemu_log_mask(LOG_UNIMP, "mpsx lcd unsupported format %d\n", s->format);
                return;
            }
        }
    }
    qemu_console_update(s->console, 0, 0, s->width, s->height);
}

static void mpsx_lcd_update(MPSXSimpleLCDState *s) {
    if (!(s->ctrl & LCD_CTRL_ENABLE)) {
        return;
    }
    if (s->status & LCD_STATUS_BUSY) {
        return;
    }
    s->status |= LCD_STATUS_BUSY;
    s->status &= ~LCD_STATUS_DONE;
    mpsx_lcd_refresh(s);
    s->status &= ~LCD_STATUS_BUSY;
    s->status |= LCD_STATUS_DONE;
    if (s->int_enable) {
        s->int_status = 1;
        qemu_set_irq(s->irq, 1);
        qemu_set_irq(s->irq, 0);
    }
}

static void mpsx_lcd_reset(DeviceState *dev) {
    MPSXSimpleLCDState *s = MPSX_SIMPLE_LCD(dev);
    s->ctrl = 0;
    s->status = LCD_STATUS_DONE;
    s->fb_addr = 0;
    s->stride = s->width * 4;
    s->format = LCD_FORMAT_ARGB8888;
    s->int_enable = 0;
    s->int_status = 0;
}

static uint64_t mpsx_lcd_read(void *opaque, hwaddr addr, unsigned size) {
    MPSXSimpleLCDState *s = opaque;
    switch(addr)
    {
    case REG_CTRL:
        return s->ctrl;
    case REG_STATUS:
        return s->status;
    case REG_WIDTH:
        return s->width;
    case REG_HEIGHT:
        return s->height;
    case REG_FB_ADDR:
        return s->fb_addr;
    case REG_STRIDE:
        return s->stride;
    case REG_FORMAT:
        return s->format;
    case REG_INT_EN:
        return s->int_enable;
    case REG_INT_STATUS:
        return s->int_status;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,"mpsx lcd bad read offset=0x%"HWADDR_PRIx"\n", addr);
        return 0;
    }
}

static void mpsx_lcd_write(void *opaque, hwaddr addr, uint64_t value, unsigned size) {
    MPSXSimpleLCDState *s = opaque;
    switch(addr) {
    case REG_CTRL:
        if (value & LCD_CTRL_RESET) {
            mpsx_lcd_reset(DEVICE(s));
            return;
        }
        s->ctrl = value;
        if (value & LCD_CTRL_UPDATE) {
            mpsx_lcd_update(s);
            s->ctrl &= ~LCD_CTRL_UPDATE;
        }
        break;
    case REG_FB_ADDR:
        s->fb_addr = value;
        break;
    case REG_STRIDE:
        s->stride = value;
        break;
    case REG_FORMAT:
        if (value <= LCD_FORMAT_RGB565) {
            s->format = value;
        } else {
            qemu_log_mask(LOG_GUEST_ERROR, "mpsx lcd invalid format=0x%"HWADDR_PRIx"\n", value);
        }
        break;
    case REG_INT_EN:
        s->int_enable = value;
        break;
    case REG_INT_STATUS:
        s->int_status &= ~value;
        break;
    case REG_WIDTH:
    case REG_HEIGHT:
        qemu_log_mask(LOG_GUEST_ERROR, "mpsx lcd width/height readonly\n");
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "mpsx lcd bad write offset=0x%"HWADDR_PRIx"\n", addr);
        break;
    }
}

static const MemoryRegionOps mpsx_lcd_ops = {
    .read = mpsx_lcd_read,
    .write = mpsx_lcd_write,
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

static bool mpsx_lcd_gfx_update(void *opaque) {
    MPSXSimpleLCDState *s = opaque;
    if (s->ctrl & LCD_CTRL_ENABLE) {
        mpsx_lcd_refresh(s);
    }
    return true;
}

static const GraphicHwOps mpsx_lcd_gfx_ops = {
    .gfx_update = mpsx_lcd_gfx_update,
};

static void mpsx_lcd_realize(DeviceState *dev, Error **errp) {
    MPSXSimpleLCDState *s = MPSX_SIMPLE_LCD(dev);
    s->width = 450;
    s->height = 450;
    s->format = LCD_FORMAT_ARGB8888;
    s->stride = s->width * 4;
    s->console = qemu_graphic_console_create(dev, 0, &mpsx_lcd_gfx_ops, s);
    if (!s->console) {
        printf("mpsx simple lcd console create failed\n");
    }
    s->surface = qemu_create_displaysurface(s->width, s->height);
    qemu_console_set_surface(s->console, s->surface);
    qemu_console_update(s->console, 0, 0, s->width, s->height);
    printf("mpsx simple lcd surface format bpp=%d\n", surface_bits_per_pixel(s->surface));
    memory_region_init_io(&s->iomem, OBJECT(dev), &mpsx_lcd_ops, s, "mpsx-simple-lcd", 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);
    mpsx_lcd_reset(dev);
    printf("mpsx simple lcd realize: %dx%d\n", s->width, s->height);
}

static void mpsx_lcd_class_init(ObjectClass *klass, const void *data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = mpsx_lcd_realize;
}

static const TypeInfo mpsx_lcd_type_info = {
    .name = TYPE_MPSX_SIMPLE_LCD,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MPSXSimpleLCDState),
    .class_init = mpsx_lcd_class_init,
};

static void mpsx_lcd_register_types(void) {
    type_register_static(&mpsx_lcd_type_info);
}

type_init(mpsx_lcd_register_types)
