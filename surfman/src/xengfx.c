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
#include <libvgaemu.h>
#include <sys/ioctl.h>
#include "xengfx.h"

#ifndef PCI_VENDOR_ID_XEN
# define PCI_VENDOR_ID_XEN 0x5853
#endif

#ifndef PCI_DEVICE_ID_XENGFX
# define PCI_DEVICE_ID_XENGFX 0xC147
#endif

#ifndef PCI_CLASS_DISPLAY_VGA
# define PCI_CLASS_DISPLAY_VGA 0x0300
#endif

#define VRAM_RESERVED_ADDRESS	0xfd000000

#define XENGFX_GART_NPAGES       64
#define XENGFX_GART_SIZE        (XENGFX_GART_NPAGES * XC_PAGE_SIZE)
#define XENGFX_GART_NENTS       (XENGFX_GART_SIZE / 4)

#define MMIO_BAR_SIZE           (round_up_power_of_2(XGFX_GART_OFFSET + \
                                                     XENGFX_GART_SIZE))
#define APERTURE_BAR_SIZE       (XENGFX_GART_NENTS * XC_PAGE_SIZE)
#define IOPORT_BAR_SIZE         8

#define APERTURE_BAR_ID         0
#define MMIO_BAR_ID             2
#define IOPORT_BAR_ID           4

#define XENGFX_MAX_VCRTCS 16

#define XENGFX_UPDATE_INTERVAL 110

/* Stole 16M */
#define STOLEN_SIZE (16 << 20)

#define DOMID(_dev) (_dev->device.d->domid)

// #define DEBUG

#ifdef DEBUG
# define XGFX_DBG(fmt, ...) surfman_info ("xengfx: "fmt, ## __VA_ARGS__)
#else
# define XGFX_DBG(fmt, ...)
#endif

#define XGFX_INFO(fmt, ...) surfman_info ("xengfx: "fmt, ## __VA_ARGS__)
#define XGFX_WARN(fmt, ...) surfman_warning ("xengfx: "fmt, ## __VA_ARGS__)
#define XGFX_ERR(fmt, ...) surfman_error ("xengfx: "fmt, ## __VA_ARGS__)


/* MMIO RAM pages map */
enum
{
  /* GART (64 pages ) */
  XENGFX_MMIO_RAM_GART = 0,
  /* BIOS Reserved 1 page */
  XENGFX_MMIO_RAM_BIOS = XENGFX_MMIO_RAM_GART + XENGFX_GART_NPAGES,
  /* EDID (1 page per CRTC) */
  XENGFX_MMIO_RAM_EDID,
  /* Posted Registers (1 page per CRTC) */
  XENGFX_MMIO_RAM_REGS = XENGFX_MMIO_RAM_EDID + XENGFX_MAX_VCRTCS,

  /* Number of pages of RAM that the device owns */
  XENGFX_MMIO_RAM_MAX = XENGFX_MMIO_RAM_REGS + XENGFX_MAX_VCRTCS,
};

struct mmio_ram_range
{
    int index;
    uint32_t offset;
    uint32_t length;
    uint32_t banks;
    uint32_t bank_len;
};

static const struct mmio_ram_range xgfx_mmio_map[] = {
    /* GART: 64 pages */
    {
        XENGFX_MMIO_RAM_GART,
        XGFX_GART_OFFSET,
        XENGFX_GART_NPAGES,
        1,
        0
    },
    /* BIOS Reserved: 1 page */
    {
        XENGFX_MMIO_RAM_BIOS,
        XGFX_BIOS_RESERVED,
        1,
        1,
        0
    },
    /* EDID: NR_VCRTCS * 1 page */
    {
        XENGFX_MMIO_RAM_EDID,
        XGFX_VCRTC_OFFSET + XGFX_VCRTC_EDID,
        1,
        XENGFX_MAX_VCRTCS,
        XGFX_VCRTC_BANK_SIZE
    },
    /* Posted Registers: NR_VCRTCS * 1 page */
    {
        XENGFX_MMIO_RAM_REGS,
        XGFX_VCRTC_OFFSET + XGFX_VCRTC_CONTROL,
        1,
        XENGFX_MAX_VCRTCS,
        XGFX_VCRTC_BANK_SIZE
    },
    { -1, 0, 0, 0, 0 }
};

#define MMIO_RAM_PTR(_x,_idx) \
    ((void *)((_x)->mmio_ram_ptr + ((_idx) * XC_PAGE_SIZE)))

struct xengfx_vcrtc
{
  int enabled;

  uint32_t status;
  uint32_t pending;
  uint32_t int_en;

  uint32_t edid_req;
  uint32_t lineoffset;

  int width, height, bpp;
  int linesize;

  uint32_t base;
  uint32_t surface_len;

  struct surface *s;
};

struct xengfx_device
{
  struct device device;

  int hires_enabled;

  uint8_t *stolen_ptr;
  uint64_t stolen_gmfn;

  int mmio_handle;
  uint64_t aperture_base;
  uint64_t mmio_base;
  uint64_t io_index;

  xen_pfn_t mmio_ram_pages[XENGFX_MMIO_RAM_MAX];
  uint8_t *mmio_ram_ptr;

  pfn_t *mfns;
  xen_pfn_t *pfns;

  struct xengfx_vcrtc crtcs[XENGFX_MAX_VCRTCS];
  uint32_t madvise;
  uint32_t control_flags;
  uint32_t isr;
  uint32_t stolen_clr;
  uint32_t *shadow_gart;

  libpciemu_handle_t iohandle;
  struct event io_event;
  struct event timer;
  struct timeval time;

  struct
  {
    s_vga vga;
    pfn_t *pages;
    uint32_t width;
    uint32_t height;
    uint32_t linesize;
  } vga;

  libpciemu_pci_t pci;
};

