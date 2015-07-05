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

#include "gesture-detector.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

/** Gesture detectors functions */

static void _flick_init(GestureContext *ctx);
static void _flick_feed(GestureContext *ctx, const Accessibility_TouchEvent *pi);
static void _flick_reset(GestureContext *ctx);
static void _flick_shutdown(GestureContext *ctx);

static void _single_tap_init(GestureContext *ctx);
static void _tap_feed(GestureContext *ctx, const Accessibility_TouchEvent *pi);
static void _tap_reset(GestureContext *ctx);
static void _tap_shutdown(GestureContext *ctx);

static void _flick_return_init(GestureContext *ctx);
static void _flick_return_feed(GestureContext *ctx, const Accessibility_TouchEvent *pi);
static void _flick_return_reset(GestureContext *ctx);
static void _flick_return_shutdown(GestureContext *ctx);

static void _double_tap_init(GestureContext *ctx);
static void _triple_tap_init(GestureContext *ctx);

/** Gesture detectors functions - END */

static void _gesture_context_init(GestureContext *ctx, const DetectorFuncs *funcs);
static void _gesture_context_shutdown(GestureContext *ctx);

G_DEFINE_TYPE(SpiGestureDetector, spi_gesture_detector, G_TYPE_OBJECT)

// keep it in enum order
static const DetectorFuncs funcs[] = {
       { Accessibility_GESTURE_N_FINGERS_LONGPRESS_HOLD, NULL, NULL, NULL, NULL},
       { Accessibility_GESTURE_N_FINGERS_SINGLE_TAP, _single_tap_init, _tap_feed, _tap_reset, _tap_shutdown },
       { Accessibility_GESTURE_N_FINGERS_DOUBLE_TAP, _double_tap_init, _tap_feed, _tap_reset, _tap_shutdown },
       { Accessibility_GESTURE_N_FINGERS_TRIPLE_TAP, _triple_tap_init, _tap_feed, _tap_reset, _tap_shutdown },
       { Accessibility_GESTURE_N_FINGERS_FLICK, _flick_init, _flick_feed, _flick_reset, _flick_shutdown },
       { Accessibility_GESTURE_N_FINGERS_FLICK_RETURN, _flick_return_init, _flick_return_feed, _flick_return_reset, _flick_return_shutdown },
       { Accessibility_GESTURE_LAST_DEFINED, NULL, NULL, NULL, NULL }
};

void spi_gesture_detector_feed_touch(SpiGestureDetector *det, const Accessibility_TouchEvent *te)
{
   int i;
   for (i = 0; i < Accessibility_GESTURE_LAST_DEFINED; i++)
     {
        GestureContext *ctx = &det->contexts[i];
        if (ctx->active) ctx->funcs->feed(ctx, te);
     }
}

SpiGestureDetector *spi_gesture_detector_new(void)
{
   SpiGestureDetector *ret;

   return g_object_new (SPI_GESTURE_DETECTOR_TYPE, NULL);
}

static void
spi_gesture_detector_object_finalize (GObject *object)
{
   SpiGestureDetector *gd;
   int i;

   gd = SPI_GESTURE_DETECTOR (object);

   for (i = 0; i < Accessibility_GESTURE_LAST_DEFINED; i++)
     _gesture_context_shutdown(&gd->contexts[i]);
}

static void
spi_gesture_detector_init (SpiGestureDetector *gd)
{
   int i;

   for (i = 0; funcs[i].type != Accessibility_GESTURE_LAST_DEFINED; i++)
     _gesture_context_init(&gd->contexts[i], &funcs[i]);
}

static void
spi_gesture_detector_class_init (SpiGestureDetectorClass *klass)
{
  GObjectClass * object_class = (GObjectClass *) klass;

  spi_gesture_detector_parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = spi_gesture_detector_object_finalize;
}

static void
_gesture_context_state_set(GestureContext *ctx, Accessibility_GestureState state, void *event_info)
{
   GList *l;
   GestureListener *cd;
   Accessibility_GestureEvent event;

   event.type = ctx->funcs->type;
   event.states = state;
   event.gesture_info = event_info;

   for (l = ctx->callbacks; l != NULL; l = l->next)
     {
        cd = l->data;
        if ((event.type == cd->type) && (cd->states_mask & state) && (cd->callback))
          cd->callback(cd->user_data, &event);
     }
   ctx->state = state;
}

