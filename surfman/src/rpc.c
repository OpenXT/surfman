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

extern const struct dmbus_rpc_ops ioemugfx_rpc_ops;

static struct event rpc_connect_event;

static void
rpc_handler (int fd, short event, void *priv)
{
  struct device *dev = priv;

  dmbus_handle_events(dev->client);
}

static int
rpc_connect (dmbus_client_t client,
             int domain,
             DeviceType type,
             int dm_domain,
             int fd,
             struct dmbus_rpc_ops **ops,
             void **priv)
{
  struct domain *d;
  struct device *dev;

  surfman_info ("DM connected. domid %d device type %d", domain, type);

  d = domain_by_domid (domain);
  if (!d)
    d = domain_create (domain);

  switch (type)
    {
    case DEVICE_TYPE_XENFB:
      dev = xenfb_device_create (d);
      *ops = NULL;
      break;
    case DEVICE_TYPE_VESA:
      dev = ioemugfx_device_create (d, ops);
      break;
    case DEVICE_TYPE_XENGFX:
      dev = xengfx_device_create (d, ops);
      break;
    case DEVICE_TYPE_EMULATION:
    case DEVICE_TYPE_PASSTHROUGH:
      dev = vgpu_device_create (d, ops);
      break;
    default:
      dev = NULL;
    }

  if (!dev)
    return -1;

  dev->device_type = type;

  event_set (&dev->ev, fd, EV_READ | EV_PERSIST, rpc_handler, dev);
  event_add (&dev->ev, NULL);
  dev->client = client;
  dev->dm_domain = dm_domain;
  *priv = dev;

  return 0;
}

static void
rpc_disconnect (dmbus_client_t client, void *priv)
{
  struct device *dev = priv;
  struct domain *d;

  surfman_info ("DM disconnected. domid %d", dev->d->domid);
  for (;;) {
    unsigned int num;
    char **x = xenstore_ls(&num, "/local/domain/%d", dev->d->domid);
    if (!x) {
      surfman_info ("Domain gone %d, proceeding", dev->d->domid);
      break;
    }
    free(x);
    usleep(1000 * 100);
  }
  surfman_info ("DM disconnected - detected domain gone, proceeding");

  event_del (&dev->ev);

  if (dev->ops && dev->ops->takedown)
    dev->ops->takedown (dev);

  d = dev->d;
  device_destroy (dev);

  if (LIST_EMPTY (&d->devices))
    domain_destroy (d);
}

static struct dmbus_service_ops service_ops = {
  .connect = rpc_connect,
  .disconnect = rpc_disconnect,
};

int
rpc_init (void)
{
  int fd;

  fd = dmbus_init (DMBUS_SERVICE_SURFMAN, &service_ops);
  if (fd == -1)
    {
      fatal ("Failed to initialize dmbus");
      return fd;
    }

  event_set (&rpc_connect_event, fd, EV_READ | EV_PERSIST,
             (void *) dmbus_handle_connect, NULL);
  event_add (&rpc_connect_event, NULL);

  return 0;
}

