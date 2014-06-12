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

static struct event plugin_poll_event;

static
LIST_HEAD (, struct plugin)
  plugin_list = LIST_HEAD_INITIALIZER;

static size_t
name_from_path (char *d, char *path, size_t len)
{
  char *p, *base;

  base = path;
  for (p = path; *p; p++)
    {
      if (*p == '/')
        base = p + 1;
    }

  len = snprintf (d, len, "%s", base);
  if (len >= 3 && !strncmp (d + len - 3, ".so", 3))
    {
      len -= 3;
      d[len] = '\0';
    }

  return len;
}

struct plugin *
load_plugin (char *path)
{
  void *handle;
  surfman_plugin_t *interface;
  surfman_version_t *version;
  struct plugin *ret;
  int rc;

  handle = dlopen (path, RTLD_LAZY | RTLD_GLOBAL);
  if (!handle)
    {
      surfman_warning ("Failed to load plugin: %s", dlerror ());
      return NULL;
    }

  interface = dlsym (handle, PLUGIN_INTERFACE_SYMBOL);
  if (!interface)
    {
      surfman_warning ("Symbol \"%s\" not found in library %s.",
               PLUGIN_INTERFACE_SYMBOL, path);
      goto sym_fail;
    }

  ret = xcalloc (1, sizeof (*ret));

  name_from_path (ret->name, path, PLUGIN_NAME_MAX);
  ret->handle = handle;
  ret->interface = interface;

  version = dlsym (handle, PLUGIN_VERSION_SYMBOL);
  if (!version)
    {
      surfman_warning ("Symbol \"%s\" not found in library %s.", PLUGIN_VERSION_SYMBOL, path);
      ret->version = PLUGIN_DEFAULT_VERSION;
    }
  else
    ret->version = *version;

  surfman_info ("Load plugin %s version "SURFMAN_VERSION_FMT".", ret->name, SURFMAN_VERSION_ARGS (ret->version));

  /* Check if surfman supported the plugin API version */
  if (!PLUGIN_VERSION_SUPPORTED(ret->version))
    {
      surfman_error ("%s: plugin API version is not supported by Surfman", ret->name);
      goto init_fail;
    }

  rc = PLUGIN_CALL (ret, init);
  if (rc != SURFMAN_SUCCESS)
    {
      surfman_warning ("%s: init method failed", ret->name);
      goto init_fail;
    }

  LIST_INSERT_HEAD (&plugin_list, ret, link);

  plugin_scan_monitors (ret);

  return ret;

init_fail:
  free (ret);
sym_fail:
  dlclose (handle);

  return NULL;
}

static void
unload_plugin (struct plugin *p)
{
  display_plugin_takedown (p);
  PLUGIN_CALL (p, shutdown);

  dlclose (p->handle);

  LIST_REMOVE (p, link);
  free (p);
}

static int
pci_vendor_match (const char *vendors)
{
  int match = 0;
  char *s;
  char *list;
  struct pci_id_match m;
  struct pci_device_iterator *iter;
  struct pci_device *device;

  m.device_id = PCI_MATCH_ANY;
  m.subvendor_id = PCI_MATCH_ANY;
  m.subdevice_id = PCI_MATCH_ANY;
  m.device_class = 0x30000;
  m.device_class_mask = 0x00ff0000;

  list = strdup (vendors);

  while ((s = strsep (&list, " ")) != NULL)
    {
      int rc;

      if (*s == '\0')
        continue;

      rc = sscanf(s, "0x%x", &m.vendor_id);
      if (rc != 1)
        {
          surfman_warning ("Failed to parse PCI Vendor ID");
          continue;
        }

      surfman_info ("Trying to match VGA compatible devices with Vendor ID: 0x%x",
            m.vendor_id);

      iter = pci_id_match_iterator_create(&m);
      while ((device = pci_device_next(iter)))
        {
          pci_device_probe(device);
          /* !! THE BLOC BELOW IS A NASTY HACK !! */
          /* !! REMOVE IT WHEN AMD FIX THEIR PLUGIN !! */
          if (!pci_device_is_boot_vga(device))
            {
              surfman_info("  - NOT using %04x:%04x at %02x:%02x.%02x, as it is NOT the primary adapter",
                  device->vendor_id, device->device_id,
                  device->bus, device->dev, device->func);
              continue;
            }
          surfman_info ("  - found %04x:%04x at %02x:%02x.%02x",
                  device->vendor_id, device->device_id,
                  device->bus, device->dev, device->func);
          match++;
        }
    }

  free (list);

  return match;
}

