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

#ifndef PULSE_SINK_H
#define PULSE_SINK_H

#include <glib.h>
#include <glib-object.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-stream.h"

G_BEGIN_DECLS

#define PULSE_TYPE_SINK                         \
        (pulse_sink_get_type ())
#define PULSE_SINK(o)                           \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_SINK, PulseSink))
#define PULSE_IS_SINK(o)                        \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_SINK))
#define PULSE_SINK_CLASS(k)                     \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_SINK, PulseSinkClass))
#define PULSE_IS_SINK_CLASS(k)                  \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_SINK))
#define PULSE_SINK_GET_CLASS(o)                 \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_SINK, PulseSinkClass))

typedef struct _PulseSink         PulseSink;
typedef struct _PulseSinkClass    PulseSinkClass;
typedef struct _PulseSinkPrivate  PulseSinkPrivate;

struct _PulseSink
{
    PulseStream parent;

    /*< private >*/
    PulseSinkPrivate *priv;
};

struct _PulseSinkClass
{
    PulseStreamClass parent_class;
};

GType        pulse_sink_get_type          (void) G_GNUC_CONST;

PulseStream *pulse_sink_new               (PulseConnection    *connection,
                                           const pa_sink_info *info);

guint32      pulse_sink_get_monitor_index (PulseStream        *stream);

gboolean     pulse_sink_update            (PulseStream        *stream,
                                           const pa_sink_info *info);

G_END_DECLS

#endif /* PULSE_SINK_H */