static inline Accessibility_GestureState
_gesture_context_state_get(GestureContext *ctx)
{
   return ctx->state;
}

static inline void
_gesture_context_data_set(GestureContext *ctx, void *data)
{
   ctx->data = data;
}

static inline void*
_gesture_context_data_get(GestureContext *ctx)
{
   return ctx->data;
}

static void
_gesture_context_activate(GestureContext *ctx)
{
   if (!ctx->active)
     {
        ctx->active = 1;
        ctx->funcs->init(ctx);
     }
}

static void
_gesture_context_deactivate(GestureContext *ctx)
{
   if (ctx->active)
     {
        ctx->active = 0;
        ctx->funcs->shutdown(ctx);
        ctx->data = NULL;
     }
}

static void
_gesture_context_shutdown(GestureContext *ctx)
{
   _gesture_context_deactivate(ctx);

   g_list_free (ctx->callbacks);
   ctx->callbacks = NULL;
}

static void
_gesture_context_init(GestureContext *ctx, const DetectorFuncs *funcs)
{
   ctx->state = 0;
   ctx->funcs = funcs;
   ctx->active = 0;
   ctx->callbacks = ctx->data = NULL;
}

GestureListener* spi_gesture_listener_new (Accessibility_GestureType type, int states_mask, GestureCB cb, gpointer user_data)
{
   GestureListener *ret;

   ret = g_new (GestureListener, 1);
   if (!ret) return NULL;

   ret->type = type;
   ret->states_mask = states_mask;
   ret->callback = cb;
   ret->user_data = user_data;

   return ret;
}

void spi_gesture_detector_add_listener (SpiGestureDetector *gd, GestureListener *listener)
{
   g_return_if_fail (listener != NULL);
   _gesture_context_activate(&gd->contexts[listener->type]);
   gd->contexts[listener->type].callbacks = g_list_append (gd->contexts[listener->type].callbacks, listener);
}

void spi_gesture_detector_del_listener (SpiGestureDetector *gd, GestureListener *listener)
{
   g_return_if_fail (listener != NULL);
   gd->contexts[listener->type].callbacks = g_list_remove (gd->contexts[listener->type].callbacks, listener);

   if (!gd->contexts[listener->type].callbacks)
     _gesture_context_deactivate(&gd->contexts[listener->type]);
}

// FLICK GESTURE RECOGNITION FUNCS

typedef struct {
     SpiPoint start, end;
     unsigned int start_time;
     unsigned int end_time;
     int device;
     double current_angle;
     double angle0;
     double length;
} LineInfo;

typedef struct {
     unsigned int flick_gesture_timeout;
     int line_min_length;
     int line_max_length;
     double line_angle_tolerance;
     int line_time_limit;
     GList *lines;
     Accessibility_GestureFlickData info;
} FlickPrivateData;

static void _flick_init(GestureContext *ctx)
{
   FlickPrivateData *data = calloc(sizeof(FlickPrivateData), 1);

   data->line_min_length = 40; // FIXME implement some config for this
   data->line_max_length = 20.0 * data->line_min_length;
   data->flick_gesture_timeout = 150; //value taken from elementary defaults
   data->line_angle_tolerance = 20.0; //value from elementary defaults
   data->line_time_limit = 150; // value from elementary defaults

   _gesture_context_data_set(ctx, data);
   _gesture_context_state_set(ctx, 0, NULL);
}

static inline LineInfo*
_flick_get_line_for_device(const GList *list, int device)
{
   LineInfo *ret;
   const GList *l;

   for (l = list; l != NULL; l = l->next)
     {
        ret = l->data;
        if (ret->device == device)
          return ret;
     }
   return NULL;
}

static LineInfo*
_flick_line_add(const Accessibility_TouchEvent *pi)
{
   LineInfo *ret = calloc(sizeof(LineInfo), 1);

   ret->device = pi->device;
   ret->start_time = pi->timestamp;
   ret->start = pi->pos;

   return ret;
}

static inline double
_angle_get(SpiPoint start, SpiPoint end)
{
   double ret;
   int dx = start.x - end.x;
   int dy = start.y - end.y;

   ret = atan2(dy, dx) * 180.0 / M_PI;

   return ret;
}

static inline double
_distance(SpiPoint a, SpiPoint b)
{
   double dx = a.x - b.x;
   double dy = a.y - b.y;
   return sqrt(dx * dx + dy * dy);
}

