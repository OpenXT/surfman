/*
 * Copyright (c) 2012 Citrix Systems, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <libvgaemu_int.h>

#define DPRINTF(fmt, ...)                       \
    do {                                        \
        fprintf(stderr, "libvga: "fmt, ## __VA_ARGS__);   \
    } while (0)

#define GET_PLANE(data, p) (((data) >> ((p) * 8)) & 0xff)

#define GMODE_TEXT     0
#define GMODE_GRAPH    1
#define GMODE_BLANK 2

#define RGBA(r, g, b, a) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define RGB(r, g, b) RGBA(r, g, b, 0xff)

/* force some bits to zero */
const uint8_t sr_mask[8] = {
    0x03,
    0x3d,
    0x0f,
    0x3f,
    0x0e,
    0x00,
    0x00,
    0xff,
};
static const uint32_t mask16[16] = {
    0x00000000,
    0x000000ff,
    0x0000ff00,
    0x0000ffff,
    0x00ff0000,
    0x00ff00ff,
    0x00ffff00,
    0x00ffffff,
    0xff000000,
    0xff0000ff,
    0xff00ff00,
    0xff00ffff,
    0xffff0000,
    0xffff00ff,
    0xffffff00,
    0xffffffff,
};

const uint8_t gr_mask[16] = {
    0x0f, /* 0x00 */
    0x0f, /* 0x01 */
    0x0f, /* 0x02 */
    0x1f, /* 0x03 */
    0x03, /* 0x04 */
    0x7b, /* 0x05 */
    0x0f, /* 0x06 */
    0x0f, /* 0x07 */
    0xff, /* 0x08 */
    0x00, /* 0x09 */
    0x00, /* 0x0a */
    0x00, /* 0x0b */
    0x00, /* 0x0c */
    0x00, /* 0x0d */
    0x00, /* 0x0e */
    0x00, /* 0x0f */
};

static const uint32_t color_table_rgb[2][8] = {
    {   /* dark */
        RGB(0x00, 0x00, 0x00),  /* black */
        RGB(0xaa, 0x00, 0x00),  /* red */
        RGB(0x00, 0xaa, 0x00),  /* green */
        RGB(0xaa, 0xaa, 0x00),  /* yellow */
        RGB(0x00, 0x00, 0xaa),  /* blue */
        RGB(0xaa, 0x00, 0xaa),  /* magenta */
        RGB(0x00, 0xaa, 0xaa),  /* cyan */
        RGB(0xaa, 0xaa, 0xaa),  /* white */
    },
    {   /* bright */
        RGB(0x00, 0x00, 0x00),  /* black */
        RGB(0xff, 0x00, 0x00),  /* red */
        RGB(0x00, 0xff, 0x00),  /* green */
        RGB(0xff, 0xff, 0x00),  /* yellow */
        RGB(0x00, 0x00, 0xff),  /* blue */
        RGB(0xff, 0x00, 0xff),  /* magenta */
        RGB(0x00, 0xff, 0xff),  /* cyan */
        RGB(0xff, 0xff, 0xff),  /* white */
    }
};

static uint16_t expand2[256];
static uint32_t expand4[256];
static uint8_t expand4to8[16];

static inline int c6_to_8(int v)
{
    int b;
    v &= 0x3f;
    b = v & 1;
    return (v << 2) | (b << 1) | b;
}

static const uint8_t cursor_glyph[32 * 4] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

s_vga vga_init(unsigned char *buffer, vga_resize_func resize,
               uint32_t stride_align, void *priv)
{
    unsigned int i, j, v, b;

    s_vga vga = NULL;

    vga = malloc(sizeof (*vga));

    if (!vga)
        return NULL;

    memset(vga, 0, sizeof (*vga));

    for(i = 0;i < 256; i++) {
        v = 0;
        for(j = 0; j < 8; j++) {
            v |= ((i >> j) & 1) << (j * 4);
        }
        expand4[i] = v;

        v = 0;
        for(j = 0; j < 4; j++) {
            v |= ((i >> (2 * j)) & 3) << (j * 4);
        }
        expand2[i] = v;
    }

    for(i = 0; i < 16; i++) {
        v = 0;
        for(j = 0; j < 4; j++) {
            b = ((i >> j) & 1);
            v |= b << (2 * j);
            v |= b << (2 * j + 1);
        }
        expand4to8[i] = v;
    }

    vga->buffer = buffer;
    vga->graphic_mode = -1; /* force full update */
    vga->resize = resize;
    vga->stride_align = stride_align;
    vga->priv = priv;

    return vga;
}

static const s_vga_range vga_ranges[] = {
    /* PIO */
    { .start = 0x3b4, .length = 2, .is_mmio = 0 },
    { .start = 0x3ba, .length = 1, .is_mmio = 0 },
    { .start = 0x3c0, .length = 16, .is_mmio = 0 },
    { .start = 0x3d4, .length = 2, .is_mmio = 0 },
    { .start = 0x3da, .length = 1, .is_mmio = 0 },
    /* MMIO */
    { .start = 0xa0000, .length = 0x20000, .is_mmio = 1 },
    /* END */
    { 0, 0, 0 }
};

const s_vga_range *vga_ranges_get()
{
    return vga_ranges;
}

void vga_destroy(s_vga vga)
{
    free(vga);
}

/* Handle dirty vram */
#define PAGE_ALIGN(addr) (((addr) + XC_PAGE_SIZE - 1) & XC_PAGE_MASK)

static int vga_vram_get_dirty(s_vga s, uint64_t start, uint32_t size)
{
    uint64_t pstart = start >> XC_PAGE_SHIFT;
    uint64_t pend = PAGE_ALIGN(start + size) >> XC_PAGE_SHIFT;
    int dirty = 0;
    uint64_t offset = 0;
    uint64_t shift = 0;

    for (; pstart < pend; pstart++)
    {
        offset = pstart / VRAM_DIRTY_ENTRY_SIZE;
        shift = pstart % VRAM_DIRTY_ENTRY_SIZE;
        dirty |= (s->dirty_vram[offset] >> shift) & 0x1;
    }

    return dirty;
}

static void vga_vram_set_dirty(s_vga s, uint64_t start, uint32_t size,
                               int flag)
{
    uint64_t pstart = start >> XC_PAGE_SHIFT;
    uint64_t pend = PAGE_ALIGN(start + size) >> XC_PAGE_SHIFT;
    uint64_t offset = 0;
    uint64_t shift = 0;

    for (; pstart < pend; pstart++)
    {
        offset = pstart / VRAM_DIRTY_ENTRY_SIZE;
        shift = pstart % VRAM_DIRTY_ENTRY_SIZE;
        if (flag)
            s->dirty_vram[offset] |= ((uint32_t)1 << shift);
        else
            s->dirty_vram[offset] &= ~((uint32_t)1 << shift);
    }
}

