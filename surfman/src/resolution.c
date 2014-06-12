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
#include "project.h"

static const char *current_display_size_path = "/xc_tools/switcher/current_display_size";
static const char *display_size_path = "switcher/display_size";

static void
set_current_display_size(unsigned int x, unsigned int y)
{
    char buf[256];

    snprintf(buf, 256, "%d %d", x, y);
    /* surfman_info ("setting %s to %s\n", current_display_size_path, buf); */
    xenstore_write (buf, current_display_size_path, buf);
}

/* XXX: This will have to go */
#define plugin_monitor_info_t(_name_, _x_)                                              \
union                                                                                   \
{                                                                                       \
    surfman_monitor_info_t  __surfman_monitor_info;                                     \
    char __surfman_monitor_buff[sizeof (surfman_monitor_info_t) +                       \
                                sizeof (surfman_monitor_mode_t) * (_x_)];               \
} __ ## _name_;                                                                         \
surfman_monitor_info_t * _name_  = (surfman_monitor_info_t*)&(__ ## _name_)

static int
get_resolution_from_monitor(struct plugin *plugin, surfman_monitor_t monitor,
                            unsigned int *w, unsigned *h)
{
    plugin_monitor_info_t(info, 20);
    unsigned internal_w, internal_h;
    int ret;

    info->mode_count = 0;
    ret = PLUGIN_CALL (plugin, get_monitor_info, monitor, info, 20);
    if (ret)
    {
        surfman_warning ("%s: get_monitor_info method failed.", plugin->name);
        return ret;
    }

    if (info->mode_count <= 0)
        return 0;

    internal_w = info->current_mode->htimings[SURFMAN_TIMING_ACTIVE];
    internal_h = info->current_mode->vtimings[SURFMAN_TIMING_ACTIVE];

    if (internal_w && internal_w < 3000 && internal_h && internal_w < 3000)
    {
        *w = internal_w;
        *h = internal_h;
        return 1;
    }
    return 0;
}

static void resolution_xs_read(unsigned int domid, unsigned int *w, unsigned int *h)
{
    char *tmp = NULL;
    unsigned int internal_w, internal_h;

    tmp = xenstore_dom_read (domid, "%s", display_size_path);
    if (tmp)
    {
        if (sscanf(tmp, "%d %d", &internal_w, &internal_h) == 2)
        {
            *w = internal_w;
            *h = internal_h;
        }
        free(tmp);
    }
}

void resolution_domain_on_monitor(unsigned int domid, struct plugin *plugin, surfman_monitor_t monitor)
{
    unsigned int w = 0, h = 0;
    unsigned int xs_w = 0, xs_h = 0;
    char buf[256];

    if (!get_resolution_from_monitor(plugin, monitor, &w, &h))
        return;
    resolution_xs_read(domid, &xs_w, &xs_h);
    if (xs_w != w || xs_h != h)
    {
        snprintf(buf, 256, "%d %d", w, h);
        surfman_info ("setting /local/domain/%d/%s to %s\n", domid, display_size_path, buf);
        xenstore_dom_write (domid, buf, display_size_path);
        set_current_display_size(w, h);
    }
}

void
resolution_refresh_current(struct plugin *plugin)
{
    unsigned int w, h;
    char buf[256];

    if (plugin->monitor_count > 0)
    {
        if (!get_resolution_from_monitor(plugin, plugin->monitors[0], &w, &h))
            return;
        set_current_display_size(w, h);
    }
}

void
resolution_init(void)
{
    struct timeval tv = {1, 0};

    set_current_display_size(0, 0);
    xenstore_chmod ("r0", 1, current_display_size_path);
}
