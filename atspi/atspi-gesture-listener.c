/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * Author : Lukasz Stanislawski <l.stanislaws@samsung.com>
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

#include "atspi-private.h"
#include <stdio.h>

typedef struct
{
  AtspiGestureListenerCB    callback;
  gpointer user_data;
  GDestroyNotify callback_destroyed;
} GestureEventHandler;

GObjectClass *gesture_parent_class;

void
atspi_gesture_tap_data_free(AtspiGestureTapData *data)
{
  g_free (data);
}

AtspiGestureTapData*
atspi_gesture_tap_data_copy (AtspiGestureTapData *src)
{
  AtspiGestureTapData *dst = g_new (AtspiGestureTapData, 1);
  memcpy(dst, src, sizeof(AtspiGestureTapData));
  return dst;
}

G_DEFINE_BOXED_TYPE (AtspiGestureTapData, atspi_gesture_tap_data, atspi_gesture_tap_data_copy, atspi_gesture_tap_data_free)

void
atspi_gesture_flick_data_free(AtspiGestureFlickData *data)
{
  g_free (data);
}

AtspiGestureFlickData*
atspi_gesture_flick_data_copy (AtspiGestureFlickData *src)
{
  AtspiGestureFlickData *dst = g_new (AtspiGestureFlickData, 1);
  memcpy(dst, src, sizeof(AtspiGestureFlickData));
  return dst;
}

G_DEFINE_BOXED_TYPE (AtspiGestureFlickData, atspi_gesture_flick_data, atspi_gesture_flick_data_copy, atspi_gesture_flick_data_free)

static AtspiGestureEvent *
atspi_gesture_event_copy (const AtspiGestureEvent *src)
{
  AtspiGestureEvent *dst = g_new0 (AtspiGestureEvent, 1);
  dst->type = src->type;
  dst->state = src->state;
  dst->timestamp = src->timestamp;
  g_value_init (&dst->any_data, G_VALUE_TYPE (&src->any_data));
  g_value_copy (&src->any_data, &dst->any_data);
  return dst;
}

void
atspi_gesture_event_free (AtspiGestureEvent *event)
{
  g_value_unset (&event->any_data);
  g_free (event);
}

G_DEFINE_BOXED_TYPE (AtspiGestureEvent,
                     atspi_gesture_event,
                     atspi_gesture_event_copy,
                     atspi_gesture_event_free)
/*
 * Misc. helpers.
 */

static GestureEventHandler *
gesture_event_handler_new (AtspiGestureListenerCB callback,
                          GDestroyNotify callback_destroyed,
                          gpointer user_data)
{
  GestureEventHandler *eh = g_new0 (GestureEventHandler, 1);

  eh->callback = callback;
  eh->callback_destroyed = callback_destroyed;
  eh->user_data = user_data;

  return eh;
}

static void
gesture_remove_datum (const AtspiGestureEvent *event, void *user_data)
{
  AtspiGestureListenerSimpleCB cb = user_data;
  cb (event);
}
  
static void
gesture_event_handler_free (GestureEventHandler *eh)
{
  g_free (eh);
}

static GList *
event_list_remove_by_cb (GList *list, AtspiGestureListenerCB callback)
{
  GList *l, *next;

  for (l = list; l; l = next)
    {
      GestureEventHandler *eh = l->data;
      next = l->next;

      if (eh->callback == callback)
      {
        list = g_list_delete_link (list, l);
        gesture_event_handler_free (eh);
      }
    }

  return list;
}

/*
 * Standard event dispatcher
 */
static guint listener_id = 0;
static GList *gesture_listeners = NULL;

static gboolean
id_is_free (guint id)
{
  GList *l;

  for (l = gesture_listeners; l; l = g_list_next (l))
  {
    AtspiGestureListener *listener = l->data;
    if (listener->id == id) return FALSE;
  }
  return TRUE;
}

/* 
 * Gesture event handler
 */
static void
atspi_gesture_event_dispatch (AtspiGestureListener               *listener,
                              const AtspiGestureEvent *event)
{
  GList *l;

  /* FIXME: re-enterancy hazard on this list */
  for (l = listener->callbacks; l; l = l->next)
    {
      GestureEventHandler *eh = l->data;
      eh->callback (event, eh->user_data);
    }
}

static void
atspi_gesture_listener_init (AtspiGestureListener *listener)
{
  do
  {
    listener->id = listener_id++;
  } while (!id_is_free (listener->id));
  gesture_listeners = g_list_append (gesture_listeners, listener);
}

static void
atspi_gesture_listener_finalize (GObject *object)
{
  AtspiGestureListener *listener = (AtspiGestureListener *) object;
  GList *l;
  
  for (l = listener->callbacks; l; l = l->next)
    {
      gesture_event_handler_free (l->data);
    }
  
  g_list_free (listener->callbacks);

  gesture_parent_class->finalize (object);
}

