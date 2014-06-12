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

#include <dbus/dbus-shared.h>

DBusConnection *connection = NULL;

static const struct
{
  const char *name;
  dbus_bool_t (*method)(DBusMessage *msg, DBusMessage *reply);
} method_list[] = {
  { "set_pv_display", dbus_set_pv_display },
  { "set_visible", dbus_set_visible },
  { "vgpu_mode", dbus_vgpu_mode },
  { "get_visible", dbus_get_visible },
  { "notify_death", dbus_notify_death },
  { "dump_all_screens", dbus_dump_all_screens },
  { "increase_brightness", dbus_increase_brightness },
  { "decrease_brightness", dbus_decrease_brightness },
  { "pre_s3", dbus_pre_s3 },
  { "post_s3", dbus_post_s3 },
  { "display_image", dbus_display_image },
  { "has_vgpu", dbus_has_vgpu },
  { "display_text", dbus_display_text },
  { NULL, NULL },
};

DBusHandlerResult dbus_message (DBusConnection *connection,
                                DBusMessage *msg,
                                void *user_data)
{
  int type;
  const char *method;
  dbus_bool_t ret;
  DBusMessage *reply = NULL;

  type = dbus_message_get_type (msg);
  method = dbus_message_get_member (msg);

  surfman_info ("Received dbus message %s:%s",
        dbus_message_type_to_string (type),
        method);

  if (!dbus_message_get_no_reply (msg))
    {
      reply = dbus_message_new_method_return (msg);
    }

  /*
   *
   * XXX: Replace that with code generation later.
   *
   */
  if (type == DBUS_MESSAGE_TYPE_METHOD_CALL)
    {
      int i;

      for (i = 0; method_list[i].name; i++)
        if (!strcmp (method_list[i].name, method))
          {
            ret = method_list[i].method (msg, reply);
            break;
          }
      if (!method_list[i].name)
        {
          surfman_error ("Unhandled method: %s", method);
          return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
    }
  else
    {
      surfman_error ("Unhandled message type: %s",
             dbus_message_type_to_string (type));
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

  if (reply)
    {
      if (!ret)
        {
          dbus_message_unref (reply);
          reply = dbus_message_new_error (msg, DBUS_ERROR_FAILED, NULL);
        }

      dbus_connection_send (connection, reply, NULL);
      dbus_message_unref (reply);
    }

  return DBUS_HANDLER_RESULT_HANDLED;
}


void
dbus_start_service (void)
{
  DBusMessage *msg;

  if (!connection)
    fatal ("dbus_start_service() failed. aborting.");

  msg = dbus_message_new_signal ("/", "com.citrix.xenclient.surfman", "start_service");

  if (!msg)
    fatal ("dbus_message_new_signal() failed. aborting.");

  dbus_connection_send(connection, msg, NULL);
  dbus_connection_flush(connection);
  dbus_message_unref(msg);
}

static void
dbus_event_handler (int fd, short event, void *priv)
{
  DBusWatch *watch = priv;
  int flags = 0;

  if (event & EV_READ)
    {
      flags |= DBUS_WATCH_READABLE;
    }
  if (event & EV_WRITE)
    {
      flags |= DBUS_WATCH_WRITABLE;
    }

  dbus_watch_handle (watch, flags);

  dbus_connection_ref (connection);

  while (dbus_connection_dispatch (connection) ==
         DBUS_DISPATCH_DATA_REMAINS)
    {
    }

  dbus_connection_unref (connection);
}

static dbus_bool_t
watch_add (DBusWatch * watch, void *data)
{
  int fd = dbus_watch_get_unix_fd (watch);
  int flags = dbus_watch_get_flags (watch);
  short ev_flags = EV_PERSIST;
  struct event *ev;

  if (dbus_watch_get_data (watch))
    fatal ("DBUS watch already added.");

  if (flags & DBUS_WATCH_READABLE)
    ev_flags |= EV_READ;

  if (flags & DBUS_WATCH_WRITABLE)
    ev_flags |= EV_WRITE;

  ev = calloc (1, sizeof (*ev));
  if (!ev)
    return FALSE;

  event_set(ev, fd, ev_flags, dbus_event_handler, watch);
  dbus_watch_set_data(watch, ev, NULL);

  if (dbus_watch_get_enabled (watch))
    event_add(ev, NULL);

  return TRUE;
}

static void
watch_remove (DBusWatch * watch, void *data)
{
  struct event *ev = dbus_watch_get_data (watch);

  if (!ev)
    fatal ("trying to remove invalid DBUS watch.");

  event_del (ev);
  free (ev);

  dbus_watch_set_data (watch, NULL, NULL);
}

static void
watch_toggle (DBusWatch * watch, void *data)
{
  struct event *ev = dbus_watch_get_data (watch);

  if (dbus_watch_get_enabled (watch))
    {
      event_add (ev, NULL);
    }
  else
    {
      event_del (ev);
    }
}

void
dbus_cleanup (void)
{
  if (connection)
    dbus_connection_unref (connection);
}

static struct DBusObjectPathVTable dbus_vtable = {
    .unregister_function = NULL,
    .message_function = dbus_message,
};

int
dbus_init (void)
{
  int rc;
  dbus_bool_t ret;

  connection = dbus_bus_get (DBUS_BUS_SYSTEM, NULL);
  if (!connection)
      return -1;

  dbus_connection_set_exit_on_disconnect(connection, FALSE);

  rc = dbus_bus_request_name (connection, "com.citrix.xenclient.surfman",
                              DBUS_NAME_FLAG_DO_NOT_QUEUE, NULL);
  if (rc == -1)
    goto unref;

  ret = dbus_connection_set_watch_functions (connection,
                                             watch_add,
                                             watch_remove,
                                             watch_toggle,
                                             NULL, NULL);
  if (!ret)
      goto unref;

  ret = dbus_connection_register_object_path (connection, "/",
                                              &dbus_vtable, NULL);
  if (!ret)
      goto unref;

  return 0;

unref:
  dbus_connection_unref (connection);
  return -1;
}

