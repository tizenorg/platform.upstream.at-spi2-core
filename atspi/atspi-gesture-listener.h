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

#ifndef _ATSPI_GESTURE_LISTENER_H_
#define _ATSPI_GESTURE_LISTENER_H_

#include "glib-object.h"

#include "atspi-types.h"

G_BEGIN_DECLS

typedef void (*AtspiGestureListenerCB)    (const AtspiGestureEvent *stroke,
                                           void                    *user_data);

typedef void (*AtspiGestureListenerSimpleCB)    (const AtspiGestureEvent *stroke);

#define ATSPI_TYPE_GESTURE_LISTENER                        (atspi_gesture_listener_get_type ())
#define ATSPI_GESTURE_LISTENER(obj)                        (G_TYPE_CHECK_INSTANCE_CAST ((obj), ATSPI_TYPE_GESTURE_LISTENER, AtspiGestureListener))
#define ATSPI_GESTURE_LISTENER_CLASS(klass)                (G_TYPE_CHECK_CLASS_CAST ((klass), ATSPI_TYPE_GESTURE_LISTENER, AtspiGestureListenerClass))
#define ATSPI_IS_GESTURE_LISTENER(obj)                     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATSPI_TYPE_GESTURE_LISTENER))
#define ATSPI_IS_GESTURE_LISTENER_CLASS(klass)             (G_TYPE_CHECK_CLASS_TYPE ((klass), ATSPI_TYPE_GESTURE_LISTENER))
#define ATSPI_GESTURE_LISTENER_GET_CLASS(obj)              (G_TYPE_INSTANCE_GET_CLASS ((obj), ATSPI_TYPE_GESTURE_LISTENER, AtspiGestureListenerClass))

typedef struct _AtspiGestureListener AtspiGestureListener;
struct _AtspiGestureListener
{
  GObject parent;
  guint id;
  GList *callbacks;
};

typedef struct _AtspiGestureListenerClass AtspiGestureListenerClass;
struct _AtspiGestureListenerClass
{
  GObjectClass parent_class;
  void (*gesture_event) (AtspiGestureListener *, const AtspiGestureEvent *);
};

GType atspi_gesture_listener_get_type (void);

AtspiGestureListener *atspi_gesture_listener_new (AtspiGestureListenerCB callback, void *user_data, GDestroyNotify callback_destroyed);

AtspiGestureListener *atspi_gesture_listener_new_simple (AtspiGestureListenerSimpleCB callback, GDestroyNotify callback_destroyed);

void atspi_gesture_listener_add_callback (AtspiGestureListener *listener, AtspiGestureListenerCB callback, GDestroyNotify callback_destroyed, void *user_data);

void atspi_gesture_listener_remove_callback (AtspiGestureListener  *listener, AtspiGestureListenerCB callback);

typedef struct _AtspiGestureTapData AtspiGestureTapData;
struct _AtspiGestureTapData
{
   gint n_fingers;
   gint n_taps;
   gint x, y;
};

/**
 * ATSPI_TYPE_GESTURE_TAP_DATA:
 * 
 * The #GType for a boxed type holding a #AtspiGestureTapData.
 */
#define  ATSPI_TYPE_GESTURE_TAP_DATA (atspi_gesture_tap_data_get_type ())

GType atspi_gesture_tap_data_get_type ();

typedef struct _AtspiGestureFlickData AtspiGestureFlickData;
struct _AtspiGestureFlickData
{
   AtspiGestureDirection direction;
   gint x1, y1;
   gint x2, y2;
   gint n_fingers;
};

/**
 * ATSPI_TYPE_GESTURE_FLICK_DATA:
 * 
 * The #GType for a boxed type holding a #AtspiGestureFlickData.
 */
#define  ATSPI_TYPE_GESTURE_FLICK_DATA (atspi_gesture_flick_data_get_type ())

GType atspi_gesture_flick_data_get_type ();

G_END_DECLS

#endif /* _ATSPI_GESTURE_LISTENER_H_ */