static inline uint64_t round_up_power_of_2(uint64_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

/*
** This function checks if we need to transition to or from vga
** and calls set_visible() if needed.
*/
static void update_visible_state (struct xengfx_device *dev)
{
  surfman_info ("update visible state\n");

  if (domain_visible (dev->device.d))
    {
      domain_set_visible (NULL, 0);
    }
}

static void *xengfx_map_gart (struct xengfx_device *dev, uint64_t g)
{
  xen_pfn_t gart_pfns[XENGFX_GART_NPAGES];
  //uint64_t g = (dev->mmio_base + 0x200000) >> XC_PAGE_SHIFT;
  int i;

  for (i = 0; i < XENGFX_GART_NPAGES; i++)
    {
      gart_pfns[i] = g + i;
    }

  return xc_map_foreign_batch (xch, DOMID(dev), PROT_READ | PROT_WRITE,
                               gart_pfns, XENGFX_GART_NPAGES);
}

static void
xengfx_update_pages (struct xengfx_device *dev,
                    int crtc_id)
{
  struct xengfx_vcrtc *crtc;
  int ready;

  if (crtc_id >= XENGFX_MAX_VCRTCS)
    return;

  crtc = &dev->crtcs[crtc_id];

  surfman_info ("update_pages for crtc %d", crtc_id);

  if (dev->hires_enabled || (!dev->hires_enabled && crtc_id != 0))
    surface_update_pfns (crtc->s, DOMID (dev),
                         dev->pfns + (crtc->base >> XC_PAGE_SHIFT),
                         dev->mfns + (crtc->base >> XC_PAGE_SHIFT),
                         (crtc->surface_len + (XC_PAGE_SIZE - 1)) >> XC_PAGE_SHIFT);
  else
    surface_update_lfb (crtc->s, DOMID (dev), dev->stolen_gmfn,
                        crtc->surface_len);
}

static int xengfx_display_resize (struct xengfx_device *dev,
                                  int crtc_id, uint32_t width, uint32_t height,
                                  uint32_t linesize, uint32_t format,
                                  uint32_t fb_offset)
{
  struct xengfx_vcrtc *crtc;
  uint32_t surface_len;
  int ready;

  crtc = &dev->crtcs[crtc_id];

  ready = surface_ready (crtc->s);

  surface_update_format (crtc->s, width, height, linesize, format, fb_offset);
  surface_len = height * linesize;

  crtc->surface_len = surface_len;
  xengfx_update_pages (dev, crtc_id);

  if (!ready && surface_ready (crtc->s))
    update_visible_state (dev);

  return 0;
}

static void
xengfx_display_get_edid (struct xengfx_device *dev, int id)
{
  struct monitor *m;
  int align;
  size_t edid_len;
  int rc = 0;
  uint8_t *edid = MMIO_RAM_PTR (dev, XENGFX_MMIO_RAM_EDID + id);

  surfman_info ("DisplayID:%d", id);

  m = display_get_monitor (id);
  if (!m)
    {
      XGFX_ERR ("Monitor %d unavailable", id);
      return;
    }

  if (!m->info)
    {
      XGFX_ERR ("Missing monitor info");
      return;
    }

  memset (edid, 0, 256);
  rc = display_get_edid (id, edid, 256);

  if (rc < 0)
    {
      XGFX_ERR ("Failed to get EDID");
      return;
    }

  if (!edid_valid (edid, 0))
    XGFX_WARN ("Invalid EDID");

  return;
}

static void xengfx_hires_enable (struct xengfx_device *dev, int enable)
{
  XGFX_INFO ("hires enable = %d", enable);

  if (dev->hires_enabled != enable)
    {
      dev->hires_enabled = enable;
      if (!dev->hires_enabled)
        {
          surfman_info ("VGA mode");
          xengfx_display_resize (dev, 0, dev->vga.width, dev->vga.height,
                                 dev->vga.linesize, SURFMAN_FORMAT_BGRX8888, 0);
        }
      else
        surfman_info ("Hires mode");
    }
}

static struct surface *
xengfx_get_surface (struct device *device, int monitor_id)
{
  struct xengfx_device *dev = (struct xengfx_device *)device;
  struct xengfx_vcrtc *crtc;

  if (monitor_id >= XENGFX_MAX_VCRTCS)
    return NULL;

  crtc = &dev->crtcs[monitor_id];

  if (crtc->enabled || (monitor_id == 0 && !dev->hires_enabled))
    return crtc->s;

  return NULL;
}

static void
xengfx_refresh_surface (struct device *device, struct surface *s)
{
  struct xengfx_device *dev = (struct xengfx_device *)device;

  if (dev->hires_enabled)
    {
      surface_refresh (s, NULL);
    }
  else
    {
      if (vga_need_refresh (dev->vga.vga))
        surface_refresh (s, NULL);
    }

  /* XXX: Implement me */
}

static void
xengfx_monitor_update (struct device *device, int monitor_id, int enable)
{
  struct xengfx_device *dev = (struct xengfx_device *)device;
  struct msg_display_info msg;
  struct monitor *m;

  surfman_info ("monitor_id:%d", monitor_id);

  msg.DisplayID = monitor_id;
  msg.max_xres = 0;
  msg.max_yres = 0;
  msg.align = 0;

  if (enable)
    {
      m = display_get_monitor (monitor_id);
      if (!m)
        {
          surfman_info ("Monitor %d unavailable", monitor_id);
          return;
        }

      msg.align = PLUGIN_GET_OPTION (m->plugin, stride_align);
      msg.max_xres = m->info->i.prefered_mode->htimings[0];
      msg.max_yres = m->info->i.prefered_mode->vtimings[0];
    }

  display_info (device->client, &msg, sizeof (msg));
}

static int
xengfx_set_visible (struct device *device)
{
  struct xengfx_device *dev = (struct xengfx_device *)device;
  struct xengfx_vcrtc *crtc;
  int i, total = 0;

  if (!dev->hires_enabled)
    {
      crtc = &dev->crtcs[0];
      return display_prepare_surface (0, device, crtc->s, NULL);
    }

  for (i = 0; i < XENGFX_MAX_VCRTCS; i++)
    {
      crtc = &dev->crtcs[i];

      surfman_info ("crtc %d enabled=%d", i, crtc->enabled);

      if (crtc->enabled)
        {
          total++;
          if (surface_ready (crtc->s))
          {
            surfman_info ("prepare surface");
            display_prepare_surface (i, device, crtc->s, NULL);
          }
          else
          {
            surfman_info ("prepare blank");
            display_prepare_blank (i, device);
          }
        }
    }

  return total ? 0 : -1;
}

static void
xengfx_takedown (struct device *device)
{
  struct xengfx_device *dev = (struct xengfx_device *)device;
  struct xengfx_vcrtc *crtc;
  int i;

  XGFX_DBG ("takedown");

  event_del (&dev->io_event);
  event_del (&dev->timer);
  libpciemu_handle_destroy (dev->iohandle);

  for (i = 0; i < XENGFX_MAX_VCRTCS; i++)
    surface_destroy (dev->crtcs[i].s);

  free (dev->mfns);
  free (dev->pfns);
  vga_destroy (dev->vga.vga);

  free (dev->shadow_gart);

  if (dev->stolen_ptr)
    munmap (dev->stolen_ptr, STOLEN_SIZE);
  if (dev->mmio_ram_ptr)
    munmap (dev->mmio_ram_ptr, XENGFX_MMIO_RAM_MAX * XC_PAGE_SIZE);

  /*
  ** HACK:
  **
  ** Help the domain to die correctly by decrementing the refcount of the
  ** pages mapped in the apperture
  */
  if (dev->aperture_base != PCI_BAR_UNMAPPED)
    xc_domain_aperture_map (xch, DOMID (dev), dev->aperture_base / XC_PAGE_SIZE,
                            NULL, XENGFX_GART_NENTS, 0);
}

static int
xengfx_is_active (struct device *device)
{
  struct xengfx_device *dev = (struct xengfx_device *)device;

  return 1;
}

static struct device_ops xengfx_device_ops = {
  .name = "xengfx",
  .refresh_surface = xengfx_refresh_surface,
  .monitor_update = xengfx_monitor_update,
  .set_visible = xengfx_set_visible,
  .takedown = xengfx_takedown,
  .get_surface = xengfx_get_surface,
  .is_active = xengfx_is_active
};


static void xengfx_clear_stolen (struct xengfx_device *dev)
{
  /* XXX: It would be nice to use the mapcache for this */
  xen_pfn_t *pfns;
  xen_pfn_t start = dev->stolen_gmfn >> XC_PAGE_SHIFT;
  uint32_t npages = STOLEN_SIZE >> XC_PAGE_SHIFT;
  uint32_t i;
  uint8_t *p;

  pfns = malloc (npages * sizeof (*pfns));
  if (!pfns)
    return;

  for (i = 0; i < npages; i++)
    pfns[i] = start + i;

  p = xc_map_foreign_pages (xch, DOMID (dev), PROT_READ | PROT_WRITE,
                            pfns, npages);

  if (!p)
    goto release;

  memset (p, 0, STOLEN_SIZE);
  munmap (p, STOLEN_SIZE);

release:
  free (pfns);
}

static void xengfx_hw_update (int fd, short event, void *opaque)
{
  struct xengfx_device *dev = opaque;

  if (dev->stolen_clr)
    {
      xengfx_clear_stolen (dev);
      dev->stolen_clr = 0;
    }

  if (dev->hires_enabled)
    {
      /* vblank interrupt */
      uint32_t i;
      int retrace = 0;

      for (i = 0; i < XENGFX_MAX_VCRTCS; i++)
        {
          struct xengfx_vcrtc *v = &dev->crtcs[i];
          uint32_t status;

          status = v->status | XGFX_VCRTC_STATUS_RETRACE;
          v->pending |= v->status ^ status;
          if (v->int_en & v->pending)
            retrace = 1;
        }

        if ((dev->control_flags & XGFX_CONTROL_INT_EN) && retrace)
          {
            dev->isr = 1;
            libpciemu_pci_interrupt_set (dev->pci, 1);
          }
    }
  else
    vga_update_display (dev->vga.vga);

  event_add (&dev->timer, &dev->time);
}

static int xengfx_format_to_bpp (uint32_t format)
{
  switch (format)
    {
    case XGFX_VCRTC_FORMAT_BGR555:
      return 15;
    case XGFX_VCRTC_FORMAT_BGR565:
      return 16;
    case XGFX_VCRTC_FORMAT_BGR888:
      return 24;
    case XGFX_VCRTC_FORMAT_BGRX8888:
      return 32;
    default:
      return -1;
    }
}

static int xengfx_format_to_surfman_format (uint32_t format)
{
  switch (format)
    {
    case XGFX_VCRTC_FORMAT_BGR565:
        return SURFMAN_FORMAT_BGR565;
    case XGFX_VCRTC_FORMAT_BGRX8888:
        return SURFMAN_FORMAT_BGRX8888;
    default:
        return SURFMAN_FORMAT_UNKNOWN;
    }
}

static void xengfx_lineoffset_post (struct xengfx_device *dev, int bank)
{
  struct xengfx_vcrtc *v = &dev->crtcs[bank];

  /* XXX: Nothing to do here, IO will be picked up by surfman */
}

static void xengfx_base_post (struct xengfx_device *dev, int bank)
{
  struct xengfx_vcrtc *v = &dev->crtcs[bank];
  int start, end, npages;
  int i;
  xen_pfn_t *pfns;
  struct xgfx_vcrtc_regs *regs =
    MMIO_RAM_PTR(dev, XENGFX_MMIO_RAM_REGS + bank);

  XGFX_DBG("bank=%d", bank);

  if (!v->enabled && regs->control & XGFX_VCRTC_CONTROL_ENABLE)
    {
      v->enabled = 1;
      update_visible_state (dev);
    }
  if (v->enabled && !(regs->control & XGFX_VCRTC_CONTROL_ENABLE))
    {
      v->enabled = 0;
      update_visible_state (dev);
      return;
    }

  /* Sanity checks */
  if ((regs->fmt & regs->valid_fmt) == 0)
    return; /* Unsupported framebuffer format */
  if (regs->hactive > regs->hmax)
    return; /* Horizontal size too big */
  if (regs->vactive > regs->vmax)
    return; /* Vertical size too big */
  if ((regs->stride & regs->stride_align) != 0)
    return; /* Line size not aligned */
  if ((v->base & regs->stride_align) != 0)
    return; /* start of FB not aligned */

  v->width = regs->hactive + 1;
  v->height = regs->vactive + 1;
  v->linesize = regs->stride;
  v->bpp = xengfx_format_to_bpp(regs->fmt);

  xengfx_display_resize (dev, bank, v->width, v->height, v->linesize,
                         xengfx_format_to_surfman_format (regs->fmt),
                         v->lineoffset);
}

static void xengfx_gart_inval (struct xengfx_device *dev)
{
  uint32_t i;
  uint64_t *pfns;
  xen_pfn_t *tpfns;
  int rc;
  void *gart = MMIO_RAM_PTR (dev, XENGFX_MMIO_RAM_GART);

  if (dev->aperture_base == PCI_BAR_UNMAPPED)
    return;

  XGFX_INFO ("gart_inval aperture = 0x%"PRIx64, dev->aperture_base);


  memcpy (dev->shadow_gart, gart, XENGFX_GART_SIZE);

  pfns = malloc (XENGFX_GART_NENTS * sizeof (*pfns));

  if (!pfns)
    return;

  tpfns = malloc (XENGFX_GART_NENTS * sizeof (*tpfns));

  if (!tpfns)
    goto inval_free_pfns;

  for (i = 0; i < XENGFX_GART_NENTS; i++)
    {
      /* Use the first page of stolen memory as a scratch page
       * for invalid gart entries.
       */
      if (!(dev->shadow_gart[i] & XGFX_GART_VALID_PFN))
        {
          pfns[i] = dev->stolen_gmfn >> XC_PAGE_SHIFT;
          tpfns[i] = dev->stolen_gmfn >> XC_PAGE_SHIFT;
        }
      else
        {
          pfns[i] = dev->shadow_gart[i] & ~XGFX_GART_VALID_PFN;
          tpfns[i] = dev->shadow_gart[i] & ~XGFX_GART_VALID_PFN;
        }
      dev->pfns[i] = pfns[i];
    }

  rc = xc_domain_aperture_map (xch, DOMID (dev),
                               dev->aperture_base >> XC_PAGE_SHIFT,
                               pfns, XENGFX_GART_NENTS, 1);

  if (rc)
    {
      XGFX_ERR ("xc_domain_aperture_map return %d, errno = %d", rc, errno);
      goto inval_free_tpfns;
    }

  rc = xc_domain_memory_translate_gpfn_list (xch, DOMID (dev), i, tpfns, tpfns);

  if (rc)
    {
      XGFX_ERR ("xc_domain_memory_translate_gpfn_list return %d errno = %d",
                rc, errno);
      goto inval_free_tpfns;
    }

  for (i = 0; i < XENGFX_GART_NENTS; i++)
     dev->mfns[i] = tpfns[i];

  rc = xc_domain_memory_release_mfn_list (xch, DOMID (dev), i, tpfns);

  if (rc)
    XGFX_ERR ("xc_domain_memory_release_mfn_list return %d errno = %d",
              rc, errno);
  for (i = 0; i < XENGFX_MAX_VCRTCS; i++)
    xengfx_update_pages (dev, i);

inval_free_tpfns:
  free (tpfns);
inval_free_pfns:
  free (pfns);
}

static void xengfx_vcrtc_enable (struct xengfx_device *dev, int bank,
                                 int enable)
{
    struct xengfx_vcrtc *v = &dev->crtcs[bank];
    uint32_t status;

    XGFX_DBG("bank=%d, enable=%d", bank, enable);

    if (enable)
      status = v->status | (XGFX_VCRTC_STATUS_HOTPLUG |
                            XGFX_VCRTC_STATUS_ONSCREEN);
    else
      status = v->status & ~(XGFX_VCRTC_STATUS_HOTPLUG |
                             XGFX_VCRTC_STATUS_ONSCREEN);

    v->pending |= v->status ^ status;
    v->status = status;
    /* Generate interrupt */
    if ((dev->control_flags & XGFX_CONTROL_INT_EN) &&
        (v->int_en & v->pending))
      {
        dev->isr = 1;
        libpciemu_pci_interrupt_set(dev->pci, 1);
      }
}

static uint32_t xengfx_vcrtc_read (struct xengfx_device *x, int bank,
                                   uint64_t addr)
{
  struct xengfx_vcrtc *v = &x->crtcs[bank];

  XGFX_DBG("bank=%d, addr=0x%"PRIx64, bank, addr);

  switch (addr)
    {
    case XGFX_VCRTC_STATUS:
      return v->status;
    case XGFX_VCRTC_STATUS_CHANGE:
      return v->pending;
    case XGFX_VCRTC_STATUS_INT:
      return v->int_en;

    /* XXX: Not Implemented */
    case XGFX_VCRTC_SCANLINE:
      return (uint32_t)-1;
    case XGFX_VCRTC_CURSOR_STATUS:
      return 0;
    case XGFX_VCRTC_CURSOR_CONTROL:
      return 0;
    case XGFX_VCRTC_CURSOR_MAXSIZE:
      return 0;
    case XGFX_VCRTC_CURSOR_SIZE:
      return 0;
    case XGFX_VCRTC_CURSOR_BASE:
      return 0;
    case XGFX_VCRTC_CURSOR_POS:
      return 0;

    case XGFX_VCRTC_EDID_REQUEST:
      return !!v->edid_req;

    case XGFX_VCRTC_BASE:
      return v->base;

    case XGFX_VCRTC_LINEOFFSET:
      return v->lineoffset;
    }
  return (uint32_t) -1;
}

static int xengfx_relocate_mmio_ram (struct xengfx_device *dev,
                                     uint64_t new_base)
{
  xen_pfn_t *pages;
  uint32_t i, j;
  const struct mmio_ram_range *r = xgfx_mmio_map;
  int rc;

  pages = malloc (XENGFX_MMIO_RAM_MAX * sizeof (xen_pfn_t));
  if (!pages)
    return -1;

  while (r->length)
    {
      for (i = 0; i < r->banks; i++)
        {
          for (j = 0; j < r->length; j++)
            pages[r->index + i * r->length + j] =
              ((new_base + r->offset + i * r->bank_len) >> XC_PAGE_SHIFT) + j;
        }
      r++;
    }

  for (i = 0; i < XENGFX_MMIO_RAM_MAX; i++)
    {
      rc = xc_domain_add_to_physmap (xch, DOMID(dev), XENMAPSPACE_gmfn,
                                     dev->mmio_ram_pages[i], pages[i]);
      if (rc)
        goto release;
      dev->mmio_ram_pages[i] = pages[i];
    }

  if (dev->mmio_ram_ptr)
    munmap (dev->mmio_ram_ptr, XENGFX_MMIO_RAM_MAX * XC_PAGE_SIZE);
  dev->mmio_ram_ptr = xc_map_foreign_pages (xch, DOMID(dev),
                                            PROT_READ | PROT_WRITE,
                                            pages, XENGFX_MMIO_RAM_MAX);
  if (!dev->mmio_ram_ptr)
    rc = -1;
release:
  free (pages);
  return rc;
}

static void xengfx_bar_update (uint8_t region, uint64_t addr, void *priv)
{
  struct xengfx_device *dev = priv;
  uint64_t old_base;

  /* FIXME: for the moment we ignore when we unmapped the BAR */
  if (addr == PCI_BAR_UNMAPPED)
    return;

  if (region == APERTURE_BAR_ID)
    {
      old_base = dev->aperture_base;

      XGFX_DBG("aperture old_addr=0x%"PRIx64" 0x%"PRIx64,
               dev->aperture_base, addr);

      if (old_base == addr)
        return;

      dev->aperture_base = addr;

      if (old_base != PCI_BAR_UNMAPPED)
        xc_domain_aperture_map (xch, DOMID(dev), old_base >> XC_PAGE_SHIFT,
                               NULL, XENGFX_GART_NENTS, 0);
      xengfx_gart_inval (dev);
    }
  else if (region == MMIO_BAR_ID)
    {
      old_base = dev->mmio_base;

      XGFX_DBG("mmio old_addr=0x%"PRIx64" 0x%"PRIx64,
               dev->mmio_base, addr);

      if (old_base != addr)
        xengfx_relocate_mmio_ram (dev, addr);

      dev->mmio_base = addr;
   }
}

static uint64_t xengfx_aperture_read (uint64_t addr, uint32_t size, void *priv)
{
  /* Never used: the IO is trapped by xen */
  XGFX_WARN("/!\\ function is normally never called");
  return 0xff;
}

static void xengfx_aperture_write (uint64_t addr, uint64_t data, uint32_t size,
                                   void *priv)
{
  XGFX_WARN("/!\\ function is normally never called");
  /* Never used: the IO is trapped by xen */
}

static void xengfx_reset_vcrtc (struct xengfx_device *dev, int id)
{
  struct xengfx_vcrtc *v = &dev->crtcs[id];
  void *edid = MMIO_RAM_PTR(dev, XENGFX_MMIO_RAM_EDID + id);
  struct xgfx_vcrtc_regs *regs = MMIO_RAM_PTR(dev, XENGFX_MMIO_RAM_REGS + id);
  struct monitor *m = display_get_monitor (id);
  uint32_t max_xres, max_yres, align;

  v->status = 0;
  v->pending = 0;
  v->base = 0;
  v->lineoffset = 0;
  v->edid_req = 0;
  v->enabled = 0;
  v->width = 0;
  v->height = 0;
  v->bpp = 0;
  v->linesize = 0;

  memset(edid, 0, 4096);
  memset(regs, 0, 4096);

  if (!m || !m->info)
    xengfx_vcrtc_enable (dev, id, 0);
  else
    {
      align = PLUGIN_GET_OPTION (m->plugin, stride_align);
      max_xres = m->info->i.prefered_mode->htimings[0];
      max_yres = m->info->i.prefered_mode->vtimings[0];

      XGFX_INFO ("Monitor %d: Max resolution %dx%d, stride alignment: %d",
                 id, max_xres, max_yres, align);

      regs->valid_fmt = XGFX_VCRTC_FORMAT_BGR565 |
                          XGFX_VCRTC_FORMAT_BGRX8888;

      if (max_xres)
        regs->hmax = max_xres - 1;
      else
        regs->hmax = 4095;

      if (max_yres)
        regs->vmax = max_yres - 1;
      else
        regs->vmax = 4095;

      if (align)
       regs->stride_align = align - 1;
      else
        regs->stride_align = 0;

      xengfx_vcrtc_enable(dev, id, 1);
    }
}

static void xengfx_reset (struct xengfx_device *dev)
{
  uint32_t i;
  uint32_t start;
  uint32_t npages;
  uint32_t *gart = MMIO_RAM_PTR(dev, XENGFX_MMIO_RAM_GART);

  XGFX_DBG("");

  /* FIXME: vga reset */

  dev->control_flags = 0;
  xengfx_hires_enable (dev, 0);

  for (i = 0; i < XENGFX_MAX_VCRTCS; i++)
    xengfx_reset_vcrtc (dev, i);

  /* Clear interrupts */
  dev->isr = 0;
  libpciemu_pci_interrupt_set (dev->pci, 0);

  start = dev->stolen_gmfn >> XC_PAGE_SHIFT;
  npages = STOLEN_SIZE >> XC_PAGE_SHIFT;

  for (i = 0; i < npages; i++)
    gart[i] = 0x80000000 | (start + i);

  for (; i < XENGFX_GART_NENTS; i++)
     gart[i] = 0x0;

  xengfx_gart_inval (dev);
}

static uint64_t xengfx_mmio_read (uint64_t addr, uint32_t size, void *priv)
{
  struct xengfx_device *dev = priv;

  XGFX_DBG("addr=%llx, size=%d", addr, size);

  if (size != 4)
    return (uint32_t) -1;

  switch (addr)
    {
    case XGFX_MAGIC:
      return XGFX_MAGIC_VALUE;
    case XGFX_REV:
      return XGFX_CURRENT_REV;
    case XGFX_CONTROL:
      XGFX_DBG("get control flags = %x", dev->control_flags);
      return dev->control_flags;
    case XGFX_ISR:
      return dev->isr;
    case XGFX_GART_SIZE:
      return XENGFX_GART_NPAGES;
    case XGFX_INVALIDATE_GART:
      xengfx_gart_inval (dev);
      return 0;
    case XGFX_STOLEN_BASE:
      return dev->stolen_gmfn >> XC_PAGE_SHIFT;
    case XGFX_STOLEN_SIZE:
      return STOLEN_SIZE >> XC_PAGE_SHIFT;
    case XGFX_STOLEN_CLEAR:
      return !!dev->stolen_clr;
    case XGFX_NVCRTC:
      return XENGFX_MAX_VCRTCS;
    case XGFX_RESET:
      xengfx_reset (dev);
      return 0;
    }

  if (addr >= XGFX_VCRTCN_BANK_OFFSET(0) &&
      addr < XGFX_VCRTCN_BANK_OFFSET(XENGFX_MAX_VCRTCS))
    {
      addr -= XGFX_VCRTC_OFFSET;

      return xengfx_vcrtc_read (dev, addr >> XGFX_VCRTC_BANK_SHIFT,
                                addr & XGFX_VCRTC_BANK_MASK);
    }

  return (uint32_t) -1;
}

static void xengfx_vcrtc_write (struct xengfx_device *dev, int bank,
                                uint64_t addr, uint32_t val)
{
  struct xengfx_vcrtc *v = &dev->crtcs[bank];

  XGFX_DBG("bank=%d, addr=%"PRIx64", val=%d", bank, addr, val);

  switch (addr)
    {
    case XGFX_VCRTC_STATUS_CHANGE:
      v->pending &= ~val;
      return;
    case XGFX_VCRTC_STATUS_INT:
      v->int_en = val;
      return;
#if 0 /* Not Implemented */
    case XGFX_VCRTC_CURSOR_CONTROL:
    case XGFX_VCRTC_CURSOR_SIZE:
    case XGFX_VCRTC_CURSOR_BASE:
    case XGFX_VCRTC_CURSOR_POS:
#endif
    case XGFX_VCRTC_EDID_REQUEST:
      if (val & 0x1)
        xengfx_display_get_edid (dev, bank);
      return;
    case XGFX_VCRTC_BASE:
      v->base = val;
      xengfx_base_post (dev, bank);
      return;
    case XGFX_VCRTC_LINEOFFSET:
      v->lineoffset = val;
      xengfx_lineoffset_post (dev, bank);
    }
}

static void xengfx_mmio_write (uint64_t addr, uint64_t data, uint32_t size,
                               void *priv)
{
  struct xengfx_device *dev = priv;

  XGFX_DBG("addr=0x%"PRIx64", data=0x%"PRIx64", size=%u", addr, data, size);

  if (size != 4)
    return;

  switch (addr)
    {
    case XGFX_CONTROL:
      XGFX_DBG("set control flags = 0x%"PRIx64, data);
      dev->control_flags = data;
      xengfx_hires_enable (dev, data & XGFX_CONTROL_HIRES_EN);
      return;
    case XGFX_ISR:
      dev->isr &= ~data;
      if (data & XGFX_ISR_INT)
        libpciemu_pci_interrupt_set (dev->pci, 0);
      return;
    case XGFX_STOLEN_CLEAR:
      if (data & 0x1)
        {
          dev->stolen_clr++;
          /* Will clear asynchronously on next vblank */;
        }
      return;
    case XGFX_MADVISE:
      dev->madvise = data;
      return;
    }

  if (addr >= XGFX_VCRTCN_BANK_OFFSET(0) &&
      addr < XGFX_VCRTCN_BANK_OFFSET(XENGFX_MAX_VCRTCS))
    {
      addr -= XGFX_VCRTC_OFFSET;

       xengfx_vcrtc_write (dev, addr >> XGFX_VCRTC_BANK_SHIFT,
                           addr & XGFX_VCRTC_BANK_MASK, data);
    }
}

static void *xengfx_get_mmio_ram (struct xengfx_device *dev, uint64_t addr)
{
  uint32_t i;
  const struct mmio_ram_range *r = xgfx_mmio_map;

  while (r->length)
    {
      for (i = 0; i < r->banks; i++)
        {
          uint64_t s = r->offset + i * r->bank_len;
          uint64_t e = s + r->length * XC_PAGE_SIZE;

          if (addr >= s && addr < e)
            return dev->mmio_ram_ptr + r->index * XC_PAGE_SIZE + (addr - s);
        }
      r++;
    }

  return NULL;
}

static uint64_t xengfx_ioport_read (uint64_t addr, uint32_t size, void *priv)
{
  struct xengfx_device *dev = priv;
  void *p;

  XGFX_DBG("addr=0x%"PRIx64", size=%u", addr, size);

  switch (addr)
    {
    case 0:
      if (size != 4)
        goto invalid;
      return dev->io_index;
    case 4:
      /* Handle MMIO RAM access */
      p = xengfx_get_mmio_ram(dev, dev->io_index);
      if (p) {
        uint32_t ret;

        memcpy(&ret, p, size);
        return ret;
      }
      if (size != 4)
        goto invalid;
      return xengfx_mmio_read(dev->io_index, 4, priv);
    }

invalid:
    return (uint32_t)-1;
}

static void xengfx_ioport_write (uint64_t addr, uint64_t data, uint32_t size,
                                 void *priv)
{
  struct xengfx_device *dev = priv;
  void *p;

  XGFX_DBG("addr=0x%"PRIx64", data=%"PRIx64", size=%u", addr, data, size);

  switch (addr)
    {
    case 0:
      if (size != 4)
        return;
      dev->io_index = data;
      break;
    case 4:
      /* Handle MMIO RAM access */
      p = xengfx_get_mmio_ram(dev, dev->io_index);
      if (p)
        {
          memcpy(p, &data, size);
          return;
        }
      if (size != 4)
        return;
      xengfx_mmio_write (dev->io_index, data, 4, priv);
      break;
    }
}

#define DEFINE_OPS(name)                                        \
static const libpciemu_io_ops_t xengfx_##name##_ops = {         \
  .read = xengfx_##name##_read,                                 \
  .write = xengfx_##name##_write,                               \
}

