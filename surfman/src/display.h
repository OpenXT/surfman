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
#ifndef DISPLAY_H_
#define DISPLAY_H_

#define DISPLAY_MONITOR_MAX 16

#define MONITOR_MODES_MAX 16

struct effect
{
  uint8_t opacity;

  unsigned int    psurface_x;
  unsigned int    psurface_y;
  unsigned int    psurface_height;
  unsigned int    psurface_width;
  unsigned int    monitor_x;
  unsigned int    monitor_y;
  unsigned int    monitor_height;
  unsigned int    monitor_width;
};

enum
{
  DISPLAY_TYPE_SURFACE,
  DISPLAY_TYPE_VMONITOR,
  DISPLAY_TYPE_BLANK,
};

typedef void (*onscreen_cb)(struct device *dev,
                            struct surface *s,
                            int onscreen,
                            void *priv);

struct display
{
  LIST_ENTRY (struct display) link;

  int display_type;

  struct device *dev;
  union
  {
    struct surface *surface;
    surfman_vmonitor_t vmonitor;
  } u;

  struct effect effect;
};

struct monitor_info
{
  surfman_monitor_info_t i;
  surfman_monitor_mode_t modes[MONITOR_MODES_MAX];
  surfman_monitor_edid_t edid;
} __attribute__ ((packed));

struct monitor
{
  /*
   * connectorid and plugin must stay assigned during the whole
   * life of a plugin.
   */
  int connectorid;       /* -1 if slot not yet assigned */
  struct plugin *plugin;

  surfman_monitor_t mon; /* NULL if the monitor has been removed */
  struct monitor_info *info;

  LIST_HEAD (, struct display) current;
  LIST_HEAD (, struct display) next;
};

#endif /* DISPLAY_H_ */
