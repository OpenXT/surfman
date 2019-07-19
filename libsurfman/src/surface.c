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

#define PRIV(s) (void *)(uintptr_t)((s)->priv)

enum
{
  TYPE_INVALID = 0,
  TYPE_MMAP,
  TYPE_PFN_ARR,
  TYPE_PFN_LINEAR,
};

struct surface_priv
{
  int type;

  union
  {
    struct
    {
      int fd;
      off_t offset;
    } mmap;
    struct
    {
      domid_t domid;
      const xen_pfn_t *arr;
    } pfn_arr;
    struct
    {
      domid_t domid;
      xen_pfn_t base;
    } pfn_linear;
  } u;

  pthread_mutex_t lock;
  char *baseptr;
  size_t len;
};

static void surface_update_mfn_list (surfman_surface_t * surface)
{
  struct surface_priv *p = PRIV(surface);
  xen_pfn_t *pfns;
  int rc;
  int domid = surface->pages_domid;
  size_t i, n = surface->page_count;

  /* p2m translation is only performed for lfb given by qemu,
   * so PFN_LINEAR or abort. */
  assert(p->type == TYPE_PFN_LINEAR);

  pfns = xcalloc(n, sizeof (xen_pfn_t));
  for (i = 0; i < n; ++i)
    pfns[i] = p->u.pfn_linear.base + i;

  if (xc_translate_gpfn_to_mfn (domid, n, pfns, surface->mfns))
    surfman_error ("Failed to translate pfns for dom%d (base:%#lx).",
                   domid, p->u.pfn_linear.base);

  free(pfns);
}

static void surface_update_mfn_arr (surfman_surface_t * surface)
{
  struct surface_priv *p = PRIV(surface);
  xen_pfn_t *pfns;
  int rc;
  int domid = surface->pages_domid;

  assert(p->type == TYPE_PFN_ARR);

  pfns = xcalloc(surface->page_count, sizeof (xen_pfn_t));
  memcpy(pfns, p->u.pfn_arr.arr, surface->page_count * sizeof (xen_pfn_t));

  if (xc_translate_gpfn_to_mfn (domid, surface->page_count, pfns, surface->mfns))
    surfman_error ("Failed to translate gpfns for dom%d.", domid);

  free(pfns);
}

static void update_mapping (struct surface_priv *p, size_t len)
{
  size_t npages = (len + XC_PAGE_SIZE - 1) / XC_PAGE_SIZE;
  xen_pfn_t *pfns;
  size_t i;

  if (p->baseptr)
    munmap (p->baseptr, p->len);

  switch (p->type)
    {
      case TYPE_MMAP:
        p->baseptr = mmap (p->baseptr, len,
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,
                           p->u.mmap.fd,
                           p->u.mmap.offset);
        break;
      case TYPE_PFN_ARR:
        pfns = xcalloc (npages, sizeof (*pfns));
        memcpy(pfns, p->u.pfn_arr.arr, npages * sizeof (*pfns));
        p->baseptr = xc_mmap_foreign (p->baseptr, len,
                                      PROT_READ | PROT_WRITE,
                                      p->u.pfn_arr.domid, pfns);
        free (pfns);
        break;
      case TYPE_PFN_LINEAR:
        pfns = xcalloc (npages, sizeof (*pfns));
        for (i = 0; i < npages; i++)
          pfns[i] = p->u.pfn_linear.base + i;

        p->baseptr = xc_mmap_foreign (p->baseptr, len,
                                      PROT_READ | PROT_WRITE,
                                      p->u.pfn_linear.domid, pfns);
        free (pfns);
        break;
      default:
        surfman_error ("invalid surface type %d", p->type);
    }

  if (p->baseptr == MAP_FAILED)
    {
      p->baseptr = NULL;
      surfman_error ("failed to map.");
    }

  p->len = len;
}

void *surface_map (surfman_surface_t * surface)
{
  struct surface_priv *p = PRIV(surface);

  pthread_mutex_lock (&p->lock);
  if (!p->baseptr)
    update_mapping (p, surface->page_count * XC_PAGE_SIZE);

  pthread_mutex_unlock (&p->lock);
  return p->baseptr;
}

xen_pfn_t surface_get_base_gfn(surfman_surface_t * surface)
{
  struct surface_priv *p = PRIV(surface);

  if (p->type != TYPE_PFN_LINEAR)
    return (xen_pfn_t)0UL;
  return p->u.pfn_linear.base;
}

void surface_unmap (surfman_surface_t * surface)
{
  struct surface_priv *p = PRIV(surface);

  pthread_mutex_lock (&p->lock);
  if (p->baseptr)
    {
      munmap (p->baseptr, p->len);
      p->baseptr = NULL;
    }
  pthread_mutex_unlock (&p->lock);
}

/*
 * Export these only to surfman and not the plugins
 */
int surfman_surface_init (surfman_surface_t * surface)
{
  struct surface_priv *p;

  p = malloc (sizeof (*p));
  if (!p)
    return -1;

  memset (p, 0, sizeof (*p));
  pthread_mutex_init (&p->lock, NULL);

  surface->priv = (uintptr_t) p;

  return 0;
}

void surfman_surface_cleanup (surfman_surface_t * surface)
{
  struct surface_priv *p = PRIV(surface);

  surface_unmap (surface);
  free (p);
}

void surfman_surface_update_mmap (surfman_surface_t * surface, int fd, size_t off)
{
  struct surface_priv *p = PRIV(surface);

  pthread_mutex_lock (&p->lock);
  p->type = TYPE_MMAP;
  p->u.mmap.fd = fd;
  p->u.mmap.offset = off;
  if (p->baseptr)
    update_mapping (p, surface->page_count * XC_PAGE_SIZE);
  pthread_mutex_unlock (&p->lock);
}

void surfman_surface_update_pfn_arr (surfman_surface_t * surface,
                                const xen_pfn_t * pfns)
{
  struct surface_priv *p = PRIV(surface);
  xc_dominfo_t info;

  pthread_mutex_lock (&p->lock);
  p->type = TYPE_PFN_ARR;
  p->u.pfn_arr.domid = surface->pages_domid;
  p->u.pfn_arr.arr = pfns;
  if (p->baseptr)
    update_mapping (p, surface->page_count * XC_PAGE_SIZE);

  if (xc_domid_getinfo (surface->pages_domid, &info) != 1)
      surfman_error ("Cannot retrieve dom%u information.", surface->pages_domid);
  else if (info.hvm)
    surface_update_mfn_arr (surface);

  pthread_mutex_unlock (&p->lock);
}

void surfman_surface_update_pfn_linear (surfman_surface_t * surface, xen_pfn_t base)
{
  struct surface_priv *p = PRIV(surface);

  pthread_mutex_lock (&p->lock);
  p->type = TYPE_PFN_LINEAR;
  p->u.pfn_linear.domid = surface->pages_domid;
  p->u.pfn_linear.base = base;
  if (p->baseptr)
    update_mapping (p, surface->page_count * XC_PAGE_SIZE);
  surface_update_mfn_list(surface);
  pthread_mutex_unlock (&p->lock);
}

