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
sample_init (surfman_plugin_t * p)
{
    return SURFMAN_SUCCESS;
}

static void
sample_shutdown (surfman_plugin_t * p)
{

}

static int
sample_display (surfman_plugin_t * p,
                surfman_display_t * config,
                size_t size)
{
    return SURFMAN_SUCCESS;
}

static int
sample_get_monitors (surfman_plugin_t * p,
                     surfman_monitor_t * monitors,
                     size_t size)
{
    return 0;
}

static int
sample_set_monitor_modes (surfman_plugin_t * p,
                          surfman_monitor_t monitor,
                          surfman_monitor_mode_t * mode)
{
    return SURFMAN_SUCCESS;
}

static int
sample_get_monitor_info (surfman_plugin_t * p,
                         surfman_monitor_t monitor,
                         surfman_monitor_info_t * info,
                         unsigned int modes_count)
{
    return SURFMAN_SUCCESS;
}

static int
sample_get_monitor_edid (surfman_plugin_t * p,
                         surfman_monitor_t monitor,
                         surfman_monitor_edid_t * edid)
{
    return SURFMAN_SUCCESS;
}

static surfman_psurface_t
sample_get_psurface_from_surface (surfman_plugin_t * p,
                                  surfman_surface_t * surface)
{
    return NULL;
}

static void
sample_refresh_psurface(surfman_plugin_t *p,
                        surfman_psurface_t psurface,
                        uint8_t *refresh_bitmap)
{

}

static void
sample_update_psurface (surfman_plugin_t *plugin,
                        surfman_psurface_t psurface,
                        surfman_surface_t *surface,
                        unsigned int flags)
{

}

static int
sample_get_pages_from_psurface (surfman_plugin_t * p,
                                surfman_psurface_t psurface,
                                pfn_t * pages)
{
    return SURFMAN_ERROR;
}

static void
sample_free_psurface_pages (struct surfman_plugin *plugin,
                            surfman_psurface_t psurface)
{

}


static int
sample_copy_surface_on_psurface (surfman_plugin_t * p,
                                 surfman_psurface_t psurface)
{
    return SURFMAN_ERROR;
}

static int
sample_copy_psurface_on_surface (surfman_plugin_t * p,
                                 surfman_psurface_t psurface)
{
    return SURFMAN_ERROR;
}

static void
sample_free_psurface (surfman_plugin_t * plugin,
                      surfman_psurface_t psurface)
{

}

static void
sample_pre_s3 (surfman_plugin_t * p)
{

}

static void
sample_post_s3 (surfman_plugin_t * p)
{

}

static void
sample_increase_brightness (surfman_plugin_t * p)
{

}

static void
sample_decrease_brightness (surfman_plugin_t * p)
{

}

surfman_vgpu_t *
sample_new_vgpu (surfman_plugin_t * p,
                 surfman_vgpu_info_t * info)
{
    return NULL;
}

void
sample_free_vgpu (surfman_plugin_t * p,
                  surfman_vgpu_t * vgpu)
{

}

int
sample_get_vgpu_mode (surfman_plugin_t * p,
                      surfman_vgpu_mode_t *mode)
{
    return SURFMAN_ERROR;
}


surfman_plugin_t surfman_plugin = {
  .init = sample_init,
  .shutdown = sample_shutdown,
  .display = sample_display,
  .new_vgpu = sample_new_vgpu,
  .free_vgpu = sample_free_vgpu,
  .get_monitors = sample_get_monitors,
  .set_monitor_modes = sample_set_monitor_modes,
  .get_monitor_info = sample_get_monitor_info,
  .get_monitor_edid = sample_get_monitor_edid,
  .get_psurface_from_surface = sample_get_psurface_from_surface,
  .update_psurface = sample_update_psurface,
  .refresh_psurface = sample_refresh_psurface,
  .get_pages_from_psurface = sample_get_pages_from_psurface,
  .free_psurface_pages = sample_free_psurface_pages,
  .copy_surface_on_psurface = sample_copy_surface_on_psurface,
  .copy_psurface_on_surface = sample_copy_psurface_on_surface,
  .free_psurface = sample_free_psurface,
  .pre_s3 = sample_pre_s3,
  .post_s3 = sample_post_s3,
  .increase_brightness = sample_increase_brightness,
  .decrease_brightness = sample_decrease_brightness,
  .get_vgpu_mode = sample_get_vgpu_mode,
  .options = { 1, SURFMAN_FEATURE_NONE },
  .notify = SURFMAN_NOTIFY_NONE
};
