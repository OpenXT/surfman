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

/* drm-plugin.c */
extern const surfman_version_t surfman_plugin_version;
extern struct list_head devices;
extern struct udev *udev;
extern struct backlight *backlight;
extern int drmp_init(surfman_plugin_t *plugin);
extern void drmp_shutdown(surfman_plugin_t *plugin);
extern int drmp_display(surfman_plugin_t *plugin, surfman_display_t *config, size_t size);
extern int drmp_get_monitors(surfman_plugin_t *plugin, surfman_monitor_t *monitors, size_t size);
extern int drmp_set_monitor_modes(struct surfman_plugin *plugin, surfman_monitor_t monitor, surfman_monitor_mode_t *mode);
extern int drmp_get_monitor_info(struct surfman_plugin *plugin, surfman_monitor_t monitor, surfman_monitor_info_t *info, unsigned int modes_count);
extern int drmp_get_monitor_info_by_monitor(surfman_plugin_t *p, surfman_monitor_t monitor, surfman_monitor_info_t *info, unsigned int modes_count);
extern surfman_psurface_t drmp_get_psurface_from_surface(surfman_plugin_t *plugin, surfman_surface_t *surfman_surface);
extern int drmp_get_monitor_edid(struct surfman_plugin *plugin, surfman_monitor_t monitor, surfman_monitor_edid_t *edid);
extern void drmp_update_psurface(surfman_plugin_t *plugin, surfman_psurface_t psurface, surfman_surface_t *surface, unsigned int flags);
extern void drmp_refresh_psurface(struct surfman_plugin *plugin, surfman_psurface_t psurface, uint8_t *db);
extern void drmp_free_psurface(surfman_plugin_t *plugin, surfman_psurface_t psurface);
extern void drmp_increase_brightness(surfman_plugin_t *plugin);
extern void drmp_decrease_brightness(surfman_plugin_t *plugin);
extern void drmp_restore_brightness(surfman_plugin_t *plugin);
extern surfman_plugin_t surfman_plugin;
/* device.c */
extern struct drm_device *drm_device_init(const char *path, const struct drm_device_ops *ops);
extern void *drm_device_from_udev(struct udev *udev, struct udev_device *device);
extern void drm_device_release(struct drm_device *device);
extern struct drm_monitor *drm_device_find_monitor(struct drm_device *device, uint32_t connector);
extern struct drm_monitor *drm_device_add_monitor(struct drm_device *device, uint32_t connector, drmModeModeInfo *prefered_mode);
extern void drm_device_del_monitor(struct drm_device *device, uint32_t connector);
extern int drm_device_crtc_is_used(struct drm_device *device, uint32_t crtc);
extern struct drm_plane *drm_device_find_plane(struct drm_device *device, uint32_t plane);
extern struct drm_plane *drm_device_add_plane(struct drm_device *device, uint32_t plane);
extern void drm_device_del_plane(struct drm_device *device, uint32_t plane);
extern int drm_device_plane_is_used(struct drm_device *device, uint32_t plane);
extern int drm_device_set_master(struct drm_device *device);
extern void drm_device_drop_master(struct drm_device *device);
/* device-intel.c */
extern const struct drm_device_ops i915_ops;
/* framebuffer-dumb.c */
extern struct drm_framebuffer *__dumb_framebuffer_create(struct drm_device *device, unsigned int width, unsigned int height, unsigned int depth, unsigned int bpp);
extern const struct drm_framebuffer_ops framebuffer_dumb_ops;
/* framebuffer-i915_foreign.c */
extern const struct drm_framebuffer_ops framebuffer_foreign_ops;
/* monitor.c */
extern void drm_monitor_info(const struct drm_monitor *m);
extern int drm_monitors_scan(struct drm_device *device);
extern int drm_monitor_init(struct drm_monitor *monitor);
/* udev.c */
extern int udev_process_subsystem(struct udev *udev, const char *subsystem, void *(*action)(struct udev *, struct udev_device *));
extern void udev_settle(struct udev *udev, unsigned int timeout);
extern struct udev_device *udev_device_new_from_drm_device(struct udev *udev, struct udev_device *dev);
extern unsigned int udev_device_get_sysattr_uint(struct udev_device *device, const char *sysattr);
extern void udev_device_set_sysattr_uint(struct udev_device *device, const char *sysattr, unsigned int u);
extern unsigned int udev_syspath_get_sysattr_uint(const char *syspath, const char *sysattr);
extern void udev_syspath_set_sysattr_uint(const char *syspath, const char *sysattr, unsigned int u);
/* hotplug.c */
extern struct hotplug *hotplug_initialize(struct udev *udev, struct udev_device *device);
extern void hotplug_release(struct hotplug *hotplug);
/* backlight.c */
extern struct backlight *backlight_init(struct udev *udev);
extern void backlight_increase(struct backlight *backlight);
extern void backlight_decrease(struct backlight *backlight);
extern void backlight_restore(struct backlight *backlight);
extern void backlight_release(struct backlight *backlight);
