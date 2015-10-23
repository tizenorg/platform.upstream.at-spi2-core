/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * 
 * at-spi-bus-launcher: Manage the a11y bus as a child process 
 *
 * Copyright 2011 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>

#include <gio/gio.h>
#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

#define APP_CONTROL_OPERATION_SCREEN_READ "http://tizen.org/appcontrol/operation/read_screen"
#include <appsvc.h>
#include <vconf.h>

//uncomment if you want debug
//#ifndef TIZEN_ENGINEER_MODE
//#define TIZEN_ENGINEER_MODE
//#endif
#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "ATSPI_BUS_LAUNCHER"

#include <dlog.h>
#include <aul.h>

//uncomment this if you want log suring startup
//seems like dlog is not working at startup time
#define ATSPI_BUS_LAUNCHER_LOG_TO_FILE

#ifdef ATSPI_BUS_LAUNCHER_LOG_TO_FILE
FILE *log_file;
#ifdef LOGD
#undef LOGD
#endif
#define LOGD(arg...) do {fprintf(log_file, ##arg);fprintf(log_file, "\n"); fflush(log_file);} while(0)
#endif


typedef enum {
  A11Y_BUS_STATE_IDLE = 0,
  A11Y_BUS_STATE_READING_ADDRESS,
  A11Y_BUS_STATE_RUNNING,
  A11Y_BUS_STATE_ERROR
} A11yBusState;

typedef struct {
  GMainLoop *loop;
  gboolean launch_immediately;
  gboolean a11y_enabled;
  gboolean screen_reader_enabled;
  GDBusConnection *session_bus;
  GSettings *a11y_schema;
  GSettings *interface_schema;

  int launch_screen_reader_repeats;
  gboolean screen_reader_needed;
  int pid;

  A11yBusState state;

  /* -1 == error, 0 == pending, > 0 == running */
  int a11y_bus_pid;
  char *a11y_bus_address;
  int pipefd[2];
  char *a11y_launch_error_message;
} A11yBusLauncher;

static A11yBusLauncher *_global_app = NULL;

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.a11y.Bus'>"
  "    <method name='GetAddress'>"
  "      <arg type='s' name='address' direction='out'/>"
  "    </method>"
  "  </interface>"
  "<interface name='org.a11y.Status'>"
  "<property name='IsEnabled' type='b' access='readwrite'/>"
  "<property name='ScreenReaderEnabled' type='b' access='readwrite'/>"
  "</interface>"
  "</node>";
static GDBusNodeInfo *introspection_data = NULL;

static void
setup_bus_child (gpointer data)
{
  A11yBusLauncher *app = data;
  (void) app;

  close (app->pipefd[0]);
  dup2 (app->pipefd[1], 3);
  close (app->pipefd[1]);

  /* On Linux, tell the bus process to exit if this process goes away */
#ifdef __linux
#include <sys/prctl.h>
  prctl (PR_SET_PDEATHSIG, 15);
#endif  
}

/**
 * unix_read_all_fd_to_string:
 *
 * Read all data from a file descriptor to a C string buffer.
 */
static gboolean
unix_read_all_fd_to_string (int      fd,
                            char    *buf,
                            ssize_t  max_bytes)
{
  ssize_t bytes_read;

  while (max_bytes > 1 && (bytes_read = read (fd, buf, MAX (4096, max_bytes - 1))))
    {
      if (bytes_read < 0)
        return FALSE;
      buf += bytes_read;
      max_bytes -= bytes_read;
    }
  *buf = '\0';
  return TRUE;
}

