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

#define _GNU_SOURCE
#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <xenctrl.h>
#include <surfman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

#include <pciaccess.h>

#include "linuxfb.h"
#include "../../surfman/src/splashscreen.h"
/* TODO: check whether we want to properly install the header instead...*/

#ifndef MIN
# define MIN(_x_, _y_) ((_x_) < (_y_) ? (_x_) : (_y_))
#endif
#ifndef DIV_ROUND_UP
# define DIV_ROUND_UP(x, y) (((x) + (y) - 1) / (y))
#endif

static int g_monitor = 1;
static xc_interface *g_xc = 0;
static surfman_psurface_t g_fb_pages_taken = NULL;

static struct
{
    struct
    {
        int     bus;
        int     dev;
        int     func;
    }           pci;

    unsigned    maxBytesPerScanline;
    unsigned    x;
    unsigned    y;
    unsigned    Bpp;
    unsigned    mapSize;
    unsigned    mapPhys;

    int         fd_dev;

    uint8_t     *map;

    int         fb_need_cleanning;

    struct pci_device *pci_dev;
}               g_fb_info;

static int sysfs_read(const char *node, const char *format, ...)
{
    FILE *fd = NULL;
    va_list ap;
    int ret;

    va_start(ap, format);

    if (!(fd = fopen(node, "r")))
        return errno;
    ret = vfscanf(fd, format, ap);
    fclose(fd);
    va_end(ap);
    return ret;
}

static unsigned int get_fb_base(void)
{
    FILE *fd = fopen("/proc/iomem", "r");
    char buff[1024];
    unsigned int base_addr = 0;

    while (fgets(buff, 1024, fd))
    {
        if (strstr(buff, "vesafb"))
        {
            char *p;

            /* Get ride of spaces at the begining */
            for (p = buff; *p == ' '; p++)
                ;
            if (sscanf(p, "%x-", &base_addr) == 1)
                goto out;
        }
    }
out:
    fclose(fd);
    return base_addr;
}

static int fb_read_info(void)
{
    sysfs_read("/sys/class/graphics/fb0/stride", "%d", &g_fb_info.maxBytesPerScanline);
    sysfs_read("/sys/class/graphics/fb0/modes", "U:%dx%dp",
            &g_fb_info.x, &g_fb_info.y);
    sysfs_read("/sys/class/graphics/fb0/bits_per_pixel", "%u", &g_fb_info.Bpp);
    g_fb_info.Bpp /= 8;
    g_fb_info.mapSize = g_fb_info.y * g_fb_info.maxBytesPerScanline;

    if (!(g_fb_info.fd_dev = open("/dev/fb0", O_RDWR)))
    {
        error ("opening /dev/fb0 failed!");
        return -1;
    }

    g_fb_info.mapPhys = get_fb_base();

    info("Vesa info");
    info("    maxBytesPerScanline: %x", g_fb_info.maxBytesPerScanline);
    info("    X: %d, Y:%d, Bpp: %u", g_fb_info.x, g_fb_info.y, g_fb_info.Bpp);
    info("    mapSize: %x", g_fb_info.mapSize);
    info("    mapPhys: %x", g_fb_info.mapPhys);
    return 0;
}

static int fb_remap_host_fb(void)
{
    if (g_fb_info.map)
    {
        munmap(g_fb_info.map, g_fb_info.mapSize);
        g_fb_info.map = 0;
    }

    g_fb_info.map = mmap(NULL, g_fb_info.mapSize,
                         PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_info.fd_dev, 0);

    return (g_fb_info.map == MAP_FAILED);
}

static void unmap_fb(fb_surface *s)
{
    surface_unmap(s->surfman_surface);
}

static int map_fb(surfman_surface_t *src, fb_surface *dst)
{
    dst->mapped_fb = surface_map(src);
    dst->mapped_fb_size = src->page_count * XC_PAGE_SIZE;
    dst->pages_domid = src->pages_domid;
    dst->surfman_surface = src;

    if (!dst->mapped_fb)
      {
        error ("failed to map framebuffer: %s", strerror (errno));
        return -1;
      }

    return 0;
}