DEFINE_OPS(aperture);
DEFINE_OPS(mmio);
DEFINE_OPS(ioport);

#define DEFINE_VGA_OPS(prefix)                                  \
static uint64_t xengfx_vga_##prefix##_read (uint64_t addr,      \
                                            uint32_t size,      \
                                            void *priv)         \
{                                                               \
    return vga_##prefix##_read (priv, addr, size);              \
}                                                               \
                                                                \
static void xengfx_vga_##prefix##_write (uint64_t addr,         \
                                         uint64_t data,         \
                                         uint32_t size,         \
                                         void *priv)            \
{                                                               \
    vga_##prefix##_write (priv, addr, data, size);              \
}                                                               \
                                                                \
static const libpciemu_io_ops_t xengfx_vga_##prefix##_ops = {   \
    .read = xengfx_vga_##prefix##_read,                         \
    .write = xengfx_vga_##prefix##_write,                       \
};                                                              \

DEFINE_VGA_OPS(ioport)
DEFINE_VGA_OPS(mem)

static void xengfx_io (int fd, short event, void *opaque)
{
  struct xengfx_device *dev = opaque;

  libpciemu_handle (dev->iohandle);
}

static void *xengfx_vga_resize (uint32_t width, uint32_t height,
                                uint32_t linesize, void *priv)
{
  struct xengfx_device *dev = priv;
  xen_pfn_t *pfns;
  xen_pfn_t start = dev->stolen_gmfn >> XC_PAGE_SHIFT;
  uint32_t npages;
  uint32_t i;
  uint32_t size;
  int rc;

  XGFX_INFO ("vga resize width = %u height = %u linesize = %u",
             width, height, linesize);

  dev->vga.width = width;
  dev->vga.height = height;
  dev->vga.linesize = linesize;

  size = linesize * height * 4;
  size = (size + XC_PAGE_SIZE - 1) & XC_PAGE_MASK;
  npages = size >> XC_PAGE_SHIFT;

  if (!dev->stolen_ptr)
    {
      XGFX_ERR ("Stolen memory is not mapped");
      return NULL;
    }

  if (size > STOLEN_SIZE)
    {
      XGFX_ERR ("Not enough memory to allocate the new buffer");
      return NULL;
    }

  pfns = malloc (npages * sizeof (*pfns));

  if (!pfns)
    return NULL;

  for (i = 0; i < npages; i++)
    pfns[i] = start + i;

  rc = xc_domain_memory_translate_gpfn_list (xch, DOMID (dev), i, pfns, pfns);

  if (rc)
    {
      XGFX_ERR ("xc_domain_memory_translate_gpfn_list return %d errno = %d",
                rc, errno);
      goto inval_resize;
    }

  for (i = 0; i < npages; i++)
      dev->vga.pages[i] = pfns[i];

  rc = xc_domain_memory_release_mfn_list (xch, DOMID (dev), i, pfns);

  memset (dev->stolen_ptr, 0, size);

  xengfx_display_resize (dev, 0, width, height, linesize,
                         SURFMAN_FORMAT_BGRX8888, 0);

  return dev->stolen_ptr;

inval_resize:
  free (pfns);
  return NULL;
}

