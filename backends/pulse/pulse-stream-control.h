/*
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSE_STREAM_CONTROL_H
#define PULSE_STREAM_CONTROL_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include <pulse/pulseaudio.h>

#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_STREAM_CONTROL               \
        (pulse_stream_control_get_type ())
#define PULSE_STREAM_CONTROL(o)                 \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_STREAM_CONTROL, PulseStreamControl))
#define PULSE_IS_STREAM_CONTROL(o)              \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_STREAM_CONTROL))
#define PULSE_STREAM_CONTROL_CLASS(k)           \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_STREAM_CONTROL, PulseStreamControlClass))
#define PULSE_IS_STREAM_CONTROL_CLASS(k)        \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_STREAM_CONTROL))
#define PULSE_STREAM_CONTROL_GET_CLASS(o)       \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_STREAM_CONTROL, PulseStreamControlClass))

typedef struct _PulseStreamControlClass    PulseStreamControlClass;
typedef struct _PulseStreamControlPrivate  PulseStreamControlPrivate;

struct _PulseStreamControl
{
    MateMixerStreamControl parent;

    /*< private >*/
    PulseStreamControlPrivate *priv;
};

struct _PulseStreamControlClass
{
    MateMixerStreamControlClass parent_class;

    /*< private >*/
    gboolean      (*set_mute)        (PulseStreamControl *control,
                                      gboolean            mute);
    gboolean      (*set_volume)      (PulseStreamControl *control,
                                      pa_cvolume         *volume);

    PulseMonitor *(*create_monitor)  (PulseStreamControl *control);
};

GType                 pulse_stream_control_get_type         (void) G_GNUC_CONST;

guint32               pulse_stream_control_get_index        (PulseStreamControl   *control);
guint32               pulse_stream_control_get_stream_index (PulseStreamControl   *control);

PulseConnection *     pulse_stream_control_get_connection   (PulseStreamControl   *control);
PulseMonitor *        pulse_stream_control_get_monitor      (PulseStreamControl   *control);

const pa_cvolume *    pulse_stream_control_get_cvolume      (PulseStreamControl   *control);
const pa_channel_map *pulse_stream_control_get_channel_map  (PulseStreamControl   *control);

void                  pulse_stream_control_set_app_info     (PulseStreamControl   *stream,
                                                             MateMixerAppInfo     *info,
                                                             gboolean              take);

void                  pulse_stream_control_set_channel_map  (PulseStreamControl   *control,
                                                             const pa_channel_map *map);
void                  pulse_stream_control_set_cvolume      (PulseStreamControl   *control,
                                                             const pa_cvolume     *cvolume,
                                                             pa_volume_t           base_volume);

G_END_DECLS

#endif /* PULSE_STREAM_CONTROL_H */
