/*
 * Copyright (c) 2011 Citrix Systems, Inc.
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

#include <X11/Xlib.h>

#include <pciaccess.h>

#include "vesa.h"

#ifndef MIN
# define MIN(_x_, _y_) ((_x_) < (_y_) ? (_x_) : (_y_))
#endif

#define OPTION_FILE "/var/cache/xorg-vesa-lfb"

static int g_monitor = 1;
static xc_interface *g_xc = NULL;
static surfman_psurface_t g_vesa_pages_taken = NULL;

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
    uint64_t    mapPhys;
    unsigned    mapSize;

    uint8_t     *map;

    int         fb_need_cleanning;

    struct pci_device *pci_dev;
}               g_vesa_info;

static int
vesa_read_info(void)
{
    char        buff[1024];
    FILE        *fd = fopen(OPTION_FILE, "r");
    uint32_t    tmp;

    while (!feof(fd) && fgets(buff, 1024, fd))
    {
        if (strstr(buff, "pci") && sscanf(buff, "pci %x\n", &tmp) == 1) {
            g_vesa_info.pci.bus = tmp >> 8;
            g_vesa_info.pci.dev = (tmp >> 3) & 0x1f;
            g_vesa_info.pci.func = tmp & 0x7;
        } else if (strstr(buff, "maxBytesPerScanline") &&
                sscanf(buff, "maxBytesPerScanline %x\n", &tmp) == 1) {
            g_vesa_info.maxBytesPerScanline = tmp;
        } else if (strstr(buff, "virtualX") &&
                sscanf(buff, "virtualX %x\n", &tmp) == 1) {
            g_vesa_info.x = tmp;
        } else if (strstr(buff, "virtualY") &&
                    sscanf(buff, "virtualY %x\n", &tmp) == 1) {
            g_vesa_info.y = tmp;
        } else if (strstr(buff, "mapPhys") &&
            sscanf(buff, "mapPhys %x\n", &tmp) == 1) {
            g_vesa_info.mapPhys = tmp;
        } else if (strstr(buff, "mapSize") &&
                sscanf(buff, "mapSize %x\n", &tmp) == 1) {
            g_vesa_info.mapSize = tmp;
        } else
        {
            info("unknown option %s", buff);
            return 1;
        }
    }
    info("Vesa info");
    info("    pci: %02x:%02x.%01x", g_vesa_info.pci.bus, g_vesa_info.pci.dev, g_vesa_info.pci.func);
    info("    maxBytesPerScanline: %x", g_vesa_info.maxBytesPerScanline);
    info("    X: %d, Y:%d", g_vesa_info.x, g_vesa_info.y);
    info("    mapPhys: %x", g_vesa_info.mapPhys);
    info("    mapSize: %x", g_vesa_info.mapSize);
    return 0;
}

static int
vesa_remap_host_fb(void)
{
    FILE *fd;
    int err;
    void *map;

    if (g_vesa_info.map)
    {
        pci_device_unmap_range (g_vesa_info.pci_dev,
                g_vesa_info.map,
                g_vesa_info.mapSize);
        g_vesa_info.map = 0;
    }

    err = pci_device_map_range (g_vesa_info.pci_dev,
            g_vesa_info.mapPhys,
            g_vesa_info.mapSize,
            PCI_DEV_MAP_FLAG_WRITABLE | PCI_DEV_MAP_FLAG_WRITE_COMBINE,
            &map);
    if (err)
        info("pci_device_map_range WC failed");

    g_vesa_info.map = map;
    return err;
}

static void
unmap_surfman_surface(uint8_t *mapped, surfman_surface_t *src)
{
    if ( munmap(mapped, src->page_count * XC_PAGE_SIZE) < 0 ) {
        error("failed to unmap framebuffer pages");
    }
}

void
unmap_fb( vesa_surface *dst)
{
    if (dst->mapped_fb)
    {
        unmap_surfman_surface(dst->mapped_fb, dst->src);
        dst->mapped_fb = NULL;
    }
    dst->mapped_fb_size = 0;
}

static uint8_t*
map_surfman_surface(surfman_surface_t *src, int caching)
{
    xen_pfn_t *fns = NULL;
    uint8_t *mapped = NULL;
    unsigned int i = 0;

    fns = malloc( src->page_count*sizeof(xen_pfn_t) );
    if (!fns) {
        error("failed to malloc mfns");
        goto out;
    }
    for (i = 0; i < src->page_count; ++i) {
        if (src->guest_base_addr)
            fns[i] = (xen_pfn_t)((src->guest_base_addr >> XC_PAGE_SHIFT) + i);
        else
            fns[i] = (xen_pfn_t) src->mfns[i];
    }
    if (src->guest_base_addr) {
        mapped = xc_map_foreign_batch_cacheattr(
                g_xc, src->pages_domid,
                PROT_READ | PROT_WRITE,
                fns, src->page_count,
                caching);
    } else {
        mapped = xc_map_foreign_pages( g_xc, src->pages_domid, PROT_READ | PROT_WRITE, fns, src->page_count );
    }
out:
    if (fns) {
        free(fns);
    }
    return mapped;
}

static int
map_fb( surfman_surface_t *src, vesa_surface *dst )
{
    unmap_fb(dst);
    if (!xc_domid_exists(dst->src->pages_domid))
        return -1;

    dst->mapped_fb = map_surfman_surface(src, XEN_DOMCTL_MEM_CACHEATTR_WC);
    if ( !dst->mapped_fb ) {
        error("failed to map framebuffer pages (WC)");
        return -1;
    }
    dst->mapped_fb_size = src->page_count * XC_PAGE_SIZE;
    info("map_fb mapped_fb[0]:%02x", dst->mapped_fb[0]);
    return 0;
}


static void
hide_x_cursor(Display *display, Window window)
{
    Cursor invis_cur;
    Pixmap empty_pix;
    XColor black;
    static char empty_data[] = { 0,0,0,0,0,0,0,0 };
    black.red = black.green = black.blue = 0;
    empty_pix = XCreateBitmapFromData(display, window, empty_data, 8, 8);
    invis_cur = XCreatePixmapCursor(
        display, empty_pix, empty_pix, 
        &black, &black, 0, 0);
    XDefineCursor(display, window, invis_cur);
    XFreeCursor(display, invis_cur);
}

static int
start_X()
{
    Display *display = NULL;
    int i;
    int ret;

    info( "starting X server");
    ret = system("X -config /etc/X11/xorg-vesa.conf");

    if (WEXITSTATUS(ret) == 10)
        return SURFMAN_SUCCESS;
    return SURFMAN_ERROR;
}

static int
stop_X()
{
    info( "stopping X server");
    return system("killall X");
}

static int
vesa_init (surfman_plugin_t * p)
{
    int rv;
    struct pci_device *dev;
    unsigned int i=0;
    info( "vesa_init");

    g_xc = xc_interface_open(NULL, NULL, 0);
    if (!g_xc) {
        error("failed to open XC interface");
        return SURFMAN_ERROR;
    }

    pci_system_init();

    rv = start_X();
    if (rv < 0) {
        error("starting X failed");
        return SURFMAN_ERROR;
    }

    if (vesa_read_info() != 0)
    {
        error("reading vesa info failed");
        return SURFMAN_ERROR;
    }

    g_vesa_info.pci_dev = pci_device_find_by_slot(0,
            g_vesa_info.pci.bus, g_vesa_info.pci.dev, g_vesa_info.pci.func);
    rv = pci_device_probe(g_vesa_info.pci_dev);
    if (!g_vesa_info.pci_dev || rv != 0)
    {
        error("opening pci device failed");
        return SURFMAN_ERROR;
    }

    if (vesa_remap_host_fb() != 0)
    {
        error("mapping vesa lfb failed");
        return SURFMAN_ERROR;
    }

    /* Make the screen green so people will notice */
    for (i = 0; i < g_vesa_info.mapSize; i += 4)
    {
        g_vesa_info.map[i] = 0x00;
        g_vesa_info.map[i + 1] = 0xff;
        g_vesa_info.map[i + 2] = 0x00;
        g_vesa_info.map[i + 3] = 0x00;
    }

    return SURFMAN_SUCCESS;
}

