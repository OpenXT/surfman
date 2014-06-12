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

#ifndef HVM_MAX_VCPUS
# define HVM_MAX_VCPUS 128
#endif

struct iorange
{
  uint64_t start;
  uint64_t end;
  int mmio;
  void *priv;

  io_read_t read[3];
  io_write_t write[3];

  LIST_ENTRY(struct iorange) link;
};

struct iohandle
{
  int fd;
  shared_iopage_t *shpage_ptr;
  buffered_iopage_t *buffered_iopage;
  xc_evtchn *evtchn;
  ioservid_t serverid;
  mapcache_t mapcache;
  unsigned int ports[HVM_MAX_VCPUS];
  int nports;
  domid_t domid;
  LIST_HEAD (, struct iorange) iorange_list;
  pthread_mutex_t iorange_lock;
};

EXTERNAL int
iohandle_get_fd (iohandle_t iohdl)
{
  if (!iohdl)
    return -1;

  return iohdl->fd;
}

static int
set_nonblock(int fd)
{
  long fdfl;

  fdfl = fcntl (fd, F_GETFL, 0);
  if ((int) fdfl == -1)
    return -1;
  fdfl = fcntl (fd, F_SETFL, fdfl | O_NONBLOCK);
  if ((int) fdfl == -1)
    return -1;

  return 0;
}

static ioreq_t *
current_ioreq (iohandle_t iohdl, int vcpuid, int *state)
{
  ioreq_t *req;
  shared_iopage_t *p;

  if (!iohdl)
    return NULL;

  p = iohdl->shpage_ptr;
  req = &(p->vcpu_ioreq[vcpuid]);

  *state = req->state;

  /* Get ioreq state, then read ioreq content */
  xen_rmb ();
  return req;
}

/* iorange lock should be taken when calling this */
static struct iorange *
get_iorange (iohandle_t iohdl, uint64_t addr, int type)
{
  struct iorange *ior;
  if (!iohdl)
    return NULL;

  LIST_FOREACH (ior, &iohdl->iorange_list, link)
    {
      if (addr >= ior->start && addr <= ior->end && type == ior->mmio)
        return ior;
    }

  return NULL;
}

/**
 * These functions do_read and do_write permits to read/write data from/to
 * memory. This memory can be handle by surfman or directly the RAM.
 * In the second case we must call mapcache.
 * Theses functions are necessary because sometime, when the data is a
 * pointer, it can something else than RAM address.
 */
static uint64_t do_read (iohandle_t iohdl, uint64_t addr, uint32_t size,
                         int is_mmio)
{
    struct iorange *range = NULL;
    int res;
    uint64_t data = 0;

    pthread_mutex_lock (&iohdl->iorange_lock);
    range = get_iorange (iohdl, addr, is_mmio);

    if (range)
    {
        switch (size) {
        case 1:
            data = range->read[0] (addr, range->priv);
            break;
        case 2:
            data = range->read[1] (addr, range->priv);
            break;
        case 4:
            data = range->read[2] (addr, range->priv);
            break;
        case 8:
            data = range->read[2] (addr, range->priv) & 0xffffffff;
            data |= ((uint64_t)(range->read[2] (addr + 4,
                                     range->priv))) << 32;
            break;
        default:
            surfman_warning("BAD io read addr: %#"PRIx64", size: %u", addr, size);
        }
        pthread_mutex_unlock (&iohdl->iorange_lock);
    }
    else if (is_mmio) /* Only call mapcache when it's an mmio */
    {
        pthread_mutex_unlock (&iohdl->iorange_lock);
        res = copy_from_domain (iohdl->mapcache, &data, addr, size);
        if (res)
        {
            surfman_warning ("Failed to copy from domain addr: %#"PRIx64", size: %u"
                     ", res: %d", addr, size, res);
        }
    }
    return data;
}