static int xengfx_vga_init (struct xengfx_device *dev)
{
  const s_vga_range *ranges;
  void *buffer;

  dev->vga.pages = calloc (STOLEN_SIZE >> XC_PAGE_SHIFT, sizeof (*dev->vga.pages));

  if (!dev->vga.pages)
    {
      XGFX_ERR ("unable to allocate pages");
      return -1;
    }

  /* fix stride align */
  if (!(buffer = xengfx_vga_resize (640, 480, 640 * 4, dev)))
      goto vga_close;

  if (!(dev->vga.vga = vga_init (buffer, xengfx_vga_resize,
                                 plugin_stride_align (), dev)))
    {
      surfman_error ("unable to initialize VGA");
      goto vga_close;
    }

  surfman_info ("vga = %p", dev->vga.vga);

  ranges = vga_ranges_get ();

  while (ranges->start || ranges->length)
    {
      const libpciemu_io_ops_t *ops;

      ops = (ranges->is_mmio) ? &xengfx_vga_mem_ops : &xengfx_vga_ioport_ops;

      libpciemu_handle_add_iorange (dev->iohandle, ranges->start,
                                    ranges->length, ranges->is_mmio,
                                    ops, dev->vga.vga);
      ranges++;
    }

  return 0;
vga_close:
  free (dev->vga.pages);
  return -1;
}

