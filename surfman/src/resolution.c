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

static const unsigned int width_for_bad_edid = 1024;
static const unsigned int height_for_bad_edid = 768;

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
        surfman_warning ("%s: get_monitor_info method failed with error %d.",
                         plugin->name, ret);
        return 0;
    }

    if (info->mode_count <= 0)
        return 0;

    internal_w = info->current_mode->htimings[SURFMAN_TIMING_ACTIVE];
    internal_h = info->current_mode->vtimings[SURFMAN_TIMING_ACTIVE];

    /* If a component (internal_w or internal_h) is 0, fail.
     * If a component is higher than 3000 and the other one can do 1920x1200, return 1920x1200.
     * If both components are between 1 and 2999, return them as-is. */
    if (!internal_w || !internal_h) {
        surfman_warning("%s: get_monitor_info returned an invalid resolution: %d x %d",
                        plugin->name, internal_w, internal_h);
        return 0;
    }
    if (internal_w > 3000 || internal_h > 3000) {
        surfman_warning("%s: get_monitor_info returned a resolution too big (%d x %d), defaulting to 1920x1200",
                        plugin->name, internal_w, internal_h);
        *w = 1920;
        *h = 1200;
        return 1;
    } else {
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

void 
resolution_domain_on_monitor(unsigned int domid, struct plugin *plugin, surfman_monitor_t monitor)
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

/**
 * Finds the largest resolution supported by all monitors. 
 * 
 * NOTE: It might be more ideal to use the supported mode information for each monitor, rather
 *  than just using the montior's native resolutions. Unfortunately, the plugin architecutre
 *  currently only gives us the maximum resolution, so we'll deal with that.
 */
void 
__find_greatest_common_resolution(struct plugin * plugin, unsigned int * width_out, unsigned int * height_out)
{
    int i, rc;

    //To start off, assume the largest possible width and height.
    //These will quickly be overridden by the monitors we pass through.
    unsigned int least_width = -1, least_height = -1;

    for(i = 0; i < plugin->monitor_count; ++ i) {

        unsigned int monitor_width, monitor_height;
        rc = get_resolution_from_monitor(plugin, plugin->monitors[i], &monitor_width, &monitor_height);

        //If we've failed to get a resolution, fall back to the default
        //used for monitors without EDIDs. TODO: possibly skip bad EDIDs instead?
        if(!rc) {
            monitor_width = width_for_bad_edid;
            monitor_height = height_for_bad_edid;
        }
        
        //Naive initial method: use the lesser of the width and height.
        //This can create some very odd modes, but it should work with 
        //the method of mode creation we use in the plugins.
        if(monitor_width < least_width) {
            least_width = monitor_width;
        }

        if(monitor_height < least_height) {
            least_height = monitor_height;
        }
    }

    //Finally, output the width and height.
    *width_out = least_width;
    *height_out = least_height;
}


void
resolution_refresh_current(struct plugin *plugin)
{
    unsigned int w, h;
    char buf[256];

    if (plugin->monitor_count > 0)
    {
        //Find a resolution that should work for all monitors...
        __find_greatest_common_resolution(plugin, &w, &h); 

        //... and present a display of that size to the domain.
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
