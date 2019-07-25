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
#include "list.h"

#if defined(__i386__)
#define mb()  asm volatile ( "lock; addl $0,0(%%esp)" : : : "memory" )
#define rmb() asm volatile ( "lock; addl $0,0(%%esp)" : : : "memory" )
#define wmb() asm volatile ( "" : : : "memory")
#elif defined(__x86_64__)
#define mb()  asm volatile ( "mfence" : : : "memory")
#define rmb() asm volatile ( "lfence" : : : "memory")
#define wmb() asm volatile ( "" : : : "memory")
#else
#error "Unknow architecture"
#endif

#define XENFB_PAGE_SIZE 4096

/*
 * XenFB2 default framebuffer values.
 * Idealy this should match the xenfb2 values to avoid unecessary resizing.
 */
#define XENFB2_DEFAULT_VIDEORAM (16 * 1024 * 1024)
#define XENFB2_DEFAULT_WIDTH 1024
#define XENFB2_DEFAULT_HEIGHT 768
#define XENFB2_DEFAULT_BPP 32
/* It is good practice to keep this a multiple of 64 to satisfy DRM
 * requirements. */
#define XENFB2_DEFAULT_PITCH \
    (XENFB2_DEFAULT_WIDTH * XENFB2_DEFAULT_BPP / 8)
#define XENFB2_DEFAULT_OFFSET 0

static struct event backend_xenstore_event;

struct xenfb_device;

struct xenfb_framebuffer
{
  struct xenfb_device *dev;
  struct surface *s;
  xen_backend_t back;
  int devid;

  void *page;
  union xenfb2_in_event *evt_reply;

  unsigned long fb_npages;
  unsigned long *fb2m;
  unsigned long fb2m_size;

  uint8_t *dirty;
  struct timeval dirty_tv;

  struct event evtchn_event;

  int cache_attr;
};

struct xenfb_device
{
  struct device device;
  xen_backend_t backend;
  int domid;

#define MAX_DEVID 16
  struct xenfb_framebuffer *framebuffers[MAX_DEVID];

  /* For now, only the availability of each monitor this backend is aware of.
   * Index is a /monitor_id/ in display[] (so, the index). */
  int monitors[DISPLAY_MONITOR_MAX];
  /* XXX */
};

static unsigned int
xenfb_get_linesize (struct xenfb_framebuffer *fb, unsigned int xres,
                    unsigned int yres, unsigned int bpp)
{
  unsigned int linesize;
  unsigned int stride_align = plugin_stride_align();

  if (bpp != 32)
    return 0;

  linesize = xres * (bpp / 8);
  if (stride_align)
    linesize = (linesize + (stride_align - 1)) & ~(stride_align - 1);

  return linesize;
}

static enum surfman_surface_format
xenfb_get_surface_format (int bpp)
{
  int ret;

  switch (bpp)
    {
    case 32:
      return SURFMAN_FORMAT_BGRX8888;
    default:
      break;
    }

  return SURFMAN_FORMAT_UNKNOWN;
}

static void
xenfb_dirty_ready (struct xenfb_framebuffer *fb)
{
  struct surface *s = fb->s;

  /*
  ** Check that the surface timer still exists,
  ** and that the plugin requires an update.
  */
  if ( s && timerisset (&fb->dirty_tv) &&
      surface_need_refresh (s) )
    {
      surface_refresh (s, fb->dirty);

      if (!fb->dirty)
        {
          struct xenfb2_page *page = fb->page;

          if (MAP_FAILED == (fb->dirty = xc_mmap_foreign (NULL, XC_PAGE_SIZE, 
                                       PROT_READ, fb->dev->domid,
                                       &page->dirty_bitmap_page)))
            {
              surfman_error ("could not xc_mmap_foreign. resetting dirty ptr");
              fb->dirty = NULL;
            }
        }
    }
  timerclear(&fb->dirty_tv);
}

static void
xenfb_resize (struct xenfb_framebuffer *fb, struct xenfb2_mode *mode)
{
  unsigned int linesize;

  linesize = xenfb_get_linesize (fb, mode->xres, mode->yres, mode->bpp);
  if (linesize == 0)
    {
      return;
    }

  if (mode->xres == 0 || mode->yres == 0)
      surface_update_offset(fb->s, mode->offset);
  else
      surface_update_format (fb->s, mode->xres, mode->yres, linesize,
                             xenfb_get_surface_format (mode->bpp),
                             mode->offset);
}

