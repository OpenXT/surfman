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

#ifndef __DRM_PLUGIN_H__
# define __DRM_PLUGIN_H__

#define DEBUG 1

#define DRM_TAG     "[PLUGIN-DRM] "	/* Xmacro? */
# define DRM_DBG(fmt, ...) surfman_debug(DRM_TAG fmt, ##__VA_ARGS__)
# define DRM_INF(fmt, ...) surfman_info(DRM_TAG fmt, ##__VA_ARGS__)
# define DRM_WRN(fmt, ...) surfman_warning(DRM_TAG fmt, ##__VA_ARGS__)
# define DRM_ERR(fmt, ...) surfman_error(DRM_TAG fmt, ##__VA_ARGS__)

/*
 * libDRM side:
 * - A DRM Device has:
 *   * Connectors,
 *   * Encoders,
 *   * CRTCs
 * - A DRM device can allocate objects (BO).
 * - A BO can be defined to be a Framebuffer considering some geometry parameters
 * - A Framebuffer can be scanned by a Device.
 * - A Device scans a Framebuffer using a CRTC -> Encoder(s) -> Connector(s) triplet.
 * - A Plane is a portion of a Framebuffer that is composed (no blending) on
 *   top of the region scanned by a CRTC.
 */

/*
 * Our side:
 * - Surfman gives us a Surface: Pages & Geometry
 * - Each Device has a list of Monitors (one per connector)
 * - Each Monitor can display one Surface.
 * - Each Surface can be displayed on one or more monitors, if not displayed we release DRM related
 *   resources allocated for it (framebuffers/BOs).
 * - Each Monitor can use different methods to display a Surface (using planes or not).
 *
 * Paradigms:
 * - A Monitor is a libDRM Connector, either displaying a CRTC or not (un-initialized).
 *   * Implies drm_monitor_init can fail if no CRTC are available. /!\
 * - A Surface is device agnostic and only knows on which Monitor it is displayed.
 * - A Monitor knows how to Refresh its display (could imply planes or not, more than one Framebuffer, etc).
 * - Surfaces and Monitors are not entangled (on may out-live the other)
 * - Devices are entangled to their Monitors.
 *
 * Examples:
 * - Surface resizing event:
 *   1 Detach Monitors from the Surface (releasing FB/BO of libDRM).
 *   2 Surface geometry is updated.
 *   3 Monitors are configured to display the new Surface (allocates new BO/FB for libDRM)
 *
 * - Cloned display:
 *   1 New Connector is detected by libudev.
 *   2 Surfman is notified to rescan the Monitors.
 *   3 Surfman (since it doesn't handle it differently) tell the plugin to display the currently
 *     displayed Surface on that Monitor.
 *   4 Surface is attached to the new Monitor.
 *   5 Monitor finds a way to display the Surface.
 *
 */

/* Simple rectangle. */
struct rect {
    unsigned int x, y;              /* Upper left corner coordinates. */
    unsigned int w, h;              /* Width and heigth. */
};

/* Generic parameters of a framebuffer. */
struct framebuffer {
    unsigned int width, height;     /* Pixel map geometry (Could be != than mode, with resize) */
    unsigned int bpp;               /* Number of bits to represent a pixel. */
    unsigned int depth;             /* Number of meaningful bits to represent a pixel. */
    unsigned int pitch;             /* Size of a line in bytes. */
    unsigned int size;              /* Actual size in bytes. */

    off_t offset;                   /* Offset at which the pixels start. */
    uint8_t *map;                   /* Mapped framebuffer (if mapped). */
};

/* Our plugin surface to surfman. */
struct drm_surface {
    struct framebuffer fb;          /* Framebuffer info. */
    /* MFN info */
    unsigned long *mfns;            /* The MFNs translated or otherwise */
    uint32_t num_mfns;              /* The cound of MFNs */
    /* Debug */
    domid_t domid;                  /* Domid the surface belongs to. */
    /* Refs. */
    struct list_head monitors;      /* Monitors the surface is displayed on. */
};

