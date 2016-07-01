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
 * libudev list_entry name is a syspath that can be used to get a device..
 */
static inline struct udev_device *udev_device_new_from_list_entry(struct udev *udev,
                                                                  struct udev_list_entry *entry)
{
    return udev_device_new_from_syspath(udev, udev_list_entry_get_name(entry));
}

static int udev_process_subsystem_enum(struct udev *udev, const char *subsystem,
                                       void *(*action)(struct udev *, struct udev_device *))
{
    struct udev_enumerate *e;
    struct udev_list_entry *devs, *deve;
    int rc = -ENODEV;

    e = udev_enumerate_new(udev);
    if (!e) {
        DRM_DBG("udev_enumerate_new failed (%s).", strerror(errno));
        return -ENODEV;
    }
    udev_enumerate_add_match_is_initialized(e);
    udev_enumerate_add_match_subsystem(e, subsystem);

    udev_enumerate_scan_devices(e);
    devs = udev_enumerate_get_list_entry(e);
    if (!devs) {
        DRM_DBG("udev_enumerate_get_list_entry failed (%s).", strerror(errno));
        udev_enumerate_unref(e);
        return -ENODEV;
    }
    udev_list_entry_foreach(deve, devs) {
        struct udev_device *d;

        d = udev_device_new_from_list_entry(udev, deve);
        if (action(udev, d) != NULL)
            rc = 0;
        udev_device_unref(d);
    }
    udev_enumerate_unref(e);
    return rc;
}