static void
on_bus_exited (GPid     pid,
               gint     status,
               gpointer data)
{
  A11yBusLauncher *app = data;
  
  app->a11y_bus_pid = -1;
  app->state = A11Y_BUS_STATE_ERROR;
  if (app->a11y_launch_error_message == NULL)
    {
      if (WIFEXITED (status))
        app->a11y_launch_error_message = g_strdup_printf ("Bus exited with code %d", WEXITSTATUS (status));
      else if (WIFSIGNALED (status))
        app->a11y_launch_error_message = g_strdup_printf ("Bus killed by signal %d", WTERMSIG (status));
      else if (WIFSTOPPED (status))
        app->a11y_launch_error_message = g_strdup_printf ("Bus stopped by signal %d", WSTOPSIG (status));
    }
  g_main_loop_quit (app->loop);
} 

static gboolean
ensure_a11y_bus (A11yBusLauncher *app)
{
  GPid pid;
  char *argv[] = { DBUS_DAEMON, NULL, "--nofork", "--print-address", "3", NULL };
  char addr_buf[2048];
  GError *error = NULL;

  if (app->a11y_bus_pid != 0)
    return FALSE;
  
  argv[1] = g_strdup_printf ("--config-file=%s/at-spi2/accessibility.conf", SYSCONFDIR);

  if (pipe (app->pipefd) < 0)
    g_error ("Failed to create pipe: %s", strerror (errno));
  
  if (!g_spawn_async (NULL,
                      argv,
                      NULL,
                      G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                      setup_bus_child,
                      app,
                      &pid,
                      &error))
    {
      app->a11y_bus_pid = -1;
      app->a11y_launch_error_message = g_strdup (error->message);
      g_clear_error (&error);
      goto error;
    }

  close (app->pipefd[1]);
  app->pipefd[1] = -1;

  g_child_watch_add (pid, on_bus_exited, app);

  app->state = A11Y_BUS_STATE_READING_ADDRESS;
  app->a11y_bus_pid = pid;
  LOGD("Launched a11y bus, child is %ld", (long) pid);
  if (!unix_read_all_fd_to_string (app->pipefd[0], addr_buf, sizeof (addr_buf)))
    {
      app->a11y_launch_error_message = g_strdup_printf ("Failed to read address: %s", strerror (errno));
      kill (app->a11y_bus_pid, SIGTERM);
      goto error;
    }
  close (app->pipefd[0]);
  app->pipefd[0] = -1;
  app->state = A11Y_BUS_STATE_RUNNING;

  /* Trim the trailing newline */
  app->a11y_bus_address = g_strchomp (g_strdup (addr_buf));
  LOGD("a11y bus address: %s", app->a11y_bus_address);

#ifdef HAVE_X11
  {
    Display *display = XOpenDisplay (NULL);
    if (display)
      {
        Atom bus_address_atom = XInternAtom (display, "AT_SPI_BUS", False);
        XChangeProperty (display,
                         XDefaultRootWindow (display),
                         bus_address_atom,
                         XA_STRING, 8, PropModeReplace,
                         (guchar *) app->a11y_bus_address, strlen (app->a11y_bus_address));
        XFlush (display);
        XCloseDisplay (display);
      }
  }
#endif

  return TRUE;
  
 error:
  close (app->pipefd[0]);
  close (app->pipefd[1]);
  app->state = A11Y_BUS_STATE_ERROR;

  return FALSE;
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  A11yBusLauncher *app = user_data;

  if (g_strcmp0 (method_name, "GetAddress") == 0)
    {
      ensure_a11y_bus (app);
      if (app->a11y_bus_pid > 0)
        g_dbus_method_invocation_return_value (invocation,
                                               g_variant_new ("(s)", app->a11y_bus_address));
      else
        g_dbus_method_invocation_return_dbus_error (invocation,
                                                    "org.a11y.Bus.Error",
                                                    app->a11y_launch_error_message);
    }
}

static GVariant *
handle_get_property  (GDBusConnection       *connection,
                      const gchar           *sender,
                      const gchar           *object_path,
                      const gchar           *interface_name,
                      const gchar           *property_name,
                    GError **error,
                    gpointer               user_data)
{
  A11yBusLauncher *app = user_data;

  if (g_strcmp0 (property_name, "IsEnabled") == 0)
    return g_variant_new ("b", app->a11y_enabled);
  else if (g_strcmp0 (property_name, "ScreenReaderEnabled") == 0)
    return g_variant_new ("b", app->screen_reader_enabled);
  else
    return NULL;
}

