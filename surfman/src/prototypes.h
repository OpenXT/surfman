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
extern int main(int argc, char *argv[]);
/* domain.c */
extern int domain_exists(struct domain *d);
extern int domain_dying(struct domain *d);
extern struct domain *domain_by_domid(int domid);
extern void domain_destroy(struct domain *d);
extern struct domain *domain_create(int domid);
extern int domain_has_vgpu(struct domain *d);
extern int domain_visible(struct domain *d);
extern int domain_set_visible(struct domain *d, int force);
extern int domain_get_visible(void);
extern void domain_monitor_update(int monitor_id, int enable);
extern void *device_create(struct domain *d, struct device_ops *ops, size_t size);
extern void device_destroy(struct device *device);
extern int dump_all_screens(const char *directory);
/* dbus_glue.c */
extern dbus_bool_t dbus_display_text(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_display_image(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_dump_all_screens(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_increase_brightness(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_decrease_brightness(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_pre_s3(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_post_s3(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_set_pv_display(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_set_visible(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_vgpu_mode(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_get_visible(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_has_vgpu(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_notify_death(DBusMessage *msg, DBusMessage *reply);
extern dbus_bool_t dbus_notify_visible_domain_changed(int domid);
/* dbus.c */
extern DBusHandlerResult dbus_message(DBusConnection *connection, DBusMessage *msg, void *user_data);
extern void dbus_start_service(void);
extern void dbus_cleanup(void);
extern int dbus_init(void);
/* util.c */
extern void helper_exec(const char *bin, ...);
extern void renable_core_dumps(void);
extern void trap_segv(void);
/* lockfile.c */
extern int lockfile_make(char *name);
extern void lockfile_lock(void);
/* surface.c */
extern int surface_register_onscreen(struct surface *s, display_handler_t h, void *priv);
extern int surface_unregister_onscreen(struct surface *s, display_handler_t h);
extern int surface_register_offscreen(struct surface *s, display_handler_t h, void *priv);
extern int surface_unregister_offscreen(struct surface *s, display_handler_t h);
extern int surface_need_refresh(struct surface *s);
extern void surface_refresh(struct surface *s, uint8_t *dirty);
extern struct surface *surface_create(struct device *dev, void *priv);
extern surfman_psurface_t surface_get_psurface(struct surface *s, struct plugin *p);
extern void surface_destroy(struct surface *s);
extern int surface_ready(struct surface *s);
extern void surface_update_mmap(struct surface *s, int domid, int fd, pfn_t *mfns, size_t npages);
extern void surface_update_pfns(struct surface *s, int domid, const xen_pfn_t *pfns, pfn_t *mfns, size_t npages);
extern void surface_update_lfb(struct surface *s, int domid, unsigned long lfb, size_t len);
extern void surface_update_format(struct surface *s, int width, int height, int stride, int format, int offset);
extern void surface_update_offset(struct surface *s, size_t offset);
extern void surface_refresh_stall(struct surface *s);
extern void surface_refresh_resume(struct surface *s);
/* xenstore.c */
extern _Bool xenstore_transaction_start(void);
extern _Bool xenstore_transaction_end(_Bool abort);
extern _Bool vxenstore_rm(const char *format, va_list arg);
extern _Bool xenstore_rm(const char *format, ...);
extern _Bool xenstore_mkdir(const char *format, ...);
extern _Bool vxenstore_write(const char *data, const char *format, va_list arg);
extern _Bool xenstore_write(const char *data, const char *format, ...);
extern char **vxenstore_ls(unsigned int *num, const char *format, va_list arg);
extern char **xenstore_ls(unsigned int *num, const char *format, ...);
extern _Bool vxenstore_write_int(int data, const char *format, va_list arg);
extern _Bool xenstore_write_int(int data, const char *format, ...);
extern char *vxenstore_read(const char *format, va_list arg);
extern char *xenstore_read(const char *format, ...);
extern _Bool vxenstore_chmod(const char *perms, unsigned int nbperm, const char *format, va_list arg);
extern _Bool xenstore_chmod(const char *perms, unsigned int nbperm, const char *format, ...);
extern _Bool vxenstore_dom_write(unsigned int domid, const char *data, const char *format, va_list arg);
extern _Bool xenstore_dom_write(unsigned int domid, const char *data, const char *format, ...);
extern _Bool vxenstore_dom_write_int(unsigned int domid, int data, const char *format, va_list arg);
extern _Bool xenstore_dom_write_int(unsigned int domid, int data, const char *format, ...);
extern char *xenstore_dom_read(unsigned int domid, const char *format, ...);
extern _Bool xenstore_dom_chmod(unsigned int domid, const char *perms, int nbperm, const char *format, ...);
extern _Bool xenstore_watch_dir(xenstore_watch_cb_t cb, void *opaque, const char *format, ...);
extern _Bool xenstore_watch(xenstore_watch_cb_t cb, void *opaque, const char *format, ...);
extern _Bool xenstore_dom_watch(unsigned int domid, xenstore_watch_cb_t cb, void *opaque, const char *format, ...);
extern char *xenstore_get_domain_path(unsigned int domid);
extern int xenstore_init(void);
extern void xenstore_release(void);
/* vgpu.c */
extern void vgpu_update_plugin_vmonitors(struct plugin *p);
extern void vgpu_update_vmonitors(struct vgpu *vgpu);
extern int vgpu_display_allow_commit(struct plugin *p, surfman_display_t *disp, int len);
extern void vgpu_blanker_init(struct vgpu *vgpu);
extern void vgpu_poll_notifications(void);
extern surfman_psurface_t vgpu_get_psurface(struct device *dev, surfman_vmonitor_t vmon);
extern void vgpu_detach(struct vgpu *vgpu);
extern struct device *vgpu_device_create(struct domain *d, struct dmbus_rpc_ops **ops);
/* plugin.c */
extern struct plugin *load_plugin(char *path);
extern void plugin_init(const char *plugin_path, int safe_graphics);
extern void plugin_cleanup(void);
extern struct plugin *plugin_lookup(char *name);
extern void plugin_scan_monitors(struct plugin *plugin);
extern int plugin_handle_notification(struct plugin *plugin);
extern void plugin_pre_s3(void);
extern void plugin_post_s3(void);
extern void plugin_increase_brightness(void);
extern void plugin_decrease_brightness(void);
extern void plugin_restore_brightness(void);
extern unsigned int plugin_stride_align(void);
extern int plugin_need_refresh(struct plugin *p);
extern struct plugin *plugin_find_vgpu(surfman_vgpu_info_t *info, surfman_vgpu_t **pvgpu);
extern int plugin_get_vgpu_modes(struct vgpu_mode *modes, int len);
extern int plugin_display_commit(int force);
/* resolution.c */
extern void resolution_domain_on_monitor(unsigned int domid, struct plugin *plugin, surfman_monitor_t monitor);
extern void resolution_refresh_current(struct plugin *plugin);
extern void resolution_init(void);
/* xenfb.c */
extern struct device *xenfb_device_create(struct domain *d);
extern void xenfb_backend_init(int dom0);
/* ioemugfx.c */
extern struct device *ioemugfx_device_create(struct domain *d, struct dmbus_rpc_ops **ops);
/* snapshot.c */
extern int surface_snapshot(surfman_surface_t *surf, const char *filename);
/* display.c */
extern struct monitor display[16];
extern struct monitor *display_get_monitor(int display_id);
extern void display_init(void);
extern struct monitor_info *get_monitor_info(struct plugin *plugin, surfman_monitor_t m);
extern int get_monitor_slot(surfman_monitor_t m);
extern void display_update_monitors(struct plugin *p, surfman_monitor_t *monitors, int count);
extern int display_prepare_blank(int monitor_id, struct device *dev);
extern int display_prepare_surface(int monitor_id, struct device *dev, struct surface *s, struct effect *e);
extern int display_prepare_vmonitor(int monitor_id, struct device *dev, surfman_vmonitor_t vmon, struct effect *e);
extern int display_commit(struct plugin *p, int force);
extern void display_vmonitor_takedown(surfman_vmonitor_t vmon);
extern void display_surface_takedown(struct surface *s);
extern void display_plugin_takedown(struct plugin *p);
extern int display_get_edid(int monitor_id, uint8_t *buff, size_t sz);
/* rpc.c */
extern int rpc_init(void);
/* xengfx.c */
extern struct device *xengfx_device_create(struct domain *d, struct dmbus_rpc_ops **ops);
/* splashscreen.c */
extern struct splashfont_8x8 simplefont;
extern int get_splash_dims(uint32_t *linesize, uint32_t *screenwidth, uint32_t *screenheight, uint32_t *screensize, unsigned int *Bpp, unsigned int *format);
extern int mmap_splash(uint8_t **fb, uint32_t *linesize, uint32_t *screenwidth, uint32_t *screenheight, unsigned int *Bpp);
extern void register_spinner(void);
extern void unregister_spinner(void);
extern int splash_init(void);
extern int splash_text(const char *text);
extern int splash_picture(const char *fname);
/* fbtap.c */
extern void fbtap_takedown(struct device *surf_dev);
extern int fbtap_device_fd(struct device *surf_dev);
extern struct device *fbtap_device_create(struct domain *d, int monitor_id, struct fbdim *dims);