/* For the moment we use dump method */
static void vga_update_retrace_info(s_vga s)
{
}

static uint8_t vga_retrace_info(s_vga s)
{
    return s->st01 ^ (ST01_V_RETRACE | ST01_DISP_ENABLE);
}

static void vga_update_memory_access(s_vga s)
{
    uint64_t base, offset, size;

    if ((s->sr[VGA_SEQ_PLANE_WRITE] & VGA_SR02_ALL_PLANES) ==
        VGA_SR02_ALL_PLANES && s->sr[VGA_SEQ_MEMORY_MODE] & VGA_SR04_CHN_4M)
    {
        offset = 0;
        switch ((s->gr[VGA_GFX_MISC] >> 2) & 3)
        {
        case 0:
            base = 0xa0000;
            size = 0x20000;
            break;
        case 1:
            base = 0xa0000;
            size = 0x10000;
            offset = s->bank_offset;
            break;
        case 2:
            base = 0xb0000;
            size = 0x8000;
            break;
        case 3:
        default:
            base = 0xb8000;
            size = 0x8000;
            break;
        }
    }
}

static void vga_get_offsets(s_vga s,
                            uint32_t *pline_offset,
                            uint32_t *pstart_addr,
                            uint32_t *pline_compare)
{
    uint32_t start_addr, line_offset, line_compare;

    /* compute line_offset in bytes */
    line_offset = s->cr[VGA_CRTC_OFFSET];
    line_offset <<= 3;

    /* starting address */
    start_addr = s->cr[VGA_CRTC_START_LO] |
        (s->cr[VGA_CRTC_START_HI] << 8);

    /* line compare */
    line_compare = s->cr[VGA_CRTC_LINE_COMPARE] |
        ((s->cr[VGA_CRTC_OVERFLOW] & 0x10) << 4) |
        ((s->cr[VGA_CRTC_MAX_SCAN] & 0x40) << 3);
    *pline_offset = line_offset;
    *pstart_addr = start_addr;
    *pline_compare = line_compare;
}

/* update start_addr and line_offset. Return TRUE if modified */
static int vga_update_basic_params(s_vga s)
{
    int full_update;
    uint32_t start_addr, line_offset, line_compare;

    full_update = 0;

    vga_get_offsets(s, &line_offset, &start_addr,  &line_compare);

    if (line_offset != s->line_offset ||
        start_addr != s->start_addr ||
        line_compare != s->line_compare)
    {
        s->line_offset = line_offset;
        s->start_addr = start_addr;
        s->line_compare = line_compare;
        full_update = 1;
    }
    return full_update;
}

static void vga_get_resolution(s_vga s, int *pwidth, int *pheight)
{
    int width, height;

    width = (s->cr[VGA_CRTC_H_DISP] + 1) * 8;
    height = s->cr[VGA_CRTC_V_DISP_END] |
        ((s->cr[VGA_CRTC_OVERFLOW] & 0x02) << 7) |
        ((s->cr[VGA_CRTC_OVERFLOW] & 0x40) << 3);
    height = (height + 1);
    *pwidth = width;
    *pheight = height;
}

uint32_t vga_ioport_readb(s_vga s, uint64_t addr)
{
    int val;
    int index;

    switch (addr)
    {
    case VGA_ATT_W:
        if (s->ar_flip_flop == 0)
            val = s->ar_index;
        else
            val = 0;
        break;
    case VGA_ATT_R:
        index = s->ar_index & 0x1f;
        if (index < VGA_ATT_C)
            val = s->ar[index];
        else
            val = 0;
        break;
    case VGA_MIS_W:
        val = s->st00;
        break;
    case VGA_SEQ_I:
        val = s->sr_index;
        break;
    case VGA_SEQ_D:
        val = s->sr[s->sr_index];
        break;
    case VGA_PEL_IR:
        val = s->dac_state;
        break;
    case VGA_PEL_IW:
        val = s->dac_write_index;
        break;
    case VGA_PEL_D:
        val = s->palette[s->dac_read_index * 3 + s->dac_sub_index];
        if (++s->dac_sub_index == 3)
        {
            s->dac_sub_index = 0;
            s->dac_read_index++;
        }
        break;
    case VGA_FTC_R:
        val = s->fcr;
        break;
    case VGA_MIS_R:
        val = s->msr;
        break;
    case VGA_GFX_I:
        val = s->gr_index;
        break;
    case VGA_GFX_D:
        val = s->gr[s->gr_index];
        break;
    case VGA_CRT_IM:
    case VGA_CRT_IC:
        val = s->cr_index;
        break;
    case VGA_CRT_DM:
    case VGA_CRT_DC:
        val = s->cr[s->cr_index];
        break;
    case VGA_IS1_RM:
    case VGA_IS1_RC:
        /* just toggle to fool polling */
        val = s->st01 = vga_retrace_info(s);
        s->ar_flip_flop = 0;
        break;
    default:
        val = 0x00;
    }

    return val;
}