static void
handle_a11y_enabled_change (A11yBusLauncher *app, gboolean enabled,
                               gboolean notify_gsettings)
{
  GVariantBuilder builder;
  GVariantBuilder invalidated_builder;

  if (enabled == app->a11y_enabled)
    return;

  app->a11y_enabled = enabled;

  if (notify_gsettings && app->interface_schema)
    {
      g_settings_set_boolean (app->interface_schema, "toolkit-accessibility",
                              enabled);
      g_settings_sync ();
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_init (&invalidated_builder, G_VARIANT_TYPE ("as"));
  g_variant_builder_add (&builder, "{sv}", "IsEnabled",
                         g_variant_new_boolean (enabled));

  g_dbus_connection_emit_signal (app->session_bus, NULL, "/org/a11y/bus",
                                 "org.freedesktop.DBus.Properties",
                                 "PropertiesChanged",
                                 g_variant_new ("(sa{sv}as)", "org.a11y.Status",
                                                &builder,
                                                &invalidated_builder),
                                 NULL);
}

static void
handle_screen_reader_enabled_change (A11yBusLauncher *app, gboolean enabled,
                               gboolean notify_gsettings)
{
  GVariantBuilder builder;
  GVariantBuilder invalidated_builder;

  if (enabled == app->screen_reader_enabled)
    return;

  /* If the screen reader is being enabled, we should enable accessibility
   * if it isn't enabled already */
  if (enabled)
    handle_a11y_enabled_change (app, enabled, notify_gsettings);

  app->screen_reader_enabled = enabled;

  if (notify_gsettings && app->a11y_schema)
    {
      g_settings_set_boolean (app->a11y_schema, "screen-reader-enabled",
                              enabled);
      g_settings_sync ();
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_init (&invalidated_builder, G_VARIANT_TYPE ("as"));
  g_variant_builder_add (&builder, "{sv}", "ScreenReaderEnabled",
                         g_variant_new_boolean (enabled));

  g_dbus_connection_emit_signal (app->session_bus, NULL, "/org/a11y/bus",
                                 "org.freedesktop.DBus.Properties",
                                 "PropertiesChanged",
                                 g_variant_new ("(sa{sv}as)", "org.a11y.Status",
                                                &builder,
                                                &invalidated_builder),
                                 NULL);
}

static gboolean
handle_set_property  (GDBusConnection       *connection,
                      const gchar           *sender,
                      const gchar           *object_path,
                      const gchar           *interface_name,
                      const gchar           *property_name,
                      GVariant *value,
                    GError **error,
                    gpointer               user_data)
{
  A11yBusLauncher *app = user_data;
  const gchar *type = g_variant_get_type_string (value);
  gboolean enabled;
  
  if (g_strcmp0 (type, "b") != 0)
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                       "org.a11y.Status.%s expects a boolean but got %s", property_name, type);
      return FALSE;
    }

  enabled = g_variant_get_boolean (value);

  if (g_strcmp0 (property_name, "IsEnabled") == 0)
    {
      handle_a11y_enabled_change (app, enabled, TRUE);
      return TRUE;
    }
  else if (g_strcmp0 (property_name, "ScreenReaderEnabled") == 0)
    {
      handle_screen_reader_enabled_change (app, enabled, TRUE);
      return TRUE;
    }
  else
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                       "Unknown property '%s'", property_name);
      return FALSE;
    }
}

static const GDBusInterfaceVTable bus_vtable =
{
  handle_method_call,
  NULL, /* handle_get_property, */
  NULL  /* handle_set_property */
};

