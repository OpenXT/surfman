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

struct mapcache_entry
{
  uint64_t gfn:52;
  uint64_t age_bits:12;
  char *mmap;
};

struct mapcache_stats
{
  unsigned long miss_count;
  unsigned long hit_count;
};

struct mapcache
{
  int domid;
  unsigned int cache_len;
  struct mapcache_stats stats;
  struct mapcache_entry *entries;

  LIST_ENTRY(struct mapcache) link;
  int refcnt;
};

#define CACHE_WAYS 4
#define CACHE_INDEX_BITS 8
#define CACHE_WAY_SIZE (1 << CACHE_INDEX_BITS)
#define CACHE_INDEX_MASK ((1 << CACHE_INDEX_BITS) - 1)
#define CACHE_LENGTH (CACHE_WAY_SIZE * CACHE_WAYS)

#define PAGE_BITS XC_PAGE_SHIFT

static LIST_HEAD(,struct mapcache) mapcache_list = LIST_HEAD_INITIALIZER;
static pthread_mutex_t mapcache_list_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct mapcache *
mapcache_lookup (int domid)
{
  struct mapcache * m;

  LIST_FOREACH (m, &mapcache_list, link)
    {
      if (m->domid == domid)
        return m;
    }

  return NULL;
}

EXTERNAL mapcache_t
mapcache_create (int domid)
{
  struct mapcache *mapcache;

  pthread_mutex_lock (&mapcache_list_mutex);
  mapcache = mapcache_lookup (domid);
  if (mapcache)
    {
      mapcache->refcnt++;
      pthread_mutex_unlock(&mapcache_list_mutex);
      return mapcache;
    }

  mapcache = malloc (sizeof (*mapcache));
  if (mapcache == NULL)
    {
      pthread_mutex_unlock (&mapcache_list_mutex);
      return NULL;
    }

  mapcache->entries = malloc (CACHE_LENGTH * sizeof (*mapcache->entries));
  if (mapcache->entries == NULL)
    {
      free(mapcache);
      pthread_mutex_unlock (&mapcache_list_mutex);
      return NULL;
    }

  mapcache->domid = domid;
  mapcache->cache_len = CACHE_LENGTH;

  memset (&mapcache->stats, 0, sizeof (mapcache->stats));
  memset (mapcache->entries, 0, CACHE_LENGTH * sizeof (*mapcache->entries));

  LIST_INSERT_HEAD (&mapcache_list, mapcache, link);
  mapcache->refcnt = 1;

  pthread_mutex_unlock (&mapcache_list_mutex);
  return mapcache;

fail:
  free (mapcache->entries);
  free (mapcache);
  pthread_mutex_unlock (&mapcache_list_mutex);
  return NULL;
}

EXTERNAL void
mapcache_destroy (mapcache_t mapcache)
{
  if (mapcache == NULL)
    return;

  pthread_mutex_lock (&mapcache_list_mutex);

  mapcache->refcnt--;
  if (mapcache->refcnt <= 0)
    {
      int i;
      LIST_REMOVE (mapcache, link);
      for ( i = 0; i < CACHE_LENGTH; i++ )
        if ( mapcache->entries[i].mmap != MAP_FAILED )
          munmap(mapcache->entries[i].mmap, XC_PAGE_SIZE);
      free (mapcache->entries);
      free (mapcache);
    }

  pthread_mutex_unlock (&mapcache_list_mutex);
}

INTERNAL int
mapcache_has_addr (mapcache_t mapcache, void *addr)
{
  int i;
  char *page_addr = (char *)((uintptr_t)addr & ~(XC_PAGE_SIZE - 1));

  for ( i = 0; i < CACHE_LENGTH; i++ )
    if ( (mapcache->entries[i].mmap != MAP_FAILED)
         && (mapcache->entries[i].mmap == page_addr) )
      return 1;

  return 0;
}

static int
do_map_page (int domid, uint64_t gfn, void *addr)
{
  privcmd_mmap_t ioctl_op;
  privcmd_mmap_entry_t mmap_entry;

  mmap_entry.va = (unsigned long) addr;
  mmap_entry.mfn = gfn;
  mmap_entry.npages = 1;
  ioctl_op.num = 1;
  ioctl_op.dom = domid;
  ioctl_op.entry = &mmap_entry;

  return ioctl (privcmd_fd, IOCTL_PRIVCMD_MMAP, &ioctl_op);
}

EXTERNAL int
mapcache_invalidate_all (mapcache_t mapcache)
{
  int i;
  for ( i = 0; i < CACHE_LENGTH; i++ ) {
    mapcache->entries[i].gfn = 0;
    mapcache->entries[i].age_bits = 0;
    if ( mapcache->entries[i].mmap != MAP_FAILED )
      munmap(mapcache->entries[i].mmap, XC_PAGE_SIZE);
    mapcache->entries[i].mmap = MAP_FAILED;
  }

  return 0;
}