static int fb_init(surfman_plugin_t * p)
{
    g_xc = xc_interface_open(NULL, NULL, 0);
    if (g_xc == 0) {
        error("failed to open XC interface");
        return SURFMAN_ERROR;
    }

    pci_system_init();

    if (fb_read_info() != 0) {
        error("reading fb info failed");
        return SURFMAN_ERROR;
    }

    if (fb_remap_host_fb() != 0) {
        error("mapping fb lfb failed");
        return SURFMAN_ERROR;
    }

    return SURFMAN_SUCCESS;
}

static void fb_shutdown(surfman_plugin_t * p)
{
    xc_interface_close(g_xc);
    g_xc = 0;
}

static void fb_clean_hostfb(void)
{
    memset(g_fb_info.map, 0, g_fb_info.y * g_fb_info.maxBytesPerScanline);
}

static int fb_display(surfman_plugin_t *p, surfman_display_t *config, size_t size)
{
    if (g_fb_pages_taken && config->psurface != g_fb_pages_taken) {
        return SURFMAN_NOMEM;
    }
    g_fb_info.fb_need_cleanning = 1;
    return SURFMAN_SUCCESS;
}

static int fb_get_monitors(surfman_plugin_t *p, surfman_monitor_t *monitors, size_t size)
{
    monitors[0] = &g_monitor;
    return 1;
}

static int fb_set_monitor_modes(surfman_plugin_t *p, surfman_monitor_t monitor, surfman_monitor_mode_t *mode)
{
    return SURFMAN_SUCCESS;
}

static int fb_get_monitor_info_by_monitor(surfman_plugin_t *p, surfman_monitor_t monitor,
                                          surfman_monitor_info_t *info, unsigned int modes_count)
{
    int w, h, screen;
    w = g_fb_info.x;
    h = g_fb_info.y;

    info->modes[0].htimings[SURFMAN_TIMING_ACTIVE] = w;
    info->modes[0].htimings[SURFMAN_TIMING_SYNC_START] = w;
    info->modes[0].htimings[SURFMAN_TIMING_SYNC_END] = w;
    info->modes[0].htimings[SURFMAN_TIMING_TOTAL] = w;

    info->modes[0].vtimings[SURFMAN_TIMING_ACTIVE] = h;
    info->modes[0].vtimings[SURFMAN_TIMING_SYNC_START] = h;
    info->modes[0].vtimings[SURFMAN_TIMING_SYNC_END] = h;
    info->modes[0].vtimings[SURFMAN_TIMING_TOTAL] = h;

    info->prefered_mode = &info->modes[0];
    info->current_mode = &info->modes[0];
    info->mode_count = 1;

    return SURFMAN_SUCCESS;
}

static int fb_get_monitor_edid_by_monitor(surfman_plugin_t *p, surfman_monitor_t monitor, surfman_monitor_edid_t *edid)
{
    return SURFMAN_SUCCESS;
}

static surfman_psurface_t fb_get_psurface_from_surface(surfman_plugin_t *p, surfman_surface_t *surfman_surface)
{
    fb_surface *surface = NULL;

    surface = calloc(1, sizeof (fb_surface));
    if (!surface) {
        return NULL;
    }
    surface->height = surfman_surface->height;
    surface->width = surfman_surface->width;
    surface->stride = surfman_surface->stride;
    switch (surfman_surface->format) {
        case SURFMAN_FORMAT_UNKNOWN:        /* NOTE: We don't deal with those (yet ?) */
        case SURFMAN_FORMAT_BGR565:
            free(surface);
            return NULL;

        case SURFMAN_FORMAT_RGBX8888:
        case SURFMAN_FORMAT_BGRX8888:
            surface->Bpp = 4;
            break;
    }

    if (map_fb(surfman_surface, surface)) {
        free(surface);
        return NULL;
    }

    return surface;
}