static const GDBusInterfaceVTable status_vtable =
{
  NULL, /* handle_method_call */
  handle_get_property,
  handle_set_property
};

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  A11yBusLauncher *app = user_data;
  GError *error;
  guint registration_id;
  
  if (connection == NULL)
    {
      g_main_loop_quit (app->loop);
      return;
    }
  app->session_bus = connection;

  if (app->launch_immediately)
    {
      ensure_a11y_bus (app);
      if (app->state == A11Y_BUS_STATE_ERROR)
        {
          g_main_loop_quit (app->loop);
          return;
        }
    }

  error = NULL;
  registration_id = g_dbus_connection_register_object (connection,
                                                       "/org/a11y/bus",
                                                       introspection_data->interfaces[0],
                                                       &bus_vtable,
                                                       _global_app,
                                                       NULL,
                                                       &error);
  if (registration_id == 0)
    {
      g_error ("%s", error->message);
      g_clear_error (&error);
    }

  g_dbus_connection_register_object (connection,
                                     "/org/a11y/bus",
                                     introspection_data->interfaces[1],
                                     &status_vtable,
                                     _global_app,
                                     NULL,
                                     NULL);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  A11yBusLauncher *app = user_data;
  if (app->session_bus == NULL
      && connection == NULL
      && app->a11y_launch_error_message == NULL)
    app->a11y_launch_error_message = g_strdup ("Failed to connect to session bus");
  g_main_loop_quit (app->loop);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  A11yBusLauncher *app = user_data;
  (void) app;
}

static int sigterm_pipefd[2];

static void
sigterm_handler (int signum)
{
  write (sigterm_pipefd[1], "X", 1);
}

static gboolean
on_sigterm_pipe (GIOChannel  *channel,
                 GIOCondition condition,
                 gpointer     data)
{
  A11yBusLauncher *app = data;
  
  g_main_loop_quit (app->loop);

  return FALSE;
}

static void
init_sigterm_handling (A11yBusLauncher *app)
{
  GIOChannel *sigterm_channel;

  if (pipe (sigterm_pipefd) < 0)
    g_error ("Failed to create pipe: %s", strerror (errno));
  signal (SIGTERM, sigterm_handler);

  sigterm_channel = g_io_channel_unix_new (sigterm_pipefd[0]);
  g_io_add_watch (sigterm_channel,
                  G_IO_IN | G_IO_ERR | G_IO_HUP,
                  on_sigterm_pipe,
                  app);
}

static gboolean
already_running ()
{
#ifdef HAVE_X11
  Atom AT_SPI_BUS;
  Atom actual_type;
  Display *bridge_display;
  int actual_format;
  unsigned char *data = NULL;
  unsigned long nitems;
  unsigned long leftover;
  gboolean result = FALSE;

  bridge_display = XOpenDisplay (NULL);
  if (!bridge_display)
	      return FALSE;
      
  AT_SPI_BUS = XInternAtom (bridge_display, "AT_SPI_BUS", False);
  XGetWindowProperty (bridge_display,
		      XDefaultRootWindow (bridge_display),
		      AT_SPI_BUS, 0L,
		      (long) BUFSIZ, False,
		      (Atom) 31, &actual_type, &actual_format,
		      &nitems, &leftover, &data);

  if (data)
  {
    GDBusConnection *bus;
    bus = g_dbus_connection_new_for_address_sync ((const gchar *)data, 0,
                                                  NULL, NULL, NULL);
    if (bus != NULL)
      {
        result = TRUE;
        g_object_unref (bus);
      }
  }

  XCloseDisplay (bridge_display);
  return result;
#else
  return FALSE;
#endif
}

static GSettings *
get_schema (const gchar *name)
{
  const char * const *schemas = NULL;
  gint i;

  schemas = g_settings_list_schemas ();
  for (i = 0; schemas[i]; i++)
  {
    if (!strcmp (schemas[i], name))
      return g_settings_new (schemas[i]);
  }

  return NULL;
}