static int
load_plugins_from_list (const char *plugin_path, const char *plugin_list,
                        int check_vendor)
{
  char *buff;
  char *p, *list;
  char *name;
  int ret = 0;

  buff = xmalloc (PATH_MAX);
  p = list = strdup (plugin_list);

  surfman_info ("attempt to load these plugins: %s, from this directory: %s",
        plugin_list, plugin_path);

  while ((name = strsep (&list, " ")) != NULL)
    {
      if (*name == '\0')
        continue;

      if (check_vendor)
        {
          const char *vendors = config_get (name, "pci_vendor_ids");
          int m;

          if (vendors)
            {
              m = pci_vendor_match (vendors);
              if (!m)
                {
                  surfman_warning ("Failed to find matching PCI device for plugin: %s",
                            name);
                  continue;
                }
              surfman_info ("%d PCI device matches for plugin: %s, proceeding...", m,
                    name);
            }
        }

      /* Try to load config file for plugin */
      snprintf (buff, PATH_MAX, "/etc/surfman-%s.conf", name);
      config_load_file (buff);

      snprintf (buff, PATH_MAX, "%s/%s.so", plugin_path, name);
      ret += load_plugin (buff) ? 1 : 0;
    }

  free (buff);
  free (list);

  return ret;
}

static void
plugin_poll (int fd, short event, void *priv)
{
  struct plugin *p;
  struct domain *d;
  struct timeval tv = {1, 0};

  (priv);

  LIST_FOREACH (p, &plugin_list, link)
    {
      plugin_handle_notification (p);
      resolution_refresh_current (p);
    }

  /* Also process individual vgpu notifications */
  vgpu_poll_notifications ();

  /* Rearm timer */
  event_add (&plugin_poll_event, &tv);
}

void
plugin_init (const char *plugin_path, int safe_graphics)
{
  int n = 0;
  struct timeval tv = {1, 0};
  const char *plugins;
  const char *fallback;

  surfman_info ("Loading plugins from %s", plugin_path);

  plugins = config_get ("surfman", "plugins");
  fallback = config_get ("surfman", "fallback");

  if (safe_graphics)
    {
      if (fallback)
        n = load_plugins_from_list (plugin_path, fallback, 0);
    }
  else
    {
      if (plugins)
        n = load_plugins_from_list (plugin_path, plugins, 1);
      if (!n && fallback)
        n = load_plugins_from_list (plugin_path, fallback, 0);
    }

  if (n)
    surfman_info ("Loaded %d plugins", n);

  event_set (&plugin_poll_event, -1, EV_TIMEOUT,
             plugin_poll, NULL);
  event_add (&plugin_poll_event, &tv);
}

void
plugin_cleanup (void)
{
  struct plugin *p, *tmp;

  event_del (&plugin_poll_event);

  LIST_FOREACH_SAFE (p, tmp, &plugin_list, link)
    {
      unload_plugin (p);
    }
}

struct plugin *
plugin_lookup (char *name)
{
  struct plugin *p;

  LIST_FOREACH (p, &plugin_list, link)
    {
      if (!strncmp (name, p->name, PLUGIN_NAME_MAX))
        return p;
    }

  return NULL;
}

void
plugin_scan_monitors (struct plugin *plugin)
{
  int rc;

  rc = PLUGIN_CALL (plugin, get_monitors,
                    plugin->monitors, PLUGIN_MONITOR_MAX);

  if (rc == SURFMAN_ERROR)
    {
      surfman_warning ("plugin %s: Couldn't update list of monitors", plugin->name);
      return;
    }
  plugin->monitor_count = rc;

  display_update_monitors (plugin, plugin->monitors, rc);
  vgpu_update_plugin_vmonitors (plugin);

  domain_set_visible (NULL, 0);

  /* This has to go */
  resolution_refresh_current (plugin);
}