void vga_ioport_writeb(s_vga s, uint64_t addr, uint32_t val)
{
    int index;

    switch (addr)
    {
    case VGA_ATT_W:
        if (s->ar_flip_flop == 0)
        {
            val &= 0x3f;
            s->ar_index = val;
        }
        else
        {
            index = s->ar_index & 0x1f;
            switch (index)
            {
            case VGA_ATC_PALETTE0 ... VGA_ATC_PALETTEF:
                s->ar[index] = val & 0x3f;
                break;
            case VGA_ATC_MODE:
                s->ar[index] = val & ~0x10;
                break;
            case VGA_ATC_OVERSCAN:
                s->ar[index] = val;
                break;
            case VGA_ATC_PLANE_ENABLE:
                s->ar[index] = val & ~0xc0;
                break;
            case VGA_ATC_PEL:
                s->ar[index] = val & ~0xf0;
                break;
            case VGA_ATC_COLOR_PAGE:
                s->ar[index] = val & ~0xf0;
                break;
            default:
                break;
            }
        }
        s->ar_flip_flop ^= 1;
        break;
    case VGA_MIS_W:
        s->msr = val & ~0x10;
        vga_update_retrace_info(s);
        break;
    case VGA_SEQ_I:
        s->sr_index = val & 7;
        break;
    case VGA_SEQ_D:
        s->sr[s->sr_index] = val & sr_mask[s->sr_index];
        if (s->sr_index == VGA_SEQ_CLOCK_MODE)
            vga_update_retrace_info(s);
        vga_update_memory_access(s);
        break;
    case VGA_PEL_IR:
        s->dac_read_index = val;
        s->dac_sub_index = 0;
        s->dac_state = 3;
        break;
    case VGA_PEL_IW:
        s->dac_write_index = val;
        s->dac_sub_index = 0;
        s->dac_state = 0;
        break;
    case VGA_PEL_D:
        s->dac_cache[s->dac_sub_index] = val;
        if (++s->dac_sub_index == 3)
        {
            memcpy(&s->palette[s->dac_write_index * 3], s->dac_cache, 3);
            s->dac_sub_index = 0;
            s->dac_write_index++;
        }
        break;
    case VGA_GFX_I:
        s->gr_index = val & 0x0f;
        break;
    case VGA_GFX_D:
        s->gr[s->gr_index] = val & gr_mask[s->gr_index];
        vga_update_memory_access(s);
        break;
    case VGA_CRT_IM:
    case VGA_CRT_IC:
        s->cr_index = val;
        break;
    case VGA_CRT_DM:
    case VGA_CRT_DC:
        /* handle CR0-7 protection */
        if ((s->cr[VGA_CRTC_V_SYNC_END] & VGA_CR11_LOCK_CR0_CR7) &&
            s->cr_index <= VGA_CRTC_OVERFLOW)
        {
            /* can always write bit 4 of CR7 */
            if (s->cr_index == VGA_CRTC_OVERFLOW)
                s->cr[VGA_CRTC_OVERFLOW] = (s->cr[VGA_CRTC_OVERFLOW] & ~0x10) |
                    (val & 0x10);
            return;
        }
        s->cr[s->cr_index] = val;

        switch (s->cr_index)
        {
        case VGA_CRTC_H_TOTAL:
        case VGA_CRTC_H_SYNC_START:
        case VGA_CRTC_H_SYNC_END:
        case VGA_CRTC_V_TOTAL:
        case VGA_CRTC_OVERFLOW:
        case VGA_CRTC_V_SYNC_END:
        case VGA_CRTC_MODE:
            vga_update_retrace_info(s);
            break;
        }
        break;
    case VGA_IS1_RM:
    case VGA_IS1_RC:
        s->fcr = val & 0x10;
        break;
    }
}

static uint32_t vga_mem_readb(s_vga s, uint64_t addr)
{
    int memory_map_mode, plane;
    uint32_t ret = 0;

    /* convert to VGA memory offset */
    memory_map_mode = (s->gr[VGA_GFX_MISC] >> 2) & 3;
    addr &= 0x1ffff;
    switch(memory_map_mode)
    {
    case 0:
        break;
    case 1:
        if (addr >= 0x10000)
            return 0xff;
        addr += s->bank_offset;
        break;
    case 2:
        addr -= 0x10000;
        if (addr >= 0x8000)
            return 0xff;
        break;
    default:
    case 3:
        addr -= 0x18000;
        if (addr >= 0x8000)
            return 0xff;
        break;
    }

    if (s->sr[VGA_SEQ_MEMORY_MODE] & VGA_SR04_CHN_4M)
        /* chain 4 mode : simplest access */
        ret = s->vram[addr];
    else if (s->gr[VGA_GFX_MODE] & 0x10)
    {
        /* odd/even mode (aka text mode mapping) */
        plane = (s->gr[VGA_GFX_PLANE_READ] & 2) | (addr & 1);
        ret = s->vram[((addr & ~1) << 1) | plane];
    }
    else
    {
        /* standard VGA latched access */
        s->latch = ((uint32_t *)s->vram)[addr];

        if (!(s->gr[VGA_GFX_MODE] & 0x08))
        {
            /* read mode 0 */
            plane = s->gr[VGA_GFX_PLANE_READ];
            ret = GET_PLANE(s->latch, plane);
        }
        else
        {
            /* read mode 1 */
            ret = (s->latch ^ mask16[s->gr[VGA_GFX_COMPARE_VALUE]]) &
                mask16[s->gr[VGA_GFX_COMPARE_MASK]];
            ret |= ret >> 16;
            ret |= ret >> 8;
            ret = (~ret) & 0xff;
        }
    }

    return ret;
}