static void
gsettings_key_changed (GSettings *gsettings, const gchar *key, void *user_data)
{
  gboolean new_val = g_settings_get_boolean (gsettings, key);
  A11yBusLauncher *app = user_data;

  if (!strcmp (key, "toolkit-accessibility"))
    handle_a11y_enabled_change (_global_app, new_val, FALSE);
  else if (!strcmp (key, "screen-reader-enabled"))
    handle_screen_reader_enabled_change (_global_app, new_val, FALSE);
}

static gboolean
_launch_screen_reader(gpointer user_data)
{
   A11yBusLauncher *bl = user_data;
   LOGD("Launching screen reader");

   bundle *kb = NULL;
   gboolean ret = FALSE;

   kb = bundle_create();

   if (kb == NULL)
     {
        LOGD("Can't create bundle");
        return FALSE;
     }
   int operation_error = appsvc_set_operation(kb, APP_CONTROL_OPERATION_SCREEN_READ);
   LOGD("appsvc_set_operation: %i", operation_error);

   bl->pid = appsvc_run_service(kb, 0, NULL, NULL);

   if (bl->pid > 0)
     {
        LOGD("Screen reader launched with pid: %i", bl->pid);
        ret = TRUE;
     }
   else
     {
        LOGD("Can't start screen-reader - error code: %i", bl->pid);
     }


   bundle_free(kb);
   return ret;
}

static gboolean
_launch_screen_reader_repeat_until_success(gpointer user_data) {
    A11yBusLauncher *bl = user_data;

    if (bl->launch_screen_reader_repeats > 100 || bl->pid > 0)
      {
         //do not try anymore
         return FALSE;
      }

    gboolean ret = _launch_screen_reader(user_data);

    if (ret)
      {
         //we managed to
         bl->launch_screen_reader_repeats = 0;
         return FALSE;
      }
    //try again
    return TRUE;
}

static gboolean
_terminate_screen_reader(A11yBusLauncher *bl)
{
   LOGD("Terminating screen reader");
   int ret;
   if (bl->pid <= 0)
     return FALSE;


   int status = aul_app_get_status_bypid(bl->pid);

   if (status < 0)
     {
       LOGD("App with pid %d already terminated", bl->pid);
       bl->pid = 0;
       return TRUE;
     }

   LOGD("terminate process with pid %d", bl->pid);
   if (!aul_terminate_pid(bl->pid))
     {
        bl->pid = 0;
        return TRUE;
     }

   LOGD("Unable to terminate process using aul api. Sending SIGTERM signal");
   ret = kill(bl->pid, SIGTERM);
   if (!ret)
     {
        bl->pid = 0;
        return TRUE;
     }

   LOGD("Unable to terminate process: %d with api or signal.", bl->pid);
   return FALSE;
}

void screen_reader_cb(keynode_t *node, void *user_data)
{
   A11yBusLauncher *bl = user_data;
   int ret;

   ret = vconf_keynode_get_bool(node);
   LOGD("vconf_keynode_get_bool(node): %i", ret);
   if (ret < 0)
     return;

   bl->screen_reader_needed = ret;
   LOGD("bl->screen_reader_needed: %i, bl->pid: %i", ret, bl->pid);
   if (!bl->screen_reader_needed && (bl->pid > 0))
     _terminate_screen_reader(bl);
   else if (bl->screen_reader_needed && (bl->pid <= 0))
     _launch_screen_reader(bl);
}

