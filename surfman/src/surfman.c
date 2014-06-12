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
#include "splashscreen.h"

#define SECS_PER_TICK	5
#define DEFAULT_PLUGIN_SEARCH_PATH "/usr/lib/surfman"

static void
usage (const char *progname)
{
  fprintf (stderr, "Usage: %s [-s] [-n] [-h|--help] [-p path]\n"
           "\t-h, --help  Show this help screen\n"
           "\t-p,         Plugin search path [default: %s]\n"
           "\t-n          Do not daemonize\n"
           "\t-c          Don't load plugins\n"
           "\t-s          Safe graphics mode\n" "\n",
           progname, DEFAULT_PLUGIN_SEARCH_PATH);
  exit (1);
}

static void startup (void)
{
  const char *startup_image;
  int rc;

  startup_image = config_get ("surfman", "startup_image");
  if (!startup_image)
    {
      surfman_warning ("no startup image configured");
      return;
    }

  splash_picture (startup_image);
  register_spinner ();
}

void surfman_pciemu_logging (libpciemu_loglvl loglvl, const char *fmt, ...)
{
  va_list ap;
  surfman_loglvl level = MESSAGE_INFO;

  switch (loglvl)
    {
      case LIBPCIEMU_MESSAGE_DEBUG:
        level = SURFMAN_DEBUG;
        break;
      case LIBPCIEMU_MESSAGE_INFO:
        level = SURFMAN_INFO;
        break;
      case LIBPCIEMU_MESSAGE_WARNING:
        level = SURFMAN_WARNING;
        break;
      case LIBPCIEMU_MESSAGE_ERROR:
        level = SURFMAN_ERROR;
        break;
      case LIBPCIEMU_MESSAGE_FATAL:
        level = SURFMAN_FATAL;
        break;
    }

  va_start (ap, fmt);
  surfman_vmessage (level, fmt, ap);
  va_end(ap);
}

int main (int argc, char *argv[])
{
  int c;
  int dont_detach = 0;
  const char *plugin_path = DEFAULT_PLUGIN_SEARCH_PATH;
  int safe_graphics = 0;
  int no_plugin = 0;

  while ((c = getopt (argc, argv, "snhcp:|help")) != -1)
    {
      switch (c)
        {
        case 'n':
          dont_detach++;
          break;
        case 'c':
          no_plugin = 1;
        case 'p':
          plugin_path = optarg;
          break;
        case 's':
          safe_graphics = 1;
          break;
          default:usage (argv[0]);
        }
    }
  openlog ("surfman", LOG_CONS, LOG_USER);

  if (!dont_detach)
    {
      if (daemon (0, 0))
        fatal ( "daemon(0,0) failed: %s", strerror (errno));
    }

  surfman_info ("Surfman daemon started with PID %d", getpid ());
  surfman_info ("Surfman API version "SURFMAN_VERSION_FMT".", SURFMAN_VERSION_ARGS (SURFMAN_API_VERSION));

  event_init ();
  renable_core_dumps ();
  trap_segv ();

  if (xenstore_init ())
    {
      fatal ("xenstore_init() failed, aborting.");
      exit (-1);
    }
  resolution_init ();

  lockfile_lock ();

  config_load_file ("/etc/surfman.conf");

  xc_init ();

  if (dbus_init ())
    {
      fatal ("dbus_init() failed. aborting.");
      exit (-1);
    }

  xenfb_backend_init (0);

  if (rpc_init ())
    {
      fatal ("rpc_init() failed. aborting.");
      exit (-1);
    }

  display_init ();
  pci_system_init ();

  if (libpciemu_init (surfman_pciemu_logging))
    {
      fatal ("libpciemu_init() failed. aborting.");
    }

  if (!no_plugin)
      plugin_init (plugin_path, safe_graphics);

  startup ();

  /* inform about our birth */
  dbus_start_service();

  surfman_info ("Dispatching events (event lib v%s. Method %s)",
          event_get_version (),
          event_get_method ());

  event_dispatch ();

  return 0;
}