EXTERNAL int
mapcache_invalidate_entry (mapcache_t mapcache, uint64_t addr)
{
  uint64_t gfn = addr >> PAGE_BITS;
  unsigned long cache_idx = gfn & CACHE_INDEX_MASK;
  int i;

  i = cache_idx;
  while (i < CACHE_LENGTH)
    {
      if (mapcache->entries[i].gfn == gfn)
        {
          mapcache->entries[i].gfn = 0;
          mapcache->entries[i].age_bits = 0;
          if ( mapcache->entries[i].mmap != MAP_FAILED )
            munmap(mapcache->entries[i].mmap, XC_PAGE_SIZE);
          mapcache->entries[i].mmap = MAP_FAILED;

          return 0;
        }

      i += CACHE_WAY_SIZE;
    }

  return -ENOENT;
}

EXTERNAL void *
mapcache_get_mapping (mapcache_t mapcache, uint64_t addr)
{
  unsigned long page_offset = addr & (XC_PAGE_SIZE - 1);
  uint64_t gfn = addr >> PAGE_BITS;
  unsigned long cache_idx = gfn & CACHE_INDEX_MASK;
  int hit = -1;
  int elected[2] = { 0, INT_MAX };
  char *m;
  int i;

  i = cache_idx;
  while (i < CACHE_LENGTH)
    {
      if (mapcache->entries[i].gfn == gfn)
        {
          hit = i;
        }

      if (hit == -1 && mapcache->entries[i].age_bits < elected[1])
        {
          elected[0] = i;
          elected[1] = mapcache->entries[i].age_bits;
        }

      mapcache->entries[i].age_bits >>= 1;
      mapcache->entries[i].age_bits |= ((hit == i) << 11);

      i += CACHE_WAY_SIZE;
    }

  if (hit != -1)
    {
      mapcache->stats.hit_count++;
      m = mapcache->entries[hit].mmap + page_offset;
    }
  else
    {
      int rc;
      struct mapcache_entry *me = mapcache->entries + elected[0];

      mapcache->stats.miss_count++;
      me->gfn = gfn;
      me->age_bits = (1 << 12) - 1;

      if ( me->mmap != MAP_FAILED )
        munmap (me->mmap, XC_PAGE_SIZE);
      me->mmap = mmap (NULL, XC_PAGE_SIZE, PROT_READ | PROT_WRITE,
                      MAP_SHARED, privcmd_fd, 0);
      if ( me->mmap == MAP_FAILED )
        goto invalidate_and_exit;
      m = me->mmap;
      rc = do_map_page (mapcache->domid, gfn, m);
      if (rc == -1)
        {
          syslog (LOG_WARNING,
                  "mapcache: IOCTL_PRIVCMD_MMAP failed: %s. gfn:%llx, va:%p\n",
                  strerror (errno), gfn, m);
          munmap(me->mmap, XC_PAGE_SIZE);
          goto invalidate_and_exit;
        }

      m += page_offset;
    }

  return m;

invalidate_and_exit:
  /* Invalidate the elected entry */
  mapcache->entries[elected[0]].gfn = 0;
  mapcache->entries[elected[0]].age_bits = 0;
  return NULL;
}

EXTERNAL void
mapcache_dump_stats (mapcache_t mapcache)
{
  syslog (LOG_INFO, "Foreign mapping cache: cache hits %lu\n",
          mapcache->stats.hit_count);
  syslog (LOG_INFO, "Foreign mapping cache: cache miss %lu\n",
          mapcache->stats.miss_count);
}

EXTERNAL int
copy_to_domain (mapcache_t mapcache, uint64_t addr, void *p, size_t sz)
{
  if (!mapcache)
    return -1;

  while (sz)
    {
      void *v;
      size_t c;

      if (-addr & (XC_PAGE_SIZE - 1))
        c = -addr & (XC_PAGE_SIZE - 1);
      else
        c = XC_PAGE_SIZE;
      c = (c > sz) ? sz : c;

      if ((v = mapcache_get_mapping (mapcache, addr)) == NULL)
        break;

      memcpy (v, p, c);
      addr += c;
      sz -= c;
    }

  return sz;
}

EXTERNAL int
copy_from_domain (mapcache_t mapcache, void *p, uint64_t addr, size_t sz)
{
  if (!mapcache)
    return -1;

  while (sz)
    {
      void *v;
      size_t c;

      if (-addr & (XC_PAGE_SIZE - 1))
        c = -addr & (XC_PAGE_SIZE - 1);
      else
        c = XC_PAGE_SIZE;
      c = (c > sz) ? sz : c;

      if ((v = mapcache_get_mapping (mapcache, addr)) == NULL)
        break;

      memcpy (p, v, c);
      addr += c;
      sz -= c;
    }

  return sz;
}