int
main (int    argc,
      char **argv)
{
#ifdef ATSPI_BUS_LAUNCHER_LOG_TO_FILE
  log_file = fopen("/tmp/at-spi-bus-launcher.log", "a");
#endif

  LOGD("Starting atspi bus launcher");
  GError *error = NULL;
  GMainLoop *loop;
  GDBusConnection *session_bus;
  int name_owner_id;
  gboolean a11y_set = FALSE;
  gboolean screen_reader_set = FALSE;
  gint i;

  if (already_running ())
    {
       LOGD("atspi bus launcher is already running");
       return 0;
    }

  _global_app = g_slice_new0 (A11yBusLauncher);
  _global_app->loop = g_main_loop_new (NULL, FALSE);
  _global_app->launch_screen_reader_repeats = 0;

  for (i = 1; i < argc; i++)
    {
      if (!strcmp (argv[i], "--launch-immediately"))
        _global_app->launch_immediately = TRUE;
      else if (sscanf (argv[i], "--a11y=%d", &_global_app->a11y_enabled) == 2)
        a11y_set = TRUE;
      else if (sscanf (argv[i], "--screen-reader=%d",
                       &_global_app->screen_reader_enabled) == 2)
        screen_reader_set = TRUE;
    else
      g_error ("usage: %s [--launch-immediately] [--a11y=0|1] [--screen-reader=0|1]", argv[0]);
    }

  _global_app->interface_schema = get_schema ("org.gnome.desktop.interface");
  _global_app->a11y_schema = get_schema ("org.gnome.desktop.a11y.applications");

  if (!a11y_set)
    {
      _global_app->a11y_enabled = _global_app->interface_schema
                                  ? g_settings_get_boolean (_global_app->interface_schema, "toolkit-accessibility")
                                  : _global_app->launch_immediately;
    }

  if (!screen_reader_set)
    {
      _global_app->screen_reader_enabled = _global_app->a11y_schema
                                  ? g_settings_get_boolean (_global_app->a11y_schema, "screen-reader-enabled")
                                  : FALSE;
    }

  if (_global_app->interface_schema)
    g_signal_connect (_global_app->interface_schema,
                      "changed::toolkit-accessibility",
                      G_CALLBACK (gsettings_key_changed), _global_app);

  if (_global_app->a11y_schema)
    g_signal_connect (_global_app->a11y_schema,
                      "changed::screen-reader-enabled",
                      G_CALLBACK (gsettings_key_changed), _global_app);

  init_sigterm_handling (_global_app);

  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  name_owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                  "org.a11y.Bus",
                                  G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
                                  on_bus_acquired,
                                  on_name_acquired,
                                  on_name_lost,
                                  _global_app,
                                  NULL);

  int ret = vconf_get_bool(VCONFKEY_SETAPPL_ACCESSIBILITY_TTS, &_global_app->screen_reader_needed);
  if (ret != 0)
    {
      LOGD("Could not read VCONFKEY_SETAPPL_ACCESSIBILITY_TTS key value.\n");
      return FALSE;
    }
  ret = vconf_notify_key_changed(VCONFKEY_SETAPPL_ACCESSIBILITY_TTS, screen_reader_cb, _global_app);
  if(ret != 0)
    {
      LOGD("Could not add information level callback\n");
      return FALSE;
    }
  if (_global_app->screen_reader_needed)
    g_timeout_add_seconds(2,_launch_screen_reader_repeat_until_success, _global_app);

  g_main_loop_run (_global_app->loop);

  if (_global_app->a11y_bus_pid > 0)
    kill (_global_app->a11y_bus_pid, SIGTERM);

  /* Clear the X property if our bus is gone; in the case where e.g. 
   * GDM is launching a login on an X server it was using before,
   * we don't want early login processes to pick up the stale address.
   */
#ifdef HAVE_X11
  {
    Display *display = XOpenDisplay (NULL);
    if (display)
      {
        Atom bus_address_atom = XInternAtom (display, "AT_SPI_BUS", False);
        XDeleteProperty (display,
                         XDefaultRootWindow (display),
                         bus_address_atom);

        XFlush (display);
        XCloseDisplay (display);
      }
  }
#endif

  if (_global_app->a11y_launch_error_message)
    {
      g_printerr ("Failed to launch bus: %s", _global_app->a11y_launch_error_message);
      return 1;
    }
  return 0;
}