static void
vesa_shutdown (surfman_plugin_t * p)
{
    int rv;
    info("shutting down");
    xc_interface_close(g_xc);
    g_xc = NULL;
}

static void
vesa_clean_hostfb(void)
{
    info("vesa clean host fb");
    memset(g_vesa_info.map, 0, g_vesa_info.y * g_vesa_info.maxBytesPerScanline);
}

static int
vesa_display (surfman_plugin_t * p,
               surfman_display_t * config,
               size_t size)
{
    int rv;
    vesa_surface *surf = NULL;
    if (g_vesa_pages_taken && config->psurface != g_vesa_pages_taken)
    {
        info("%s: returns error", __func__);
        return SURFMAN_NOMEM;
    }

    surf = config->psurface;
    g_vesa_info.fb_need_cleanning = 1;
    return SURFMAN_SUCCESS;
}

static int
vesa_get_monitors (surfman_plugin_t * p,
                    surfman_monitor_t * monitors,
                    size_t size)
{
    static surfman_monitor_t m = NULL;
    info( "vesa_get_monitors");
    monitors[0] = &g_monitor;
    return 1;
}

static int
vesa_set_monitor_modes (surfman_plugin_t * p,
                         surfman_monitor_t monitor,
                         surfman_monitor_mode_t * mode)
{
    info( "vesa_set_monitor_modes");
    return SURFMAN_SUCCESS;
}

