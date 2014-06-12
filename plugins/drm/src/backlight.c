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
 * XXX: Hack to counter XCPMD until I figure out a proper way to fix that...
 * Disable ACPI module if we are using proprietary method to take right of way from xcpmd.
 */
static int acpi_video_brightness_switch(int state)
{
    const char *values[] = { "N", "Y" };
    int fd, rc = 0;

    fd = open("/sys/module/video/parameters/brightness_switch_enabled", O_RDWR);
    if (fd < 0) {
        rc = -errno;
        DRM_DBG("open(): failed (%s).", strerror(-rc));
        return rc;
    }
    if (write(fd, values[!!state], 1) != 1) {
        rc = -errno;
        DRM_DBG("write(): failed (%s).", strerror(-rc));
    }
    close(fd);
    return rc;
}

static inline void backlight_override_acpi_bcl_control(struct backlight *backlight)
{
    int rc;

    switch (backlight->method) {
        case BACKLIGHT_PROPRIETARY:
            rc = acpi_video_brightness_switch(0);
            if (rc) {
                DRM_DBG("Could not disable video acpi brigthness... (%s).", strerror(-rc));
            }
            break;
        case BACKLIGHT_ACPI:
            /* Nothing... */
            break;
    }
}

INTERNAL struct backlight *backlight_init(struct udev *udev)
{
    struct backlight *bl;

    bl = calloc(1, sizeof (*bl));
    if (!bl) {
        DRM_ERR("calloc: %s", strerror(errno));
        return NULL;
    }
    bl->handle = udev_ref(udev);
    bl->method = BACKLIGHT_PROPRIETARY;
    bl->device = udev_device_new_from_subsystem_sysname(bl->handle,
                                                    "backlight", "intel_backlight");
    if (!bl->device) {
        DRM_DBG("backlight/intel_backlight missing (%s).", strerror(errno));
        bl->method = BACKLIGHT_ACPI;
        bl->device = udev_device_new_from_subsystem_sysname(bl->handle,
                                                        "backlight", "acpi_video0");
        if (!bl->device) {
            DRM_DBG("backlight/acpi_video0 missing (%s).", strerror(errno));
            goto fail_devi;
        }
    }
    bl->brightness_max = udev_device_get_sysattr_uint(bl->device, "max_brightness");
    if (bl->brightness_max == ~0U) {
        DRM_DBG("backlight/max_brightness missing (%s).", strerror(errno));
        goto fail_read;
    }
    bl->brightness = udev_device_get_sysattr_uint(bl->device, "brightness");
    if (bl->brightness == ~0U) {
        DRM_DBG("backlight/brightness missing (%s).", strerror(errno));
        goto fail_read;
    }
    bl->brightness_step = bl->brightness_max / BRIGHTNESS_STEP_COUNT;

    return bl;

fail_read:
    udev_device_unref(bl->device);
fail_devi:
    udev_unref(bl->handle);
    free(bl);
    return NULL;
}

INTERNAL void backlight_increase(struct backlight *backlight)
{
    if (backlight->brightness + backlight->brightness_step > backlight->brightness_max) {
        backlight->brightness = backlight->brightness_max;
    } else {
        backlight->brightness += backlight->brightness_step;
    }
    backlight_override_acpi_bcl_control(backlight);
    udev_device_set_sysattr_uint(backlight->device, "brightness", backlight->brightness);
}

INTERNAL void backlight_decrease(struct backlight *backlight)
{
    if (backlight->brightness == backlight->brightness_max) {
        backlight->brightness -= backlight->brightness_max % BRIGHTNESS_STEP_COUNT;
    }
    if (backlight->brightness <= backlight->brightness_step) {
        backlight->brightness = 0;
    } else {
        backlight->brightness -= backlight->brightness_step;
    }
    backlight_override_acpi_bcl_control(backlight);
    udev_device_set_sysattr_uint(backlight->device, "brightness", backlight->brightness);
}

INTERNAL void backlight_restore(struct backlight *backlight)
{
    backlight_override_acpi_bcl_control(backlight);
    udev_device_set_sysattr_uint(backlight->device, "brightness", backlight->brightness);
}

INTERNAL void backlight_release(struct backlight *backlight)
{
    udev_device_unref(backlight->device);
    udev_unref(backlight->handle);
    free(backlight);
}
