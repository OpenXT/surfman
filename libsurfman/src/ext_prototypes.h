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
/* mapcache.c */
extern mapcache_t mapcache_create(int domid);
extern void mapcache_destroy(mapcache_t mapcache);
extern int mapcache_invalidate_all(mapcache_t mapcache);
extern int mapcache_invalidate_entry(mapcache_t mapcache, uint64_t addr);
extern void *mapcache_get_mapping(mapcache_t mapcache, uint64_t addr);
extern void mapcache_dump_stats(mapcache_t mapcache);
extern int copy_to_domain(mapcache_t mapcache, uint64_t addr, void *p, size_t sz);
extern int copy_from_domain(mapcache_t mapcache, void *p, uint64_t addr, size_t sz);
/* io.c */
extern int iohandle_get_fd(iohandle_t iohdl);
extern int io_handle(iohandle_t iohdl);
extern void iohandle_iorange_setpriv(iohandle_t iohdl, uint64_t addr, void *priv);
extern void iohandle_remove_iorange(iohandle_t iohdl, uint64_t addr, int mmio);
extern void iohandle_add_iorange(iohandle_t iohdl, uint64_t addr, uint64_t size, int mmio, io_ops_t *ops, void *priv);
extern iohandle_t iohandle_create(int domid);
extern void iohandle_destroy(iohandle_t iohdl);
/* util.c */
extern void message(int flags, const char *file, const char *function, int line, const char *fmt, ...);
extern void *xcalloc(size_t n, size_t s);
extern void *xmalloc(size_t s);
extern void *xrealloc(void *p, size_t s);
extern char *xstrdup(const char *s);
/* backtrace.c */
extern void dump_backtrace(void);
/* xc.c */
extern xc_interface *xch;
extern int privcmd_fd;
extern int xc_has_vtd(void);
extern void xc_init(void);
extern int xc_domid_exists(int domid);
extern void *xc_mmap_foreign(void *addr, size_t length, int prot, int domid, xen_pfn_t *pages);
/* rect.c */
extern int rect_from_dirty_bitmap(uint8_t *dirty, unsigned int width, unsigned int height, unsigned int stride, enum surfman_surface_format format, surfman_rect_t *rect);
/* configfile.c */
extern const char *config_get(const char *prefix, const char *key);
extern const char *config_dump(void);
extern int config_load_file(const char *filename);
/* surface.c */
extern void *surface_map(surfman_surface_t *surface);
extern xen_pfn_t surface_get_base_gfn(surfman_surface_t * surface);
extern void surface_unmap(surfman_surface_t *surface);
