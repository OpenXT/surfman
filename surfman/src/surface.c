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

#define __min(x, y) ((x) > (y) ? (y) : (x))

/* libsurfman's secret functions */
int surfman_surface_init(surfman_surface_t *surface);
void surfman_surface_cleanup(surfman_surface_t *surface);
void surfman_surface_update_mmap(surfman_surface_t *surface, int fd, size_t off);
void surfman_surface_update_pfn_arr(surfman_surface_t *surface, const xen_pfn_t *pfns);
void surfman_surface_update_pfn_linear(surfman_surface_t *surface, xen_pfn_t base);

static int register_display_handler(display_handler_t handler,
                                    void *priv, struct handler_list_head *list)
{
  struct display_handler *h;

  h = calloc (1, sizeof (*h));
  if (!h)
      return -1;

  h->handler = handler;
  h->priv = priv;

  LIST_INSERT_HEAD (list, h, link);

  return 0;
}

static int unregister_display_handler(display_handler_t handler,
                                      struct handler_list_head *list)
{
  struct display_handler *h, *tmp;

  LIST_FOREACH_SAFE (h, tmp, list, link)
    {
      if (h->handler == handler)
        {
          LIST_REMOVE (h, link);
          free (h);
          return 0;
        }
    }

  return -1; /* Not found */
}

int
surface_register_onscreen (struct surface *s, display_handler_t h, void *priv)
{
  if (s->handlers_lock)
    {
      surfman_error ("display handler list is beeing used");
      return -1;
    }

  return register_display_handler (h, priv, &s->onscreen_handlers);
}

int
surface_unregister_onscreen (struct surface *s, display_handler_t h)
{
  if (s->handlers_lock)
    {
      surfman_error ("display handler list is beeing used");
      return -1;
    }

  return unregister_display_handler (h, &s->onscreen_handlers);
}

int
surface_register_offscreen (struct surface *s, display_handler_t h, void *priv)
{
  if (s->handlers_lock)
    {
      surfman_error ("display handler list is beeing used");
      return -1;
    }

  return register_display_handler (h, priv, &s->offscreen_handlers);
}

int
surface_unregister_offscreen (struct surface *s, display_handler_t h)
{
  if (s->handlers_lock)
    {
      surfman_error ("display handler list is beeing used");
      return -1;
    }

  return unregister_display_handler (h, &s->offscreen_handlers);
}

int
surface_need_refresh (struct surface *s)
{
  struct psurface *ps;

  LIST_FOREACH (ps, &s->cache, link)
    {
      if (plugin_need_refresh (ps->plugin))
        return 1;
    }

  return 0;
}

void
surface_refresh (struct surface *s, uint8_t *dirty)
{
  struct psurface *ps;

  LIST_FOREACH (ps, &s->cache, link)
    {
      if (plugin_need_refresh (ps->plugin))
        PLUGIN_CALL (ps->plugin, refresh_psurface, ps->psurface, dirty);
    }
}

static void
surface_refresh_timer (int fd, short event, void *opaque)
{
  struct surface *s = opaque;
  uint8_t *dirty = NULL;
  int async = 0;
  struct timeval tv;

  if (!surface_need_refresh(s))
    return; /* Exit without rearming */

  s->dev->ops->refresh_surface(s->dev, s);

rearm:
  tv.tv_sec = 0;
  tv.tv_usec = 16667; /* 1/60 s */

  event_add (&s->refresh, &tv);
}

static void surface_onscreen(struct plugin *p, struct surface *s,
                             int monitor_id, void *priv)
{
    if (plugin_need_refresh (p))
      surface_refresh_resume (s);
}

static void surface_offscreen(struct plugin *p, struct surface *s,
                              int monitor_id, void *priv)
{
    surface_refresh_stall (s);
}

struct surface *
surface_create (struct device *dev, void *priv)
{
  struct surface *s;

  s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;

  LIST_INIT(&s->cache);
  s->surface = calloc (1, sizeof (surfman_surface_t));

  if (!s->surface)
    goto fail_surface;

  if (surfman_surface_init (s->surface))
      goto fail_surface2;

  event_set (&s->refresh, -1, EV_TIMEOUT, surface_refresh_timer, s);
  s->dev = dev;
  s->priv = priv;

  LIST_HEAD_INIT (&s->onscreen_handlers);
  LIST_HEAD_INIT (&s->offscreen_handlers);

  surface_register_onscreen (s, surface_onscreen, NULL);
  surface_register_offscreen (s, surface_offscreen, NULL);

  return s;

fail_surface2:
  free (s->surface);
fail_surface:
  free (s);
  return NULL;
}

surfman_psurface_t
surface_get_psurface (struct surface *s, struct plugin *p)
{
  struct psurface *ps;

  if (!surface_ready (s))
    return NULL;

  LIST_FOREACH (ps, &s->cache, link)
    {
      if (ps->plugin == p)
        break;
    }

  if (!ps)
    {
      ps = calloc (1, sizeof (*ps));
      if (!ps)
        return NULL;

      ps->plugin = p;
      ps->psurface = PLUGIN_CALL (p, get_psurface_from_surface,
                                  s->surface);
      if (!ps->psurface)
        {
          free (ps);
          return NULL;
        }
      LIST_INSERT_HEAD(&s->cache, ps, link);
    }

  return ps->psurface;
}

