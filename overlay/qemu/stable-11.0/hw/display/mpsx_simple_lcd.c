#include "qemu/osdep.h"
#include "hw/display/mpsx_simple_lcd.h"
#include "hw/core/qdev.h"

#include "system/address-spaces.h"
#include "system/memory.h"

#include "qemu/log.h"

#define REG_WIDTH       0x00
#define REG_HEIGHT      0x04
#define REG_FB_ADDR     0x08
#define REG_UPDATE      0x0c
#define REG_FORMAT      0x10

static void mpsx_lcd_refresh(MPSXSimpleLCDState *s) {
    uint32_t x = 0, y = 0;
    uint32_t *dst = NULL;
    uint32_t pixel = 0;
    MemTxResult ret = MEMTX_OK;

    if (!s->surface) {
        return;
    }
    if (s->fb_addr == 0) {
        return;
    }
    dst = surface_data(s->surface);
    for (y = 0; y < s->height; y++) {
        for (x = 0; x < s->width; x++) {
            ret = address_space_read(
                &address_space_memory,
                s->fb_addr + (y * s->width + x) * 4,
                MEMTXATTRS_UNSPECIFIED,
                &pixel,
                sizeof(pixel)
            );
            if (ret != MEMTX_OK) {
                printf("fb read failed addr=0x%" HWADDR_PRIx "\n", (hwaddr)s->fb_addr);
                return;
            }
            dst[y * s->width + x] = pixel;
        }
    }
    qemu_console_update(s->console, 0, 0, s->width, s->height);
}

static uint64_t mpsx_lcd_read(void *opaque, hwaddr addr, unsigned size) {
    MPSXSimpleLCDState *s = opaque;
    printf("LCD READ addr=0x%" HWADDR_PRIx "\n", addr);
    switch (addr) {
    case REG_WIDTH:
        return s->width;
    case REG_HEIGHT:
        return s->height;
    case REG_FB_ADDR:
        return s->fb_addr;
    case REG_UPDATE:
        return s->update;
    case REG_FORMAT:
        return s->format;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "mpsx-simple-lcd: bad read " HWADDR_FMT_plx "\n", addr);
        return 0;
    }
}

static void mpsx_lcd_write(void *opaque, hwaddr addr, uint64_t value, unsigned size) {
    MPSXSimpleLCDState *s = opaque;
    printf("LCD WRITE addr=0x%" HWADDR_PRIx" value=0x%" PRIx64 "\n", addr, value);
    switch (addr) {
    case REG_FB_ADDR:
        s->fb_addr = value;
        break;
    case REG_UPDATE:
        printf("LCD UPDATE write %lld\n", value);
        s->update = value;
        mpsx_lcd_refresh(s);
        qemu_log("mpsx-simple-lcd update fb=0x%08x\n", s->fb_addr);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "mpsx-simple-lcd: bad write " HWADDR_FMT_plx "\n", addr);
        break;
    }
}

static const MemoryRegionOps mpsx_lcd_ops = {
    .read = mpsx_lcd_read,
    .write = mpsx_lcd_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static bool mpsx_lcd_gfx_update(void *opaque) {
    MPSXSimpleLCDState *s = opaque;
    mpsx_lcd_refresh(s);
    return true;
}

static const GraphicHwOps mpsx_lcd_gfx_ops = {
    .gfx_update = mpsx_lcd_gfx_update,
};

static void mpsx_lcd_realize(DeviceState *dev, Error **errp) {
    MPSXSimpleLCDState *s = MPSX_SIMPLE_LCD(dev);
    s->width = 320;
    s->height = 240;
    s->format = 0;
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
