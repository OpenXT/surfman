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

static xc_interface *xch;
static int privcmd_fd = -1;

void xc_init (void)
{
  if (!xch)
    {
      xch = xc_interface_open (NULL, NULL, 0);
      if (!xch)
        surfman_fatal ("Failed to open XC interface");
    }

  if (privcmd_fd == -1)
    {
      privcmd_fd = open ("/proc/xen/privcmd", O_RDWR);
      if (privcmd_fd < 0)
        surfman_fatal ("Failed to open privcmd");
    }
}

int xc_domid_getinfo(int domid, xc_dominfo_t *info)
{
  int rc;

  rc = xc_domain_getinfo (xch, domid, 1, info);
  if (rc == 1)
    return info->domid == (domid_t)domid ? 1 : -ENOENT;
  return rc;
}

int xc_domid_exists(int domid)
{
  xc_dominfo_t info = { 0 };

  return !!xc_domid_getinfo(domid, &info);
}

void *xc_mmap_foreign(void *addr, size_t length, int prot,
                      int domid, xen_pfn_t *pages)
{
  void *ret;
  int rc;
  privcmd_mmapbatch_t ioctlx;
  size_t i;

  ret = mmap (addr, length, prot, MAP_SHARED, privcmd_fd, 0);
  if (ret == MAP_FAILED)
    return ret;

  ioctlx.num = (length + XC_PAGE_SIZE - 1) / XC_PAGE_SIZE;
  ioctlx.dom = domid;
  ioctlx.addr = (unsigned long)ret;
  ioctlx.arr = pages;

  rc = ioctl(privcmd_fd, IOCTL_PRIVCMD_MMAPBATCH, &ioctlx);

  for (i = 0; i < length; i += XC_PAGE_SIZE)
    pages[i >> XC_PAGE_SHIFT] &= ~XEN_DOMCTL_PFINFO_LTAB_MASK;

  if (rc < 0)
    {
      munmap(ret, length);
      ret = MAP_FAILED;
    }

  return ret;
}

int xc_translate_gpfn_to_mfn (int domid, size_t pfn_count,
			      xen_pfn_t *pfns, pfn_t *mfns)
{
  int rc = 0;

  assert(xch != NULL);
  assert(pfns != NULL);
  assert(mfns != NULL);

  /* This will try to pin the pfns, but the device model should take care of
   * that really and this should only check for pfns to be pinned or fail
   * otherwise. */
  rc = xc_domain_memory_translate_gpfn_list (xch, domid, pfn_count,
                                             pfns, mfns);
  if (rc)
    surfman_error ("xc_domain_memory_translate_gpfn_list failed (%s).",
                   strerror (errno));
  /* Since this is done a bit early for QEMU, it will later try to pin the pfns
   * and fail (because they are pinned by us). So unpin them immediately... */
  xc_domain_memory_release_mfn_list(xch, domid, pfn_count, mfns);

  return rc;
}

int xc_hvm_get_dirty_vram(int domid, uint64_t base_pfn, size_t n,
                          unsigned long *db)
{
  return xc_hvm_track_dirty_vram (xch, domid, base_pfn, n, db);
}
