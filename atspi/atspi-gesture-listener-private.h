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

#ifndef _ATSPI_GESTURE_LISTENER_PRIVATE_H_
#define _ATSPI_GESTURE_LISTENER_PRIVATE_H_

#include "atspi-gesture-listener.h"

#include "dbus/dbus.h"

G_BEGIN_DECLS

DBusHandlerResult _atspi_dbus_handle_GestureEvent (DBusConnection *bus, DBusMessage *message, void *data);

gchar *_atspi_gesture_listener_get_path (AtspiGestureListener *listener);

G_END_DECLS

#endif	/* _ATSPI_GESTURE_LISTENER_H_ */
