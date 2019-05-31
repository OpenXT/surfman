/*
 * Copyright (c) 2014 Citrix Systems, Inc.
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

#include "project.h"

const surfman_version_t surfman_plugin_version = SURFMAN_API_VERSION;

/* The list of devices this plugin found. */
struct list_head devices;

/* LibUdev handler. */
struct udev *udev;

/* Backlight object (should be device dependent, but we don't have that freedom). */
struct backlight *backlight;


/**
 * Define a set of acceptable scaling modes.
 */
enum scaling_modes {
    SCALING_MODE_FULLSCREEN = 0,
    SCALING_MODE_ASPECT,
    SCALING_MODE_FULL,    
    SCALING_MODE_NONE,   
    SCALING_MODE_COUNT
};

static const int scaling_modes_drm [SCALING_MODE_COUNT] = {
    [SCALING_MODE_FULLSCREEN] = DRM_MODE_SCALE_FULLSCREEN,
    [SCALING_MODE_ASPECT    ] = DRM_MODE_SCALE_ASPECT,
    [SCALING_MODE_FULL      ] = DRM_MODE_SCALE_CENTER,
    [SCALING_MODE_NONE      ] = DRM_MODE_SCALE_NONE

};

static const char * scaling_modes_config [SCALING_MODE_COUNT] = {
    [SCALING_MODE_FULLSCREEN] = "fullscreen",
    [SCALING_MODE_ASPECT    ] = "aspect",
    [SCALING_MODE_FULL      ] = "full",
    [SCALING_MODE_NONE      ] = "none"
};

/* Stores the scaling mode read from the configuration. Used when possible. */
int configured_scaling_mode = DRM_MODE_SCALE_FULLSCREEN;

/**
 * Attempts to read the default scaling mode from surfman.conf,
 * and populates configured_scaling_mode.
 */  
static void __read_configuration_scaling_mode() {

    int i;

    //Read the scaling mode provided in the configuration.
    const char * scaling_configuration = config_get(PLUGIN_NAME, CONFIG_SCALING_MODE);
  
    //If the user hasn't adjusted the configuration, use the default.
    if(!scaling_configuration) {
        return;
    }

    //Iterate over all scaling modes, and check to see if it's 
    //included in our configuration file.
    for(i = 0; i < SCALING_MODE_COUNT; ++i) {

        const char * scaling_mode = scaling_modes_config[i];

        //If we have a match, use it to set the scaling mode!
        if(strstr(scaling_configuration, scaling_mode)) {
            configured_scaling_mode = scaling_modes_drm[i]; 
            return;
        }
    }

}


INTERNAL int drmp_init(surfman_plugin_t *plugin)
{
    (void) plugin;
    int rc;

    INIT_LIST_HEAD(&devices);
    udev = udev_new();
    if (!udev) {
        DRM_ERR("Could not initialize libudev.");
        return SURFMAN_ERROR;
    }

    udev_settle(udev, 20);
    rc = udev_process_subsystem(udev, "drm", &drm_device_from_udev);
    if (rc || list_empty(&devices)) {
        DRM_ERR("Could not find any DRM compatible devices.");
        udev_unref(udev);
        return SURFMAN_ERROR;
    }

    backlight = backlight_init(udev);
    if (!backlight) {
        DRM_WRN("Could not manage backlight in Surfman.");
    }

    __read_configuration_scaling_mode();

    return SURFMAN_SUCCESS;
}

INTERNAL void drmp_shutdown(surfman_plugin_t *plugin)
{
    (void) plugin;
    struct drm_device *dev, *tmp;

    backlight_release(backlight);
    list_for_each_entry_safe(dev, tmp, &devices, l) {
        drm_device_release(dev);
    }
    udev_unref(udev);
}

