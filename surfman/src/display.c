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
#include "project.h"

struct monitor display[DISPLAY_MONITOR_MAX];

struct display_list
{
  surfman_display_t *disp;
  int max;
  int len;
};

struct monitor *
display_get_monitor(int display_id)
{
  if (display_id >= DISPLAY_MONITOR_MAX)
    return NULL;
  if (!display[display_id].plugin ||
      !display[display_id].mon)
    return NULL;

  return display + display_id;
}

static int
display_list_init (struct display_list *l)
{
  l->disp = calloc (16, sizeof (surfman_display_t));

  if (!l->disp)
    return -1;

  l->max = 16;
  l->len = 0;

  return 0;
}

static void
display_list_cleanup (struct display_list *l)
{
  free (l->disp);
  l->disp = NULL;
  l->max = 0;
  l->len = 0;
}

static int
display_list_append (struct display_list *l,
                     surfman_monitor_t m,
                     surfman_psurface_t ps,
                     struct effect *e)
{
  if (l->len == l->max)
    {
      l->disp = realloc (l->disp, (l->max + 16) * sizeof (surfman_display_t));
      if (!l->disp)
        return -1;
      l->max += 16;
    }

  l->disp[l->len].monitor = m;
  l->disp[l->len].psurface = ps;
  /* TODO: Compositing here */

  l->len++;

  return 0;
}

/*
 * XXX: James' horrible hack to force switching on igfx HDX VMs
 *
 * Consider:
 *  a) Adding the force parametter permanently to the plugin interface.
 *  b) Removing in-guest blanker switching restrictions.
 */
#define PLUGIN_CALL_CAST(p,cast,method,args...) \
    ((cast) (p)->interface->method) ((p)->interface, args)
typedef int (*surfman_display_extra_func_t)(struct surfman_plugin *,
                                            surfman_display_t *, size_t, int);


static int
display_list_commit (struct plugin *p, struct display_list *l, int force)
{
  int rc;

  rc = PLUGIN_CALL_CAST (p, surfman_display_extra_func_t, display,
                         l->disp, l->len, force);

  return rc;
}

static void
evict_display (struct plugin *p, struct display *d, int monitor_id)
{
  if (d->display_type == DISPLAY_TYPE_SURFACE)
    {
      struct surface *s = d->u.surface;
      struct display_handler *h;

      s->handlers_lock++;
      LIST_FOREACH (h, &s->offscreen_handlers, link)
        h->handler(p, s, monitor_id, h->priv);
      s->handlers_lock--;
    }

  LIST_REMOVE (d, link);
  free (d);
}

static void
prepare_display (struct plugin *p, struct display *d, int monitor_id)
{
  if (d->display_type == DISPLAY_TYPE_SURFACE)
    {
      struct surface *s = d->u.surface;
      struct display_handler *h;

      s->handlers_lock++;
      LIST_FOREACH (h, &s->onscreen_handlers, link)
        h->handler(p, s, monitor_id, h->priv);
      s->handlers_lock--;
    }
}

static surfman_psurface_t
get_psurface (struct plugin *p, struct display *d)
{
  if (d->display_type == DISPLAY_TYPE_SURFACE)
    {
      return surface_get_psurface (d->u.surface, p);
    }
  else if (d->display_type == DISPLAY_TYPE_VMONITOR)
    {
      return vgpu_get_psurface(d->dev, d->u.vmonitor);
    }

  /* DISPLAY_TYPE_BLANK */
  return NULL;
}

void
display_init (void)
{
  int i;

  memset (display, 0, sizeof (display));

  for (i = 0; i < DISPLAY_MONITOR_MAX; i++)
    {
      display[i].connectorid = -1;
      LIST_HEAD_INIT (&display[i].current);
      LIST_HEAD_INIT (&display[i].next);
    }
}

struct monitor_info *
get_monitor_info (struct plugin *plugin,
                  surfman_monitor_t m)
{
  struct monitor_info *info;
  int rc;

  info = calloc (1, sizeof (*info));
  if (!info)
    return NULL;

  rc = PLUGIN_CALL (plugin, get_monitor_info, m, &info->i,
                    MONITOR_MODES_MAX);
  if (rc)
    {
      surfman_error ("Plugin %s: get_monitor_info() failed", plugin->name);
      free (info);
      return NULL;
    }

  rc = PLUGIN_CALL (plugin, get_monitor_edid, m, &info->edid);
  if (rc)
    {
      surfman_warning ("Plugin %s: get_monitor_edid() failed", plugin->name);
    }

  /*
  ** Check if EDID is valid, if not forge a fake one using the monitor's
  ** prefered resolution.
  */
  if (rc || !edid_valid(info->edid.edid, 1))
    {
      int hres = info->i.prefered_mode->htimings[SURFMAN_TIMING_ACTIVE];
      int vres = info->i.prefered_mode->vtimings[SURFMAN_TIMING_ACTIVE];

      surfman_info ("Generating EDID for %dx%d prefered resolution", hres, vres);

      edid_init_common(info->edid.edid, hres, vres);
    }

  return info;
}

