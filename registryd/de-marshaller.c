/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2008 Novell, Inc.
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

#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include <dbus/dbus.h>

#include "de-types.h"

dbus_bool_t spi_dbus_message_iter_get_struct(DBusMessageIter *iter, ...)
{
  va_list args;
  DBusMessageIter iter_struct;
  int type;
  void *ptr;

  dbus_message_iter_recurse(iter, &iter_struct);
  va_start(args, iter);
  for (;;)
  {
    type = va_arg(args, int);
    if (type == DBUS_TYPE_INVALID) break;
    if (type != dbus_message_iter_get_arg_type(&iter_struct))
    {
      va_end(args);
      return FALSE;
    }
    ptr = va_arg(args, void *);
    dbus_message_iter_get_basic(&iter_struct, ptr);
    dbus_message_iter_next(&iter_struct);
  }
  dbus_message_iter_next(iter);
  va_end(args);
  return TRUE;
}

dbus_bool_t spi_dbus_message_iter_append_struct(DBusMessageIter *iter, ...)
{
  va_list args;
  DBusMessageIter iter_struct;
  int type;
  void *ptr;

  if (!dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &iter_struct)) return FALSE;
  va_start(args, iter);
  for (;;)
  {
    type = va_arg(args, int);
    if (type == DBUS_TYPE_INVALID) break;
    ptr = va_arg(args, void *);
    dbus_message_iter_append_basic(&iter_struct, type, ptr);
  }
  if (!dbus_message_iter_close_container(iter, &iter_struct)) return FALSE;
  va_end(args);
  return TRUE;
}

dbus_bool_t spi_dbus_marshal_deviceEvent(DBusMessage *message, const Accessibility_DeviceEvent *e)
{
  DBusMessageIter iter;

  if (!message) return FALSE;
  dbus_message_iter_init_append(message, &iter);
  return spi_dbus_message_iter_append_struct(&iter, DBUS_TYPE_UINT32, &e->type, DBUS_TYPE_INT32, &e->id, DBUS_TYPE_UINT32, &e->hw_code, DBUS_TYPE_UINT32, &e->modifiers, DBUS_TYPE_INT32, &e->timestamp, DBUS_TYPE_STRING, &e->event_string, DBUS_TYPE_BOOLEAN, &e->is_text, DBUS_TYPE_INVALID);
}

dbus_bool_t spi_dbus_demarshal_deviceEvent(DBusMessage *message, Accessibility_DeviceEvent *e)
{
  DBusMessageIter iter;
  dbus_uint16_t hw_code;
  dbus_uint16_t modifiers;

  dbus_message_iter_init(message, &iter);
  if (spi_dbus_message_iter_get_struct(&iter, DBUS_TYPE_UINT32, &e->type, DBUS_TYPE_INT32, &e->id, DBUS_TYPE_INT32, &e->hw_code, DBUS_TYPE_INT32, &e->modifiers, DBUS_TYPE_INT32, &e->timestamp, DBUS_TYPE_STRING, &e->event_string, DBUS_TYPE_BOOLEAN, &e->is_text, DBUS_TYPE_INVALID))
    return TRUE;
  /* TODO: Perhaps remove the below code for 2.1 */
  if (!spi_dbus_message_iter_get_struct(&iter, DBUS_TYPE_UINT32, &e->type, DBUS_TYPE_INT32, &e->id, DBUS_TYPE_INT16, &hw_code, DBUS_TYPE_INT16, &modifiers, DBUS_TYPE_INT32, &e->timestamp, DBUS_TYPE_STRING, &e->event_string, DBUS_TYPE_BOOLEAN, &e->is_text, DBUS_TYPE_INVALID))
    return FALSE;
  e->hw_code = hw_code;
  e->modifiers = modifiers;
  return TRUE;
}

dbus_bool_t spi_dbus_marshal_gestureEvent(DBusMessage *message, const Accessibility_GestureEvent *e)
{
  DBusMessageIter iter, variant, iter_struct;
  Accessibility_GestureTapData *td;
  Accessibility_GestureFlickData *fd;

  if (!message) return FALSE;
  dbus_message_iter_init_append (message, &iter);

  dbus_message_iter_append_basic (&iter, DBUS_TYPE_UINT32, &e->type);
  dbus_message_iter_append_basic (&iter, DBUS_TYPE_UINT32, &e->states);
  dbus_message_iter_append_basic (&iter, DBUS_TYPE_UINT32, &e->timestamp);

  switch (e->type)
    {
     case Accessibility_GESTURE_N_FINGERS_LONGPRESS_HOLD:
     case Accessibility_GESTURE_N_FINGERS_SINGLE_TAP:
     case Accessibility_GESTURE_N_FINGERS_DOUBLE_TAP:
     case Accessibility_GESTURE_N_FINGERS_TRIPLE_TAP:
        dbus_message_iter_open_container (&iter, DBUS_TYPE_VARIANT, "(iiii)", &variant);
        dbus_message_iter_open_container (&variant, DBUS_TYPE_STRUCT, NULL, &iter_struct);
        td = e->gesture_info;
        if (td)
          {
             dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32, &td->n_fingers);
             dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32, &td->n_taps);
             dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32, &td->x);
             dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32, &td->y);
          }
        dbus_message_iter_close_container (&variant, &iter_struct);
        break;
     case Accessibility_GESTURE_N_FINGERS_FLICK:
        dbus_message_iter_open_container (&iter, DBUS_TYPE_VARIANT, "(uiiiii)", &variant);
        dbus_message_iter_open_container (&variant, DBUS_TYPE_STRUCT, NULL, &iter_struct);
        fd = e->gesture_info;
        if (fd)
          {
             dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_UINT32, &fd->direction);
             dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32, &fd->x1);
             dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32, &fd->y1);
             dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32, &fd->x2);
             dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32, &fd->y2);
             dbus_message_iter_append_basic (&iter_struct, DBUS_TYPE_INT32, &fd->n_fingers);
          }
        dbus_message_iter_close_container (&variant, &iter_struct);
        break;
     default:
        return FALSE;
    }
  dbus_message_iter_close_container (&iter, &variant);
  return TRUE;
}

dbus_bool_t spi_dbus_demarshal_gestureEvent(DBusMessage *message, Accessibility_GestureEvent *e)
{
  if (!message) return FALSE;

  return TRUE;
}