INTERNAL int drmp_display(surfman_plugin_t *plugin, surfman_display_t *config, size_t size)
{
    (void) plugin;
    unsigned int i;
    int rc;

    for (i = 0; i < size; ++i) {
        struct drm_surface *s = config[i].psurface;
        struct drm_monitor *m = config[i].monitor;
        struct drm_device *d = m->device;

        assert(d != NULL);
        if (s == NULL) {
            /* Surfman wants to blank the monitor (rude!).
             * Unset any resource and move along... */
            drm_monitor_info(m);
            continue;
        }
        if (m->surface == s) {
            /* The monitor is already setup and displays that surface, nothing to do. */
            drm_monitor_info(m);
            continue;
        }
        if (!m->crtc) {
            /* The monitor is not setup yet. */
            rc = drm_monitor_init(m);
            if (rc) {
                drm_monitor_info(m);
                continue;   /* Skip errors. */
            }
        }
        if (m->surface) {
            /* Release the displayed surface resources. */
            d->ops->unset(m);
        }
        /* The monitor is already setup, but does not display the correct surface. */
        rc = d->ops->set(m, s);
        if (rc) {
            drm_monitor_info(m);
            continue;   /* Skip errors. */
        }
        if (d->ops->refresh) {
            struct rect r = {
                .x = 0, .y = 0, .w = s->fb.width, .h = s->fb.height
            };
            d->ops->refresh(m, s, &r);
            drm_monitor_info(m);
        }
    }

    return SURFMAN_SUCCESS;
}

INTERNAL int drmp_get_monitors(surfman_plugin_t *plugin, surfman_monitor_t *monitors, size_t size)
{
    (void) plugin;
    struct drm_device *d, *dd;
    unsigned int j = 0;
    int rc;

    list_for_each_entry_safe(d, dd, &devices, l) {
        struct drm_monitor *m, *mm;

        rc = drm_monitors_scan(d);
        if (rc < 0) {
            DRM_WRN("Failed to scan for monitors on device %s (%s).",
                    d->devnode, strerror(-rc));
            continue;
        }

        DRM_DBG("Device %s monitors:", d->devnode);
        list_for_each_entry_safe(m, mm, &(d->monitors), l_dev) {
            if (j >= size) {
                DRM_WRN("Surfman cannot manage all the reported monitors.");
                return SURFMAN_SUCCESS; /* Surfman cannot deal with more monitors. */
            }
            drm_monitor_info(m);
            monitors[j++] = m;
        }
    }
    return j;
}

INTERNAL int drmp_set_monitor_modes(struct surfman_plugin *plugin,
                                    surfman_monitor_t monitor, surfman_monitor_mode_t *mode)
{
    (void) plugin;
    (void) monitor;
    (void) mode;

    DRM_DBG("%s", __FUNCTION__);
    DRM_WRN("TBD");
    return SURFMAN_SUCCESS;
}

INTERNAL int drmp_get_monitor_info(struct surfman_plugin *plugin, surfman_monitor_t monitor,
                                   surfman_monitor_info_t *info, unsigned int modes_count)
{
    (void) plugin;
    struct drm_monitor *m = monitor;
    struct drm_device *d = m->device;
    drmModeConnector *c;
    unsigned int i;

    c = drmModeGetConnector(d->fd, m->connector);
    if (!c) {
        DRM_WRN("Could not access connector %u on device \"%s\" (%s).",
                m->connector, m->device->devnode, strerror(errno));
        return SURFMAN_ERROR;
    }
    /* From the biggest to the lowest, depending on how many Surfman can manage. */
    modes_count = min(modes_count, (unsigned int)c->count_modes);
    for (i = 0; i < modes_count; ++i) {
        info->modes[i].htimings[SURFMAN_TIMING_ACTIVE] = c->modes[i].hdisplay;
        info->modes[i].htimings[SURFMAN_TIMING_SYNC_START] = c->modes[i].hsync_start;
        info->modes[i].htimings[SURFMAN_TIMING_SYNC_END] = c->modes[i].hsync_end;
        info->modes[i].htimings[SURFMAN_TIMING_TOTAL] = c->modes[i].htotal;

        info->modes[i].vtimings[SURFMAN_TIMING_ACTIVE] = c->modes[i].vdisplay;
        info->modes[i].vtimings[SURFMAN_TIMING_SYNC_START] = c->modes[i].vsync_start;
        info->modes[i].vtimings[SURFMAN_TIMING_SYNC_END] = c->modes[i].vsync_end;
        info->modes[i].vtimings[SURFMAN_TIMING_TOTAL] = c->modes[i].vtotal;
    }
    drmModeFreeConnector(c);

    info->current_mode = &info->modes[0];
    info->prefered_mode = &info->modes[0];
    info->mode_count = modes_count;
    info->connectorid = m->connector;

    return SURFMAN_SUCCESS;
}

