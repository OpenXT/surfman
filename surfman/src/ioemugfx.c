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
#include "project.h"

struct ioemugfx_device
{
  struct device device;

  struct surface *s;
  uint64_t lfb_addr;
  size_t lfb_len;
  uint8_t *dirty_buffer;

  /* For now, only the availability of each monitor this backend is aware of.
   * Index is a /monitor_id/ in display[] (so, the index). */
  int monitors[DISPLAY_MONITOR_MAX];
};

#define DIV_ROUND_UP(x, y) (((x) + (y) - 1) / (y))

static int
ioemugfx_display_resize(void *priv,
                        struct msg_display_resize *msg,
                        size_t msglen,
                        struct msg_empty_reply *out)
{
  struct ioemugfx_device *dev = priv;
  int format;

  if (!msg->width || !msg->height || !msg->linesize)
    {
      /* Disable the head */
      if (dev->s)
        {
          surface_destroy (dev->s);
          dev->s = NULL;
        }
      if (dev->dirty_buffer)
        free (dev->dirty_buffer);

      dev->dirty_buffer = NULL;
      dev->lfb_addr = 0;
      dev->lfb_len = 0;

      return 0;
    }

  if (!dev->s)
    dev->s = surface_create (&dev->device, dev);

  dev->lfb_len = msg->height * msg->linesize;
  dev->lfb_addr = msg->lfb_addr;

  if (msg->lfb_traceable)
    dev->dirty_buffer = realloc (dev->dirty_buffer,
                                 DIV_ROUND_UP(DIV_ROUND_UP(dev->lfb_len, XC_PAGE_SIZE), 8));
  else
    {
      if (dev->dirty_buffer)
        free(dev->dirty_buffer);
      dev->dirty_buffer = NULL;
    }

  if (msg->format == FRAMEBUFFER_FORMAT_BGRX8888)
    format = SURFMAN_FORMAT_BGRX8888;
  else if (msg->format == FRAMEBUFFER_FORMAT_BGR565)
    format = SURFMAN_FORMAT_BGR565;
  else
    {
      surfman_error ("Unsupported framebuffer format %d", msg->format);
      return -1;
    }

  surface_update_format (dev->s, msg->width, msg->height, msg->linesize,
                         format, msg->fb_offset);
  surface_update_lfb (dev->s, dev->device.d->domid, dev->lfb_addr,
                      dev->lfb_len);

  return 0;
}

static int
ioemugfx_display_get_info(void *priv,
                          struct msg_display_get_info *msg,
                          size_t msglen,
                          struct msg_display_info *out)
{
  struct ioemugfx_device *dev = priv;
  struct monitor *m;
  int align;

  surfman_info ("DisplayID:%d", msg->DisplayID);

  m = display_get_monitor (msg->DisplayID);
  if (!m) {
      surfman_info ("Monitor %d unavailable", msg->DisplayID);
      return -1;
  }

  out->DisplayID = msg->DisplayID;
  out->align = PLUGIN_GET_OPTION (m->plugin, stride_align);
  out->max_xres = m->info->i.prefered_mode->htimings[0];
  out->max_yres = m->info->i.prefered_mode->vtimings[0];

  surfman_info ("Monitor %d: Max resolution: %dx%d, stride alignment:%d",
        msg->DisplayID, out->max_xres, out->max_yres, out->align);

  return 0;
}

static struct dmbus_rpc_ops ioemugfx_rpc_ops = {
  .display_resize = ioemugfx_display_resize,
  .display_get_info = ioemugfx_display_get_info,
};

static struct surface *
ioemugfx_get_surface (struct device *device, int monitor_id)
{
  struct ioemugfx_device *dev = (struct ioemugfx_device *)device;

  if (monitor_id != 0)
    return NULL;

  return dev->s;
}

static void
ioemugfx_refresh_surface (struct device *device, struct surface *s)
{
  struct ioemugfx_device *dev = (struct ioemugfx_device *)device;
  size_t len = DIV_ROUND_UP(surface_length(s), XC_PAGE_SIZE);
  int rc;

  if (!dev->dirty_buffer)
    surface_refresh(s, NULL);
  else
    {
      rc = xc_hvm_track_dirty_vram (xch, device->d->domid,
                                    dev->lfb_addr >> XC_PAGE_SHIFT, len,
                                    (unsigned long *)dev->dirty_buffer);
      if (!rc)
        surface_refresh (s, dev->dirty_buffer);
      else if (errno == ENODATA)
        surface_refresh (s, NULL);
  }
}

static void
ioemugfx_monitor_update (struct device *device, int monitor_id, int enable)
{
  struct ioemugfx_device *dev = (struct ioemugfx_device *)device;

  /* XXX: Dumb to match the hack in domain.c:device_create. */
  dev->monitors[monitor_id] = enable;
}

static int
ioemugfx_set_visible (struct device *device)
{
  struct ioemugfx_device *dev = (struct ioemugfx_device *)device;
  unsigned int i;

  if (!dev->s)
    {
      surfman_error ("Surface not ready");
      return -1;
    }

  for (i = 0; i < DISPLAY_MONITOR_MAX; ++i)
    if (dev->monitors[i])
      if (display_prepare_surface (i, device, dev->s, NULL))
        surfman_warning ("Could not prepare surface %p on monitor %u.", dev->s, i);

  return 0;
}

static void
ioemugfx_takedown (struct device *device)
{
  struct ioemugfx_device *dev = (struct ioemugfx_device *)device;
  int i;

  if (dev->dirty_buffer)
    free (dev->dirty_buffer);
  if (dev->s)
    surface_destroy (dev->s);
}

static int
ioemugfx_is_active (struct device *device)
{
  struct ioemugfx_device *dev = (struct ioemugfx_device *)device;

  return 1;
}

static struct device_ops ioemugfx_device_ops = {
  .name = "ioemugfx",
  .refresh_surface = ioemugfx_refresh_surface,
  .monitor_update = ioemugfx_monitor_update,
  .set_visible = ioemugfx_set_visible,
  .takedown = ioemugfx_takedown,
  .get_surface = ioemugfx_get_surface,
  .is_active = ioemugfx_is_active,
};

struct device *
ioemugfx_device_create(struct domain *d, struct dmbus_rpc_ops **ops)
{
  struct ioemugfx_device *dev;

  surfman_info ("Creating IOEMU device %d", d->domid);

  dev = device_create (d, &ioemugfx_device_ops, sizeof (*dev));
  if (!dev)
    return NULL;

  *ops = &ioemugfx_rpc_ops;

  return &dev->device;
}