static void do_write (iohandle_t iohdl, uint64_t addr, uint64_t data,
                      uint32_t size, int is_mmio)
{
    struct iorange *range = NULL;
    int res;

    pthread_mutex_lock (&iohdl->iorange_lock);
    range = get_iorange (iohdl, addr, is_mmio);

    if (range)
    {
        switch (size) {
        case 1:
            range->write[0] (addr, data & 0xff, range->priv);
            break;
        case 2:
            range->write[1] (addr, data & 0xffff, range->priv);
            break;
        case 4:
            range->write[2] (addr, data & 0xffffffff, range->priv);
            break;
        case 8:
            range->write[2] (addr, data & 0xffffffff, range->priv);
            range->write[2] (addr + 4, data >> 32, range->priv);
            break;
        default:
            surfman_warning("BAD io write addr: %#"PRIx64", data: %"PRIx64","
                    " size: %u", addr, data, size);
        }
        pthread_mutex_unlock (&iohdl->iorange_lock);
    }
    else if (is_mmio) /* Only call mapcache when it's an mmio */
    {
        pthread_mutex_unlock (&iohdl->iorange_lock);
        res = copy_to_domain (iohdl->mapcache, addr, &data, size);
        if (res)
            surfman_warning("Failed to copy to domain addr: %#"PRIx64", data: %#"PRIx64
                    ",size: %u, res: %d", addr, data, size, res);
    }
}

#define COMPUTE(req, i)                                                 \
    (int64_t)(((req)->df ? -1 : 1) * (int64_t)i * (int64_t)(req)->size)

static void handle_copy (iohandle_t iohdl, ioreq_t *req)
{
    uint32_t i;
    uint64_t data;

    if (req->dir == IOREQ_READ)
    {
        for (i = 0; i < req->count; i++)
        {
            data = do_read (iohdl, req->addr + COMPUTE(req, i),
                           req->size, 1);
            if (req->data_is_ptr)
                do_write (iohdl, req->data + COMPUTE(req, i), data,
                          req->size, 1);
            else
                req->data = data;
        }
    }
    else
    {
        for (i = 0; i < req->count; i++)
        {
            if (req->data_is_ptr)
                data = do_read (iohdl, req->data + COMPUTE(req, i),
                                req->size, 1);
            else
                data = req->data;
            do_write (iohdl, req->addr + COMPUTE(req, i), data,
                      req->size, 1);
        }
    }
}

static void handle_pio (iohandle_t iohdl, ioreq_t *req)
{
    uint32_t i;
    uint64_t data;

    if (!req->data_is_ptr)
    {
        if (req->dir == IOREQ_READ)
            req->data = do_read (iohdl, req->addr, req->size, 0);
        else
            do_write (iohdl, req->addr, req->data, req->size, 0);
    }
    else
    {
        for (i = 0; i < req->count; i++)
        {
            if (req->dir == IOREQ_READ)
            {
                data = do_read (iohdl, req->addr, req->size, 0);
                do_write (iohdl, req->data + COMPUTE(req, i), data,
                          req->size, 1);
            }
            else
            {
                data = do_read (iohdl, req->data + COMPUTE(req, i),
                                req->size, 1);
                do_write (iohdl, req->addr, data, req->size, 0);
            }
        }
    }
}

static void io_dispatch (iohandle_t iohdl, ioreq_t *req)
{
  switch (req->type)
  {
  case IOREQ_TYPE_INVALIDATE:
      /* Process mapcache invalidation requests */
      mapcache_invalidate_all (iohdl->mapcache);
      break;
  case IOREQ_TYPE_PIO:
      handle_pio (iohdl, req);
      break;
  case IOREQ_TYPE_COPY:
      handle_copy (iohdl, req);
      break;
  }
}

static void handle_buffered_io(iohandle_t iohdl)
{
    buf_ioreq_t *buf_req = NULL;
    ioreq_t req;
    int qw;

    memset (&req, 0, sizeof (req));

    while (iohdl->buffered_iopage->read_pointer != iohdl->buffered_iopage->write_pointer)
      {
        buf_req = &iohdl->buffered_iopage->buf_ioreq[
            iohdl->buffered_iopage->read_pointer % IOREQ_BUFFER_SLOT_NUM];
        req.size = 1UL << buf_req->size;
        req.count = 1;
        req.addr = buf_req->addr;
        req.data = buf_req->data;
        req.state = STATE_IOREQ_READY;
        req.dir = buf_req->dir;
        req.df = 1;
        req.type = buf_req->type;
        req.data_is_ptr = 0;
        qw = (req.size == 8);
        if (qw)
          {
            buf_req = &iohdl->buffered_iopage->buf_ioreq[
                (iohdl->buffered_iopage->read_pointer - 1) % IOREQ_BUFFER_SLOT_NUM];
            req.data |= ((uint64_t)buf_req->data) << 32;
          }

        io_dispatch (iohdl, &req);

        xen_mb ();
        iohdl->buffered_iopage->read_pointer += qw ? 2 : 1;
      }
}