void
surface_destroy (struct surface *s)
{
  struct psurface *ps, *psn;
  struct display_handler *h, *hn;

  display_surface_takedown (s);

  LIST_FOREACH_SAFE (ps, psn, &s->cache, link)
    {
      PLUGIN_CALL (ps->plugin, free_psurface, ps->psurface);
      LIST_REMOVE (ps, link);
      free (ps);
    }

  LIST_FOREACH_SAFE (h, hn, &s->onscreen_handlers, link)
    {
      LIST_REMOVE (h, link);
      free (h);
    }

  LIST_FOREACH_SAFE (h, hn, &s->offscreen_handlers, link)
    {
      LIST_REMOVE (h, link);
      free (h);
    }

  surfman_surface_cleanup (s->surface);
  free (s->surface);
  free (s);
}

static void
surface_update (struct surface *s, int flags)
{
  struct psurface *ps;

  s->flags |= flags;

  if (s->flags == SURFMAN_UPDATE_ALL)
    {
      LIST_FOREACH (ps, &s->cache, link)
        {
          PLUGIN_CALL (ps->plugin, update_psurface, ps->psurface,
                       s->surface, flags);
        }
    }
}

int
surface_ready (struct surface *s)
{
  return (s->flags == SURFMAN_UPDATE_ALL);
}

static void
surface_update_pages (struct surface *s,
                      int domid,
                      pfn_t *mfns,
                      size_t npages)
{
  if (s->surface->page_count != npages)
    s->surface = realloc (s->surface, sizeof (surfman_surface_t) +
                                      npages * sizeof (pfn_t));

  s->surface->page_count = npages;
  s->surface->pages_domid = domid;

  memcpy (s->surface->mfns, mfns, npages * sizeof (pfn_t));
}

void
surface_update_mmap (struct surface *s,
                     int domid,
                     int fd,
                     pfn_t *mfns,
                     size_t npages)
{
    surface_update_pages (s, domid, mfns, npages);

    surfman_surface_update_mmap (s->surface, fd, 0);

    surface_update (s, SURFMAN_UPDATE_PAGES);
}

void
surface_update_pfns (struct surface *s,
                     int domid,
                     const xen_pfn_t *pfns,
                     pfn_t *mfns,
                     size_t npages)
{
    surface_update_pages (s, domid, mfns, npages);

    surfman_surface_update_pfn_arr (s->surface, pfns);

    surface_update (s, SURFMAN_UPDATE_PAGES);
}

void
surface_update_lfb (struct surface *s,
                    int domid,
                    unsigned long lfb,
                    size_t len)
{
  xen_pfn_t *x;
  int rc;
  size_t i;
  size_t npages = (len + (XC_PAGE_SIZE - 1)) >> XC_PAGE_SHIFT;

  x = calloc (npages, sizeof (xen_pfn_t));
  if (!x)
    return;

  for (i = 0; i < npages; i++)
    x[i] = (lfb >> XC_PAGE_SHIFT) + i;

  if (s->surface->page_count != npages)
    s->surface = realloc (s->surface, sizeof (surfman_surface_t) +
                          npages * sizeof (pfn_t));
  if (!s->surface)
    goto fail;


  s->surface->page_count = npages;
  s->surface->pages_domid = domid;

  surfman_surface_update_pfn_linear (s->surface, lfb >> XC_PAGE_SHIFT);

  rc = xc_domain_memory_translate_gpfn_list (xch, domid, npages, x,
                                             s->surface->mfns);
  if (rc)
    {
      surfman_error ("failed to translate pfns");
      goto fail;
    }
  xc_domain_memory_release_mfn_list(xch, domid, npages,
                                    s->surface->mfns);

  // XXX: We release mfns before passing them as we don't need the page_info
  // reference anymore.  The device-model is supposed to have a reference on
  // those pages to keep the mfns where they are (so DMA is possible).
  surface_update (s, SURFMAN_UPDATE_PAGES);

fail:
  free (x);
}

void
surface_update_format (struct surface *s,
                       int width,
                       int height,
                       int stride,
                       int format,
                       int offset)
{
  surfman_surface_t *surface = s->surface;

  surface->width = width;
  surface->height = height;
  surface->stride = stride;
  surface->format = format;
  surface->offset = offset;

  surface_update (s, SURFMAN_UPDATE_FORMAT | SURFMAN_UPDATE_OFFSET);
}

void
surface_update_offset(struct surface *s,
                      size_t offset)
{
  surfman_surface_t *surface = s->surface;

  surface->offset = offset;

  surface_update (s, SURFMAN_UPDATE_OFFSET);
}

void
surface_refresh_stall (struct surface *s)
{
  event_del (&s->refresh);
}

void
surface_refresh_resume (struct surface *s)
{
  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = 16667; /* 1/60 s */

  event_add (&s->refresh, &tv);
}