INTERNAL int drmp_get_monitor_info_by_monitor(surfman_plugin_t *p, surfman_monitor_t monitor,
                                              surfman_monitor_info_t *info, unsigned int modes_count)
{
    (void) p;
    (void) monitor;
    (void) info;
    (void) modes_count;
    DRM_WRN("TBD");

    return SURFMAN_SUCCESS;
}

INTERNAL surfman_psurface_t drmp_get_psurface_from_surface(surfman_plugin_t *plugin,
                                                           surfman_surface_t *surfman_surface)
{
    (void) plugin;
    struct drm_surface *s;

    s = malloc(sizeof (*s));
    if (!s) {
        DRM_ERR("Could not allocate memory (%s).", strerror(errno));
        return NULL;
    }
    s->fb.height = surfman_surface->height;
    s->fb.width = surfman_surface->width;
    s->fb.bpp = surfman_format_to_bpp(surfman_surface->format);
    s->fb.depth = surfman_format_to_depth(surfman_surface->format);
    if (!s->fb.bpp || !s->fb.depth) {
        DRM_ERR("Unknown framebuffer format.");
        free(s);
        return NULL;
    }
    s->fb.pitch = surfman_surface->stride;
    s->fb.size = surfman_surface->page_count * XC_PAGE_SIZE;
    s->fb.map = surface_map(surfman_surface);
    s->domid = surfman_surface->pages_domid;

    s->mfns = malloc(surfman_surface->page_count * sizeof (*s->mfns));
    if (!s->mfns) {
        DRM_ERR("Could not allocate memory (%s).", strerror(errno));
        free(s);
        return NULL;
    }
    memcpy(s->mfns, surfman_surface->mfns, surfman_surface->page_count * sizeof (*s->mfns));
    s->num_mfns = surfman_surface->page_count;

    INIT_LIST_HEAD(&(s->monitors));

    drm_surface_dump(s);
    return s;
}

INTERNAL int drmp_get_monitor_edid(struct surfman_plugin *plugin, surfman_monitor_t monitor,
                                   surfman_monitor_edid_t *edid)
{
    (void) plugin;
    (void) monitor;
    (void) edid;
    return SURFMAN_ERROR;
}