EXTERNAL int
io_handle (iohandle_t iohdl)
{
  unsigned int port;
  int vcpu;
  ioreq_t *req = NULL;
  int req_state;

  if (!iohdl)
    return -1;

  handle_buffered_io(iohdl);

  port = xc_evtchn_pending (iohdl->evtchn);
  if ((int) port == -1)
    {
      /* read on evtchn fd probably returned -EAGAIN */
      return -1;
    }
  for (vcpu = 0; vcpu < iohdl->nports; vcpu++)
    {
      if (port == iohdl->ports[vcpu])
        {
          xc_evtchn_unmask (iohdl->evtchn, port);
          req = current_ioreq (iohdl, vcpu, &req_state);
          break;
        }
    }

  if (!req || req_state != STATE_IOREQ_READY)
    return -1;

  req->state = STATE_IOREQ_INPROCESS;

  io_dispatch (iohdl, req);

  xen_wmb (); /* Update ioreq contents then update state */
  req->state = STATE_IORESP_READY;
  xc_evtchn_notify (iohdl->evtchn, iohdl->ports[vcpu]);

  return 0;
}

EXTERNAL void
iohandle_iorange_setpriv (iohandle_t iohdl, uint64_t addr, void *priv)
{
  struct iorange *ior;
  if (!iohdl)
    return;

  pthread_mutex_lock (&iohdl->iorange_lock);
  LIST_FOREACH (ior, &iohdl->iorange_list, link)
    {
      if (addr >= ior->start && addr <= ior->end)
        {
          ior->priv = priv;
          break;
        }
    }
  pthread_mutex_unlock (&iohdl->iorange_lock);
}

EXTERNAL void
iohandle_remove_iorange (iohandle_t iohdl, uint64_t addr, int mmio)
{
  struct iorange *ior;
  if (!iohdl)
    return;

  xc_hvm_unmap_io_range_from_ioreq_server (xch, iohdl->domid,
                                           iohdl->serverid, mmio, addr);

  pthread_mutex_lock (&iohdl->iorange_lock);
  LIST_FOREACH (ior, &iohdl->iorange_list, link)
    {
      if (addr >= ior->start && addr <= ior->end && mmio == ior->mmio)
        {
          LIST_REMOVE (ior, link);
          free (ior);
          break;
        }
    }
  pthread_mutex_unlock (&iohdl->iorange_lock);
}

EXTERNAL void
iohandle_add_iorange (iohandle_t iohdl,
                      uint64_t addr,
                      uint64_t size,
                      int mmio,
                      io_ops_t *ops,
                      void *priv)
{
  struct iorange *ior;
  int rc;

  if (!iohdl)
    return;

  pthread_mutex_lock (&iohdl->iorange_lock);
  ior = malloc (sizeof (*ior));
  if (ior)
    {
      ior->start = addr;
      ior->end = addr + size - 1;
      ior->mmio = mmio;
      ior->priv = priv;
      ior->read[0] = ops->readb;
      ior->read[1] = ops->readw;
      ior->read[2] = ops->readl;
      ior->write[0] = ops->writeb;
      ior->write[1] = ops->writew;
      ior->write[2] = ops->writel;
      LIST_INSERT_HEAD (&iohdl->iorange_list, ior, link);
    }
  else
    surfman_error("malloc: %s", strerror(errno));
  pthread_mutex_unlock (&iohdl->iorange_lock);

  rc = xc_hvm_map_io_range_to_ioreq_server (xch, iohdl->domid,
                                       iohdl->serverid, mmio, addr,
                                       addr + size - 1);
  if (rc)
    surfman_error("xc_hvm_map_io_range_to_ioreq_server failed with code %d", rc);
}