static void vga_mem_writeb(s_vga s, uint64_t addr, uint32_t val)
{
    int memory_map_mode, plane, write_mode, b, func_select, mask;
    uint32_t write_mask, bit_mask, set_mask;

   /* convert to VGA memory offset */
    memory_map_mode = (s->gr[VGA_GFX_MISC] >> 2) & 3;
    addr &= 0x1ffff;

    switch(memory_map_mode)
    {
    case 0:
        break;
    case 1:
        if (addr >= 0x10000)
            return;
        addr += s->bank_offset;
        break;
    case 2:
        addr -= 0x10000;
        if (addr >= 0x8000)
            return;
        break;
    default:
    case 3:
        addr -= 0x18000;
        if (addr >= 0x8000)
            return;
        break;
    }

    if (s->sr[VGA_SEQ_MEMORY_MODE] & VGA_SR04_CHN_4M)
    {
        /* chain 4 mode : simplest access */
        plane = addr & 3;
        mask = (1 << plane);
        if (s->sr[VGA_SEQ_PLANE_WRITE] & mask)
        {
            addr = ((addr & ~1) << 1) | plane;
            s->vram[addr] = val;
            s->plane_updated |= mask; /* only used to detect font change */
            vga_vram_set_dirty(s, addr, 1, 1);
        }
    }
    else if (s->gr[VGA_GFX_MODE] & 0x10)
    {
        /* odd/even mode (aka text mode mapping) */
        plane = (s->gr[VGA_GFX_PLANE_READ] & 2) | (addr & 1);
        mask = (1 << plane);
        if (s->sr[VGA_SEQ_PLANE_WRITE] & mask) {
            addr = ((addr & ~1) << 1) | plane;
            s->vram[addr] = val;
            s->plane_updated |= mask; /* only used to detect font change */
            vga_vram_set_dirty(s, addr, 1, 1);
        }
    }
    else
    {
        /* standard VGA latched access */
        write_mode = s->gr[VGA_GFX_MODE] & 3;
        switch(write_mode)
        {
        default:
        case 0:
            /* rotate */
            b = s->gr[VGA_GFX_DATA_ROTATE] & 7;
            val = ((val >> b) | (val << (8 - b))) & 0xff;
            val |= val << 8;
            val |= val << 16;

            /* apply set/reset mask */
            set_mask = mask16[s->gr[VGA_GFX_SR_ENABLE]];
            val = (val & ~set_mask) |
                (mask16[s->gr[VGA_GFX_SR_VALUE]] & set_mask);
            bit_mask = s->gr[VGA_GFX_BIT_MASK];
            break;
        case 1:
            val = s->latch;
            goto do_write;
        case 2:
            val = mask16[val & 0x0f];
            bit_mask = s->gr[VGA_GFX_BIT_MASK];
            break;
        case 3:
            /* rotate */
            b = s->gr[VGA_GFX_DATA_ROTATE] & 7;
            val = (val >> b) | (val << (8 - b));

            bit_mask = s->gr[VGA_GFX_BIT_MASK] & val;
            val = mask16[s->gr[VGA_GFX_SR_VALUE]];
            break;
        }

        /* apply logical operation */
        func_select = s->gr[VGA_GFX_DATA_ROTATE] >> 3;
        switch(func_select)
        {
        case 0:
        default:
            /* nothing to do */
            break;
        case 1:
            /* and */
            val &= s->latch;
            break;
        case 2:
            /* or */
            val |= s->latch;
            break;
        case 3:
            /* xor */
            val ^= s->latch;
            break;
        }

        /* apply bit mask */
        bit_mask |= bit_mask << 8;
        bit_mask |= bit_mask << 16;
        val = (val & bit_mask) | (s->latch & ~bit_mask);

    do_write:
        /* mask data according to sr[2] */
        mask = s->sr[VGA_SEQ_PLANE_WRITE];
        s->plane_updated |= mask; /* only used to detect font change */
        write_mask = mask16[mask];
        ((uint32_t *)s->vram)[addr] =
            (((uint32_t *)s->vram)[addr] & ~write_mask) |
            (val & write_mask);
        vga_vram_set_dirty(s, addr << 2, sizeof (uint32_t), 1);
    }
}

uint32_t vga_ioport_read(s_vga s, uint64_t addr, uint32_t size)
{
    uint32_t i = 0;
    uint32_t res = 0;

    for (i = 0; i < size; i++)
    {
        res |= vga_ioport_readb(s, addr + i) << (i * 8);
    }

    return res;
}

void vga_ioport_write(s_vga s, uint64_t addr, uint32_t val, uint32_t size)
{
    uint32_t i = 0;

    for (i = 0; i < size; i++)
        vga_ioport_writeb(s, addr + i, (val >> (i * 8)) & 0xff);
}

uint32_t vga_mem_read(s_vga s, uint64_t addr, uint32_t size)
{
    uint32_t i = 0;
    uint64_t res = 0;

    for (i = 0; i < size; i++)
        res |= vga_mem_readb(s, addr + i) << (i * 8);

    return res;
}

# define VGA_BPP 4

/* return true if the palette was modified */
static int vga_update_palette16(s_vga s)
{
    int full_update, i;
    uint32_t v, col, *palette;

    full_update = 0;
    palette = s->last_palette;
    for(i = 0; i < 16; i++) {
        v = s->ar[i];
        if (s->ar[VGA_ATC_MODE] & 0x80) {
            v = ((s->ar[VGA_ATC_COLOR_PAGE] & 0xf) << 4) | (v & 0xf);
        } else {
            v = ((s->ar[VGA_ATC_COLOR_PAGE] & 0xc) << 4) | (v & 0x3f);
        }
        v = v * 3;
        col = RGB(c6_to_8(s->palette[v]),
                  c6_to_8(s->palette[v + 1]),
                  c6_to_8(s->palette[v + 2]));
        if (col != palette[i])
        {
            full_update = 1;
            palette[i] = col;
        }
    }

    return full_update;
}

static void vga_get_text_resolution(s_vga s, int *pwidth, int *pheight,
                                    int *pcwidth, int *pcheight)
{
    int width, cwidth, height, cheight;

    /* total width & height */
    cheight = (s->cr[VGA_CRTC_MAX_SCAN] & 0x1f) + 1;
    cwidth = 8;
    if (!(s->sr[VGA_SEQ_CLOCK_MODE] & VGA_SR01_CHAR_CLK_8DOTS)) {
        cwidth = 9;
    }
    if (s->sr[VGA_SEQ_CLOCK_MODE] & 0x08) {
        cwidth = 16; /* NOTE: no 18 pixel wide */
    }
    width = (s->cr[VGA_CRTC_H_DISP] + 1);
    if (s->cr[VGA_CRTC_V_TOTAL] == 100) {
        /* ugly hack for CGA 160x100x16 - explain me the logic */
        height = 100;
    } else {
        height = s->cr[VGA_CRTC_V_DISP_END] |
            ((s->cr[VGA_CRTC_OVERFLOW] & 0x02) << 7) |
            ((s->cr[VGA_CRTC_OVERFLOW] & 0x40) << 3);
        height = (height + 1) / cheight;
    }

    *pwidth = width;
    *pheight = height;
    *pcwidth = cwidth;
    *pcheight = cheight;
}

static inline void vga_draw_glyph_line(uint8_t *d,
                                       uint32_t font_data,
                                       uint32_t xorcol,
                                       uint32_t bgcol)
{
    ((uint32_t *)d)[0] = (-((font_data >> 7)) & xorcol) ^ bgcol;
    ((uint32_t *)d)[1] = (-((font_data >> 6) & 1) & xorcol) ^ bgcol;
    ((uint32_t *)d)[2] = (-((font_data >> 5) & 1) & xorcol) ^ bgcol;
    ((uint32_t *)d)[3] = (-((font_data >> 4) & 1) & xorcol) ^ bgcol;
    ((uint32_t *)d)[4] = (-((font_data >> 3) & 1) & xorcol) ^ bgcol;
    ((uint32_t *)d)[5] = (-((font_data >> 2) & 1) & xorcol) ^ bgcol;
    ((uint32_t *)d)[6] = (-((font_data >> 1) & 1) & xorcol) ^ bgcol;
    ((uint32_t *)d)[7] = (-((font_data >> 0) & 1) & xorcol) ^ bgcol;
}

