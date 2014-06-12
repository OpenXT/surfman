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
#ifndef PLUGIN_H_
#define PLUGIN_H_

#define PLUGIN_NAME_MAX 64
#define PLUGIN_INTERFACE_SYMBOL "surfman_plugin"
#define PLUGIN_VERSION_SYMBOL "surfman_plugin_version"

#define PLUGIN_MONITOR_MAX 16

/* Default plugin version if we can't find version in the plugin library */
# define PLUGIN_DEFAULT_VERSION SURFMAN_VERSION(2, 0, 0)

struct plugin
{
  LIST_ENTRY (struct plugin) link;

  char name[PLUGIN_NAME_MAX];
  void *handle;
  surfman_plugin_t *interface;
  surfman_version_t version;

  int monitor_count;
  surfman_monitor_t monitors[PLUGIN_MONITOR_MAX];
};

struct vgpu_mode
{
  struct plugin *p;
  surfman_vgpu_mode_t m;
};


#define PLUGIN_CALL(p,method,...) \
    ( (p)->interface->method((p)->interface, ##__VA_ARGS__) )
#define PLUGIN_CALL_CAST(p,cast,method,args...) \
    ((cast) (p)->interface->method) ((p)->interface, args) 
#define PLUGIN_HAS_METHOD(p,method) \
    ( (p)->interface->method )
#define PLUGIN_GET_OPTION(p,opt) \
    ( (p)->interface->options.opt )
/* Check if the plugin version is greater or equal */
#define PLUGIN_CHECK_VERSION(p, major, minor, micro) \
    ( (p)->version >= SURFMAN_VERSION((major), (minor), (micro)) )

#define PLUGIN_FORCE_UPDATE 1
#define PLUGIN_LAZY_UPDATE 0

#endif /* PLUGIN_H_ */
