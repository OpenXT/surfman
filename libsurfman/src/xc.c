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

xc_interface *xch;
int privcmd_fd = -1;

static void vmessage (xentoollog_logger *logger_in,
                      xentoollog_level level,
                      int errnoval,
                      const char *context,
                      const char *format,
                      va_list al)
{
  char buff[1024];
  int sz = 0;
  char *lvl;

  (void) logger_in;
  switch (level)
    {
    case XTL_DEBUG:
    case XTL_VERBOSE:
    case XTL_DETAIL:
    case XTL_INFO:
    case XTL_NOTICE:
      lvl = "Info";
      break;
    case XTL_WARN:
      lvl = "Warning";
      break;
    case XTL_ERROR:
      lvl = "Error";
      break;
    case XTL_CRITICAL:
      lvl = "Fatal";
      break;
    default:
    case XTL_NONE:
    case XTL_PROGRESS:
      return;
    }

#define append(fmt...) sz += snprintf(buff + sz, 1024 - sz, fmt)
  append ("xenctrl:%s:", lvl);
  if (context)
    append ("%s:", context);

  sz += vsnprintf(buff + sz, 1024 - sz, format, al);

  if (errnoval >= 0)
    append(":errno=%s", strerror(errnoval));
#undef append

  fprintf(stderr, "%s\n", buff);
  fflush(stderr);
  syslog (LOG_ERR, "%s", buff);
}

static void progress (struct xentoollog_logger *logger_in,
                      const char *context,
                      const char *doing_what, int percent,
                      unsigned long done, unsigned long total)
{
    (void) logger_in;
    (void) context;
    (void) doing_what;
    (void) percent;
    (void) done;
    (void) total;
}

static void destroy (struct xentoollog_logger *logger_in)
{
    (void) logger_in;
}

INTERNAL struct xentoollog_logger xc_logger = {
  .vmessage = vmessage,
  .progress = progress,
  .destroy = destroy,
};

EXTERNAL int
xc_has_vtd (void)
{
  static int has_hvm_directio = -1;

#define MAX_CPU_ID 255
  if (has_hvm_directio == -1)
    {
      xc_physinfo_t info;

      info.max_cpu_id = MAX_CPU_ID;
      if (xc_physinfo(xch, &info))
        {
          fatal ("xc_physinfo(): %s", strerror(errno));
          return 0;
        }

      has_hvm_directio = info.capabilities & XEN_SYSCTL_PHYSCAP_hvm_directio;
    }

  return !!has_hvm_directio;
}


EXTERNAL void
xc_init (void)
{
  if (!xch)
    {
      xch = xc_interface_open (&xc_logger, &xc_logger, 0);
      if (!xch)
        fatal ("Failed to open XC interface");
    }

  if (privcmd_fd == -1)
    {
      privcmd_fd = open ("/proc/xen/privcmd", O_RDWR);
      if (privcmd_fd < 0)
        fatal ("Failed to open privcmd");
    }
}

EXTERNAL int
xc_domid_exists (int domid)
{
  xc_dominfo_t info;
  int rc;

  rc = xc_domain_getinfo (xch, domid, 1, &info);
  return rc >= 0 ? info.domid == (domid_t)domid : 0;
}

EXTERNAL void *xc_mmap_foreign(void *addr, size_t length, int prot,
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