static void
xenfb_check_mode (struct xenfb_framebuffer *fb, struct xenfb2_mode *mode)
{
  unsigned int linesize;

  fb->evt_reply = xcalloc (1, sizeof (union xenfb2_in_event));
  fb->evt_reply->type = XENFB2_TYPE_MODE_REPLY;

  linesize = xenfb_get_linesize (fb, mode->xres, mode->yres, mode->bpp);

  fb->evt_reply->mode_reply.pitch = linesize;
  fb->evt_reply->mode_reply.mode_ok = ! !linesize;
}

static void
xenfb_set_pages (struct xenfb_framebuffer *fb)
{
  surface_update_pfns (fb->s, fb->dev->domid, fb->fb2m, fb->fb2m, fb->fb_npages);
}

static void
xenfb_send_event (struct xenfb_framebuffer *fb, union xenfb2_in_event *event)
{
  uint32_t prod;
  struct xenfb2_page *page = fb->page;

  prod = page->in_prod;
  mb ();
  XENFB2_IN_RING_REF (page, prod) = *event;
  wmb ();
  page->in_prod = prod + 1;

  backend_evtchn_notify (fb->back, fb->devid);
}

static void
xenfb_handle_events (struct xenfb_framebuffer *fb)
{
  uint32_t prod, cons;
  struct xenfb2_page *page = fb->page;

  prod = page->out_prod;
  if (prod == page->out_cons)
    return;
  rmb ();
  for (cons = page->out_cons; cons != prod; cons++)
    {
      union xenfb2_out_event *event = &XENFB2_OUT_RING_REF (page, cons);

      switch (event->type)
        {
        case XENFB2_TYPE_MODE_REQUEST:
          xenfb_check_mode (fb, &event->mode);
          break;
        case XENFB2_TYPE_RESIZE:
          xenfb_resize (fb, &event->mode);
          break;
        case XENFB2_TYPE_DIRTY_READY:
          xenfb_dirty_ready (fb);
        default:
          break;
        }
    }
  mb ();
  page->out_cons = cons;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */

static xen_device_t
xenfb_alloc (xen_backend_t backend, int devid, void *priv)
{
  struct xenfb_device *dev = priv;
  struct xenfb_framebuffer *fb;

  if (devid >= MAX_DEVID)
    {
      surfman_error ("Can't allocate device ID %d, too high !", devid);
      return NULL;
    }

  fb = xcalloc (1, sizeof (*fb));

  fb->devid = devid;
  fb->dev = dev;
  fb->back = backend;
  timerclear(&fb->dirty_tv);

  fb->s = surface_create (&dev->device, fb);

  dev->framebuffers[devid] = fb;

  return fb;
}

static int
xenfb_init (xen_device_t xendev)
{
  struct xenfb_framebuffer *fb = xendev;

  backend_print (fb->back, fb->devid,
    "default-xres", "%u", XENFB2_DEFAULT_WIDTH);
  backend_print (fb->back, fb->devid,
    "default-yres", "%u", XENFB2_DEFAULT_HEIGHT);
  backend_print (fb->back, fb->devid,
    "default-bpp", "%u", XENFB2_DEFAULT_BPP);
  backend_print (fb->back, fb->devid,
    "default-pitch", "%u", XENFB2_DEFAULT_PITCH);
  backend_print (fb->back, fb->devid,
    "videoram", "%u", XENFB2_DEFAULT_VIDEORAM);

  surface_update_format (fb->s,
    XENFB2_DEFAULT_WIDTH,
    XENFB2_DEFAULT_HEIGHT,
    XENFB2_DEFAULT_PITCH,
    SURFMAN_FORMAT_BGRX8888,
    XENFB2_DEFAULT_OFFSET);

  return 0;
}

static void
xenfb_evtchn_handler (int fd, short event, void *priv)
{
    backend_evtchn_handler (priv);
}

static int
xenfb_connect (xen_device_t xendev)
{
  struct xenfb_framebuffer *fb = xendev;
  struct xenfb2_page *page;
  int fd;

  fd = backend_bind_evtchn (fb->back, fb->devid);
  if (fd < 0)
    return -1;

  event_set (&fb->evtchn_event, fd, EV_READ | EV_PERSIST,
             xenfb_evtchn_handler,
             backend_evtchn_priv (fb->back, fb->devid));
  event_add (&fb->evtchn_event, NULL);

  page = fb->page = backend_map_shared_page (fb->back, fb->devid);
  if (!page)
    return -1;

  fb->fb_npages = page->fb2m_nents;
  fb->fb2m_size =
    (page->fb2m_nents * sizeof (unsigned long) +
     (XENFB_PAGE_SIZE - 1)) & ~(XENFB_PAGE_SIZE - 1);

  if (MAP_FAILED == (fb->fb2m = xc_mmap_foreign (NULL, fb->fb2m_size,
                              PROT_READ | PROT_WRITE,
                              fb->dev->domid, page->fb2m)))
    {
      surfman_error ("xc_mmap_foreign failed!");
      fb->fb2m = NULL;
      return -1;
    }    

  if (MAP_FAILED == (fb->dirty = xc_mmap_foreign (NULL, XC_PAGE_SIZE,
                               PROT_READ, fb->dev->domid,
                               &page->dirty_bitmap_page)))
    {
      surfman_error ("xc_mmap_foreign failed!");
      fb->dirty = NULL;
    }    

  xenfb_set_pages(fb);

  if (domain_visible (fb->dev->device.d))
    {
      domain_set_visible (NULL, 0);
    }

  return 0;
}

static void
xenfb_framebuffer_cleanup (struct xenfb_framebuffer *fb)
{
  event_del (&fb->evtchn_event);

  if (fb->dirty)
    {
      munmap (fb->dirty, XC_PAGE_SIZE);
      fb->dirty = NULL;
    }
  if (fb->fb2m)
    {
      munmap (fb->fb2m, fb->fb2m_size);
      fb->fb2m_size = 0;
      fb->fb2m = NULL;
    }
  if (fb->page)
    {
      backend_unmap_shared_page (fb->back, fb->devid, fb->page);
      fb->page = NULL;
    }
}

static void
xenfb_disconnect (xen_device_t xendev)
{
  struct xenfb_framebuffer *fb = xendev;

  xenfb_framebuffer_cleanup (fb);
}

static void
xenfb_backend_changed (xen_device_t xendev, const char *node, const char *val)
{
  struct xenfb_framebuffer *fb = xendev;
}

static void
xenfb_frontend_changed (xen_device_t xendev,
                        const char *node, const char *val)
{
  struct xenfb_framebuffer *fb = xendev;
}

static void
xenfb_event (xen_device_t xendev)
{
  struct xenfb_framebuffer *fb = xendev;

  xenfb_handle_events (fb);
  backend_evtchn_notify (fb->back, fb->devid);

  if (fb->evt_reply)
    {
      xenfb_send_event (fb, fb->evt_reply);
      free (fb->evt_reply);
      fb->evt_reply = NULL;
    }
}

static void
xenfb_free (xen_device_t xendev)
{
  struct xenfb_framebuffer *fb = xendev;
  struct xenfb_device *dev = fb->dev;

  xenfb_framebuffer_cleanup (fb);

  dev->framebuffers[fb->devid] = NULL;
  surface_destroy (fb->s);
  free (fb);
}

static struct xen_backend_ops xenfb_backend_ops = {
  xenfb_alloc,
  xenfb_init,
  xenfb_connect,
  xenfb_disconnect,
  xenfb_backend_changed,
  xenfb_frontend_changed,
  xenfb_event,
  xenfb_free
};

static struct surface *
xenfb_get_surface (struct device *device, int monitor_id)
{
  struct xenfb_device *dev = (struct xenfb_device *)device;
  struct xenfb_framebuffer *fb = NULL;
  int i;

  if (monitor_id >= MAX_DEVID)
    return NULL;

  fb = dev->framebuffers[monitor_id];

  if (!fb)
    return NULL;

  return fb->s;
}

static void
xenfb_refresh_surface (struct device *device, struct surface *s)
{
  struct xenfb_framebuffer *fb = s->priv;
  struct xenfb2_update_dirty evt;
  struct timeval tv;

  gettimeofday (&tv, NULL);

  /* Check that the frontend is still connected */
  if (!fb->page)
    return;

  if (timerisset (&fb->dirty_tv))
    {
      struct timeval delay;

      timersub (&tv, &fb->dirty_tv, &delay);

      if (delay.tv_sec >= 10)
        {
          surfman_warning ("Frontend unresponsive, clearing timer and refreshing manually");
          surface_refresh (s, NULL);
          timerclear(&fb->dirty_tv);
        }

      return;
    }

  fb->dirty_tv = tv;
  evt.type = XENFB2_TYPE_UPDATE_DIRTY;
  xenfb_send_event (fb, (union xenfb2_in_event *)&evt);

  /*
   * We receive the dirty bitmap update asynchronously.
   */
}

static void
xenfb_monitor_update (struct device *device, int monitor_id, int enable)
{
  struct xenfb_device *dev = (struct xenfb_device *)device;

  /* XXX: Dumb to match the hack in domain.c:device_create. */
  dev->monitors[monitor_id] = enable;
  /* TODO: Update fb dimension xenstore node -- What? */
}

static int
xenfb_set_visible (struct device *device)
{
  struct xenfb_device *dev = (struct xenfb_device *)device;
  struct xenfb_framebuffer *fb = NULL;
  unsigned int i;

  /* Very naive. Just take the first xenfb device and
   * display it on the first monitor */

  for (i = 0; i < MAX_DEVID; i++)
    if (dev->framebuffers[i])
      {
        fb = dev->framebuffers[i];
        break;
      }

  if (!fb)
    {
      surfman_error ("nothing to display");
      return -1;
    }

  if (!fb->s || !surface_ready (fb->s))
    {
      for (i = 0; i < DISPLAY_MONITOR_MAX; ++i)
        if (dev->monitors[i])
          if (display_prepare_blank (i, device))
            surfman_warning ("Could not blank monitor %u.", i);
    }
  else
    {
      for (i = 0; i < DISPLAY_MONITOR_MAX; ++i)
        if (dev->monitors[i])
          if (display_prepare_surface (i, device, fb->s, NULL))
            surfman_warning ("Could not prepare surface %p on monitor %u.", fb->s, i);
    }
  return 0;
}

static void
xenfb_takedown (struct device *device)
{
  struct xenfb_device *dev = (struct xenfb_device *) device;

  surfman_info ("Taking down XenFB device: %d", dev->domid);

  backend_release (dev->backend);
  dev->backend = NULL;
}

static int
xenfb_is_active (struct device *device)
{
  struct xenfb_device *dev = (struct xenfb_device *) device;

  return (dev->backend != NULL);
}

static struct device_ops xenfb_device_ops = {
  .name = "xenfb",
  .refresh_surface = xenfb_refresh_surface,
  .monitor_update = xenfb_monitor_update,
  .set_visible = xenfb_set_visible,
  .takedown = xenfb_takedown,
  .get_surface = xenfb_get_surface,
  .is_active = xenfb_is_active,
};

struct device *
xenfb_device_create (struct domain *d)
{
  struct xenfb_device *dev;

  surfman_info ("Creating XenFB device: %d", d->domid);

  dev = device_create (d, &xenfb_device_ops, sizeof (*dev));
  if (!dev)
    return NULL;

  dev->domid = d->domid;
  dev->backend = backend_register ("vfb", d->domid, &xenfb_backend_ops, dev);
  if (!dev->backend)
    {
      device_destroy (&dev->device);
      return NULL;
    }

  return &dev->device;
}

static void
xenfb_xenstore_handler (int fd, short event, void *priv)
{
  backend_xenstore_handler (priv);
}

void
xenfb_backend_init (int dom0)
{
  int rc;

  rc = backend_init (dom0);
  if (rc)
    surfman_fatal ("Failed to initialize libxenbackend");

  event_set (&backend_xenstore_event,
             backend_xenstore_fd (),
             EV_READ | EV_PERSIST,
             xenfb_xenstore_handler,
             NULL);
  event_add (&backend_xenstore_event, NULL);
}