int
plugin_handle_notification (struct plugin *plugin)
{
  int ret;

  switch (plugin->interface->notify)
    {
    case SURFMAN_NOTIFY_MONITOR_RESCAN:
      surfman_info ("Detected hotplug notification, rescanning list of monitors");
      plugin_scan_monitors (plugin);
      ret = 1;
      break;
    case SURFMAN_NOTIFY_NONE:
      ret = 0;
      break;
    default:
      ret = -1;
      break;
    }

  plugin->interface->notify = SURFMAN_NOTIFY_NONE;
  return ret;
}

void
plugin_pre_s3 (void)
{
  struct plugin *p;

  LIST_FOREACH (p, &plugin_list, link)
    {
      if (PLUGIN_HAS_METHOD (p, pre_s3))
        PLUGIN_CALL (p, pre_s3);
    }
}

void
plugin_post_s3 (void)
{
  struct plugin *p;

  LIST_FOREACH (p, &plugin_list, link)
    {
      if (PLUGIN_HAS_METHOD (p, post_s3))
        PLUGIN_CALL (p, post_s3);
    }
}

void
plugin_increase_brightness (void)
{
  struct plugin *p;

  LIST_FOREACH (p, &plugin_list, link)
    {
      if (PLUGIN_HAS_METHOD (p, increase_brightness))
        PLUGIN_CALL (p, increase_brightness);
    }
}

void
plugin_decrease_brightness (void)
{
  struct plugin *p;

  LIST_FOREACH (p, &plugin_list, link)
    {
      if (PLUGIN_HAS_METHOD (p, decrease_brightness))
        PLUGIN_CALL (p, decrease_brightness);
    }
}

void plugin_restore_brightness (void)
{
  struct plugin *p;

  LIST_FOREACH (p, &plugin_list, link)
    {
      /* restore_brightness has been implemented from  2.1.0 */
      if (PLUGIN_CHECK_VERSION(p, 2, 1, 0)
          && PLUGIN_HAS_METHOD (p, restore_brightness))
        PLUGIN_CALL (p, restore_brightness);
    }
}

#if 0
void
plugin_set_guest_resolution (unsigned int domid)
{
  struct plugin *p;

  LIST_FOREACH (p, &plugin_list, link)
    {
      resolution_domain_on_monitor (domid, p, p->monitors[0]);
    }
}
#endif

unsigned int
plugin_stride_align (void)
{
  struct plugin *p;
  unsigned int max_stride = 0;

  LIST_FOREACH (p, &plugin_list, link)
    {
      if (PLUGIN_GET_OPTION (p, stride_align) > max_stride)
        max_stride = PLUGIN_GET_OPTION (p, stride_align);
    }
  return max_stride;
}

int
plugin_need_refresh (struct plugin *p)
{
  return PLUGIN_GET_OPTION(p, features) & SURFMAN_FEATURE_NEED_REFRESH;
}

struct plugin *
plugin_find_vgpu (surfman_vgpu_info_t *info, surfman_vgpu_t **pvgpu)
{
  surfman_vgpu_t *vgpu;
  struct plugin *p;

  LIST_FOREACH (p, &plugin_list, link)
  {
    vgpu = PLUGIN_CALL (p, new_vgpu, info);

    if (vgpu)
      {
        *pvgpu = vgpu;
        return p;
      }
  }

  return NULL;
}

int
plugin_get_vgpu_modes(struct vgpu_mode *modes, int len)
{
  struct plugin *p;
  int count = 0;
  int rc;

  LIST_FOREACH (p, &plugin_list, link)
  {
    int i;

    if (count == len)
      break;

    if (PLUGIN_HAS_METHOD (p, get_vgpu_mode))
      {
        rc = PLUGIN_CALL (p, get_vgpu_mode, &modes[count].m);
        if (!rc)
          modes[count++].p = p;
      }
  }

  return count;
}

int
plugin_display_commit (int force)
{
  struct plugin *p;
  int rc = 0;

  LIST_FOREACH (p, &plugin_list, link)
    {
      rc |= display_commit (p, force);
    }

  return rc;
}