static void
atspi_gesture_listener_class_init (AtspiGestureListenerClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  gesture_parent_class = g_type_class_peek_parent (klass);
  object_class->finalize = atspi_gesture_listener_finalize;

  klass->gesture_event = atspi_gesture_event_dispatch;
}

G_DEFINE_TYPE (AtspiGestureListener, atspi_gesture_listener,
			  G_TYPE_OBJECT)

/**
 * atspi_gesture_listener_new:
 * @callback: (scope notified): an #AtspiGestureListenerCB callback function,
 *            or NULL.
 * @user_data: (closure): a pointer to data which will be passed to the
 * callback when invoked.
 * @callback_destroyed: A #GDestroyNotify called when the listener is freed
 * and data associated with the callback should be freed. It can be NULL.
 *
 * Creates a new #AtspiGestureListener with a specified callback function.
 *
 * Returns: (transfer full): a pointer to a newly-created #AtspiGestureListener.
 *
 **/
AtspiGestureListener *
atspi_gesture_listener_new (AtspiGestureListenerCB callback,
                           void *user_data,
                           GDestroyNotify callback_destroyed)
{
  AtspiGestureListener *listener = g_object_new (atspi_gesture_listener_get_type (), NULL);

  if (callback)
    atspi_gesture_listener_add_callback (listener, callback, callback_destroyed,
                                       user_data);
  return listener;
}

/**
 * atspi_gesture_listener_new_simple:
 * @callback: (scope notified): an #AtspiGestureListenerCB callback function,
 *            or NULL.
 * @callback_destroyed: A #GDestroyNotify called when the listener is freed
 * and data associated with the callback should be freed.  It an be NULL.
 *
 * Creates a new #AtspiGestureListener with a specified callback function.
 * This method is similar to #atspi_gesture_listener_new, but callback
 * takes no user data.
 *
 * Returns: a pointer to a newly-created #AtspiGestureListener.
 *
 **/
AtspiGestureListener *
atspi_gesture_listener_new_simple (AtspiGestureListenerSimpleCB callback,
                           GDestroyNotify callback_destroyed)
{
  return atspi_gesture_listener_new (gesture_remove_datum, callback, callback_destroyed);
}

/**
 * atspi_gesture_listener_add_callback:
 * @listener: the #AtspiGestureListener instance to modify.
 * @callback: (scope notified): an #AtspiGestureListenerCB function pointer.
 * @callback_destroyed: A #GDestroyNotify called when the listener is freed
 * and data associated with the callback should be freed. It can be NULL.
 * @user_data: (closure): a pointer to data which will be passed to the
 *             callback when invoked. 
 *
 * Adds an in-process callback function to an existing #AtspiGestureListener.
 *
 * Returns: #TRUE if successful, otherwise #FALSE.
 *
 **/
void
atspi_gesture_listener_add_callback (AtspiGestureListener  *listener,
			     AtspiGestureListenerCB callback,
			     GDestroyNotify callback_destroyed,
			     void                      *user_data)
{
  g_return_if_fail (ATSPI_IS_GESTURE_LISTENER (listener));
  GestureEventHandler *new_handler;

  new_handler = gesture_event_handler_new (callback,
                                           callback_destroyed, user_data);

  listener->callbacks = g_list_prepend (listener->callbacks, new_handler);
}

/**
 * atspi_gesture_listener_remove_callback:
 * @listener: the #AtspiGestureListener instance to modify.
 * @callback: (scope call): an #AtspiGestureListenerCB function pointer.
 *
 * Removes an in-process callback function from an existing 
 * #AtspiGestureListener.
 *
 * Returns: #TRUE if successful, otherwise #FALSE.
 *
 **/
void
atspi_gesture_listener_remove_callback (AtspiGestureListener  *listener,
				AtspiGestureListenerCB callback)
{
  g_return_if_fail (ATSPI_IS_GESTURE_LISTENER (listener));

  listener->callbacks = event_list_remove_by_cb (listener->callbacks, (void *) callback);
}

#define DBUS_ITER_GET_BASIC_TYPE_OR_RETURN_VAL(iter, type, var, ret) \
  if (dbus_message_iter_get_arg_type (iter) != (type)) return ret; \
  dbus_message_iter_get_basic ((iter), (var)); \
  dbus_message_iter_next (iter);

#define DBUS_ITER_GET_BASIC_TYPE_OR_RETURN(iter, type, var) \
   DBUS_ITER_GET_BASIC_TYPE_OR_RETURN_VAL(iter, type, var, )

static dbus_bool_t
demarshal_tap_data (DBusMessageIter *iter, AtspiGestureTapData *tap)
{
  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN_VAL(iter, DBUS_TYPE_INT32, &tap->n_fingers, FALSE);
  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN_VAL(iter, DBUS_TYPE_INT32, &tap->n_taps, FALSE);
  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN_VAL(iter, DBUS_TYPE_INT32, &tap->x, FALSE);
  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN_VAL(iter, DBUS_TYPE_INT32, &tap->y, FALSE);

  return TRUE;
}