INTERNAL int udev_process_subsystem(struct udev *udev, const char *subsystem,
                                    void *(*action)(struct udev *, struct udev_device *))
{
    int rc = 0;
    fd_set readfds;
    int udevfd;
    struct timeval tv;
    struct udev_monitor *udev_mon;
    struct udev_device *udev_dev;

    udev_mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(udev_mon, subsystem, NULL);
    udev_monitor_enable_receiving(udev_mon);
    udevfd = udev_monitor_get_fd(udev_mon);

    do {
        /* Try to enumerate to find our device in case it was already created */
        if (!rc) {
            if (!udev_process_subsystem_enum(udev, subsystem, action)) {
                DRM_INF("Found %s device using enumeration.", subsystem);
                break;
            }
        }

        /* Try to monitor to find our device in case it has not been created */
        FD_ZERO(&readfds);
        FD_SET(udevfd, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 1000;

        rc = select(udevfd + 1, &readfds, NULL, NULL, &tv);
        if (rc > 0 && FD_ISSET(udevfd, &readfds)) {
            udev_dev = udev_monitor_receive_device(udev_mon);
            if (!udev_dev) {
                DRM_DBG("No device in monitor callback?");
                continue;
            }

            if (action(udev, udev_dev) != NULL) {
                DRM_INF("Found %s device using monitor.", subsystem);
                udev_device_unref(udev_dev);
                rc = 0;
                break;
            }
            udev_device_unref(udev_dev);
        }
    } while (1);

    udev_monitor_unref(udev_mon);
    return rc;
}

/*
 * Wait /timeout/ seconds or until udev queue is empty, which ever comes first.
 */
INTERNAL void udev_settle(struct udev *udev, unsigned int timeout)
{
    struct udev_queue *queue;
    unsigned int i;

    queue = udev_queue_new(udev);
    if (!queue) {
        DRM_DBG("udev_queue_new failed (%s).", strerror(errno));
        return;
    }

    for (i = 0; i < timeout; ++i) {
        if (udev_queue_get_queue_is_empty(queue)) {
            break;
        }
        DRM_DBG("udev queue is not empty, waiting...");
        sleep(1);
    }
    DRM_DBG("udev queue is empty.");
    udev_queue_unref(queue);
}

/*
 * Devices in /drm/ subsystem don't know about their drivers directly, instead this information is in
 * de /pci/ subsystem, which is accessible through link /device/ from subsystem /drm/.
 * That helper does that...
 */
INTERNAL struct udev_device *udev_device_new_from_drm_device(struct udev *udev, struct udev_device *dev)
{
    const char *syspath;
    char new_syspath[256] = { 0 };
    struct udev_device *new_dev;

    syspath = udev_device_get_syspath(dev);
    if (!syspath) {
        DRM_WRN("Could not recover syspath of device %p (%s).", dev, strerror(errno));
        return NULL;
    }
    if (snprintf(new_syspath, 255, "%s/%s", syspath, "device") >= 255) {
        DRM_WRN("syspath %s is too long...", syspath);
        return NULL;
    }

    new_dev = udev_device_new_from_syspath(udev, new_syspath);
    if (!new_dev) {
        DRM_WRN("Could not recover device from syspath %s (%s).", new_syspath, strerror(errno));
        return NULL;
    }
    return new_dev;
}

/*
 * Read and returns the uint value of sysattr for device.
 */
INTERNAL unsigned int udev_device_get_sysattr_uint(struct udev_device *device, const char *sysattr)
{
    const char *value;
    char *end;
    unsigned int u;

    DRM_DBG("udev_device_get_sysattr_value(%s, %s)",
            udev_device_get_syspath(device), sysattr);
    value = udev_device_get_sysattr_value(device, sysattr);
    if (!value) {
        errno = ENOENT;
        return ~0U;
    }
    u = strtoul(value, &end, 0);
    if (value == end) {
        errno = EINVAL;
        return ~0U;
    }
    return u;
}

/*
 * Write the uint value of sysattr for device.
 */
INTERNAL void udev_device_set_sysattr_uint(struct udev_device *device, const char *sysattr,
                                           unsigned int u)
{
    char path[256];
    char tmp[16];
    int n, fd;

    n = snprintf(tmp, 16, "%u", u);
    /* XXX: Not yet standard... Will come with later versions of udev. */
    //udev_device_set_sysattr_value(device, sysattr, tmp);

    snprintf(path, 256, "%s/%s", udev_device_get_syspath(device), sysattr);
    fd = open(path, O_RDWR);
    if (fd < 0) {
        DRM_DBG("open(): failed (%s).", strerror(errno));
        return;
    }
    if (write(fd, tmp, n) != n) {
        DRM_DBG("write(): failed or incomplet (%s).", strerror(errno));
    }
    close(fd);
}

/*
 * Read and return uint value of sysattr under syspath in sysfs.
 */
INTERNAL unsigned int udev_syspath_get_sysattr_uint(const char *syspath, const char *sysattr)
{
    char path[256];
    char tmp[16], *end;
    int n, fd;
    unsigned int u = ~0U;

    snprintf(path, 256, "%s/%s", syspath, sysattr);
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        DRM_DBG("open(): failed (%s).", strerror(errno));
        return ~0U;
    }
    n = read(fd, tmp, 16);
    if (n < 0) {
        DRM_DBG("read(): failed (%s).", strerror(errno));
    } else {
        u = strtoul(tmp, &end, 0);
        if (tmp == end) {
            errno = EINVAL;
            u = ~0U;
        }
    }
    close(fd);
    return u;
}

/*
 * Write uint to sysattr value under syspath in sysfs.
 */
INTERNAL void udev_syspath_set_sysattr_uint(const char *syspath, const char *sysattr,
                                            unsigned int u)
{
    char path[256];
    char tmp[16];
    int n, fd;

    n = snprintf(tmp, 16, "%u", u);
    snprintf(path, 256, "%s/%s", syspath, sysattr);
    fd = open(path, O_RDWR);
    if (fd < 0) {
        DRM_DBG("open(): failed (%s).", strerror(errno));
        return;
    }
    if (write(fd, tmp, n) != n) {
        DRM_DBG("write(): failed or incomplet (%s).", strerror(errno));
    }
    close(fd);
}