static void fb_update_psurface(surfman_plugin_t *plugin, surfman_psurface_t psurface,
                               surfman_surface_t *surface, unsigned int flags)
{
    fb_surface *s = psurface;

    assert(psurface != NULL);   /* NOTE: This would be a surfman bug I guess, so bailing out the whole thing is safer. */

    g_fb_info.fb_need_cleanning = 1;

    /*NOTE: changing surface geometry without SURFMAN_UPDATE_PAGES seems sooo wrong. */
    s->height = surface->height;
    s->width = surface->width;
    s->stride = surface->stride;
    switch (surface->format) {
        case SURFMAN_FORMAT_UNKNOWN:        /* NOTE: We don't deal with those (yet ?) */
        case SURFMAN_FORMAT_BGR565:
            error("Surfman is using an unsuported format. Shit's thrown in the fan from now.");
            return;

        case SURFMAN_FORMAT_RGBX8888:
        case SURFMAN_FORMAT_BGRX8888:
            s->Bpp = 4;
    }
    if (flags & SURFMAN_UPDATE_PAGES) {
        s->surfman_surface = surface;
        unmap_fb(psurface);
        map_fb(surface, psurface);
    }
}

static void fb_dump_refresh_bitmap(uint8_t *refresh_bitmap, unsigned int size)
{
    char *tmp = NULL;
    unsigned int i;

    if (refresh_bitmap)
        for (i = 0; i < DIV_ROUND_UP(DIV_ROUND_UP(size, XC_PAGE_SIZE), 8); i++)
            asprintf(&tmp, "%s%02x", tmp ? tmp : "", refresh_bitmap[i]);
    info("%s: %s", __func__, tmp);
}

/* Sick arithmetic ... Most variables are just aliases to make it "readable". */
static void fb_copy_converted(fb_surface *surface, uint8_t *refresh_bitmap)
{
    uint8_t *hfb = g_fb_info.map;                       /* host framebuffer. */
    unsigned int hh = g_fb_info.y;                      /* height */
    unsigned int hw = g_fb_info.x;                      /* width */
    unsigned int hs = g_fb_info.maxBytesPerScanline;    /* stride */

    uint8_t *gfb = surface->mapped_fb;                  /* guest framebuffer. */
    unsigned int gh = surface->height;                  /* height */
    unsigned int gw = surface->width;                   /* width */
    unsigned int gs = surface->stride;                  /* stride */

    unsigned int bpp = g_fb_info.Bpp;                   /* Bits per pixel (x offset calculation),
                                                           we safelly assume this is consistent between both surfaces. */
    unsigned int vs = hs < gs ? hs : gs;                /* Smallest stride. */

    /* host and guest offset to apply surfaces on each other croping/centering the right one (because it's beeeaaauuuutiful) */
    /* hy = gy - dgy + dhy ; gy = hy - dhy + dhy */
    unsigned int dhx = (hw < gw) ? 0 : (hw - gw) / 2;
    unsigned int dhy = (hh < gh) ? 0 : (hh - gh) / 2;
    unsigned int dgx = (hw > gw) ? 0 : (gw - hw) / 2;
    unsigned int dgy = (hh > gh) ? 0 : (gh - hh) / 2;

    unsigned int hy, gy;                                /* host/guest fb longitudinal axis iterators. */

    if (!refresh_bitmap) {
        for (hy = dhy, gy = dgy; (hy < hh) && (gy < gh); ++hy, ++gy) {
            memcpy(hfb + (hy * hs) + (dhx * bpp), gfb + (gy * gs) + (dgx * bpp), vs);
        }
    } else {
        unsigned int n = DIV_ROUND_UP(gh * gs, XC_PAGE_SIZE); /* How many pages in our bitmap ? */
        unsigned int i = 0;                                   /* Page iterator. */
        unsigned int gy_end = dgy;                            /* y coordinate where the page ends (initialy the first line). */
        unsigned int pgy_end;                                 /* previous gy_end. */

        do {
            if (!refresh_bitmap[i / 8]) {
                i += 8;
                continue;
            }

            do {
                if (refresh_bitmap[i / 8] & (1 << (i % 8))) {
                    /* Turns out it's better for perfs to refresh the whole line instead of search the
                     * exact correct position of the dirty page in the framebuffer. Or I missed something ...
                     * Anyway, it'll be bad so ... */
                    gy = (i * XC_PAGE_SIZE) / gs;                       /* guest y coordinate from page number. */
                    if (gy >= (gh - dgy)) {
                        return;                                         /* we're already out of visible part, bail out. */
                    }
                    pgy_end = gy_end;
                    gy_end = ((i + 1) * XC_PAGE_SIZE) / gs;
                    if (gy_end < dgy) {
                        return;                                         /* we're already out of visible part, bail out. */
                    }

                    if (gy_end > pgy_end) {
                        for (gy = pgy_end + 1; (gy <= gy_end) && (gy < gh); ++gy) {
                            hy = gy - dgy + dhy;                        /* host y coordinate relative to previous gy. */
                            memcpy(hfb + (hy * hs) + dhx * bpp,
                                   gfb + (gy * gs) + dgx * bpp, vs);
                        }
                    }
                }
                ++i;
            } while ((i % 8) && (i < n));
        } while (i < n);
    }
}