int
get_monitor_slot (surfman_monitor_t m)
{
    int i;

    for (i = 0; i < DISPLAY_MONITOR_MAX; i++)
      {
        if (display[i].mon == m)
          return i;
      }

    return -1;
}

void
display_update_monitors (struct plugin *p,
                         surfman_monitor_t *monitors,
                         int count)
{
  int i, j;

  /* First prune disabled monitors... */
  for (i = 0; i < DISPLAY_MONITOR_MAX; i++)
    {
      if (!display[i].mon)
        continue;
      if (display[i].plugin == p)
        {
          for (j = 0; j < count; j++)
            {
              if (monitors[j] == display[i].mon)
                break;
            }

          if (j == count)
            {
              struct display *d, *tmp;

              surfman_info ("plugin %s, monitor %p disabled (ID:%d)",
                    p->name, display[i].mon, i);

              LIST_FOREACH_SAFE (d, tmp, &display[i].current, link)
                evict_display (p, d, i);

              display[i].mon = NULL;
              free (display[i].info);
              display[i].info = NULL;

              domain_monitor_update (i, 0);
            }
        }
    }

  /* ... then refresh the rest */
  for (i = 0; i < count; i++)
    {
      struct monitor_info *info = get_monitor_info (p, monitors[i]);

      for (j = 0; j < DISPLAY_MONITOR_MAX; j++)
        {
          if (display[j].plugin == p &&
              display[j].connectorid == info->i.connectorid)
            {
              if (!display[j].mon)
                {
                  surfman_info ("plugin %s, monitor %p enabled (ID:%d)",
                        p->name, monitors[i], j);
                  display[j].mon = monitors[i];
                  display[j].info = info;

                  domain_monitor_update (j, 1);
                  break;
                }
              else if (memcmp (info, display[j].info, sizeof (*info)))
                {
                  surfman_info ("plugin %s, monitor ID %d updated", p->name,
                        j);
                  display[j].mon = monitors[i];
                  free(display[j].info);
                  display[j].info = info;

                  domain_monitor_update (j, 0);
                  domain_monitor_update (j, 1);
                  break;
                }
            }
        }

      if (j == DISPLAY_MONITOR_MAX)
        {
          /* Allocate a new slot */
          for (j = 0; j < DISPLAY_MONITOR_MAX; j++)
            {
              if (display[j].connectorid == -1)
                break;
            }

          if (j == DISPLAY_MONITOR_MAX)
            {
              surfman_error ("plugin %s, failed to allocate a slot for monitor %p",
                     p->name, monitors[i]);
              continue;
            }

          surfman_info ("plugin %s, Allocated slot %d for monitor %p",
                p->name, j, monitors[i]);

          display[j].connectorid = info->i.connectorid;
          display[j].plugin = p;
          display[j].mon = monitors[i];
          display[j].info = info;

          domain_monitor_update (j, 1);
        }
    }
}

int
display_prepare_blank (int monitor_id,
                       struct device *dev)
{
  struct display *d;

  if (!display[monitor_id].mon)
    {
      surfman_warning ("Attempting to bind surface to a disabled monitor");
      return -1;
    }

  d = calloc (1, sizeof (*d));
  if (!d)
    return -1;

  d->display_type = DISPLAY_TYPE_BLANK;

  return 0;
}

int
display_prepare_surface (int monitor_id,
                         struct device *dev,
                         struct surface *s,
                         struct effect *e)
{
  struct display *d;

  if (!display[monitor_id].mon)
    {
      surfman_warning ("Attempting to bind surface to a disabled monitor");
      return -1;
    }

  d = calloc (1, sizeof (*d));
  if (!d)
    return -1;

  d->display_type = DISPLAY_TYPE_SURFACE;
  d->dev = dev;
  d->u.surface = s;
  if (e)
    d->effect = *e;