static void vga_draw_glyph8(uint8_t *d, int linesize,
                            const uint8_t *font_ptr, int h,
                            uint32_t fgcol, uint32_t bgcol)
{
    uint32_t font_data, xorcol;

    xorcol = bgcol ^ fgcol;
    do {
        font_data = font_ptr[0];
        vga_draw_glyph_line(d, font_data, xorcol, bgcol);
        font_ptr += 4;
        d += linesize;
    } while (--h);
}

static void vga_draw_glyph16(uint8_t *d, int linesize,
                             const uint8_t *font_ptr, int h,
                             uint32_t fgcol, uint32_t bgcol)
{
    uint32_t font_data, xorcol;

    xorcol = bgcol ^ fgcol;
    do {
        font_data = font_ptr[0];
        vga_draw_glyph_line(d, expand4to8[font_data >> 4], xorcol, bgcol);
        vga_draw_glyph_line(d + 8 * VGA_BPP, expand4to8[font_data & 0x0f],
                            xorcol, bgcol);
        font_ptr += 4;
        d += linesize;
    } while (--h);
}

static void vga_draw_glyph9(uint8_t *d, int linesize,
                            const uint8_t *font_ptr, int h,
                            uint32_t fgcol, uint32_t bgcol, int dup9)
{
    uint32_t font_data, xorcol, v;

    xorcol = bgcol ^ fgcol;
    do {
        font_data = font_ptr[0];
        ((uint32_t *)d)[0] = (-((font_data >> 7)) & xorcol) ^ bgcol;
        ((uint32_t *)d)[1] = (-((font_data >> 6) & 1) & xorcol) ^ bgcol;
        ((uint32_t *)d)[2] = (-((font_data >> 5) & 1) & xorcol) ^ bgcol;
        ((uint32_t *)d)[3] = (-((font_data >> 4) & 1) & xorcol) ^ bgcol;
        ((uint32_t *)d)[4] = (-((font_data >> 3) & 1) & xorcol) ^ bgcol;
        ((uint32_t *)d)[5] = (-((font_data >> 2) & 1) & xorcol) ^ bgcol;
        ((uint32_t *)d)[6] = (-((font_data >> 1) & 1) & xorcol) ^ bgcol;
        v = (-((font_data >> 0) & 1) & xorcol) ^ bgcol;
        ((uint32_t *)d)[7] = v;
        if (dup9)
            ((uint32_t *)d)[8] = v;
        else
            ((uint32_t *)d)[8] = bgcol;
        font_ptr += 4;
        d += linesize;
    } while (--h);
}

static uint32_t vga_get_linesize(s_vga s)
{
    /* Fix align on stride_align */
    return (s->last_scr_width * VGA_BPP + s->stride_align - 1)
        & (~(s->stride_align - 1));
}

static void vga_draw_text(s_vga s, int full_update)
{
    uint32_t cx, cy, cheight, cw, ch, cattr, height, width, ch_attr;
    uint32_t linesize, x_incr, line, line1;
    uint32_t offset, fgcol, bgcol, v, cursor_offset;
    uint8_t *d1, *d, *src, *dest, *cursor_ptr;
    const uint8_t *font_ptr, *font_base[2];
    int dup9, line_offset;
    uint32_t *palette;
    uint32_t *ch_attr_ptr;

    /* compute font data address (in plane 2) */
    v = s->sr[VGA_SEQ_CHARACTER_MAP];
    offset = (((v >> 4) & 1) | ((v << 1) & 6)) * 8192 * 4 + 2;
    if (offset != s->font_offsets[0])
    {
        s->font_offsets[0] = offset;
        full_update = 1;
    }
    font_base[0] = s->vram + offset;

    offset = (((v >> 5) & 1) | ((v >> 1) & 6)) * 8192 * 4 + 2;
    font_base[1] = s->vram + offset;
    if (offset != s->font_offsets[1])
    {
        s->font_offsets[1] = offset;
        full_update = 1;
    }

    if (s->plane_updated & (1 << 2))
    {
        /* if the plane 2 was modified since the last display, it
           indicates the font may have been modified */
        s->plane_updated = 0;
        full_update = 1;
    }
    full_update |= vga_update_basic_params(s);

    line_offset = s->line_offset;

    vga_get_text_resolution(s, &width, &height, &cw, &cheight);
    if ((height * width) > CH_ATTR_SIZE)
        /* better than nothing: exit if transient size is too big */
        return;

    if (width != s->last_width || height != s->last_height ||
        cw != s->last_cw || cheight != s->last_ch)
    {
        s->last_scr_width = width * cw;
        s->last_scr_height = height * cheight;
        s->buffer = s->resize(s->last_scr_width, s->last_scr_height,
                              vga_get_linesize(s), s->priv);
        if (!s->buffer)
            return;
        s->last_width = width;
        s->last_height = height;
        s->last_ch = cheight;
        s->last_cw = cw;
        full_update = 1;
    }
    full_update |= vga_update_palette16(s);
    palette = s->last_palette;
    x_incr = cw * 4;

    cursor_offset = ((s->cr[VGA_CRTC_CURSOR_HI] << 8) |
                     s->cr[VGA_CRTC_CURSOR_LO]) - s->start_addr;
    if (cursor_offset != s->cursor_offset ||
        s->cr[VGA_CRTC_CURSOR_START] != s->cursor_start ||
        s->cr[VGA_CRTC_CURSOR_END] != s->cursor_end)
    {
      /* if the cursor position changed, we update the old and new
         chars */
        if (s->cursor_offset < CH_ATTR_SIZE)
            s->last_ch_attr[s->cursor_offset] = -1;
        if (cursor_offset < CH_ATTR_SIZE)
            s->last_ch_attr[cursor_offset] = -1;
        s->cursor_offset = cursor_offset;
        s->cursor_start = s->cr[VGA_CRTC_CURSOR_START];
        s->cursor_end = s->cr[VGA_CRTC_CURSOR_END];
    }
    cursor_ptr = s->vram + (s->start_addr + cursor_offset) * 4;

    dest = s->buffer;
    linesize = vga_get_linesize(s);
    ch_attr_ptr = s->last_ch_attr;
    line = 0;
    offset = s->start_addr * 4;
    for(cy = 0; cy < height; cy++) {
        d1 = dest;
        src = s->vram + offset;
        for(cx = 0; cx < width; cx++) {
            ch_attr = *(uint16_t *)src;
            if (full_update || ch_attr != *ch_attr_ptr) {
                s->refresh = 1;
                *ch_attr_ptr = ch_attr;
                ch = ch_attr & 0xff;
                cattr = ch_attr >> 8;
                font_ptr = font_base[(cattr >> 3) & 1];
                font_ptr += 32 * 4 * ch;
                bgcol = palette[cattr >> 4];
                fgcol = palette[cattr & 0x0f];
                if (cw != 9) {
                    if (cw == 16)
                        vga_draw_glyph16(d1, linesize, font_ptr, cheight,
                                         fgcol, bgcol);
                    else
                        vga_draw_glyph8(d1, linesize, font_ptr, cheight,
                                        fgcol, bgcol);
                } else {
                    dup9 = 0;
                    if (ch >= 0xb0 && ch <= 0xdf &&
                        (s->ar[VGA_ATC_MODE] & 0x04)) {
                        dup9 = 1;
                    }
                    vga_draw_glyph9(d1, linesize,
                                    font_ptr, cheight, fgcol, bgcol, dup9);
                }
                if (src == cursor_ptr &&
                    !(s->cr[VGA_CRTC_CURSOR_START] & 0x20)) {
                    uint32_t line_start, line_last, h;
                    /* draw the cursor */
                    line_start = s->cr[VGA_CRTC_CURSOR_START] & 0x1f;
                    line_last = s->cr[VGA_CRTC_CURSOR_END] & 0x1f;
                    /* XXX: check that */
                    if (line_last > cheight - 1)
                        line_last = cheight - 1;
                    if (line_last >= line_start && line_start < cheight) {
                        h = line_last - line_start + 1;
                        d = d1 + linesize * line_start;
                        if (cw != 9) {
                            if (cw == 16)
                                vga_draw_glyph16(d, linesize, cursor_glyph,
                                                 h, fgcol, bgcol);
                            else
                                vga_draw_glyph8(d, linesize,
                                                cursor_glyph, h, fgcol, bgcol);
                        } else {
                            vga_draw_glyph9(d, linesize,
                                            cursor_glyph, h, fgcol, bgcol, 1);
                        }
                    }
                }
            }
            d1 += x_incr;
            src += 4;
            ch_attr_ptr++;
        }
        dest += linesize * cheight;
        line1 = line + cheight;
        offset += line_offset;
        if (line < s->line_compare && line1 >= s->line_compare)
            offset = 0;
        line = line1;
    }
}