static int xengfx_mmio_ram_populate (struct xengfx_device *dev, uint64_t addr)
{
  int rc, i;
  xen_pfn_t *pages;

  XGFX_DBG("Populating MMIO RAM pages at 0x%"PRIx64, addr);

  pages = malloc (XENGFX_MMIO_RAM_MAX * sizeof (*pages));
  if (!pages)
    return -1;

  for (i = 0; i < XENGFX_MMIO_RAM_MAX; i++)
    {
      pages[i] = dev->mmio_ram_pages[i] = (addr >> XC_PAGE_SHIFT) + i;
    }

  rc = xc_domain_populate_physmap_exact (xch, DOMID(dev), i, 0, 0, pages);
  if (rc)
    {
      XGFX_DBG("");
      free (pages);
      return rc;
    }

  dev->mmio_ram_ptr = xc_map_foreign_pages (xch, DOMID(dev),
                                            PROT_READ | PROT_WRITE,
                                            pages, XENGFX_MMIO_RAM_MAX);
  free (pages);

  if (!dev->mmio_ram_ptr)
    {
      XGFX_DBG("");
      return -1;
    }

  return 0;
}

static int xengfx_stolen_ram_populate (struct xengfx_device *dev,
                                       uint64_t addr)
{
  unsigned long nr_pfn;
  xen_pfn_t *pfn_list;
  unsigned long i;
  int rc;

  XGFX_DBG("Populating stolen RAM pages at 0x%"PRIx64, addr);

  nr_pfn = STOLEN_SIZE >> XC_PAGE_SHIFT;

  pfn_list = malloc (sizeof(*pfn_list) * nr_pfn);

  if (!pfn_list)
    return -1;

  for (i = 0; i < nr_pfn; i++)
    pfn_list[i] = (addr >> XC_PAGE_SHIFT) + i;

  rc = xc_domain_populate_physmap_exact (xch, DOMID(dev), nr_pfn, 0, 0, pfn_list);

  if (rc)
    {
      XGFX_DBG("");
      free (pfn_list);
      return -1;
    }

  dev->stolen_ptr = xc_map_foreign_batch_cacheattr (xch, DOMID(dev),
                                                    PROT_READ | PROT_WRITE,
                                                    pfn_list, nr_pfn,
                                                    XC_MAP_CACHEATTR_WC);

  free (pfn_list);

  if (!dev->stolen_ptr)
    return -1;

  return 0;
}