INTERNAL void drmp_update_psurface(surfman_plugin_t *plugin, surfman_psurface_t psurface,
                                   surfman_surface_t *surface, unsigned int flags)
{
    (void) plugin;
    struct drm_surface *s = psurface;
    int rc;

    if (flags & (SURFMAN_UPDATE_PAGES | SURFMAN_UPDATE_OFFSET)) {
        /* Remap Surfman's framebuffer. */
        surface_unmap(surface);
        s->fb.map = surface_map(surface);
    }
    s->fb.size = surface->page_count * XC_PAGE_SIZE;
    s->fb.offset = surface->offset;
    s->fb.pitch = surface->stride;
    s->fb.width = surface->width;
    s->fb.height = surface->height;
    s->fb.depth = surfman_format_to_depth(surface->format);
    s->fb.bpp = surfman_format_to_bpp(surface->format);
    s->domid = surface->pages_domid;

    if (flags & SURFMAN_UPDATE_PAGES) {
        s->mfns = realloc(s->mfns, sizeof (*s->mfns) * surface->page_count);
        if (!s->mfns) {
            /* XXX: We can't fail here!? */
            DRM_ERR("Could not allocate memory (%s).", strerror(errno));
        }
        memcpy(s->mfns, surface->mfns, sizeof (*s->mfns) * surface->page_count);
        s->num_mfns = surface->page_count;
    }

    if (!s->fb.depth || !s->fb.bpp) {
        DRM_WRN("Invalid pixel format for dom%u surface.", s->domid);
    }

    if (flags & (SURFMAN_UPDATE_FORMAT | SURFMAN_UPDATE_PAGES)) {
        struct drm_monitor *m, *mm;
        struct list_head ms;
        /* TODO: That list management is a bit painful but required. */
        struct ptr {
            struct list_head l;
            void *p;
        } *t;

        INIT_LIST_HEAD(&ms);
        list_for_each_entry_safe(m, mm, &(s->monitors), l_sur) {
            m->device->ops->unset(m);
            t = calloc(1, sizeof (*t));
            t->p = m;
            list_add_tail(&(t->l), &ms);
        }
        while (&ms != ms.next) {
            t = container_of(ms.next, struct ptr, l);
            m = t->p;
            rc = m->device->ops->set(m, s);
            if (rc) {
                DRM_WRN("set(%u, dom%u) (%s).", m->connector, s->domid, strerror(-rc));
            }
            /* XXX: No need to refresh here, the device model will have to regenerate
             *      dirty-rectangles or the dirty-bitmap will be filled. */
            list_del(&(t->l));
            free(t);
        }
    }
}

#define DIV_CEIL(x, y) (((x) + (y) - 1) / (y))
INTERNAL void drmp_refresh_psurface(struct surfman_plugin *plugin,
                                    surfman_psurface_t psurface, uint8_t *db)
{
    (void) plugin;
    struct drm_surface *s = psurface;
    unsigned int b, i;  /* First and last dirty page. */
    struct drm_monitor *m, *mm;
    struct rect r = {
        .x = 0, .y = 0,
        .w = s->fb.width, .h = s->fb.height
    };

    /* TODO: Shortcut that if we don't need refresh... */
    if (db) {
        unsigned int n = DIV_CEIL(s->fb.pitch * s->fb.height, XC_PAGE_SIZE);
        /* TODO: Take this arithmetic in Qemu and generate dirty rects ... */
	/* TODO: bsf/bsr!! Sounds like a beer and crisps evening.
         *       The implementation in libsurfman is too simple, we want more than one rect. */
        for (i = 0; i < n; ++i) {
            if (db[i / 8] & (1 << (i % 8))) {
                b = i;
                do {
                    ++i;
                } while ((i < n) && (db[i / 8] & (1 << (i % 8))));
                r.y = (b * XC_PAGE_SIZE) / s->fb.pitch;
                r.h = (i * XC_PAGE_SIZE - 1) / s->fb.pitch;
                r.h = min(r.h, s->fb.height - 1);   /* Page can end outside the FB region. */
                r.h -= r.y - 1; /* Don't forget r.y itslelf. */
                list_for_each_entry_safe(m, mm, &(s->monitors), l_sur) {
                    m->device->ops->refresh(m, s, &r);
                }
            }
        }
    } else {
        list_for_each_entry_safe(m, mm, &(s->monitors), l_sur) {
            m->device->ops->refresh(m, s, &r);
        }
    }
}

INTERNAL void drmp_free_psurface(surfman_plugin_t *plugin, surfman_psurface_t psurface)
{
    (void) plugin;
    struct drm_surface *s = psurface;
    struct drm_monitor *m, *mm;

    list_for_each_entry_safe(m, mm, &(s->monitors), l_sur) {
        m->device->ops->unset(m);
    }
    /* XXX: Surfman should unmap the surface. */
    free(s->mfns);
    free(s);
}

INTERNAL void drmp_increase_brightness(surfman_plugin_t *plugin)
{
    (void) plugin;
    DRM_DBG("%s, backlight=%p...", __FUNCTION__, backlight);
    if (!backlight) {
        return;
    }
    backlight_increase(backlight);
}

