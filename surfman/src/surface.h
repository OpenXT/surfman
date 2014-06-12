/*
 * Copyright (c) 2012 Citrix Systems, Inc.
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
#ifndef SURFACE_H_
#define SURFACE_H_

#include "list.h"

struct plugin;
struct backend;
struct surface;

struct psurface
{
  LIST_ENTRY(struct psurface) link;

  surfman_psurface_t psurface;
  struct plugin *plugin;
};

typedef void (*display_handler_t)(struct plugin *p,
                                  struct surface *s,
                                  int monitor_id,
                                  void *priv);

struct display_handler
{
    LIST_ENTRY(struct display_handler) link;
    display_handler_t handler;
    void *priv;
};

LIST_HEAD(handler_list_head, struct display_handler);

struct surface
{
  struct device *dev;
  surfman_surface_t *surface;
  int flags;
  LIST_HEAD(, struct psurface) cache;
  void *priv;
  struct event refresh;

  int handlers_lock;
  struct handler_list_head onscreen_handlers;
  struct handler_list_head offscreen_handlers;
};

static inline size_t
surface_length (struct surface *s)
{
  return s->surface->stride *
         s->surface->height;
}

#endif /* SURFACE_H_ */