typedef void vga_draw_line_func(s_vga s1, uint8_t *d,
                                const uint8_t *s, int width);

#define PUT_PIXEL2(d, n, v) \
((uint32_t *)d)[2*(n)] = ((uint32_t *)d)[2*(n)+1] = (v)

/*
 * 4 color mode
 */
static void vga_draw_line2(s_vga s1, uint8_t *d,
                           const uint8_t *s, int width)
{
    uint32_t plane_mask, *palette, data, v;
    int x;

    palette = s1->last_palette;
    plane_mask = mask16[s1->ar[VGA_ATC_PLANE_ENABLE] & 0xf];
    width >>= 3;
    for(x = 0; x < width; x++) {
        data = ((uint32_t *)s)[0];
        data &= plane_mask;
        v = expand2[GET_PLANE(data, 0)];
        v |= expand2[GET_PLANE(data, 2)] << 2;
        ((uint32_t *)d)[0] = palette[v >> 12];
        ((uint32_t *)d)[1] = palette[(v >> 8) & 0xf];
        ((uint32_t *)d)[2] = palette[(v >> 4) & 0xf];
        ((uint32_t *)d)[3] = palette[(v >> 0) & 0xf];

        v = expand2[GET_PLANE(data, 1)];
        v |= expand2[GET_PLANE(data, 3)] << 2;
        ((uint32_t *)d)[4] = palette[v >> 12];
        ((uint32_t *)d)[5] = palette[(v >> 8) & 0xf];
        ((uint32_t *)d)[6] = palette[(v >> 4) & 0xf];
        ((uint32_t *)d)[7] = palette[(v >> 0) & 0xf];
        d += VGA_BPP * 8;
        s += 4;
    }
}

/*
 * 4 color mode, dup2 horizontal
 */
static void vga_draw_line2d2(s_vga s1, uint8_t *d,
                             const uint8_t *s, int width)
{
    uint32_t plane_mask, *palette, data, v;
    int x;

    palette = s1->last_palette;
    plane_mask = mask16[s1->ar[VGA_ATC_PLANE_ENABLE] & 0xf];
    width >>= 3;
    for(x = 0; x < width; x++) {
        data = ((uint32_t *)s)[0];
        data &= plane_mask;
        v = expand2[GET_PLANE(data, 0)];
        v |= expand2[GET_PLANE(data, 2)] << 2;
        PUT_PIXEL2(d, 0, palette[v >> 12]);
        PUT_PIXEL2(d, 1, palette[(v >> 8) & 0xf]);
        PUT_PIXEL2(d, 2, palette[(v >> 4) & 0xf]);
        PUT_PIXEL2(d, 3, palette[(v >> 0) & 0xf]);

        v = expand2[GET_PLANE(data, 1)];
        v |= expand2[GET_PLANE(data, 3)] << 2;
        PUT_PIXEL2(d, 4, palette[v >> 12]);
        PUT_PIXEL2(d, 5, palette[(v >> 8) & 0xf]);
        PUT_PIXEL2(d, 6, palette[(v >> 4) & 0xf]);
        PUT_PIXEL2(d, 7, palette[(v >> 0) & 0xf]);
        d += VGA_BPP * 16;
        s += 4;
    }
}

/*
 * 16 color mode
 */
static void vga_draw_line4(s_vga s1, uint8_t *d,
                           const uint8_t *s, int width)
{
    uint32_t plane_mask, data, v, *palette;
    int x;

    palette = s1->last_palette;
    plane_mask = mask16[s1->ar[VGA_ATC_PLANE_ENABLE] & 0xf];
    width >>= 3;
    for(x = 0; x < width; x++) {
        data = ((uint32_t *)s)[0];
        data &= plane_mask;
        v = expand4[GET_PLANE(data, 0)];
        v |= expand4[GET_PLANE(data, 1)] << 1;
        v |= expand4[GET_PLANE(data, 2)] << 2;
        v |= expand4[GET_PLANE(data, 3)] << 3;
        ((uint32_t *)d)[0] = palette[v >> 28];
        ((uint32_t *)d)[1] = palette[(v >> 24) & 0xf];
        ((uint32_t *)d)[2] = palette[(v >> 20) & 0xf];
        ((uint32_t *)d)[3] = palette[(v >> 16) & 0xf];
        ((uint32_t *)d)[4] = palette[(v >> 12) & 0xf];
        ((uint32_t *)d)[5] = palette[(v >> 8) & 0xf];
        ((uint32_t *)d)[6] = palette[(v >> 4) & 0xf];
        ((uint32_t *)d)[7] = palette[(v >> 0) & 0xf];
        d += VGA_BPP * 8;
        s += 4;
    }
}