static int
vesa_get_monitor_info_by_monitor (surfman_plugin_t * p,
                                   surfman_monitor_t monitor,
                                   surfman_monitor_info_t * info,
                                   unsigned int modes_count)
{
    int w,h,screen;
    w = g_vesa_info.x;
    h = g_vesa_info.y;

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

static int
vesa_get_monitor_edid_by_monitor (surfman_plugin_t * p,
                                   surfman_monitor_t monitor,
                                   surfman_monitor_edid_t * edid)
{
    info( "vesa_get_edid_by_monitor");
    return SURFMAN_SUCCESS;
}

static surfman_psurface_t
vesa_get_psurface_from_surface (surfman_plugin_t * p,
                                 surfman_surface_t * surfman_surface)
{
    vesa_surface *surface = NULL;
    info("vesa_get_psurface_from_surface");

    surface = calloc(1, sizeof(vesa_surface));
    if (!surface) {
        return NULL;
    }
    surface->initialised = 0;
    surface->mapped_fb = NULL;
    surface->src = surfman_surface;
    map_fb( surfman_surface, surface );

    return surface;
}

static void
vesa_update_psurface (surfman_plugin_t *plugin,
                       surfman_psurface_t psurface,
                       surfman_surface_t *surface,
                       unsigned int flags)
{
    vesa_surface *vesasurf = (vesa_surface*)psurface;
    info( "vesa_update_psurface surface:%p flags:%x", surface, flags);
    g_vesa_info.fb_need_cleanning = 1;

    vesasurf->src = surface;
    if (!psurface) {
        return;
    }
    if (flags & SURFMAN_UPDATE_PAGES)
    {
        unmap_fb( psurface);
        map_fb( surface, psurface);
    }
}

static void
vesa_dump_refresh_bitmap(uint8_t *refresh_bitmap, unsigned int size)
{
    char *tmp = NULL;
    unsigned int i;

    if (refresh_bitmap)
        for (i = 0; i < (size / XC_PAGE_SIZE / 8); i++)
            asprintf(&tmp, "%s%02x", tmp ? tmp : "", refresh_bitmap[i]);
    info("%s: %s", __func__, tmp);
}

static void
vesa_copy_converted(vesa_surface *surf,
                    uint8_t *refresh_bitmap)
{
    uint8_t *vesa_fb;
    uint8_t *guest_fb = surf->mapped_fb;
    unsigned int i;
    uint32_t size = surf->src->stride * surf->src->height;

    vesa_fb = g_vesa_info.map +
        ((g_vesa_info.y - (surf->src->height)) / 2) * g_vesa_info.maxBytesPerScanline +
        ((g_vesa_info.x - (surf->src->width)) / 2) * 4;
    if (refresh_bitmap == NULL)
    {
        for (i = 0; i < surf->src->height; i++)
        {
            memcpy(vesa_fb, guest_fb, surf->src->width * 4);
            vesa_fb += g_vesa_info.maxBytesPerScanline;
            guest_fb += surf->src->stride;
        }
    }
    else
    {
        for (i = 0; i < surf->src->height; i++)
        {
            unsigned int p = i * surf->src->stride / XC_PAGE_SIZE;
            if (refresh_bitmap[p / 8] & (1 << (p % 8)))
            {
                memcpy(vesa_fb + i * g_vesa_info.maxBytesPerScanline,
                        guest_fb + i * surf->src->stride, surf->src->width * 4);
            }
        }
    }
}

static void
vesa_refresh_surface(struct surfman_plugin *plugin,
                      surfman_psurface_t psurface,
                      uint8_t *refresh_bitmap)
{
    vesa_surface *dst = (vesa_surface*) psurface;
    uint32_t size = dst->src->stride * dst->src->height;
    unsigned int i = 0;

    if (g_vesa_info.fb_need_cleanning)
    {
        vesa_clean_hostfb();
        g_vesa_info.fb_need_cleanning = 0;
        refresh_bitmap = NULL;
    }

    if (!dst) {
        return;
    }

    /* Check is resolution mismatch */
    if (dst->src->width != g_vesa_info.x ||
            dst->src->height != g_vesa_info.y)
    {
        vesa_copy_converted(dst, refresh_bitmap);
        return;
    }

    for (i = 0; i < ((size / XC_PAGE_SIZE) + 1); i++)
    {
        if (refresh_bitmap == NULL || refresh_bitmap[i / 8] & (1 << (i % 8)))
            memcpy(g_vesa_info.map + i * XC_PAGE_SIZE, dst->mapped_fb + i * XC_PAGE_SIZE, XC_PAGE_SIZE);
    }
}


static int
vesa_get_pages_from_psurface (surfman_plugin_t * p,
                               surfman_psurface_t psurface,
                               uint64_t * pages)
{
    vesa_surface *v = (vesa_surface*)psurface;
    unsigned int i = 0;
    unsigned int vesa_pages = g_vesa_info.mapSize / XC_PAGE_SIZE;

    /* Debug, disable psurface pages for the vesa plugin for the moment */
    return SURFMAN_ERROR;

    if (g_vesa_pages_taken)
        return SURFMAN_NOMEM;

    g_vesa_pages_taken = psurface;
    if (v->src->width != g_vesa_info.x ||
            v->src->height != g_vesa_info.y)
        return SURFMAN_ERROR;

    for (i = 0; i < v->src->page_count; i++)
        pages[i] = (g_vesa_info.mapPhys >> XC_PAGE_SHIFT) + (i % vesa_pages);
    info( "vesa_get_pages_from_psurface %lx", pages[0]);
    return SURFMAN_SUCCESS;
}

static void
vesa_free_psurface_pages (surfman_plugin_t * p,
                          surfman_psurface_t psurface)
{
    info("vesa_free_psurface_pages");
    g_vesa_pages_taken = NULL;
    return;
}

static void
print_transfert_rate(struct timeval *tv, unsigned int size)
{
    info("transfert rate %.2fMB/s (%d.%06d sec)",
            ((size * 1.0) / (tv->tv_sec + tv->tv_usec / 1000000)) / (1024 * 1024 * 1.0),
            tv->tv_sec, tv->tv_usec);
}

static void
vesa_timed_copy(uint8_t *dst, uint8_t *src, unsigned int size)
{
    struct timeval tv1, tv2, tv_res;

    gettimeofday(&tv1, NULL);
    memcpy(dst, src, size);
    gettimeofday(&tv2, NULL);
    timersub(&tv2, &tv1, &tv_res);
    print_transfert_rate(&tv_res, size);
}

static int
vesa_copy_surface_on_psurface (surfman_plugin_t * p,
                                surfman_psurface_t psurface)
{
    vesa_surface *v = (vesa_surface*) psurface;
    unsigned int size = v->src->stride * v->src->height;
    size_t rc;

    if (map_fb(v->src, psurface) != 0)
        return SURFMAN_ERROR;

    info("%s: %p,%p,%x v->mapped_fb[0]=%02x g_vesa_info.map[0]=%02x domid=%d", __func__,
            g_vesa_info.map, v->mapped_fb, size, v->mapped_fb[0],
            g_vesa_info.map[0], v->src->pages_domid);

    vesa_timed_copy(g_vesa_info.map, v->mapped_fb, size);
    return SURFMAN_SUCCESS;
}

static int
vesa_copy_psurface_on_surface (surfman_plugin_t * p,
                                surfman_psurface_t psurface)
{
    vesa_surface *v = (vesa_surface*) psurface;
    struct timeval tv1, tv2, tv_res;
    unsigned int size = v->src->stride * v->src->height;
    unsigned int i = 0;

    if (map_fb(v->src, psurface) != 0)
        return SURFMAN_ERROR;

    info("%s: %p,%p,%x v->mapped_fb[0]=%02x g_vesa_info.map[0]=%02x domid=%d", __func__,
            g_vesa_info.map, v->mapped_fb, size,v->mapped_fb[0],
            g_vesa_info.map[0], v->src->pages_domid);
    vesa_timed_copy(v->mapped_fb, g_vesa_info.map, size);
    return SURFMAN_SUCCESS;
}

static void
vesa_free_psurface (surfman_plugin_t * plugin,
                     surfman_psurface_t psurface)
{
    vesa_surface *surf = (vesa_surface*) psurface;
    info( "vesa_free_psurface");
    unmap_fb(surf);
}

surfman_vgpu_t *
vesa_new_vgpu (surfman_plugin_t * p,
                surfman_vgpu_info_t * info)
{
    info( "vesa_new_vgpu");
    return NULL;
}

void
vesa_free_vgpu (surfman_plugin_t * p,
                 surfman_vgpu_t * vgpu)
{
    info( "vesa_free_vgpu");
}


surfman_plugin_t surfman_plugin = {
    .init = vesa_init,
    .shutdown = vesa_shutdown,
    .display = vesa_display,
    .new_vgpu = vesa_new_vgpu,
    .free_vgpu = vesa_free_vgpu,
    .get_monitors = vesa_get_monitors,
    .set_monitor_modes = vesa_set_monitor_modes,
    .get_monitor_info = vesa_get_monitor_info_by_monitor,
    .get_monitor_edid = vesa_get_monitor_edid_by_monitor,
    .get_psurface_from_surface = vesa_get_psurface_from_surface,
    .update_psurface = vesa_update_psurface,
    .refresh_psurface = vesa_refresh_surface,
    .get_pages_from_psurface = vesa_get_pages_from_psurface,
    .free_psurface_pages = vesa_free_psurface_pages,
    .copy_surface_on_psurface = vesa_copy_surface_on_psurface,
    .copy_psurface_on_surface = vesa_copy_psurface_on_surface,
    .free_psurface = vesa_free_psurface,
    .options = {64, SURFMAN_FEATURE_NEED_REFRESH},
    .notify = SURFMAN_NOTIFY_NONE
};