static dbus_bool_t
demarshal_flick_data (DBusMessageIter *iter, AtspiGestureFlickData *flick)
{
  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN_VAL(iter, DBUS_TYPE_UINT32, &flick->direction, FALSE);
  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN_VAL(iter, DBUS_TYPE_INT32, &flick->x1, FALSE);
  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN_VAL(iter, DBUS_TYPE_INT32, &flick->y1, FALSE);
  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN_VAL(iter, DBUS_TYPE_INT32, &flick->x2, FALSE);
  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN_VAL(iter, DBUS_TYPE_INT32, &flick->y2, FALSE);
  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN_VAL(iter, DBUS_TYPE_INT32, &flick->n_fingers, FALSE);

  return TRUE;
}

static void
read_gesture_event_from_iter (DBusMessageIter *iter, AtspiGestureEvent *event)
{
  DBusMessageIter iter_variant, iter_struct;
  AtspiGestureTapData tap_data;
  AtspiGestureFlickData flick_data;

  memset (event, 0x0, sizeof(AtspiGestureEvent));

  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN(iter, DBUS_TYPE_UINT32, &event->type);
  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN(iter, DBUS_TYPE_UINT32, &event->state);
  DBUS_ITER_GET_BASIC_TYPE_OR_RETURN(iter, DBUS_TYPE_UINT32, &event->timestamp);

  if (dbus_message_iter_get_arg_type (iter) != DBUS_TYPE_VARIANT)
    {
       g_warning ("Invalid signature. Expected type variant.");
       return;
    }

  dbus_message_iter_recurse (iter, &iter_variant);
  if (dbus_message_iter_get_arg_type (&iter_variant) == DBUS_TYPE_STRUCT)
  {
     dbus_message_iter_recurse (&iter_variant, &iter_struct);
     switch (event->type)
     {
      case ATSPI_GESTURE_N_FINGERS_LONGPRESS_HOLD:
      case ATSPI_GESTURE_N_FINGERS_SINGLE_TAP:
      case ATSPI_GESTURE_N_FINGERS_DOUBLE_TAP:
      case ATSPI_GESTURE_N_FINGERS_TRIPLE_TAP:
        if (demarshal_tap_data (&iter_struct, &tap_data))
         {
           g_value_init (&event->any_data, ATSPI_TYPE_GESTURE_TAP_DATA);
           g_value_set_boxed (&event->any_data, &tap_data);
         }
        else
          g_warning ("Failed to demarshal tap data");
         break;
      case ATSPI_GESTURE_N_FINGERS_FLICK:
      case ATSPI_GESTURE_N_FINGERS_FLICK_RETURN:
        if (demarshal_flick_data (&iter_struct, &flick_data))
         {
           g_value_init (&event->any_data, ATSPI_TYPE_GESTURE_FLICK_DATA);
           g_value_set_boxed (&event->any_data, &flick_data);
         }
        else
          g_warning ("Failed to demarshal flick data");
         break;
      default:
         break;
     }
  }
  else
    g_warning ("Invalid signature. Expected type struct.");
}

DBusHandlerResult
_atspi_dbus_handle_GestureEvent (DBusConnection *bus, DBusMessage *message, void *data)
{
  const char *path = dbus_message_get_path (message);
  int id;
  AtspiGestureEvent event;
  AtspiGestureListener *listener;
  DBusMessageIter iter;
  AtspiGestureListenerClass *klass;
  dbus_bool_t retval = FALSE;
  GList *l;
  DBusMessage *reply;

  if (strcmp (dbus_message_get_signature (message), "uuuv") != 0)
  {
    g_warning ("Atspi: Unknown signature for an event. Expected 'uuuv', got '%s'",
               dbus_message_get_signature (message));
    goto done;
  }

  if (sscanf (path, "/org/a11y/atspi/gesture/listeners/%d", &id) != 1)
  {
    g_warning ("Atspi: Bad listener path: %s\n", path);
    goto done;
  }

  for (l = gesture_listeners; l; l = g_list_next (l))
  {
    listener = l->data;
    if (listener->id == id) break;
  }

  if (!l)
  {
    goto done;
  }

  dbus_message_iter_init (message, &iter);
  read_gesture_event_from_iter (&iter, &event);
  klass = ATSPI_GESTURE_LISTENER_GET_CLASS (listener);
  if (klass->gesture_event)
  {
    (*klass->gesture_event) (listener, &event);
  }
done:
  reply = dbus_message_new_method_return (message);
  if (reply)
  {
    dbus_message_append_args (reply, DBUS_TYPE_BOOLEAN, &retval, DBUS_TYPE_INVALID);
    dbus_connection_send (_atspi_bus(), reply, NULL);
    dbus_message_unref (reply);
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

gchar *
_atspi_gesture_listener_get_path (AtspiGestureListener *listener)
{  return g_strdup_printf ("/org/a11y/atspi/gesture/listeners/%d", listener->id);
}

