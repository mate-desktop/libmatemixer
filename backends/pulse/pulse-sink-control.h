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

#ifndef PULSE_SINK_CONTROL_H
#define PULSE_SINK_CONTROL_H

#include <glib.h>
#include <glib-object.h>

#include <pulse/pulseaudio.h>

#include "pulse-stream-control.h"
#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_SINK_CONTROL                 \
        (pulse_sink_control_get_type ())
#define PULSE_SINK_CONTROL(o)                   \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_SINK_CONTROL, PulseSinkControl))
#define PULSE_IS_SINK_CONTROL(o)                \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_SINK_CONTROL))
#define PULSE_SINK_CONTROL_CLASS(k)             \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_SINK_CONTROL, PulseSinkControlClass))
#define PULSE_IS_SINK_CONTROL_CLASS(k)          \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_SINK_CONTROL))
#define PULSE_SINK_CONTROL_GET_CLASS(o)         \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_SINK_CONTROL, PulseSinkControlClass))

typedef struct _PulseSinkControlClass    PulseSinkControlClass;
typedef struct _PulseSinkControlPrivate  PulseSinkControlPrivate;

struct _PulseSinkControl
{
    PulseStreamControl parent;
};

struct _PulseSinkControlClass
{
    PulseStreamControlClass parent_class;
};

GType             pulse_sink_control_get_type (void) G_GNUC_CONST;

PulseSinkControl *pulse_sink_control_new      (PulseConnection    *connection,
                                               const pa_sink_info *info,
                                               PulseSink          *sink);

void              pulse_sink_control_update   (PulseSinkControl   *control,
                                               const pa_sink_info *info);

G_END_DECLS

#endif /* PULSE_SINK_CONTROL_H */