/* Interface to manipulate DRM framebuffers. */
struct drm_framebuffer;
struct drm_device;
struct drm_framebuffer_ops {
    const char *name;
    struct drm_framebuffer *(*create)(struct drm_device *device, const struct drm_surface *surface);
    int (*map)(struct drm_framebuffer *this);
    void (*unmap)(struct drm_framebuffer *this);
    void (*refresh)(struct drm_framebuffer *this, const struct framebuffer *source,
                    const struct rect *region);
    void (*release)(struct drm_framebuffer *this);
};
struct drm_framebuffer {
    /* Generic geometry. */
    struct framebuffer fb;
    /* libDRM handles for allocated objects. */
    uint32_t handle;                    /* libDRM BO handler. */
    uint32_t id;                        /* libDRM framebuffer id. */

    /* Interface. */
    const struct drm_framebuffer_ops *ops;

    /* Refs. */
    struct drm_device *device;          /* Device for which that framebuffer is allocated. */
    int fd; /* private device fd for holding foreign mappings */
};

struct drm_plane {
    struct list_head l;                 /* List header for struct drm_device. */
    uint32_t id;                        /* libDRM id for that plane. */
    uint32_t crtc;                      /* libDRM CRTC scanning that plane. */

    /* Refs. */
    struct drm_framebuffer *framebuffer;
    struct drm_device *device;          /* TODO: Poor design with this back ref. */
};

/* Monitor informations to interface with Surfman. */
struct drm_monitor {
    struct list_head l_dev;         /* List header for struct drm_device. */
    struct list_head l_sur;         /* List header for struct drm_surface (ref only). */

    drmModeModeInfo prefered_mode;  /* Mode that should be prefered by Surfman. */

    uint32_t connector;             /* libDRM connector's ID. */
    uint32_t encoder;               /* libDRM encoder's ID. */
    uint32_t crtc;                  /* libDRM CRTC's ID. */

    struct drm_framebuffer *framebuffer; /* Currently displayed framebuffer on that monitor. */
    struct drm_plane *plane;        /* Plane composed on this monitor. */

    uint32_t dpms_prop_id;          /* libDRM DPMS property id for this connector. */

    /* Refs */
    struct drm_surface *surface;    /* Surface displayed currently. */
    struct drm_device *device;      /* Reference to the device (in case of multiple devices). */
};

/* Operations expected of a device. */
struct drm_device_ops {
    /* Set a source to display on a sink. */
    int (*set)(struct drm_monitor *sink, struct drm_surface *source);
    /* Unbind sink from source and release the sink resources. */
    void (*unset)(struct drm_monitor *sink);
    /* Sync the source and the sink (OPTIONNAL). */
    void (*refresh)(struct drm_monitor *sink, const struct drm_surface *source,
                    const struct rect *rectangle);

    /* Match the device to this set of ops.*/
    int (*match)(struct udev *udev, struct udev_device *dev);
};

/* Agregates ressources around one device this plugin manages. */
struct drm_device {
    struct list_head l;                 /* List header. */

    char devnode[256];                  /* Path in the devfs to the dri interface. */
    int fd;                             /* Open fd to the DRI interface. */

    const struct drm_device_ops *ops;   /* Standard ops for the device. */

    struct list_head monitors;          /* List of plugged monitors (in a pipe or not). List of connected connector to libDRM. */
    struct list_head planes;            /* List of planes currently in use. */

    struct hotplug *hotplug;            /* Object dealing with hoplug for that device. */
};

/* Hotplug handling. This is udev notifying the plugin. */
struct hotplug {
    struct udev *handle;
    struct udev_device *device;
    struct udev_monitor *monitor;

    struct event event;
};

/* Backlight handling. This has nothing to do with DRM in fact... */
struct backlight {
    struct udev *handle;
    struct udev_device *device;

    enum {
        BACKLIGHT_PROPRIETARY,
        BACKLIGHT_ACPI,
    } method;                           /* XXX: Do we mess with xcpmd or not? */
    unsigned int brightness_max;        /* Maximum value as reported by udev. */
#define BRIGHTNESS_STEP_COUNT 15        /* Go through 15 steps of brightness. */
    unsigned int brightness_step;       /* Step we want to increase brightness. */
    unsigned int brightness;            /* Current value (required for restore). */
};

#endif /* __DRM_PLUGIN_H__*/

