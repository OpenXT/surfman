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

#include <sys/un.h>
#include <sys/socket.h>

static LIST_HEAD (, struct vgpu)
  vgpu_list = LIST_HEAD_INITIALIZER;

/* Blanker define */
#define BLANKER_TIMEOUT 3000     /* In milliseconds */
#define BLANKER_PING    1000    /* In milliseconds */

enum blanker_state
{
  BLANKER_IDLE = 0,
  BLANKER_OP_START = 1,
  BLANKER_OP_COMPLETE = 2,
  BLANKER_OP_ERROR = 3,
};

void
vgpu_update_plugin_vmonitors (struct plugin *p)
{
  struct vgpu *vgpu;

  LIST_FOREACH (vgpu, &vgpu_list, link)
    {
      if (vgpu->plugin == p)
        vgpu_update_vmonitors (vgpu);
    }
}

void
vgpu_update_vmonitors (struct vgpu *vgpu)
{
  struct plugin *plugin = vgpu->plugin;
  int i;

  for (i = 0; i < plugin->monitor_count; i++)
    {
      struct monitor_info *minfo;
      int monitor_changed = 0;

      minfo = get_monitor_info (plugin, plugin->monitors[i]);
      if (!minfo)
        {
          surfman_error ("%s: get_monitor_info() failed.", plugin->name);
          return;
        }

      if (vgpu->minfos[i])
        {
          monitor_changed = !!memcmp (vgpu->minfos[i], minfo,
                                      sizeof (*minfo));

          if (monitor_changed)
            {
              VGPU_CALL (vgpu, free_vmonitor, vgpu->vmonitors[i]);
              free (vgpu->minfos[i]);
              vgpu->minfos[i] = NULL;
            }
        }

      if (!vgpu->minfos[i])
        {
          vgpu->minfos[i] = minfo;
          vgpu->vmonitors[i] = VGPU_CALL (vgpu, new_vmonitor, &minfo->i, &minfo->edid);
        }
      else
        free (minfo);
    }

  for (i = plugin->monitor_count; i < vgpu->vmonitor_count; i++)
    {
      if (vgpu->minfos[i])
        {
          VGPU_CALL (vgpu, free_vmonitor, vgpu->vmonitors[i]);
          free (vgpu->minfos[i]);
          vgpu->minfos[i] = NULL;
        }
    }

  vgpu->vmonitor_count = plugin->monitor_count;
}

static void
vgpu_blanker_write (int domid, enum blanker_state op)
{
  xenstore_dom_write_int (domid, op, "switcher/enter");
}

static enum blanker_state
vgpu_blanker_read (int domid)
{
  char *val;
  int i;

  val = xenstore_dom_read (domid, "switcher/enter");

  if (!val)
    return BLANKER_OP_ERROR;

  i = atoi (val);
  free (val);

  return i;
}

/* Return -1 if the blanker ping as timeout else 0 */
static int vgpu_blanker_timeout(struct vgpu *vgpu)
{
  time_t t = time (NULL);

  if ((vgpu->blanker_alive <= t) &&
      ((t - vgpu->blanker_alive) >= (BLANKER_TIMEOUT / 1000)))
      return -1;

  return 0;
}

/**
 * The blanker is divided in 2 functions:
 * - vgpu_blanker_ping: ask the blanker if it's still alive and setup a watch
 *   on the answer node;
 * - vgpu_blanker_alive: watch as fired, check if the value is correct.
 *
 * The former code (in igfx) was a loop. If this solution is imported
 * unchanged surfman is unable to handle I/O during few seconds. So
 * guests could block.
 */