INTERNAL void drmp_decrease_brightness(surfman_plugin_t *plugin)
{
    (void) plugin;
    DRM_DBG("%s, backlight=%p...", __FUNCTION__, backlight);
    if (!backlight) {
        return;
    }
    backlight_decrease(backlight);
}

INTERNAL void drmp_restore_brightness(surfman_plugin_t *plugin)
{
    (void) plugin;
    DRM_DBG("%s, backlight=%p...", __FUNCTION__, backlight);
    if (!backlight) {
        return;
    }
    backlight_restore(backlight);
}

INTERNAL void drmp_dpms_on(surfman_plugin_t *plugin)
{
    (void) plugin;
    struct drm_device *d;
    struct drm_monitor *m;
    int rc;

    DRM_DBG("%s, dpms on", __FUNCTION__);

    list_for_each_entry(d, &devices, l) {
        drm_device_set_master(d);
        list_for_each_entry(m, &(d->monitors), l_dev) {
            rc = drm_monitor_dpms_on(m);
            if (rc) {
                DRM_DBG("%s, dpms on failed for monitor at conn=%d, enc=%d, "
                        "crtc=%d - %s", __FUNCTION__, m->connector, m->encoder,
                        m->crtc, strerror(rc));
            }
        }
        drm_device_drop_master(d);
    }
}

INTERNAL void drmp_dpms_off(surfman_plugin_t *plugin)
{
    (void) plugin;
    struct drm_device *d;
    struct drm_monitor *m;
    int rc;

    DRM_DBG("%s, dpms off", __FUNCTION__);

    list_for_each_entry(d, &devices, l) {
        drm_device_set_master(d);
        list_for_each_entry(m, &(d->monitors), l_dev) {
            rc = drm_monitor_dpms_off(m);
            if (rc) {
                DRM_DBG("%s, dpms off failed for monitor at conn=%d, enc=%d, "
                        "crtc=%d - %s", __FUNCTION__, m->connector, m->encoder,
                        m->crtc, strerror(rc));
            }
        }
        drm_device_drop_master(d);
    }
}

#define OPTIONAL (NULL)
#define REQUIRED ((void*)0xDEADBEEF)
/* Surfman plugin interface. */
surfman_plugin_t surfman_plugin = {
    /* Entry/Exit points */
    .init = drmp_init,
    .shutdown = drmp_shutdown,

    /* Management of surface to monitor mapping. */
    .display = drmp_display,

    /* Monitors management. */
    .get_monitors = drmp_get_monitors,
    .set_monitor_modes = drmp_set_monitor_modes,
    .get_monitor_info = drmp_get_monitor_info,
    .get_monitor_edid = drmp_get_monitor_edid,

    /* psurface management. */
    .get_psurface_from_surface = drmp_get_psurface_from_surface,
    .update_psurface = drmp_update_psurface,
    .refresh_psurface = drmp_refresh_psurface,
    .get_pages_from_psurface = OPTIONAL,    /* Not even called in surfman ? */
    .free_psurface_pages = OPTIONAL,
    .copy_surface_on_psurface = OPTIONAL,
    .free_psurface = drmp_free_psurface,

    /* S3 management. */
    .pre_s3 = OPTIONAL,                 /* DRM engine should deal with that. */
    .post_s3 = OPTIONAL,                /* DRM engine should deal with that. */

    /* Brightness management (for integrated displays). */
    .increase_brightness = drmp_increase_brightness,
    .decrease_brightness = drmp_decrease_brightness,
    .restore_brightness = drmp_restore_brightness,

    /* DPMS mode management. */
    .dpms_on = drmp_dpms_on,
    .dpms_off = drmp_dpms_off,

    .options = {
        64,   /* libDRM requires a 64 bytes alignment (not 64bit ;). */
        0     /* TODO: SURFMAN_FEATURE_NEED_REFRESH triggers a cache-incohrency with xenfb2
                       and foreign method (looks like scrambling when moving something on the screen.
                       I thought this was fixed with linux-pq.git:master/enable-pat. */
    },
    .notify = SURFMAN_NOTIFY_NONE
};
