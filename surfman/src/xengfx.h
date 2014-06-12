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
#ifndef _XENGFX_H_
#define _XENGFX_H_

#define XGFX_MAGIC_VALUE               0x58464758
#define XGFX_CURRENT_REV               0x1

#define XGFX_MAGIC                     0x0000  /* reads 0x58464758 'XGFX' */
#define XGFX_REV                       0x0004  /* currently reads 0x1 */
#define XGFX_CONTROL                   0x0100
#define XGFX_ISR                       0x0104
#define XGFX_GART_SIZE                 0x0200
#define XGFX_INVALIDATE_GART           0x0204
#define XGFX_STOLEN_BASE               0x0208
#define XGFX_STOLEN_SIZE               0x020C
#define XGFX_STOLEN_CLEAR              0x0210
#define XGFX_NVCRTC                    0x0300
#define XGFX_RESET                     0x0400
#define XGFX_MADVISE                   0x1000
#define XGFX_BIOS_RESERVED             0x2000

/* XGFX_CONTROL bits */
#define XGFX_CONTROL_HIRES_EN          0x00000001
#define XGFX_CONTROL_INT_EN            0x00000002

/* XGFX_ISR bits */
#define XGFX_ISR_INT                   0x00000001

/* VCRTC Register banks */
#define XGFX_VCRTC_OFFSET              0x100000

#define XGFX_VCRTC_STATUS              0x0000
#define XGFX_VCRTC_STATUS_CHANGE       0x0004
#define XGFX_VCRTC_STATUS_INT          0x0008
#define XGFX_VCRTC_SCANLINE            0x000C
#define XGFX_VCRTC_CURSOR_STATUS       0x0010
#define XGFX_VCRTC_CURSOR_CONTROL      0x0014
#define XGFX_VCRTC_CURSOR_MAXSIZE      0x0018
#define XGFX_VCRTC_CURSOR_SIZE         0x001C
#define XGFX_VCRTC_CURSOR_BASE         0x0020
#define XGFX_VCRTC_CURSOR_POS          0x0024
#define XGFX_VCRTC_EDID_REQUEST        0x1000
#define XGFX_VCRTC_CONTROL             0x2000
#define XGFX_VCRTC_VALID_FORMAT        0x2004
#define XGFX_VCRTC_FORMAT              0x2008
#define XGFX_VCRTC_MAX_HORIZONTAL      0x2010
#define XGFX_VCRTC_HORIZONTAL_ACTIVE   0x2014
#define XGFX_VCRTC_MAX_VERTICAL        0x2018
#define XGFX_VCRTC_VERTICAL_ACTIVE     0x201c
#define XGFX_VCRTC_STRIDE_ALIGNMENT    0x2020
#define XGFX_VCRTC_STRIDE              0x2024
#define XGFX_VCRTC_BASE                0x3000
#define XGFX_VCRTC_LINEOFFSET          0x4000
#define XGFX_VCRTC_EDID                0x5000

struct xgfx_vcrtc_regs
{
    uint32_t control;
    uint32_t valid_fmt;
    uint32_t fmt;
    uint32_t _pad1;
    uint32_t hmax;
    uint32_t hactive;
    uint32_t vmax;
    uint32_t vactive;
    uint32_t stride_align;
    uint32_t stride;
} __attribute__ ((packed));

/* XGFX_VCRTC_STATUS bits */
#define XGFX_VCRTC_STATUS_HOTPLUG           0x00000001
#define XGFX_VCRTC_STATUS_ONSCREEN          0x00000002
#define XGFX_VCRTC_STATUS_RETRACE           0x00000004

/* XGFX_VCRTC_STATUS_CHANGE bits */
#define XGFX_VCRTC_STATUS_CHANGE_D_HOTPLUG  0x00000001
#define XGFX_VCRTC_STATUS_CHANGE_D_ONSCREEN 0x00000002
#define XGFX_VCRTC_STATUS_CHANGE_D_RETRACE  0x00000004

/* XGFX_VCRTC_STATUS_INT bits */
#define XGFX_VCRTC_STATUS_INT_HOTPLUG_EN    0x00000001
#define XGFX_VCRTC_STATUS_INT_ONSCREEN_EN   0x00000002
#define XGFX_VCRTC_STATUS_INT_RETRACE_EN    0x00000004

/* XGFX_VCRTC_CURSOR_STATUS bits */
#define XGFX_VCRTC_CURSOR_STATUS_SUPPORTED  0x00000001

/* XGFX_VCRTC_CURSOR_CONTROL bits */
#define XGFX_VCRTC_CURSOR_CONTROL_SHOW      0x00000001

/* XGFX_VCRTC_CONTROL bits */
#define XGFX_VCRTC_CONTROL_ENABLE           0x00000001

/* XGFX_VCRTC_FORMAT bits */
#define XGFX_VCRTC_FORMAT_NONE        0x00000000
#define XGFX_VCRTC_FORMAT_RGB555      0x00000001
#define XGFX_VCRTC_FORMAT_BGR555      0x00000002
#define XGFX_VCRTC_FORMAT_RGB565      0x00000004
#define XGFX_VCRTC_FORMAT_BGR565      0x00000008
#define XGFX_VCRTC_FORMAT_RGB888      0x00000010
#define XGFX_VCRTC_FORMAT_BGR888      0x00000020
#define XGFX_VCRTC_FORMAT_RGBX8888    0x00000040
#define XGFX_VCRTC_FORMAT_BGRX8888    0x00000080


#define XGFX_VCRTC_BANK_SIZE            0x10000
#define XGFX_VCRTC_BANK_SHIFT           16
#define XGFX_VCRTC_BANK_MASK            0xFFFF

/* Bank/register offset macro. Use values 0x0 through 0xF for
   the bank. */
#define XGFX_VCRTCN_BANK_OFFSET(bank) \
    (XGFX_VCRTC_OFFSET + (bank * XGFX_VCRTC_BANK_SIZE))

/* GART Registers */
#define XGFX_GART_OFFSET                0x200000

#define XGFX_GART_VALID_PFN             0x80000000
#define XGFX_GART_CLEAR_PFN             0x00000000

/* GART offset macro. Returns offset to requested PFN register */
#define XGFX_GART_REG_OFFSET(reg) ((XGFX_GART_OFFSET + (reg<<2))

#endif