/* Watcher on switcher/enter node */
static void vgpu_blanker_alive(const char *path, void *opaque)
{
  struct vgpu *vgpu = opaque;
  enum blanker_state s;
  struct timeval t;

  s = vgpu_blanker_read (vgpu->domid);

  if (s != BLANKER_OP_COMPLETE)
    return;

  /* Blanker is alived */
  vgpu->blanker_alive = time (NULL);

  xenstore_watch (NULL, NULL, path);

  /* FIXME */
  if (vgpu->need_brightness_reset)
  {
      surfman_info ("Restore brightness");
      plugin_restore_brightness();
      vgpu->need_brightness_reset = false;
  }

  /* The blanker locks up it wake up with a commande set to 1 */
  vgpu_blanker_write (vgpu->domid, BLANKER_IDLE);

  /* Remove the previous ping and set the next ping earlier */
  event_del(&vgpu->ev_blanker);
  t.tv_sec = BLANKER_PING / 1000;
  t.tv_usec = (BLANKER_PING % 1000) * 1000;
  event_add(&vgpu->ev_blanker, &t);
}

static void vgpu_blanker_ping (struct vgpu *vgpu)
{
  struct timeval t;

  vgpu_blanker_write (vgpu->domid, BLANKER_OP_START);

  /**
   * Be sure to remove the previous watch. If it's timeout, the watch
   * will not be removed.
   */
  xenstore_dom_watch (vgpu->domid, NULL, NULL, "switcher/enter");

  if (!xenstore_dom_watch (vgpu->domid, vgpu_blanker_alive,
                           vgpu, "switcher/enter"))
    surfman_warning ("VGPU: domid = %u to watch blanker node", vgpu->domid);

  /**
   * Set up the next ping in the maximum timeout. If the blanker
   * is alive (see vgpu_blanker_alive) the next ping will earlie
   */
  t.tv_sec = BLANKER_TIMEOUT / 1000;
  t.tv_usec = (BLANKER_TIMEOUT % 1000) * 1000;
  event_add (&vgpu->ev_blanker, &t);
}

/* Libevent wrapper for blanker ping */
static void vgpu_blanker_ping_event (int fd, short event, void *opaque)
{
  vgpu_blanker_ping (opaque);
}

/**
 * This function checks if the blanker is running in the PVM. If not
 * switch is denied. This code was formerly in igfx but was move here
 * to deny switch with other plugin (for instance AMD).
 * Basically the switch must be denied during boot and sthudown.
 * The current check has some side effects:
 *   - switch is denied if the blanker is not running in the PVM;
 *   - sometimes the blanker is killed too late during shutdown process.
 * This function returns 0 if commit is accepted.
 */
int
vgpu_display_allow_commit (struct plugin *p, surfman_display_t *disp, int len)
{
  struct vgpu *vgpu;
  int res = 0;
  int i;

  LIST_FOREACH (vgpu, &vgpu_list, link)
    {
      /* We only check is the PVM is running for a pass-through GPU */
      if (vgpu->plugin != p ||
          vgpu->type != SURFMAN_VGPU_INFO_TYPE_PASSTHROUGH)
        continue;

      surfman_info ("VGPU: domid = %d current_time = %ld blanker_alive = %ld",
                    vgpu->domid, time(NULL), vgpu->blanker_alive);

      /* Denied commit if the blanker didn't answer after n secs */
      if (vgpu_blanker_timeout (vgpu))
        return -1;
    }

  return 0;
}

void
vgpu_blanker_init (struct vgpu *vgpu)
{
  char perms[1024];

  if (vgpu->type != SURFMAN_VGPU_INFO_TYPE_PASSTHROUGH)
    return;

  vgpu_blanker_write(vgpu->domid, BLANKER_IDLE);
  snprintf (perms, sizeof (perms), "r%u", vgpu->domid);
  if (!xenstore_dom_chmod (vgpu->domid, perms, 1, "switcher/enter"))
    surfman_error ("VGPU: domid = %d unable to give permission %s",
                   vgpu->domid, perms);

  /* Create the blanker ping event */
  event_set (&vgpu->ev_blanker, -1, EV_TIMEOUT | EV_PERSIST,
             vgpu_blanker_ping_event, vgpu);
  vgpu_blanker_ping (vgpu);
}

