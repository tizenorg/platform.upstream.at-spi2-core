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

#ifndef GESTURE_DETECTOR_H_
#define GESTURE_DETECTOR_H_

#include <glib.h>
#include <glib-object.h>
#include <de-types.h>

typedef struct _SpiGestureDetector SpiGestureDetector;
typedef struct _GestureListener GestureListener;

typedef void (*GestureCB)(gpointer user_data, const Accessibility_GestureEvent *event);

struct _GestureListener {
   Accessibility_GestureType type;
   int states_mask;
   GestureCB callback;
   gpointer user_data;
};

G_BEGIN_DECLS

#define SPI_GESTURE_DETECTOR_TYPE      (spi_gesture_detector_get_type ())
#define SPI_GESTURE_DETECTOR(o)        (G_TYPE_CHECK_INSTANCE_CAST ((o), SPI_GESTURE_DETECTOR_TYPE, SpiGestureDetector))
#define SPI_GESTURE_DETECTOR_CLASS(k)  (G_TYPE_CHECK_CLASS_CAST((k), SPI_GESTURE_DETECTOR_TYPE, SpiGestureDetectorClass))
#define SPI_IS_GESTURE_DETECTOR(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), SPI_GESTURE_DETECTOR_TYPE))
#define SPI_IS_GESTURE_DETECTOR_CLASS(k)    (G_TYPE_CHECK_CLASS_TYPE ((k), SPI_GESTURE_DETECTOR_TYPE))
#define SPI_GESTURE_DETECTOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SPI_GESTURE_DETECTOR_TYPE, SpiGestureDetectorClass))

typedef struct _DetectorFuncs DetectorFuncs;

typedef struct {
     void *data;
     Accessibility_GestureState state;
     GList *callbacks;
     const DetectorFuncs *funcs;
     int active : 1;
} GestureContext;

struct _DetectorFuncs {
     Accessibility_GestureType type;
     void (*init)(GestureContext *ctx);
     void (*feed)(GestureContext *ctx, const Accessibility_TouchEvent *te);
     void (*reset)(GestureContext *ctx);
     void (*shutdown)(GestureContext *ctx);
};

struct _SpiGestureDetector {
  GObject parent;
  GestureContext contexts[Accessibility_GESTURE_LAST_DEFINED];
};

typedef struct {
  GObjectClass parent_class;
} SpiGestureDetectorClass;

GType               spi_gesture_detector_get_type (void);
SpiGestureDetector *spi_gesture_detector_new   (void);

void spi_gesture_detector_feed_touch (SpiGestureDetector *det, const Accessibility_TouchEvent *te);

GestureListener* spi_gesture_listener_new (Accessibility_GestureType type, int states_mask, GestureCB cb, gpointer user_data);

void spi_gesture_detector_add_listener (SpiGestureDetector *gd, GestureListener *listener);
void spi_gesture_detector_del_listener (SpiGestureDetector *gd, GestureListener *listener);

G_END_DECLS

#endif /* end of include guard: GESTURE-DETECTOR_H_ */