/*
 * 16 color mode, dup2 horizontal
 */
static void vga_draw_line4d2(s_vga s1, uint8_t *d,
                             const uint8_t *s, int width)
{
    uint32_t plane_mask, data, v, *palette;
    int x;

    palette = s1->last_palette;
    plane_mask = mask16[s1->ar[VGA_ATC_PLANE_ENABLE] & 0xf];
    width >>= 3;
    for(x = 0; x < width; x++) {
        data = ((uint32_t *)s)[0];
        data &= plane_mask;
        v = expand4[GET_PLANE(data, 0)];
        v |= expand4[GET_PLANE(data, 1)] << 1;
        v |= expand4[GET_PLANE(data, 2)] << 2;
        v |= expand4[GET_PLANE(data, 3)] << 3;
        PUT_PIXEL2(d, 0, palette[v >> 28]);
        PUT_PIXEL2(d, 1, palette[(v >> 24) & 0xf]);
        PUT_PIXEL2(d, 2, palette[(v >> 20) & 0xf]);
        PUT_PIXEL2(d, 3, palette[(v >> 16) & 0xf]);
        PUT_PIXEL2(d, 4, palette[(v >> 12) & 0xf]);
        PUT_PIXEL2(d, 5, palette[(v >> 8) & 0xf]);
        PUT_PIXEL2(d, 6, palette[(v >> 4) & 0xf]);
        PUT_PIXEL2(d, 7, palette[(v >> 0) & 0xf]);
        d += VGA_BPP * 16;
        s += 4;
    }
}

/*
 * 256 color mode, double pixels
 *
 * XXX: add plane_mask support (never used in standard VGA modes)
 */
static void vga_draw_line8d2(s_vga s1, uint8_t *d,
                             const uint8_t *s, int width)
{
    uint32_t *palette;
    int x;

    palette = s1->last_palette;
    width >>= 3;
    for(x = 0; x < width; x++) {
        PUT_PIXEL2(d, 0, palette[s[0]]);
        PUT_PIXEL2(d, 1, palette[s[1]]);
        PUT_PIXEL2(d, 2, palette[s[2]]);
        PUT_PIXEL2(d, 3, palette[s[3]]);
        d += VGA_BPP * 8;
        s += 4;
    }
}

/*
 * standard 256 color mode
 *
 * XXX: add plane_mask support (never used in standard VGA modes)
 */
static void vga_draw_line8(s_vga s1, uint8_t *d,
                           const uint8_t *s, int width)
{
    uint32_t *palette;
    int x;

    palette = s1->last_palette;
    width >>= 3;
    for(x = 0; x < width; x++) {
        ((uint32_t *)d)[0] = palette[s[0]];
        ((uint32_t *)d)[1] = palette[s[1]];
        ((uint32_t *)d)[2] = palette[s[2]];
        ((uint32_t *)d)[3] = palette[s[3]];
        ((uint32_t *)d)[4] = palette[s[4]];
        ((uint32_t *)d)[5] = palette[s[5]];
        ((uint32_t *)d)[6] = palette[s[6]];
        ((uint32_t *)d)[7] = palette[s[7]];
        d += VGA_BPP * 8;
        s += 8;
    }
}

/* XXX: optimize */

/*
 * 15 bit color
 */
static void vga_draw_line15(s_vga s1, uint8_t *d,
                            const uint8_t *s, int width)
{
    int w;
    uint32_t v, r, g, b;

    w = width;
    do {
        v = *((uint16_t *)s);
        r = (v >> 7) & 0xf8;
        g = (v >> 2) & 0xf8;
        b = (v << 3) & 0xf8;
        ((uint32_t *)d)[0] = RGB(r, g, b);
        s += 2;
        d += VGA_BPP;
    } while (--w != 0);
}

/*
 * 16 bit color
 */
static void vga_draw_line16(s_vga s1, uint8_t *d,
                            const uint8_t *s, int width)
{
    int w;
    uint32_t v, r, g, b;

    w = width;
    do {
        v = *((uint16_t *)s);
        r = (v >> 8) & 0xf8;
        g = (v >> 3) & 0xfc;
        b = (v << 3) & 0xf8;
        ((uint32_t *)d)[0] = RGB(r, g, b);
        s += 2;
        d += VGA_BPP;
    } while (--w != 0);
}

/*
 * 24 bit color
 */
static void vga_draw_line24(s_vga s1, uint8_t *d,
                            const uint8_t *s, int width)
{
    int w;
    uint32_t r, g, b;

    w = width;
    do {
        b = s[0];
        g = s[1];
        r = s[2];
        ((uint32_t *)d)[0] = RGB(r, g, b);
        s += 3;
        d += VGA_BPP;
    } while (--w != 0);
}

/*
 * 32 bit color
 */
static void vga_draw_line32(s_vga s1, uint8_t *d,
                            const uint8_t *s, int width)
{
    int w;
    uint32_t r, g, b;

    w = width;
    do {
        b = s[0];
        g = s[1];
        r = s[2];
        ((uint32_t *)d)[0] = RGB(r, g, b);
        s += 4;
        d += VGA_BPP;
    } while (--w != 0);
}

enum {
    VGA_DRAW_LINE2,
    VGA_DRAW_LINE2D2,
    VGA_DRAW_LINE4,
    VGA_DRAW_LINE4D2,
    VGA_DRAW_LINE8D2,
    VGA_DRAW_LINE8,
    VGA_DRAW_LINE15,
    VGA_DRAW_LINE16,
    VGA_DRAW_LINE24,
    VGA_DRAW_LINE32,
    VGA_DRAW_LINE_NB,
};

