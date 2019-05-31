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

/*
 * Callback for udev when drm subsystem throws an event.
 */
static void hotplug_event_handler(int fd, short event, void *priv)
{
    struct hotplug *hotplug = priv;
    struct udev_device *dev;
    const char *devnode;

    (void) fd;
    (void) event;

    /* Notify surfman to rescan monitors.
     * This will trigger a callback to the plugin on Surfman's terms. */
    surfman_plugin.notify |= SURFMAN_NOTIFY_MONITOR_RESCAN;

    dev = udev_monitor_receive_device(hotplug->monitor);
    if (!dev) {
        DRM_WRN("Could not recover the device which triggered this udev event (%s).",
                strerror(errno));
        return;
    }

    devnode = udev_device_get_devnode(dev);
    DRM_DBG("%s: %s (%s) triggred `%s' event.", udev_device_get_subsystem(dev),
            udev_device_get_sysname(dev), devnode, udev_device_get_action(dev));
    udev_device_unref(dev);
    return;
}

/*
 * Create relevant structure to asynchronously recover hotplug event.
 */
static int hotplug_setup_event_handler(struct hotplug *hotplug)
{
    int rc;

    hotplug->monitor = udev_monitor_new_from_netlink(hotplug->handle, "udev");
    if (!hotplug->monitor) {
        rc = -errno;
        DRM_DBG("udev_monitor_new_from_netlink failed (%s).", strerror(errno));
        return rc;
    }
    udev_monitor_filter_add_match_subsystem_devtype(hotplug->monitor, "drm", NULL);

    rc = udev_monitor_get_fd(hotplug->monitor);
    if (rc < 0) {
        rc = -errno;
        DRM_DBG("udev_monitor_get_fd failed (%s).", strerror(errno));
        udev_monitor_unref(hotplug->monitor);
        return rc;
    }

    event_set(&(hotplug->event), rc, EV_READ | EV_PERSIST, hotplug_event_handler, hotplug);
    event_add(&(hotplug->event), NULL);

    udev_monitor_enable_receiving(hotplug->monitor);

    return 0;
}

INTERNAL struct hotplug *hotplug_initialize(struct udev *udev, struct udev_device *device)
{
    struct hotplug *hp;
    int rc;

    hp = calloc(1, sizeof (*hp));
    if (!hp) {
        DRM_ERR("calloc: failed (%s).", strerror(errno));
        return NULL;
    }
    hp->handle = udev_ref(udev);
    hp->device = device;
    rc = hotplug_setup_event_handler(hp);
    if (rc) {
        DRM_ERR("Could not setup hotplug detection on device %s (%s).",
                udev_device_get_syspath(device), strerror(-rc));
        free(hp);
        return NULL;
    }
    return hp;
}

INTERNAL void hotplug_release(struct hotplug *hotplug)
{
    event_del(&(hotplug->event));
    udev_monitor_unref(hotplug->monitor);
    udev_unref(hotplug->handle);
    free(hotplug);
}