static void
_flick_line_update(LineInfo *li, const Accessibility_TouchEvent *pi)
{
   li->end = pi->pos;
   li->end_time = pi->timestamp;

   li->current_angle = _angle_get(li->start, li->end);
   if (!li->angle0)
     li->angle0 = li->current_angle;
   li->length = _distance(li->start, li->end);
}

static void
_flick_info_update(FlickPrivateData *data, const Accessibility_TouchEvent *pi)
{
   LineInfo *li;
   GList *l;

   data->info.n_fingers = g_list_length(data->lines);
   data->info.direction = Accessibility_GESTURE_DIRECTION_UNDEFINED;

   data->info.x1 = data->info.y1 = 0;
   data->info.x2 = data->info.y2 = 0;

   for(l = data->lines; l != NULL; l = l->next)
     {
        li = l->data;
        data->info.x1 += li->start.x;
        data->info.y1 += li->start.y;
        data->info.x2 += li->end.x;
        data->info.y2 += li->end.y;
     }

   data->info.x1 /= data->info.n_fingers;
   data->info.y1 /= data->info.n_fingers;
   data->info.x2 /= data->info.n_fingers;
   data->info.y2 /= data->info.n_fingers;

   int dx = data->info.x1 - data->info.x2;
   int dy = data->info.y1 - data->info.y2;

   if (abs(dy) > abs(dx))
     {
        if (dy > 0)
          data->info.direction = Accessibility_GESTURE_DIRECTION_UP;
        else
          data->info.direction = Accessibility_GESTURE_DIRECTION_DOWN;
     }
   else
     {
        if (dx > 0)
          data->info.direction = Accessibility_GESTURE_DIRECTION_RIGHT;
        else
          data->info.direction = Accessibility_GESTURE_DIRECTION_LEFT;
     }
}

/**
 * create line with velocity and angle
 * line can be ended if velocity is under some threshold value or finger is up
 */
static void _flick_feed(GestureContext *ctx, const Accessibility_TouchEvent *pi)
{
   FlickPrivateData *data = _gesture_context_data_get(ctx);
   Accessibility_GestureState state = _gesture_context_state_get(ctx);
   LineInfo *li = _flick_get_line_for_device(data->lines, pi->device);

   switch (pi->type)
     {
      case Accessibility_TOUCH_MOVE:
         if (li)
           {
              if (state == Accessibility_GESTURE_STATE_ABORTED)
                return;

              /* Line gesture for finger is ongoing */
              if (li->end_time)
                {
                   _flick_line_update(li, pi);

                   // if gesture too long
                   if (li->length > data->line_max_length)
                     {
                        _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_ABORTED, &data->info);
                        return;
                     }
                   // if gesture not in straight line
                   double adiff = fabs(li->angle0 - li->current_angle);
                   if (adiff > 180.0) adiff = 360.0 - adiff;
                   if (adiff > data->line_angle_tolerance)
                     {
                        _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_ABORTED, &data->info);
                        return;
                     }
                   // if gesture time is too long
                   if ((pi->timestamp - li->start_time) > data->line_time_limit)
                     {
                        _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_ABORTED, &data->info);
                        return;
                     }
                   _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_CONTINUED, &data->info);
                }
              else
                {
                   double d = _distance(li->start, pi->pos);
                   if (d > data->line_min_length)
                     {
                        _flick_line_update(li, pi);
                        _flick_info_update(data, pi);
                        _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_CONTINUED, &data->info);
                     }
                }
           }
         break;
      case Accessibility_TOUCH_DOWN:
         if (!li)
           {
              li = _flick_line_add(pi);
              data->lines = g_list_append(data->lines, li);

              if (g_list_length(data->lines) == 1)
                 _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_BEGIN, &data->info);
           }
         break;
      case Accessibility_TOUCH_UP:
         if (li)
           {
              data->lines = g_list_remove(data->lines, li);
              free(li);

              if (!g_list_length(data->lines))
                {
                   if (state == Accessibility_GESTURE_STATE_ABORTED)
                    _gesture_context_state_set(ctx, 0, &data->info);
                   else if (state == Accessibility_GESTURE_STATE_CONTINUED)
                     _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_ENDED, &data->info);
                   else
                     _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_ABORTED, &data->info);
                }
           }
         break;
     }
}