static vga_draw_line_func * const vga_draw_line_table[VGA_DRAW_LINE_NB] = {
    vga_draw_line2,
    vga_draw_line2d2,
    vga_draw_line4,
    vga_draw_line4d2,
    vga_draw_line8d2,
    vga_draw_line8,
    vga_draw_line15,
    vga_draw_line16,
    vga_draw_line24,
    vga_draw_line32,
};
static void vga_draw_graphic(s_vga s, int full_update)
{
    uint32_t y1, y, update, linesize, double_scan, mask;
    uint32_t width, height, shift_control, line_offset, bwidth, bits;
    uint64_t page0, page1, page_min, page_max;
    uint32_t disp_width, multi_scan, multi_run;
    uint8_t *d;
    uint32_t v, addr1, addr;
    vga_draw_line_func *vga_draw_line;

    full_update |= vga_update_basic_params(s);

    vga_get_resolution(s, &width, &height);
    disp_width = width;

    shift_control = (s->gr[VGA_GFX_MODE] >> 5) & 3;
    double_scan = (s->cr[VGA_CRTC_MAX_SCAN] >> 7);
    if (shift_control != 1) {
        multi_scan = (((s->cr[VGA_CRTC_MAX_SCAN] & 0x1f) + 1) << double_scan)
            - 1;
    }
    else
    {
        /* in CGA modes, multi_scan is ignored */
        /* XXuint32_tis it correct ? */
        multi_scan = double_scan;
    }
    multi_run = multi_scan;
    if (shift_control != s->shift_control ||
        double_scan != s->double_scan)
    {
        full_update = 1;
        s->shift_control = shift_control;
        s->double_scan = double_scan;
    }

    if (shift_control == 0)
    {
        if (s->sr[VGA_SEQ_CLOCK_MODE] & 8)
            disp_width <<= 1;
    }
    else if (shift_control == 1) {
        if (s->sr[VGA_SEQ_CLOCK_MODE] & 8)
            disp_width <<= 1;
    }

    if (s->line_offset != s->last_line_offset ||
        disp_width != s->last_width ||
        height != s->last_height)
    {
        s->last_scr_width = disp_width;
        s->last_scr_height = height;
        s->buffer = s->resize(s->last_scr_width, s->last_scr_height,
                              vga_get_linesize(s), s->priv);
        if (!s->buffer)
            return;
        s->last_width = disp_width;
        s->last_height = height;
        s->last_line_offset = s->line_offset;
        full_update = 1;
    }

    if (shift_control == 0)
    {
        full_update |= vga_update_palette16(s);
        if (s->sr[VGA_SEQ_CLOCK_MODE] & 8) {
            v = VGA_DRAW_LINE4D2;
        } else {
            v = VGA_DRAW_LINE4;
        }
        bits = 4;
    }
    else if (shift_control == 1)
    {
        full_update |= vga_update_palette16(s);
        if (s->sr[VGA_SEQ_CLOCK_MODE] & 8) {
            v = VGA_DRAW_LINE2D2;
        } else {
            v = VGA_DRAW_LINE2;
        }
        bits = 4;
    }
    else
        v = VGA_DRAW_LINE32;

    vga_draw_line = vga_draw_line_table[v];

    line_offset = s->line_offset;
    addr1 = (s->start_addr * 4);
    bwidth = (width * bits + 7) / 8;
    page_min = -1;
    page_max = 0;
    d = s->buffer;
    linesize = vga_get_linesize(s);
    y1 = 0;
    for(y = 0; y < height; y++) {
        addr = addr1;
        if (!(s->cr[VGA_CRTC_MODE] & 1))
        {
            int shift;
            /* CGA compatibility handling */
            shift = 14 + ((s->cr[VGA_CRTC_MODE] >> 6) & 1);
            addr = (addr & ~(1 << shift)) | ((y1 & 1) << shift);
        }
        if (!(s->cr[VGA_CRTC_MODE] & 2))
            addr = (addr & ~0x8000) | ((y1 & 2) << 14);
        update = full_update;
        page0 = addr;
        page1 = addr + bwidth - 1;
        update |= vga_vram_get_dirty(s, page0, page1 - page0);
        /* explicit invalidation for the hardware cursor */
        update |= (s->invalidated_y_table[y >> 5] >> (y & 0x1f)) & 1;
        if (update)
        {
            if (page0 < page_min)
                page_min = page0;
            if (page1 > page_max)
                page_max = page1;

            s->refresh = 1;
            vga_draw_line(s, d, s->vram + addr, width);
        }
        if (!multi_run)
        {
            mask = (s->cr[VGA_CRTC_MODE] & 3) ^ 3;
            if ((y1 & mask) == mask)
                addr1 += line_offset;
            y1++;
            multi_run = multi_scan;
        }
        else
            multi_run--;

        /* line compare acts on the displayed lines */
        if (y == s->line_compare)
            addr1 = 0;
        d += linesize;
    }
    /* reset modified pages */
    if (page_max >= page_min)
        vga_vram_set_dirty(s, page_min, page_max - page_min, 0);

    memset(s->invalidated_y_table, 0, ((height + 31) >> 5) * 4);
}

static void vga_draw_blank(s_vga s, int full_update)
{
    uint32_t val, w, i;
    uint8_t *d;

    if (!full_update)
        return;
    if (s->last_scr_width <= 0 || s->last_scr_height <= 0)
        return;

    s->refresh = 1;

    val = RGB(0, 0, 0);
    d = s->buffer;
    w = vga_get_linesize(s);

    for (i = 0; i < s->last_scr_height; i++, d += w)
        memset(d, val, w);
}

void vga_update_display(s_vga s)
{
    int full_update;
    int graphic_mode;
    int refresh = s->refresh;

    if (!s->buffer)
        return;

    s->refresh = 0;

    full_update = 0;
    if (!(s->ar_index & 0x20))
        graphic_mode = GMODE_BLANK;
    else
        graphic_mode = s->gr[VGA_GFX_MISC] & VGA_GR06_GRAPHICS_MODE;
    if (graphic_mode != s->graphic_mode)
    {
        s->graphic_mode = graphic_mode;
        full_update = 1;
    }
    switch (graphic_mode)
    {
    case GMODE_TEXT:
        vga_draw_text(s, full_update);
        break;
    case GMODE_GRAPH:
        vga_draw_graphic(s, full_update);
        break;
    case GMODE_BLANK:
    default:
        vga_draw_blank(s, full_update);
        break;
    }

    if (refresh)
        s->refresh = 1;
}

int vga_need_refresh(s_vga s)
{
    int refresh = s->refresh;

    s->refresh = 0;

    return refresh;
}

void vga_mem_write(s_vga s, uint64_t addr, uint32_t val, uint32_t size)
{
    uint32_t i = 0;

    for (i = 0; i < size; i++)
        vga_mem_writeb(s, addr + i, (val >> (i * 8)) & 0xff);

}
