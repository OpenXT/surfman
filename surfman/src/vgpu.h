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
#ifndef VGPU_H_
#define VGPU_H_

#define VGPU_VMONITOR_MAX 16

struct vgpu
{
  struct device device;

  LIST_ENTRY (struct vgpu) link;

  int domid;

  surfman_vgpu_t *interface;
  struct plugin *plugin;
  iohandle_t iohdl;

  uint8_t bus;
  uint8_t dev;
  uint8_t func;

  surfman_update_bar_t bars[6];

  surfman_vmonitor_t vmonitors[VGPU_VMONITOR_MAX];
  struct monitor_info *minfos[VGPU_VMONITOR_MAX];

  int vmonitor_count;

  /* VGPU type */
  int type;

  /* Last time the blanker answered */
  time_t blanker_alive;
  /* Blanker ping event */
  struct event ev_blanker;

  /* Do we need to reset brightness */
  bool need_brightness_reset;
};

#define VGPU_CALL(v,method,...) \
  ((v)->interface->method((v)->interface, ##__VA_ARGS__))

#endif /* VGPU_H_ */
