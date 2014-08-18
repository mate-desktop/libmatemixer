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

#ifndef PULSE_MONITOR_H
#define PULSE_MONITOR_H

#include <glib.h>
#include <glib-object.h>

#include <pulse/pulseaudio.h>

#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_MONITOR                      \
        (pulse_monitor_get_type ())
#define PULSE_MONITOR(o)                        \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_MONITOR, PulseMonitor))
#define PULSE_IS_MONITOR(o)                     \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_MONITOR))
#define PULSE_MONITOR_CLASS(k)                  \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_MONITOR, PulseMonitorClass))
#define PULSE_IS_MONITOR_CLASS(k)               \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_MONITOR))
#define PULSE_MONITOR_GET_CLASS(o)              \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_MONITOR, PulseMonitorClass))

typedef struct _PulseMonitorClass    PulseMonitorClass;
typedef struct _PulseMonitorPrivate  PulseMonitorPrivate;

struct _PulseMonitor
{
    GObject parent;

    /*< private >*/
    PulseMonitorPrivate *priv;
};

struct _PulseMonitorClass
{
    GObjectClass parent_class;

    /*< private >*/
    void (*value) (PulseMonitor *monitor,
                   gdouble       value);
};

GType         pulse_monitor_get_type     (void) G_GNUC_CONST;

PulseMonitor *pulse_monitor_new          (pa_context   *context,
                                          pa_proplist  *proplist,
                                          guint32       index_source,
                                          guint32       index_sink_input);

gboolean      pulse_monitor_get_enabled  (PulseMonitor *monitor);
gboolean      pulse_monitor_set_enabled  (PulseMonitor *monitor,
                                          gboolean      enabled);

G_END_DECLS

#endif /* PULSE_MONITOR_H */
