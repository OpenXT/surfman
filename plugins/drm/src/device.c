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

enum supported_device {
    SUPPORTED_DEVICE_I915 = 0,
    SUPPORTED_DEVICE_MAX
};

static const struct drm_device_ops *supported_devices[SUPPORTED_DEVICE_MAX] = {
    [SUPPORTED_DEVICE_I915] = &i915_ops,
};

/* Open the char-dev provided by DRM and initializes our object for it. */
INTERNAL struct drm_device *drm_device_init(const char *path, const struct drm_device_ops *ops)
{
    struct drm_device *d = NULL;

    d = calloc(1, sizeof (*d));
    if (!d) {
        DRM_ERR("Could not allocate memory (%s).", strerror(errno));
        return NULL;
    }
    strncpy(d->devnode, path, 255);
    INIT_LIST_HEAD(&d->monitors);
    INIT_LIST_HEAD(&d->planes);

    d->fd = open(d->devnode, O_RDWR | O_CLOEXEC);
    if (d->fd < 0) {
        DRM_ERR("Could not open device at \"%s\" (%s).", d->devnode, strerror(errno));
        free(d);
        return NULL;
    }
    d->ops = ops;

    list_add_tail(&d->l, &devices);
    drm_monitors_scan(d);

    return d;
}

INTERNAL void *drm_device_from_udev(struct udev *udev, struct udev_device *device)
{
    struct drm_device *d;
    unsigned int i;
    int rc;

    for (i = 0; i < SUPPORTED_DEVICE_MAX; ++i) {
        rc = supported_devices[i]->match(udev, device);
        switch (rc) {
            case 0:         /* New device. */
                d = drm_device_init(udev_device_get_devnode(device), supported_devices[i]);
                if (!d) {
                    DRM_ERR("Could not intialize device %s.", udev_device_get_syspath(device));
                    return NULL;
                }
                d->hotplug = hotplug_initialize(udev, device);  /* TODO: Might need some exclusion for multiple devices. */
                if (!d->hotplug) {
                    DRM_WRN("No hotplug management for device %s.", udev_device_get_syspath(device));
                }
                /* XXX: Backlight should be initialized per device here, but Surfman considers it a
                 *      plugin wide thing as plugins were device specific at first. */
                return d;
            case EEXIST:    /* Ignore potential udev redundancies (e.g. constrol64) */
                return NULL;
            default:
                continue;
        }
    }
    DRM_WRN("Unsupported graphic device %s.", udev_device_get_syspath(device));
    return NULL;
}

INTERNAL void drm_device_release(struct drm_device *device)
{
    if (device->hotplug) {
        hotplug_release(device->hotplug);
    }

    while (&device->monitors != device->monitors.next) {
        struct drm_monitor *m = list_entry(device->monitors.next, struct drm_monitor, l_dev);

        device->ops->unset(m);
        list_del(device->monitors.next);
        free(m);
    }
    free(device);
}

/* Search /d->monitors/ for one that is using connector with libDRM id /id/. */
INTERNAL struct drm_monitor *drm_device_find_monitor(struct drm_device *device,
                                                     uint32_t connector)
{
    struct drm_monitor *m, *mm;

    list_for_each_entry_safe(m, mm, &(device->monitors), l_dev) {
        if (m->connector == connector) {
            return m;   /* There's only one. */
        }
    }
    return NULL;
}

/* Add a monitor connected to connector /connector/ with prefered mode /prefered_mode/.
 * Check for duplicates. */
INTERNAL struct drm_monitor *drm_device_add_monitor(struct drm_device *device,
                                                    uint32_t connector,
                                                    drmModeModeInfo *prefered_mode)
{
    struct drm_monitor *m;

    m = drm_device_find_monitor(device, connector);
    if (!m) {
        m = calloc(1, sizeof (*m));
        if (!m) {
            DRM_ERR("Could not allocate memory (%s).", strerror(errno));
            return NULL;
        }
        m->connector = connector;
        memcpy(&(m->prefered_mode), prefered_mode, sizeof (*prefered_mode));
        m->device = device;
        list_add_tail(&m->l_dev, &(device->monitors));
    }
    return m;
}

/* Remove a monitor connected to connector /connector/ from device /device/ internal list.
 * Pipe is deconfigured and memory released for this monitor (if found) after this call. */
INTERNAL void drm_device_del_monitor(struct drm_device *device, uint32_t connector)
{
    struct drm_monitor *m;

    m = drm_device_find_monitor(device, connector);
    if (m) {
        list_del(&m->l_dev);
        if (m->surface) {
            device->ops->unset(m);
        }
        free(m);
    }
}

/* Find out if crtc /crtc/ is used by a monitor already. */
INTERNAL int drm_device_crtc_is_used(struct drm_device *device, uint32_t crtc)
{
    struct drm_monitor *m, *mm;

    list_for_each_entry_safe(m, mm, &(device->monitors), l_dev) {
        if (m->crtc == crtc) {
            return 1;
        }
    }
    return 0;
}

INTERNAL struct drm_plane *drm_device_find_plane(struct drm_device *device, uint32_t plane)
{
    struct drm_plane *p, *pp;

    list_for_each_entry_safe(p, pp, &device->planes, l) {
        if (p->id == plane) {
            return p;
        }
    }
    return NULL;
}

/* Add plane with id /plane/ to the list of currently active planes.
 * Check for duplicates. */
INTERNAL struct drm_plane *drm_device_add_plane(struct drm_device *device, uint32_t plane)
{
    struct drm_plane *p;

    p = drm_device_find_plane(device, plane);
    if (!p) {
        p = calloc(1, sizeof (*p));
        if (!p)
            return NULL;
        p->id = plane;
        p->device = device;
        list_add_tail(&p->l, &device->planes);
    }
    return p;
}

/* Remove a plane with id /plane/ from device /device/ internal list.
 * The framebuffer bound to that plane, if any, is released (DRM therefor deconfigure the plane). */
INTERNAL void drm_device_del_plane(struct drm_device *device, uint32_t plane)
{
    struct drm_plane *p;

    p = drm_device_find_plane(device, plane);
    if (p) {
        if (p->framebuffer) {
            p->framebuffer->ops->release(p->framebuffer);
        }
        list_del(&p->l);
        free(p);
    }
}

/* Find out if plane /plane/ is used on a monitor already. */
INTERNAL int drm_device_plane_is_used(struct drm_device *device, uint32_t plane)
{
    struct drm_plane *p, *pp;

    list_for_each_entry_safe(p, pp, &(device->planes), l) {
        if (p->id == plane) {
            return 1;
        }
    }
    return 0;
}

INTERNAL int drm_device_set_master(struct drm_device *device)
{
    int rc;

    rc = drmSetMaster(device->fd);
    if (rc) {
        rc = -errno;
        DRM_DBG("drmSetMaster failed (%s).", strerror(errno));
    }
    return rc;
}

INTERNAL void drm_device_drop_master(struct drm_device *device)
{
    if (drmDropMaster(device->fd)) {
        DRM_DBG("drmDropMaster failed (%s).", strerror(errno));
    }
}