  LIST_INSERT_HEAD (&display[monitor_id].next, d, link);

  return 0;
}

int
display_prepare_vmonitor (int monitor_id,
                          struct device *dev,
                          surfman_vmonitor_t vmon,
                          struct effect *e)
{
  struct display *d;

  if (!display[monitor_id].mon)
    {
      surfman_warning ("Attempting to bind vmonitor to a disabled monitor");
      return -1;
    }

  d = calloc (1, sizeof (*d));
  if (!d)
    return -1;

  d->display_type = DISPLAY_TYPE_VMONITOR;
  if (e)
    d->effect = *e;
  d->dev = dev;
  d->u.vmonitor = vmon;
  LIST_INSERT_HEAD (&display[monitor_id].next, d, link);

  return 0;
}

int
display_commit (struct plugin *p, int force)
{
  int i;
  struct display_list dlist;
  int rc;
  int display_vmonitor = 0;

  rc = display_list_init (&dlist);
  if (rc)
    {
      surfman_error ("Failed to allocate display list");
      return rc;
    }

  for (i = 0; i < DISPLAY_MONITOR_MAX; i++)
    {
      if (!display[i].mon)
        continue;

      if (display[i].plugin == p)
        {
          struct display *d, *tmp;

          LIST_FOREACH_SAFE (d, tmp, &display[i].current, link)
            evict_display (p, d, i);

          /* If nothing to display, blank the monitor */
          if (LIST_EMPTY (&display[i].next))
            rc |= display_list_append (&dlist, display[i].mon, NULL, NULL);
          else
            {
              LIST_FOREACH_SAFE (d, tmp, &display[i].next, link)
                {
                  surfman_psurface_t ps;

                  LIST_REMOVE (d, link);
                  ps = get_psurface (p, d);
                  prepare_display (p, d, i);
                  if (d->display_type == DISPLAY_TYPE_VMONITOR)
                    display_vmonitor = 1;
                  rc |= display_list_append (&dlist, display[i].mon, ps, &d->effect);
                  LIST_INSERT_HEAD (&display[i].current, d, link);
                }
            }
        }
    }

  if (rc)
    surfman_error ("Memory allocation failures in display list");

  /* Check if we are allowed to commit only a vmonitor isn't displayed */
  if (!display_vmonitor)
    rc = vgpu_display_allow_commit (p, dlist.disp, dlist.len);

  if (rc)
    surfman_error ("Surfman denied commit for plugin %s", p->name);
  else
    {
      rc = display_list_commit (p, &dlist, force);
      if (rc)
        surfman_error ("Plugin %s display() method failed", p->name);
    }

  display_list_cleanup (&dlist);

  return rc;
}

void
display_vmonitor_takedown (surfman_vmonitor_t vmon)
{
  int i;

  for (i = 0; i < DISPLAY_MONITOR_MAX; i++)
    {
      struct display *d, *tmp;

      LIST_FOREACH_SAFE (d, tmp, &display[i].current, link)
        {
          if (d->display_type == DISPLAY_TYPE_VMONITOR &&
              d->u.vmonitor == vmon)
            evict_display (NULL, d, i);
        }
    }
}

void
display_surface_takedown (struct surface *s)
{
  int i;

  for (i = 0; i < DISPLAY_MONITOR_MAX; i++)
    {
      struct display *d, *tmp;

      LIST_FOREACH_SAFE (d, tmp, &display[i].current, link)
        {
          if (d->display_type == DISPLAY_TYPE_SURFACE &&
              d->u.surface == s)
            evict_display (NULL, d, i);
        }
    }
}

void
display_plugin_takedown (struct plugin *p)
{
  int i;

  for (i = 0; i < DISPLAY_MONITOR_MAX; i++)
    {
      if (display[i].plugin == p)
        {
          struct display *d, *tmp;

          LIST_FOREACH_SAFE (d, tmp, &display[i].current, link)
            evict_display (p, d, i);

          memset (&display[i], 0, sizeof (struct display));
          display[i].connectorid = -1;
        }
    }
}

int
display_get_edid (int monitor_id, uint8_t *buff, size_t sz)
{
  if (monitor_id >= DISPLAY_MONITOR_MAX ||
      monitor_id < 0)
    return -1;

  if (!display[monitor_id].mon)
    return -1;

  if (sz > sizeof (surfman_monitor_edid_t))
    sz = sizeof (surfman_monitor_edid_t);

  memcpy (buff, &display[monitor_id].info->edid, sz);

  return sz;
}