static int
vgpu_set_visible (struct device *dev)
{
  struct vgpu *vgpu = (struct vgpu *)dev;
  int i;

  surfman_info ("Monitor count = %d (%d)", vgpu->vmonitor_count, vgpu->plugin->monitor_count);

  for (i = 0; i < vgpu->vmonitor_count; i++)
    {
      int slot = get_monitor_slot (vgpu->plugin->monitors[i]);

      surfman_info ("vmonitor %d -> display slot %d", i, slot);

      if (slot == -1)
        {
          surfman_error ("Display slot not found for monitor");
          continue;
        }

      display_prepare_vmonitor (slot, dev, vgpu->vmonitors[i], NULL);
    }

  return 0;
}

static void
vgpu_handle_notifications (struct vgpu *vgpu)
{
  switch (vgpu->interface->notify)
    {
    case SURFMAN_NOTIFY_MONITOR_RESCAN:
      {
        struct domain *d = domain_by_domid (vgpu->domid);

        surfman_info ("Rescanning vmonitors");
        vgpu_update_vmonitors (vgpu);

        /*
        ** If domain was previously running in emulation mode
        ** (vesa, xengfx, xenfb), with a vgpu device inserted,
        ** here is the only place where we can detect that the
        ** vgpu driver has loaded in the guest OS.
        **
        ** Consequently, that should be the right place to switch.
        */
        domain_set_visible (NULL, 0);
      }
      break;
    case SURFMAN_NOTIFY_NONE:
      break;
    default:
      break;
    }

  vgpu->interface->notify = SURFMAN_NOTIFY_NONE;
}

void
vgpu_poll_notifications (void)
{
  struct vgpu *vgpu;

  LIST_FOREACH (vgpu, &vgpu_list, link)
    {
      vgpu_handle_notifications (vgpu);
    }
}

surfman_psurface_t
vgpu_get_psurface(struct device *dev, surfman_vmonitor_t vmon)
{
  struct vgpu *vgpu = (struct vgpu *)dev;

  return VGPU_CALL(vgpu, get_psurface_by_vmonitor, vmon);
}

static int
vgpu_attach (struct vgpu *vgpu,
             uint8_t bus,
             uint8_t device,
             uint8_t function,
             int passthrough)
{
  surfman_vgpu_info_t info;
  struct timeval t;

  info.domid = vgpu->domid;
  info.bus = bus;
  info.dev = device;
  info.func = function;

  if (passthrough)
    {
      info.io_handle = iohandle_create (vgpu->domid);
      info.type = SURFMAN_VGPU_INFO_TYPE_PASSTHROUGH;
      vgpu->type = SURFMAN_VGPU_INFO_TYPE_PASSTHROUGH;
      vgpu->need_brightness_reset = true;
      /* Initialize the blanker */
      vgpu_blanker_init (vgpu);
    }
  else
    {
      info.io_handle = NULL;
      info.type = SURFMAN_VGPU_INFO_TYPE_EMULATION;
      vgpu->type = SURFMAN_VGPU_INFO_TYPE_EMULATION;
    }

  vgpu->plugin = plugin_find_vgpu (&info, &vgpu->interface);

  if (!vgpu->plugin)
    {
      if (info.io_handle)
        iohandle_destroy (info.io_handle);
      return -1;
    }

  vgpu->bus = bus;
  vgpu->dev = device;
  vgpu->func = function;

  vgpu->iohdl = info.io_handle;

  LIST_INSERT_HEAD (&vgpu_list, vgpu, link);

  vgpu_update_vmonitors(vgpu);

  return 0;
}

void
vgpu_detach(struct vgpu *vgpu)
{
  int v;
  struct vgpu vgpu_data = *vgpu;

  for (v = 0; v < vgpu->vmonitor_count; v++)
    {
      display_vmonitor_takedown (vgpu->vmonitors[v]);
      VGPU_CALL (vgpu, free_vmonitor, vgpu->vmonitors[v]);
    }
  vgpu->vmonitor_count = 0;
  vgpu->interface = NULL;
  vgpu->plugin = NULL;
  LIST_REMOVE (vgpu, link);

  PLUGIN_CALL (vgpu_data.plugin, free_vgpu, vgpu_data.interface);

  if (vgpu_data.iohdl)
    {
      int timeout = 100;

      iohandle_destroy (vgpu_data.iohdl);

      /*
       * In the passthrough case, the device won't be
       * deattached until the domain dies. To prevent
       * any race conditions we need to block here until
       * the domain dies.
       */
      while (timeout--)
        {
          if (!xc_domid_exists (vgpu_data.domid))
            break;
          usleep (500 * 1000);
        }
    }

  if (vgpu_data.type == SURFMAN_VGPU_INFO_TYPE_PASSTHROUGH)
    {
      xenstore_dom_watch (vgpu_data.domid, NULL, NULL, "switcher/enter");
      event_del (&vgpu_data.ev_blanker);
    }
}

