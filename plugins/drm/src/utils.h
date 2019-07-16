/*
 * Copyright (c) 2013 Citrix Systems, Inc.
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

#ifndef __UTILS_H_
# define __UTILS_H_

// Just to boast out...
static inline unsigned int min(unsigned int x, unsigned int y)
{
    return y ^ ((x ^ y) & -(x < y));
}
static inline unsigned int max(unsigned int x, unsigned int y)
{
    return x ^ ((x ^ y) & -(x < y));
}

#define ARRAY_SIZE(arr) (sizeof (arr) / sizeof ((arr)[0]))

struct type_name {
    int type;   /* More likely an enum... */
    char *name;
};
/* Generic function /res/_str generator for DRM enums. */
#define type_name_fn(res) \
    static inline const char *res##_str(int type) {     \
        unsigned int i;                                 \
        for (i = 0; i < ARRAY_SIZE(res##_names); ++i) { \
            if (res##_names[i].type == type)            \
                return res##_names[i].name;             \
        }                                               \
        return "(invalid)";                             \
    }

static struct type_name connector_type_names[] = {
    { DRM_MODE_CONNECTOR_Unknown, "unknown" },
    { DRM_MODE_CONNECTOR_VGA, "VGA" },
    { DRM_MODE_CONNECTOR_DVII, "DVI-I" },
    { DRM_MODE_CONNECTOR_DVID, "DVI-D" },
    { DRM_MODE_CONNECTOR_DVIA, "DVI-A" },
    { DRM_MODE_CONNECTOR_Composite, "composite" },
    { DRM_MODE_CONNECTOR_SVIDEO, "s-video" },
    { DRM_MODE_CONNECTOR_LVDS, "LVDS" },
    { DRM_MODE_CONNECTOR_Component, "component" },
    { DRM_MODE_CONNECTOR_9PinDIN, "9-pin DIN" },
    { DRM_MODE_CONNECTOR_DisplayPort, "DP" },
    { DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
    { DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
    { DRM_MODE_CONNECTOR_TV, "TV" },
    { DRM_MODE_CONNECTOR_eDP, "eDP" },
};
type_name_fn(connector_type);

static struct type_name connector_status_names[] = {
    { DRM_MODE_CONNECTED, "connected" },
    { DRM_MODE_DISCONNECTED, "disconnected" },
    { DRM_MODE_UNKNOWNCONNECTION, "unknown" },
};
type_name_fn(connector_status);

static struct type_name encoder_type_names[] = {
    { DRM_MODE_ENCODER_NONE, "none" },
    { DRM_MODE_ENCODER_DAC, "DAC" },
    { DRM_MODE_ENCODER_TMDS, "TMDS" },
    { DRM_MODE_ENCODER_LVDS, "LVDS" },
    { DRM_MODE_ENCODER_TVDAC, "TVDAC" },
};
type_name_fn(encoder_type);

/* Convert Surfman's pixel format to bits per pixels. */
static inline unsigned int surfman_format_to_bpp(enum surfman_surface_format format)
{
    switch (format) {
        case SURFMAN_FORMAT_BGR565:
            return 16;
        case SURFMAN_FORMAT_RGBX8888:
        case SURFMAN_FORMAT_BGRX8888:
            return 32;
        default:
            return 0;   /* XXX: Why not an enum? */
    }
}

/* Convert Surfman's pixel format to depth (number of bits to represent a pixel). */
static inline unsigned int surfman_format_to_depth(enum surfman_surface_format format)
{
    switch (format) {
        case SURFMAN_FORMAT_BGR565:
            return 16;
        case SURFMAN_FORMAT_RGBX8888:
        case SURFMAN_FORMAT_BGRX8888:
            return 24;
        default:
            return 0;   /* XXX: Why not an enum? */
    }
}

static inline void surfman_print_format(enum surfman_surface_format format)
{
    switch (format) {
        case SURFMAN_FORMAT_UNKNOWN:
           surfman_info("Format: Unknown.");
           break;
        case SURFMAN_FORMAT_BGR565:
           surfman_info("Format: BGR (5-6-5) 16bits.");
           break;
        case SURFMAN_FORMAT_RGBX8888:
           surfman_info("Format: RGB (8-8-8) 32bits.");
           break;
        case SURFMAN_FORMAT_BGRX8888:
           surfman_info("Format: BGR (8-8-8) 32bits.");
           break;
    }
}

static inline void surfman_surface_dump(const surfman_surface_t *s)
{
    if (!s) {
        surfman_debug("surfman_surface_t (%p) = { }", s);
    } else {
        surfman_debug("surfman_surface_t (%p) = { dom%u, %ux%u, stride=%u, page_count=%u, offset=%u }",
                      s, s->pages_domid, s->width, s->height, s->stride, s->page_count, s->offset);
    }
}

static inline void framebuffer_dump(const char *indent, const struct framebuffer *fb)
{
    if (!fb) {
        DRM_DBG("%sframebuffer (%p) = { }", indent, fb);
    } else {
        DRM_DBG("%sframebuffer (%p) = { %ux%u %u/%ubpp, %u stride, %u bytes, @%p:%ld }",
                indent,
                fb, fb->width, fb->height, fb->bpp, fb->depth, fb->pitch, fb->size,
                fb->map, fb->offset);
    }
}

static inline void drm_framebuffer_dump(const char *indent, const struct drm_framebuffer *drm)
{
    if (!drm) {
        DRM_DBG("%sdrm_framebuffer (%p) = { }", indent, drm);
    } else {
        char ind[256];

        sprintf(ind, "%s	", indent);
        DRM_DBG("%sdrm_framebuffer (%p) = { type=%s, handle=%u, id=%u,",
                indent, drm, drm->ops->name, drm->handle, drm->id);
        framebuffer_dump(ind, &drm->fb);
        DRM_DBG("%s                         device=%s }", indent, drm->device->devnode);
    }
}

static inline void drm_surface_dump(const struct drm_surface *s)
{
    if (!s) {
        DRM_DBG("struct drm_surface (%p) = { }", s);
    } else {
        DRM_DBG("struct drm_surface (%p) = { dom%u, ", s, s->domid);
        framebuffer_dump("    ", &s->fb);
        DRM_DBG("}");
    }
}

static inline void drm_plane_dump(const char *indent, const struct drm_plane *p)
{
    if (!p) {
        DRM_DBG("%sstruct plane (%p) = { }", indent, p);
    } else {
        char ind[256];

        sprintf(ind, "%s	", indent);
        DRM_DBG("%sstruct plane (%p) = { id=%u,", indent, p, p->id);
        drm_framebuffer_dump(ind, p->framebuffer);
        DRM_DBG("%s                      device=%s }", indent, p->device->devnode);
    }
}

static inline void drm_monitor_dump(const struct drm_monitor *m)
{
    if (!m) {
        DRM_DBG("struct drm_monitor (%p) = { }", m);
    } else {
        DRM_DBG("struct drm_monitor (%p) = { connector=%u, encoder=%u, crtc=%u,",
                m, m->connector, m->encoder, m->crtc);
        drm_framebuffer_dump("	", m->framebuffer);
        drm_plane_dump("	", m->plane);
        DRM_DBG("}");
    }
}

static inline void drm_mode_print(const drmModeModeInfo *m)
{
    if (m) {
        DRM_DBG("(%s) %ux%u@%uHz: %4uHz %7u %4u %4u %4u %4u %4u %4u %4u%s%s%s", m->name,
                m->hdisplay, m->vdisplay, m->vrefresh,
                m->clock,
                m->hdisplay, m->hsync_start, m->hsync_end, m->htotal,
                m->vdisplay, m->vsync_start, m->vsync_end, m->vtotal,
                m->flags & DRM_MODE_FLAG_PHSYNC ? " +HSync" : m->flags & DRM_MODE_FLAG_NHSYNC ? " -HSync" : "XXXXXX",
                m->flags & DRM_MODE_FLAG_PVSYNC ? " +VSync" : m->flags & DRM_MODE_FLAG_NVSYNC ? " -VSync" : "XXXXXX",
                m->flags & DRM_MODE_FLAG_INTERLACE ? " Interlace" : "");
    }
}

#endif /* __UTILS_H_ */