static void _flick_reset(GestureContext *ctx)
{
   GList *l;
   FlickPrivateData *data = _gesture_context_data_get(ctx);
   for (l = data->lines; l != NULL; l = l->next)
     free(l->data);
   g_list_free(data->lines);
   data->lines = NULL;

   _gesture_context_state_set(ctx, 0, NULL);
}

static void _flick_shutdown(GestureContext *ctx)
{
   GList *l;
   FlickPrivateData *data = _gesture_context_data_get(ctx);
   for (l = data->lines; l != NULL; l++)
     free(l->data);
   g_list_free(data->lines);
   free(data);
}

// FLICK GESTURE RECOGNITION FUNCS - END

// TAP GESTURE RECOGNITION FUNCS

typedef struct {
   SpiPoint point;
   unsigned int timestamp;
   int device;
   int taps;
   int finished : 1;
} TapPlace;

typedef struct {
   GList *taps;
   int timer;
   double tap_timeout;
   int finger_size;
   Accessibility_GestureTapData info;
   int fingers_down;
   gboolean tap_points_set;
   int n_taps;
   int taps_required;
} TapPrivateData;

static void _single_tap_init(GestureContext *ctx)
{
   TapPrivateData *pd = calloc(1, sizeof(TapPrivateData));

   pd->tap_timeout = 250;
   pd->finger_size = 2 * 40;
   pd->taps_required = 1;

   _gesture_context_data_set(ctx, pd);
}

static void _double_tap_init(GestureContext *ctx)
{
   TapPrivateData *pd = calloc(1, sizeof(TapPrivateData));

   pd->tap_timeout = 350;
   pd->finger_size = 2 * 40;
   pd->taps_required = 2;

   _gesture_context_data_set(ctx, pd);
}

static void _triple_tap_init(GestureContext *ctx)
{
   TapPrivateData *pd = calloc(1, sizeof(TapPrivateData));

   pd->tap_timeout = 850;
   pd->finger_size = 2 * 40;
   pd->taps_required = 3;

   _gesture_context_data_set(ctx, pd);
}

static TapPlace*
_tap_for_device_get(GList *taps, int device)
{
   TapPlace *ret;
   GList *l;

   for(l = taps; l != NULL; l = l->next)
     {
        ret = l->data;
        if (!ret->finished && (ret->device == device))
          return ret;
     }

   return NULL;
}

static TapPlace*
_tap_point_get(TapPrivateData *data, const Accessibility_TouchEvent *pi)
{
   GList *l;
   TapPlace *tp = NULL;
   
   for(l = data->taps; l != NULL; l = l->next)
     {
        tp = l->data;
        if (tp->finished && (_distance(tp->point, pi->pos) < data->finger_size))
          {
             tp->taps++;
             tp->timestamp = pi->timestamp;
             tp->finished = 0;
             tp->device = pi->device;
             return tp;
          }
     }
   return NULL;
}

static TapPlace*
_tap_point_new(TapPrivateData *data, const Accessibility_TouchEvent *pi)
{
   TapPlace *tp;
   tp = malloc(sizeof(TapPlace));
   tp->point = pi->pos;
   tp->timestamp = pi->timestamp;
   tp->device = pi->device;
   tp->finished = 0;
   tp->taps = 1;

   data->taps = g_list_append(data->taps, tp);

   return tp;
}

static void
_tap_data_reset(TapPrivateData *pd)
{
   GList *l;
   for (l = pd->taps; l != NULL; l = l->next)
     free(l->data);
   g_list_free(pd->taps);

   if (pd->timer) g_source_remove(pd->timer);

   memset(&pd->info, 0x0, sizeof(Accessibility_GestureTapData));
   pd->timer = 0;
   pd->taps = NULL;
   pd->tap_points_set = FALSE;
   pd->n_taps = 0;
}

static gboolean
_tap_timer_cb(void *data)
{
   GestureContext *ctx = data;
   TapPrivateData *pd = _gesture_context_data_get(ctx);
   GList *l;
   TapPlace *tp;
   int finished_counter = 0;

   for (l = pd->taps; l != NULL; l= l->next)
     {
        tp = l->data;
        if (!tp->finished)
          {
             finished_counter++;
             break;
          }
     }

   if (finished_counter == pd->taps_required)
      _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_ENDED, &pd->info);
   else {
      _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_ABORTED, &pd->info);
      if (pd->fingers_down == 0)
        _gesture_context_state_set(ctx, 0, &pd->info);
   }

   pd->timer = 0;
   _tap_data_reset(pd);

   return FALSE;
}