static int
vgpu_config_read(void *priv,
                 struct msg_config_io_read *msg,
                 size_t msglen,
                 struct msg_config_io_reply *out)
{
  struct vgpu *vgpu = priv;

  if (!vgpu->interface)
    return -1;

  out->data = VGPU_CALL (vgpu, config_read, msg->offset, msg->size);

  return 0;
}

static int
vgpu_config_write(void *priv,
                  struct msg_config_io_write *msg,
                  size_t msglen,
                  struct msg_empty_reply *out)
{
  struct vgpu *vgpu = priv;

  if (!vgpu->interface)
    return -1;

  VGPU_CALL (vgpu, config_write, msg->offset, msg->size, msg->data);

  return 0;
}

static int
vgpu_attach_device(void *priv,
                   struct msg_attach_pci_device *msg,
                   size_t msglen,
                   struct msg_empty_reply *out)
{
  struct vgpu *vgpu = priv;
  int rc;

  int passthrough = 0;

  if (vgpu->iohdl || vgpu->interface)
    {
      surfman_error ("VGPU already attached");
      return -1;
    }

  if (vgpu->device.device_type == DEVICE_TYPE_PASSTHROUGH)
    passthrough = 1;

  rc = vgpu_attach (vgpu, msg->bus, msg->device, msg->function,
                    passthrough);
  if (rc)
    surfman_error ("Failed to attach vgpu to domain");

  return rc;
}

static int
vgpu_update_bar(void *priv,
                struct msg_update_pci_bar *msg,
                size_t msglen,
                struct msg_empty_reply *out)
{
  struct vgpu *vgpu = priv;
  surfman_update_bar_t *bar;
  int rc;

  if (!vgpu->iohdl || !vgpu->interface)
    {
      surfman_error ("VGPU not yet initalized");
      return -1;
    }

  if (msg->barID >= 6)
    return -1;

  bar = &vgpu->bars[msg->barID];

  bar->id = msg->barID;
  bar->hostaddr = msg->hostaddr;
  bar->guestaddr = msg->guestaddr;
  bar->len = msg->len;

  rc = VGPU_CALL (vgpu, update_bar, bar);

  return rc;
}

static struct dmbus_rpc_ops vgpu_rpc_ops = {
  .config_io_read = vgpu_config_read,
  .config_io_write = vgpu_config_write,
  .attach_pci_device = vgpu_attach_device,
  .update_pci_bar = vgpu_update_bar,
};

static void
vgpu_takedown (struct device *device)
{
  struct vgpu *vgpu = (struct vgpu *)device;

  surfman_info ("Taking down vgpu device %d", vgpu->domid);
  vgpu_detach (vgpu);
}

static int
vgpu_is_active (struct device *device)
{
  struct vgpu *vgpu = (struct vgpu *)device;

  return (vgpu->iohdl && vgpu->interface);
}

static struct device_ops vgpu_device_ops = {
  .name = "vgpu",
  .takedown = vgpu_takedown,
  .set_visible = vgpu_set_visible,
  .is_active = vgpu_is_active,
};

struct device *
vgpu_device_create(struct domain *d, struct dmbus_rpc_ops **ops)
{
  struct vgpu *vgpu;

  surfman_info ("Creating VGPU %d", d->domid);

  vgpu = device_create (d, &vgpu_device_ops, sizeof (*vgpu));
  if (!vgpu)
    return NULL;

  *ops = &vgpu_rpc_ops;

  vgpu->domid = d->domid;

  return &vgpu->device;
}

