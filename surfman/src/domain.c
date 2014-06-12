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
#include <limits.h>
#include "prototypes.h"

static LIST_HEAD (, struct domain)
  domain_list = LIST_HEAD_INITIALIZER;

static struct domain *visible_domain = NULL;

int
domain_exists (struct domain *d)
{
  return xc_domid_exists (d->domid);
}

int
domain_dying (struct domain *d)
{
  xc_dominfo_t info;

  if (xc_domain_getinfo (xch, d->domid, 1, &info) == 1)
    {
      return info.dying;
    }
  return 0;
}

struct domain *
domain_by_domid (int domid)
{
  struct domain *d;

  LIST_FOREACH (d, &domain_list, link)
  {
    if (d->domid == domid)
      return d;
  }

  return NULL;
}

void
domain_destroy (struct domain *d)
{
  struct device *dev, *tmp;

  /* Avoid recursive misery */
  if (LIST_EMPTY (&d->devices))
    {
      if (d == visible_domain)
        {
          visible_domain = NULL;
        }

      LIST_REMOVE (d, link);
      memset (d, 0, sizeof (*d));
      free (d);
    }
  else
    {
      LIST_FOREACH_SAFE (dev, tmp, &d->devices, link)
        {
          /*
           * Will call domain_destroy() after the last device
           * is destroyed.
           */
          if (dev->client)
            dmbus_client_disconnect (dev->client);
        }
    }
}

struct domain *
domain_create (int domid)
{
  struct domain *ret;

  if (domain_by_domid (domid))
    {
      surfman_error ("Domain %d already exists", domid);
      return NULL;
    }

  ret = xcalloc (1, sizeof (*ret));

  ret->domid = domid;
  LIST_INSERT_HEAD (&domain_list, ret, link);
#if 0
  plugin_set_guest_resolution (domid);
#endif
  LIST_INIT (&ret->devices);

  return ret;
}

int
domain_has_vgpu (struct domain *d)
{
  struct device *dev;

  LIST_FOREACH (dev, &d->devices, link)
    {
      if (dev->device_type == DEVICE_TYPE_EMULATION ||
          dev->device_type == DEVICE_TYPE_PASSTHROUGH)
          return 1;
    }

  return 0;
}

static struct device *
domain_active_device (struct domain *d)
{
  struct device *dev;
  struct device *active_dev = NULL;

  /* Look for legacy VGA dev first */
  LIST_FOREACH (dev, &d->devices, link)
    {
      if (!strcmp(dev->ops->name, "ioemugfx"))
        {
          if (dev->ops->is_active (dev))
            {
              surfman_info ("Chose active device: %s", dev->ops->name);
              return dev;
            }
          break;
        }
    }

  /* Then check other devices */
  LIST_FOREACH (dev, &d->devices, link)
    {
      if (strcmp(dev->ops->name, "ioemugfx"))
        {
          if (dev->ops->is_active (dev))
            {
              active_dev = dev;
              break;
            }
        }
    }

  if (!active_dev)
    surfman_error ("Could not find any active device to display");
  else
    surfman_info ("Chose active device: %s", active_dev->ops->name);

  return active_dev;
}

int
domain_visible (struct domain *d)
{
    return (d == visible_domain);
}

int
domain_set_visible (struct domain *d, int force)
{
  int rc = -1;

  /*
   * If the domain currently displayed is gone or
   * stuck in the dying state. Destroy it now, so
   * the switching operation can force the plugin
   * to reinitiliaze the hardware correctly now
   * instead of doing it later.
   *
   * In the case of IGFX passthrough domains, this
   * allows the igfx plugin to call init_raster()
   * *after* the GPU has received an FLR.
   */
  if (visible_domain && d != visible_domain)
    {
      if (!domain_exists (visible_domain) ||
          domain_dying(visible_domain))
        {
          domain_destroy (visible_domain);
        }
    }

  if (!d)
    d = visible_domain;

  if (d)
    {
      /* XXX */
      struct device *dev = domain_active_device (d);

      if (dev)
        rc = dev->ops->set_visible (dev);
    }

  if (rc)
    {
      surfman_error ("failed");
      return rc;
    }

  rc = plugin_display_commit (force);
  if (rc)
    {
      surfman_error ("Failed to commit display");
      return rc;
    }

  visible_domain = d;
  if (visible_domain)
    dbus_notify_visible_domain_changed(visible_domain->domid);
  return 0;
}

int
domain_get_visible (void)
{
  if (!visible_domain)
    return -1;

  return visible_domain->domid;
}

void
domain_monitor_update (int monitor_id, int enable)
{
  struct domain *d;

  LIST_FOREACH (d, &domain_list, link)
    {
      struct device *dev;

      LIST_FOREACH (dev, &d->devices, link)
        {
          if (dev->ops && dev->ops->monitor_update)
            dev->ops->monitor_update (dev, monitor_id, enable);
        }
    }
}

void *
device_create (struct domain *d, struct device_ops *ops, size_t size)
{
  unsigned int i;
  struct device *ret;

  ret = xcalloc (1, size);

  ret->d = d;

  LIST_INSERT_HEAD (&d->devices, ret, link);
  ret->ops = ops;

  /* XXX: Huge hack...
   *      Until now cloning monitor was done as a hack in IGFX.
   *      In the current state of things, monitor_update need to be called to notify a backend
   *      that a plugin detected a new monitor. It would be fine if backends were initialized
   *      at surfman's start, but they can also be created dynamically.
   *      So we monitor_update() backends at their creation (even if there is no monitor).
   *      This is all quite dodgy and not well thought... */
  for (i = 0; i < (sizeof (display) / sizeof (*display)); ++i)
    if (display[i].mon)
      if (ret->ops && ret->ops->monitor_update)
        ret->ops->monitor_update(ret, i, 1);

  return ret;
}

void
device_destroy (struct device *device)
{
  LIST_REMOVE (device, link);
  memset (device, 0, sizeof (*device));
  free (device);
}

int
dump_all_screens (const char *directory)
{
  int ret = 0;
  int dev_num, surf_num;
  char filename[PATH_MAX];
  struct domain *d;
  struct device *dev;
  struct surface *s;

  LIST_FOREACH (d, &domain_list, link)
    {
      if (SPLASH_DOM_ID == d->domid)
        continue;

      dev_num = 0;

      LIST_FOREACH (dev, &(d->devices), link)
        {
          surfman_info ("dumping screen for domain id: %d, device num: %d...", 
                d->domid, dev_num);

          for (surf_num = 0;
               dev->ops 
               && dev->ops->get_surface 
               && (s = dev->ops->get_surface(dev, surf_num))
               && (surf_num == 0); /* surface probing seems broken */
               surf_num++)
            {
              if (0 >= snprintf (filename, PATH_MAX, 
                                 "%s/dom%d-dev%d-surface%d.png", 
                                 directory, d->domid, dev_num++, surf_num))
                continue;

              ret |= surface_snapshot (s->surface, filename);
            }
        }
    }

  return ret;
}