static void
_tap_info_update(TapPrivateData *pd)
{
   GList *l;
   TapPlace *tp;
   pd->info.n_fingers = g_list_length(pd->taps);
   pd->info.x = pd->info.y = 0;

   for (l = pd->taps; l != NULL; l = l->next)
     {
        tp = l->data;
        pd->info.x += tp->point.x;
        pd->info.y += tp->point.y;
        pd->n_taps = tp->taps > pd->n_taps ? tp->taps : pd->n_taps;
     }

   pd->info.n_taps = pd->n_taps;
   pd->info.x /= pd->info.n_fingers;
   pd->info.y /= pd->info.n_fingers;
}

static void _tap_feed(GestureContext *ctx, const Accessibility_TouchEvent *pi)
{
   TapPrivateData *pd = _gesture_context_data_get(ctx);
   Accessibility_GestureState state = _gesture_context_state_get(ctx);
   TapPlace *tap = NULL;

   switch (pi->type)
     {
        /**
         * Check if user don't move fingers while tapping
         */
      case Accessibility_TOUCH_MOVE:
         if (state != Accessibility_GESTURE_STATE_CONTINUED &&
             state != Accessibility_GESTURE_STATE_BEGIN)
           goto check;
         tap = _tap_for_device_get(pd->taps, pi->device);
         // abort if outside of finger size
         if (_distance(tap->point, pi->pos) > pd->finger_size)
           {
              goto abort;
           }
         break;

      case Accessibility_TOUCH_DOWN:
         pd->fingers_down++;
         if (state == Accessibility_GESTURE_STATE_ABORTED)
           goto check;
         if (!pd->tap_points_set) {
            // create new tap point
            tap = _tap_point_new(pd, pi);
         }
         else {
           // reuse existing tap point
           tap = _tap_point_get(pd, pi);
         }
         if (!tap) {
           goto abort;
         }
         if (pd->timer)
           g_source_remove(pd->timer);
         pd->timer = g_timeout_add(pd->tap_timeout, _tap_timer_cb, ctx);

         _tap_info_update(pd);

         if (g_list_length(pd->taps) == 1 && tap->taps == 1)
            // if first tap - start gesture
           _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_BEGIN, &pd->info);
         else
            // if sequential tap - continue gesture
           _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_CONTINUED, &pd->info);
         break;

      case Accessibility_TOUCH_UP:
         pd->fingers_down--;
         if (state == Accessibility_GESTURE_STATE_ABORTED)
           goto check;
         // after first finger goes up, we fix tap points.
         if (!pd->tap_points_set)
           pd->tap_points_set = TRUE;
         tap = _tap_for_device_get(pd->taps, pi->device);
         if (!tap) return;
         // abort time between tap down and up is too long
         if ((pi->timestamp - tap->timestamp) > pd->tap_timeout)
           {
              goto abort;
           }
         else
           tap->finished = 1;
         break;
     }

   return;

abort:
   _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_ABORTED, &pd->info);
   _tap_data_reset(pd);
check:
   if (!pd->fingers_down)
     _gesture_context_state_set(ctx, 0, &pd->info);
}

static void _tap_reset(GestureContext *ctx)
{
   TapPrivateData *pd = _gesture_context_data_get(ctx);
   GList *l;

   if (pd->timer) g_source_remove(pd->timer);
   for (l = pd->taps; l != NULL; l = l->next)
     free(l->data);
   g_list_free(pd->taps);
   memset(pd, 0x0, sizeof(TapPrivateData));
}

static void _tap_shutdown(GestureContext *ctx)
{
   TapPrivateData *pd = _gesture_context_data_get(ctx);
   GList *l;

   if (pd->timer) g_source_remove(pd->timer);
   for (l = pd->taps; l != NULL; l = l->next)
     free(l->data);
   g_list_free(pd->taps);
   free(pd);
}
// TAP GESTURE RECOGNITION FUNCS - END

typedef struct
{
   double x, y;
} SpiVector;

enum
{
   STARTED,
   FORWARDING,
   RETURNING,
   INVALID,
} FlickReturnPhase;

