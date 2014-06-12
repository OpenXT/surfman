/*
 * Copyright (c) 2011 Citrix Systems, Inc.
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

static int
websocket_init (surfman_plugin_t * p)
{
    return SURFMAN_SUCCESS;
}

static void
websocket_shutdown (surfman_plugin_t * p)
{

}

static int
websocket_display (surfman_plugin_t * p,
                surfman_display_t * config,
                size_t size)
{
    return SURFMAN_SUCCESS;
}

static int
websocket_get_monitors (surfman_plugin_t * p,
                     surfman_monitor_t * monitors,
                     size_t size)
{
    return 0;
}

static int
websocket_set_monitor_modes (surfman_plugin_t * p,
                          surfman_monitor_t monitor,
                          surfman_monitor_mode_t * mode)
{
    return SURFMAN_SUCCESS;
}

static int
websocket_get_monitor_info (surfman_plugin_t * p,
                         surfman_monitor_t monitor,
                         surfman_monitor_info_t * info,
                         unsigned int modes_count)
{
    return SURFMAN_SUCCESS;
}

static int
websocket_get_monitor_edid (surfman_plugin_t * p,
                         surfman_monitor_t monitor,
                         surfman_monitor_edid_t * edid)
{
    return SURFMAN_SUCCESS;
}

static surfman_psurface_t
websocket_get_psurface_from_surface (surfman_plugin_t * p,
                                  surfman_surface_t * surface)
{
    return NULL;
}

static void
websocket_refresh_psurface(surfman_plugin_t *p,
                        surfman_psurface_t psurface,
                        uint8_t *refresh_bitmap)
{

}

static void
websocket_update_psurface (surfman_plugin_t *plugin,
                        surfman_psurface_t psurface,
                        surfman_surface_t *surface,
                        unsigned int flags)
{

}

static int
websocket_get_pages_from_psurface (surfman_plugin_t * p,
                                surfman_psurface_t psurface,
                                uint64_t * pages)
{
    return SURFMAN_ERROR;
}

static void
websocket_free_psurface_pages (struct surfman_plugin *plugin,
                            surfman_psurface_t psurface)
{

}


static int
websocket_copy_surface_on_psurface (surfman_plugin_t * p,
                                 surfman_psurface_t psurface)
{
    return SURFMAN_ERROR;
}

static int
websocket_copy_psurface_on_surface (surfman_plugin_t * p,
                                 surfman_psurface_t psurface)
{
    return SURFMAN_ERROR;
}

static void
websocket_free_psurface (surfman_plugin_t * plugin,
                      surfman_psurface_t psurface)
{

}

static void
websocket_pre_s3 (surfman_plugin_t * p)
{

}

static void
websocket_post_s3 (surfman_plugin_t * p)
{

}

static void
websocket_increase_brightness (surfman_plugin_t * p)
{

}

static void
websocket_decrease_brightness (surfman_plugin_t * p)
{

}

surfman_vgpu_t *
websocket_new_vgpu (surfman_plugin_t * p,
                 surfman_vgpu_info_t * info)
{
    return NULL;
}

void
websocket_free_vgpu (surfman_plugin_t * p,
                  surfman_vgpu_t * vgpu)
{

}

int
websocket_get_vgpu_mode (surfman_plugin_t * p,
                      surfman_vgpu_mode_t *mode)
{
    return SURFMAN_ERROR;
}


surfman_plugin_t surfman_plugin = {
  .init = websocket_init,
  .shutdown = websocket_shutdown,
  .display = websocket_display,
  .new_vgpu = websocket_new_vgpu,
  .free_vgpu = websocket_free_vgpu,
  .get_monitors = websocket_get_monitors,
  .set_monitor_modes = websocket_set_monitor_modes,
  .get_monitor_info = websocket_get_monitor_info,
  .get_monitor_edid = websocket_get_monitor_edid,
  .get_psurface_from_surface = websocket_get_psurface_from_surface,
  .update_psurface = websocket_update_psurface,
  .refresh_psurface = websocket_refresh_psurface,
  .get_pages_from_psurface = websocket_get_pages_from_psurface,
  .free_psurface_pages = websocket_free_psurface_pages,
  .copy_surface_on_psurface = websocket_copy_surface_on_psurface,
  .copy_psurface_on_surface = websocket_copy_psurface_on_surface,
  .free_psurface = websocket_free_psurface,
  .pre_s3 = websocket_pre_s3,
  .post_s3 = websocket_post_s3,
  .increase_brightness = websocket_increase_brightness,
  .decrease_brightness = websocket_decrease_brightness,
  .get_vgpu_mode = websocket_get_vgpu_mode,
  .options = { 1, SURFMAN_FEATURE_NONE },
  .notify = SURFMAN_NOTIFY_NONE
};