static int xengfx_pci_init (struct xengfx_device *dev)
{
  libpciemu_pci_info_t info;

  memset (&info, 0, sizeof (info));

  info.vendor_id = PCI_VENDOR_ID_XEN;
  info.device_id = PCI_DEVICE_ID_XENGFX;
  info.subvendor_id = PCI_VENDOR_ID_XEN;
  info.subdevice_id = PCI_DEVICE_ID_XENGFX;
  /* Use 00:02.0 slot */
  info.device = 2;
  info.class = PCI_CLASS_DISPLAY_VGA;
  info.revision = 1;
  info.bar_update = xengfx_bar_update;

  if (!(dev->shadow_gart = malloc (XENGFX_GART_SIZE)))
    goto error_shadow;

  if (xengfx_stolen_ram_populate (dev, VRAM_RESERVED_ADDRESS))
    goto error_stolen;

  dev->aperture_base = PCI_BAR_UNMAPPED;
  dev->mmio_base = PCI_BAR_UNMAPPED;
  dev->stolen_gmfn = VRAM_RESERVED_ADDRESS;

  if (xengfx_mmio_ram_populate (dev, VRAM_RESERVED_ADDRESS + STOLEN_SIZE))
    goto error_mmio;

  dev->pci = libpciemu_pci_device_init (&info, dev);

  if (!dev->pci)
    goto error_pci_init;

  libpciemu_pci_register_bar (dev->pci, APERTURE_BAR_ID, 1,
                              PCI_BAR_TYPE_PREFETCH, APERTURE_BAR_SIZE,
                              &xengfx_aperture_ops);
  libpciemu_pci_register_bar (dev->pci, MMIO_BAR_ID, 1, 0,
                              MMIO_BAR_SIZE, &xengfx_mmio_ops);
  libpciemu_pci_register_bar (dev->pci, IOPORT_BAR_ID, 0, 0, IOPORT_BAR_SIZE,
                              &xengfx_ioport_ops);

  /* INT#A */
  libpciemu_pci_set_interrupt_pin (dev->pci, 1);

  if (libpciemu_pci_device_register (dev->iohandle, dev->pci))
    goto error_pci_register;

  return 0;

error_pci_register:
  libpciemu_pci_device_release (dev->pci);
error_pci_init:
  munmap (dev->mmio_ram_ptr, XENGFX_MMIO_RAM_MAX);
error_mmio:
  munmap (dev->stolen_ptr, STOLEN_SIZE);
error_stolen:
  free(dev->shadow_gart);
error_shadow:
  return 1;
}

