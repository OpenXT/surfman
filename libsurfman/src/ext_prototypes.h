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
