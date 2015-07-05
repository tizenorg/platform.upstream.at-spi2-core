/*
 * AT-SPI - Assistive Technology Service Provider Interface
 * (Gnome Accessibility Project; http://developer.gnome.org/projects/gap)
 *
 * Copyright 2009  Codethink Ltd
 * Copyright 2001, 2002 Sun Microsystems Inc.,
 * Copyright 2001, 2002 Ximian, Inc.
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

#ifndef SPI_DE_TYPES_H_
#define SPI_DE_TYPES_H_

#include <dbus/dbus.h>

typedef unsigned long Accessibility_ControllerEventMask;

struct _SpiPoint {
    gint x;
    gint y;
};
typedef struct _SpiPoint SpiPoint;

typedef enum {
    Accessibility_KEY_PRESSED_EVENT,
    Accessibility_KEY_RELEASED_EVENT,
    Accessibility_BUTTON_PRESSED_EVENT,
    Accessibility_BUTTON_RELEASED_EVENT,
} Accessibility_EventType;

typedef enum {
    Accessibility_KEY_PRESSED,
    Accessibility_KEY_RELEASED,
} Accessibility_KeyEventType;

typedef enum {
    Accessibility_TOUCH_DOWN,
    Accessibility_TOUCH_MOVE,
    Accessibility_TOUCH_UP,
} Accessibility_TouchEventType;

typedef enum {
    Accessibility_KEY_PRESS,
    Accessibility_KEY_RELEASE,
    Accessibility_KEY_PRESSRELEASE,
    Accessibility_KEY_SYM,
    Accessibility_KEY_STRING,
} Accessibility_KeySynthType;

typedef struct _Accessibility_DeviceEvent Accessibility_DeviceEvent;
struct _Accessibility_DeviceEvent
{
  Accessibility_EventType type;
  dbus_uint32_t id;
  dbus_uint32_t hw_code;
  dbus_uint32_t modifiers;
  dbus_uint32_t timestamp;
  char * event_string;
  dbus_bool_t is_text;
};

typedef struct _Accessibility_TouchEvent Accessibility_TouchEvent;
struct _Accessibility_TouchEvent
{
   int device;
   SpiPoint pos;
   Accessibility_TouchEventType type;
   unsigned int timestamp;
};

typedef struct _Accessibility_EventListenerMode Accessibility_EventListenerMode;
struct _Accessibility_EventListenerMode
{
  dbus_bool_t synchronous;
  dbus_bool_t preemptive;
  dbus_bool_t global;
};

typedef struct _Accessibility_KeyDefinition Accessibility_KeyDefinition;
struct _Accessibility_KeyDefinition
{
  dbus_int32_t keycode;
  dbus_int32_t keysym;
  char *keystring;
  dbus_int32_t unused;
};

typedef enum {
   Accessibility_GESTURE_N_FINGERS_LONGPRESS_HOLD,
   Accessibility_GESTURE_N_FINGERS_SINGLE_TAP,
   Accessibility_GESTURE_N_FINGERS_DOUBLE_TAP,
   Accessibility_GESTURE_N_FINGERS_TRIPLE_TAP,
   Accessibility_GESTURE_N_FINGERS_FLICK,
   Accessibility_GESTURE_N_FINGERS_FLICK_RETURN,
   Accessibility_GESTURE_LAST_DEFINED
} Accessibility_GestureType;

typedef enum {
   Accessibility_GESTURE_STATE_BEGIN     = (1 << 0),
   Accessibility_GESTURE_STATE_CONTINUED = (1 << 1),
   Accessibility_GESTURE_STATE_ENDED     = (1 << 2),
   Accessibility_GESTURE_STATE_ABORTED   = (1 << 3)
} Accessibility_GestureState;

typedef enum {
   Accessibility_GESTURE_DIRECTION_UNDEFINED,
   Accessibility_GESTURE_DIRECTION_LEFT,
   Accessibility_GESTURE_DIRECTION_RIGHT,
   Accessibility_GESTURE_DIRECTION_UP,
   Accessibility_GESTURE_DIRECTION_DOWN
} Accessibility_GestureDirection;

typedef struct _Accessibility_GestureEvent Accessibility_GestureEvent;
struct _Accessibility_GestureEvent
{
   Accessibility_GestureType type;
   int states;
   unsigned int timestamp;
   void *gesture_info;
};

typedef struct _Accessibility_GestureTapData Accessibility_GestureTapData;
struct _Accessibility_GestureTapData
{
   gint n_fingers;
   gint n_taps;
   gint x, y;
};

typedef struct _Accessibility_GestureFlickData Accessibility_GestureFlickData;
struct _Accessibility_GestureFlickData
{
   Accessibility_GestureDirection direction;
   gint x1, y1;
   gint x2, y2;
   gint vx, vy;
   gint n_fingers;
};

#endif /* SPI_DE_TYPES_H_ */