struct device *
xengfx_device_create (struct domain *d, struct dmbus_rpc_ops **ops)
{
  struct xengfx_device *dev;
  int i;

  surfman_info ("Creating XenGFX device %d", d->domid);

  dev = device_create (d, &xengfx_device_ops, sizeof (*dev));
  if (!dev)
    return NULL;

  dev->iohandle = libpciemu_handle_create (d->domid);

  if (!dev->iohandle)
      goto error_iohandle;

  if (xengfx_pci_init (dev))
      goto error_pci;

  event_set (&dev->io_event, libpciemu_handle_get_fd (dev->iohandle),
             EV_READ | EV_PERSIST, xengfx_io, dev);
  event_add (&dev->io_event, NULL);

  *ops = NULL;

  dev->mfns = calloc (XENGFX_GART_NENTS, sizeof (*dev->mfns));
  dev->pfns = calloc (XENGFX_GART_NENTS, sizeof (*dev->pfns));

  for (i = 0; i < XENGFX_MAX_VCRTCS; i++)
    dev->crtcs[i].s = surface_create (&dev->device, &dev->crtcs[i]);

  if (xengfx_vga_init (dev))
      goto error_vga;

  xengfx_reset (dev);

  /* Enable update timer */
  event_set (&dev->timer, -1, EV_TIMEOUT | EV_PERSIST, xengfx_hw_update,
             dev);
  dev->time.tv_sec = 0;
  dev->time.tv_usec = XENGFX_UPDATE_INTERVAL * 1000;
  event_add (&dev->timer, &dev->time);

  return &dev->device;

error_vga:
  for (i = 0; i < XENGFX_MAX_VCRTCS; i++)
    {
       if (dev->crtcs[i].s)
         surface_destroy (dev->crtcs[i].s);
    }
error_pci:
  libpciemu_handle_destroy (dev->iohandle);
error_iohandle:
  device_destroy (&dev->device);
  return NULL;
}