typedef struct
{
   SpiPoint origin;     //origin of gesture (where finger was intially presed)
   SpiPoint inflection; //point where first flick was ended.
   SpiVector flick_dir; //direction vector of initial flick
   SpiVector return_flick_dir; //direction vector of return flick
   int dev;
   unsigned int timestamp; //timestamp of finger down event
   int phase; //indicate the phase of return flick
   double inflection_point_distance; //distance from inflection point to origin
   Accessibility_GestureDirection direction; // direction of flick
} ReturnFlick;

typedef struct
{
   int flick_return_max_time;
   int flick_return_min_length;
   int finger_size;
   double angular_tolerance;
   double inflection_angular_tolerance;
   double return_angular_tolerance;
   Accessibility_GestureFlickData info;
   GList *return_flicks;
} FlickReturnPrivateData;

static void _flick_return_init(GestureContext *ctx)
{
   FlickReturnPrivateData *data = calloc(1, sizeof(FlickReturnPrivateData));

   data->flick_return_max_time = 600;
   data->flick_return_min_length = 90;
   data->finger_size = 80;
   data->angular_tolerance = 20.0;
   data->return_angular_tolerance = 25.0;
   data->inflection_angular_tolerance = 80.0;

   _gesture_context_data_set(ctx, data);
}

static void _flick_return_shutdown(GestureContext *ctx)
{
   _flick_return_reset(ctx);

   FlickReturnPrivateData *data = _gesture_context_data_get(ctx);
   free(data);
}

static void _flick_return_reset(GestureContext *ctx)
{
   GList *l;
   FlickReturnPrivateData *data = _gesture_context_data_get(ctx);
   for (l = data->return_flicks; l; l = l->next)
     free(l->data);
   g_list_free(data->return_flicks);
   data->return_flicks = NULL;
}

static ReturnFlick*
_flick_return_touch_point_new (const Accessibility_TouchEvent *te)
{
   ReturnFlick *ret = calloc(1, sizeof(ReturnFlick));

   ret->origin = te->pos;
   ret->dev = te->device;
   ret->timestamp = te->timestamp;
   ret->direction = Accessibility_GESTURE_DIRECTION_UNDEFINED;

   return ret;
}

static inline double
_length (const SpiVector *v)
{
   return sqrt (v->x * v->x + v->y * v->y);
}

static SpiVector
_normalize(const SpiVector *v)
{
   SpiVector ret;
   double l = _length(v);
   ret.x = (v->x) / l;
   ret.y = (v->y) / l;
   return ret;
}

#define PI2DEG (180.0 / 3.1415)
// a*b = |a||b|cosA
static double
_angle_between_vectors (SpiVector *v1, SpiVector *v2)
{
   return PI2DEG * acos ((v1->x * v2->x + v1->y * v2->y) / (_length(v1) * _length(v2)));
}

static Accessibility_GestureDirection
_get_flick_direction(SpiVector *v)
{
   if (fabs(v->x) > fabs(v->y))
     {
        if (v->x > 0)
          return Accessibility_GESTURE_DIRECTION_RIGHT;
        else
          return Accessibility_GESTURE_DIRECTION_LEFT;
     }
   else
     {
        if (v->y > 0)
          return Accessibility_GESTURE_DIRECTION_DOWN;
        else
          return Accessibility_GESTURE_DIRECTION_UP;
     }
}

