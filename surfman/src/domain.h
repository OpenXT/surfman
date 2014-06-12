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
#ifndef DOMAIN_H_
#define DOMAIN_H_

#include "list.h"

struct device;

struct device_ops
{
  const char *name;

  void            (*refresh_surface) (struct device *dev, struct surface *s);
  void            (*monitor_update)  (struct device *dev, int monitor_id, int enable);
  int             (*set_visible)     (struct device *dev);
  void            (*takedown)        (struct device *dev);
  struct surface *(*get_surface)     (struct device *dev, int monitor_id);
  int             (*is_active)       (struct device *dev);
};

struct device
{
  LIST_ENTRY (struct device) link;

  struct domain *d;

  struct device_ops *ops;
  int device_type;

  struct event ev;
  dmbus_client_t client;
  int dm_domain;
};

struct domain
{
  LIST_ENTRY (struct domain) link;

  int domid;
  mapcache_t mapcache;

  LIST_HEAD (, struct device) devices;
};

#endif /* DOMAIN_H_ */