EXTERNAL iohandle_t
iohandle_create (int domid)
{
  struct iohandle *iohdl;
  int rc, i;
  unsigned int ports[HVM_MAX_VCPUS];
  unsigned long pfn;
  uint32_t port;

  iohdl = malloc (sizeof (*iohdl));
  if (!iohdl)
    return NULL;

  iohdl->evtchn = xc_evtchn_open (&xc_logger, 0);
  if (!iohdl->evtchn)
    goto bad_evtchn;

  iohdl->fd = xc_evtchn_fd (iohdl->evtchn);
  if (iohdl->fd == -1 || set_nonblock (iohdl->fd))
    goto bad_fd;

  rc = xc_hvm_register_ioreq_server (xch, domid);
  if (rc < 0)
    goto bad_serverid;

  iohdl->serverid = rc;

  rc = xc_get_hvm_param(xch, domid, HVM_PARAM_IO_PFN_FIRST, &pfn);
  /**
   * The HVM_PARAM_IO_PFN_FIRST is used in Xen. We have 2 PFN allocate
   * by device model. The first is for synchronous IO and the second for
   * buffered IO
   */
  pfn += (iohdl->serverid - 1) * 2 + 1;
  iohdl->shpage_ptr = rc ? NULL : xc_map_foreign_range (xch, domid,
                                                        XC_PAGE_SIZE,
                                                        PROT_READ | PROT_WRITE,
                                                        pfn);
  if (!iohdl->shpage_ptr)
    goto bad_serverid;

  iohdl->buffered_iopage = xc_map_foreign_range (xch, domid, XC_PAGE_SIZE,
                                                 PROT_READ | PROT_WRITE,
                                                 pfn + 1);

  if (!iohdl->buffered_iopage)
    goto bad_bufshpage;

  for (i = 0; i < HVM_MAX_VCPUS; i++)
    {
      port = iohdl->shpage_ptr->vcpu_ioreq[i].vp_eport;
      if (!port)
        break;
      iohdl->ports[i] = xc_evtchn_bind_interdomain (iohdl->evtchn, domid,
                                                    port);
      if (iohdl->ports[i] == (unsigned int)-1)
        break;
    }
  iohdl->nports = i;

  iohdl->mapcache = mapcache_create(domid);
  if (!iohdl->mapcache)
    goto bad_mapcache;

  iohdl->domid = domid;
  LIST_HEAD_INIT (&iohdl->iorange_list);
  pthread_mutex_init (&iohdl->iorange_lock, NULL);

  return iohdl;

  bad_mapcache:
  for (i = 0; i < iohdl->nports; i++)
    xc_evtchn_unbind (iohdl->evtchn, iohdl->ports[i]);
  munmap (iohdl->buffered_iopage, XC_PAGE_SIZE);
  bad_bufshpage:
  munmap (iohdl->shpage_ptr, XC_PAGE_SIZE);
  bad_serverid:
  bad_fd:
  xc_evtchn_close (iohdl->evtchn);
  bad_evtchn:
  free (iohdl);

  return NULL;
}

EXTERNAL void
iohandle_destroy (iohandle_t iohdl)
{
  int i;
  struct iorange *ior, *tmp;
  struct s_pci *pci, *ptmp;

  if (!iohdl)
    return;

  pthread_mutex_lock (&iohdl->iorange_lock);
  LIST_FOREACH_SAFE (ior, tmp, &iohdl->iorange_list, link)
    {
      xc_hvm_unmap_io_range_from_ioreq_server (xch, iohdl->domid,
                                               iohdl->serverid,
                                               ior->mmio,
                                               ior->start);
      LIST_REMOVE (ior, link);
      free (ior);
    }
  pthread_mutex_unlock (&iohdl->iorange_lock);

  mapcache_destroy (iohdl->mapcache);

  for (i = 0; i < iohdl->nports; i++)
    xc_evtchn_unbind (iohdl->evtchn, iohdl->ports[i]);

  munmap (iohdl->shpage_ptr, XC_PAGE_SIZE);
  munmap (iohdl->buffered_iopage, XC_PAGE_SIZE);
  xc_evtchn_close (iohdl->evtchn);

  free (iohdl);
}