static void fb_refresh_surface(struct surfman_plugin *plugin, surfman_psurface_t psurface, uint8_t *refresh_bitmap)
{
    fb_surface *ps = psurface;
    uint32_t size = DIV_ROUND_UP(ps->stride * ps->height, XC_PAGE_SIZE);
    unsigned int n = 0, i = 0;

    assert(psurface != NULL);   /* NOTE: This would be a surfman bug I guess, so bailing out the whole thing is safer. */

    if (g_fb_info.fb_need_cleanning)
    {
        fb_clean_hostfb();
        g_fb_info.fb_need_cleanning = 0;
        refresh_bitmap = NULL;
    }
    fb_copy_converted(ps, refresh_bitmap);
}


static int fb_get_pages_from_psurface(surfman_plugin_t *p, surfman_psurface_t psurface, pfn_t *pages)
{
    /* Debug disable mapping mode */
    return SURFMAN_ERROR;
}

static void fb_free_psurface_pages(surfman_plugin_t *p, surfman_psurface_t psurface)
{
    g_fb_pages_taken = NULL;
    return;
}

static int fb_copy_surface_on_psurface(surfman_plugin_t *p, surfman_psurface_t psurface)
{
    return SURFMAN_ERROR;
}

static int fb_copy_psurface_on_surface (surfman_plugin_t *p, surfman_psurface_t psurface)
{
    return SURFMAN_ERROR;
}

static void fb_free_psurface(surfman_plugin_t *plugin, surfman_psurface_t psurface)
{
    unmap_fb(psurface);
}

surfman_vgpu_t *fb_new_vgpu(surfman_plugin_t *p, surfman_vgpu_info_t *info)
{
    return NULL;
}

void fb_free_vgpu(surfman_plugin_t *p, surfman_vgpu_t *vgpu)
{
    return;
}

surfman_plugin_t surfman_plugin = {
    .init = fb_init,
    .shutdown = fb_shutdown,
    .display = fb_display,
    .new_vgpu = fb_new_vgpu,
    .free_vgpu = fb_free_vgpu,
    .get_monitors = fb_get_monitors,
    .set_monitor_modes = fb_set_monitor_modes,
    .get_monitor_info = fb_get_monitor_info_by_monitor,
    .get_monitor_edid = fb_get_monitor_edid_by_monitor,
    .get_psurface_from_surface = fb_get_psurface_from_surface,
    .update_psurface = fb_update_psurface,
    .refresh_psurface = fb_refresh_surface,
    .get_pages_from_psurface = fb_get_pages_from_psurface,
    .free_psurface_pages = fb_free_psurface_pages,
    .copy_surface_on_psurface = fb_copy_surface_on_psurface,
    .copy_psurface_on_surface = fb_copy_psurface_on_surface,
    .free_psurface = fb_free_psurface,
    .options = {1, SURFMAN_FEATURE_NEED_REFRESH},
    .notify = SURFMAN_NOTIFY_NONE
};