static void
_flick_return_update_touch_point(ReturnFlick *tp, const Accessibility_TouchEvent *te, const FlickReturnPrivateData *data)
{
   SpiVector dir, tmp;
   double a;

   if (tp->phase == INVALID)
     return;

   // check if gesture hasn't expired
   if ((te->timestamp - tp->timestamp) > data->flick_return_max_time)
     {
        tp->phase = INVALID;
        return;
     }

   // check if finger has left initial finger-sized circle
   if (tp->phase == STARTED)
     {
        if (_distance(tp->origin, te->pos) > data->finger_size)
          {
             dir.x = te->pos.x - tp->origin.x;
             dir.y = te->pos.y - tp->origin.y;
             tp->flick_dir = _normalize(&dir);
             tp->phase = FORWARDING;
             tp->inflection = te->pos;
             tp->inflection_point_distance = 1.0;
             tp->direction = _get_flick_direction(&tp->flick_dir);
             return;
          }
     }
   // check if first state flick is inside boundaries, calculate inflection point
   if (tp->phase == FORWARDING)
     {
        dir.x = te->pos.x - tp->origin.x;
        dir.y = te->pos.y - tp->origin.y;
        a = _angle_between_vectors (&dir, &tp->flick_dir);
        if ((tp->inflection_point_distance > data->flick_return_min_length) && (a > data->angular_tolerance))
          {
             // calculate return direction vector
             tp->return_flick_dir.x = te->pos.x - tp->inflection.x;
             tp->return_flick_dir.y = te->pos.y - tp->inflection.y;
             tmp.x = tp->origin.x - tp->inflection.x;
             tmp.y = tp->origin.y - tp->inflection.y;
             a = _angle_between_vectors (&tp->return_flick_dir, &tmp);
             if (a > data->inflection_angular_tolerance)
               {
                  tp->phase = INVALID;
               }
             else
               {
                  tp->phase = RETURNING;
               }
          }
        else
          {
             // calculate inflection point
             double len = (double)(dir.x * tp->flick_dir.x + dir.y * tp->flick_dir.y );

             if (len > tp->inflection_point_distance)
               {
                  tp->inflection.x = tp->origin.x + len * tp->flick_dir.x;
                  tp->inflection.y = tp->origin.y + len * tp->flick_dir.y;
                  tp->inflection_point_distance = len;
               }
          }
        return;
     }
   if (tp->phase == RETURNING)
     {
        dir.x = te->pos.x - tp->inflection.x;
        dir.y = te->pos.y - tp->inflection.y;
        a = _angle_between_vectors (&dir, &tp->return_flick_dir);
        if (a > data->return_angular_tolerance)
          {
             tp->phase = INVALID;
          }
     }
}

static ReturnFlick*
_flick_return_get_touch_point_for_device(GList *touch_points, int device)
{
   GList *l;
   ReturnFlick *ret;

   for (l = touch_points; l; l = l->next)
     {
        ret = l->data;
        if (ret->dev == device)
          return ret;
     }
   return  NULL;
}

static void
_flick_return_update_info(FlickReturnPrivateData *pd)
{
   GList *l;
   ReturnFlick *rf;
   pd->info.n_fingers = g_list_length (pd->return_flicks);
   pd->info.direction = ((ReturnFlick*)(pd->return_flicks->data))->direction;

   for (l = pd->return_flicks->next; l; l = l->next)
     {
        rf = l->data;
        if (rf->direction != pd->info.direction)
          {
             pd->info.direction = Accessibility_GESTURE_DIRECTION_UNDEFINED;
             break;
          }
     }
}

#define FLICK_IS_INVALID(f) ((f)->phase == INVALID)
#define FLICK_IS_RETURNING(f) ((f)->phase == RETURNING)

static void _flick_return_feed(GestureContext *ctx, const Accessibility_TouchEvent *pi)
{
   FlickReturnPrivateData *data = _gesture_context_data_get(ctx);
   Accessibility_GestureState state = _gesture_context_state_get(ctx);
   ReturnFlick *tp = _flick_return_get_touch_point_for_device(data->return_flicks, pi->device);

   switch (pi->type)
     {
      case Accessibility_TOUCH_MOVE:
         if (!tp || state == Accessibility_GESTURE_STATE_ABORTED)
           return;
         _flick_return_update_touch_point(tp, pi, data);
         _flick_return_update_info(data);
         if (FLICK_IS_INVALID(tp))
            _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_ABORTED, &data->info);
         break;
      case Accessibility_TOUCH_DOWN:
         if (tp || state == Accessibility_GESTURE_STATE_ABORTED)
           return;
         tp = _flick_return_touch_point_new(pi);
         data->return_flicks = g_list_append(data->return_flicks, tp);
         _flick_return_update_info(data);
         if (state == 0)
            _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_BEGIN, &data->info);
         else
            _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_CONTINUED, &data->info);
         break;
      case Accessibility_TOUCH_UP:
         if (tp)
           {
              data->return_flicks = g_list_remove(data->return_flicks, tp);
              if (!FLICK_IS_RETURNING(tp))
                {
                   _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_ABORTED, &data->info);
                   state = Accessibility_GESTURE_STATE_ABORTED;
                }
              free (tp);
           }
         if (!data->return_flicks)
           {
              if (state != Accessibility_GESTURE_STATE_ABORTED)
                _gesture_context_state_set(ctx, Accessibility_GESTURE_STATE_ENDED, &data->info);
              else
                _gesture_context_state_set(ctx, 0, &data->info);
           }
         break;
     }
}

// RETURN FLICK GESTURE RECOGNITION FUNCS - END
