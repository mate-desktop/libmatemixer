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

#include <glib.h>
#include <glib-object.h>

#include <pulse/pulseaudio.h>

#include "pulse-monitor.h"

struct _PulseMonitorPrivate
{
    pa_context  *context;
    pa_proplist *proplist;
    pa_stream   *stream;
    guint32      index_source;
    guint32      index_sink_input;
    gboolean     enabled;
};

enum {
    VALUE,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void pulse_monitor_class_init (PulseMonitorClass *klass);
static void pulse_monitor_init       (PulseMonitor      *port);
static void pulse_monitor_finalize   (GObject           *object);

G_DEFINE_TYPE (PulseMonitor, pulse_monitor, G_TYPE_OBJECT);

static gboolean monitor_prepare        (PulseMonitor *monitor);
static gboolean monitor_connect_record (PulseMonitor *monitor);
static void     monitor_read_cb        (pa_stream    *stream,
                                        size_t        length,
                                        void         *userdata);

static void
pulse_monitor_class_init (PulseMonitorClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = pulse_monitor_finalize;

    signals[VALUE] =
        g_signal_new ("value",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseMonitorClass, value),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__DOUBLE,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_DOUBLE);

    g_type_class_add_private (object_class, sizeof (PulseMonitorPrivate));
}

static void
pulse_monitor_init (PulseMonitor *monitor)
{
    monitor->priv = G_TYPE_INSTANCE_GET_PRIVATE (monitor,
                                                 PULSE_TYPE_MONITOR,
                                                 PulseMonitorPrivate);
}

static void
pulse_monitor_finalize (GObject *object)
{
    PulseMonitor *monitor;

    monitor = PULSE_MONITOR (object);

    if (monitor->priv->stream)
        pa_stream_unref (monitor->priv->stream);

    pa_context_unref (monitor->priv->context);
    pa_proplist_free (monitor->priv->proplist);

    G_OBJECT_CLASS (pulse_monitor_parent_class)->finalize (object);
}

PulseMonitor *
pulse_monitor_new (pa_context  *context,
                   pa_proplist *proplist,
                   guint32      index_source,
                   guint32      index_sink_input)
{
    PulseMonitor *monitor;

    monitor = g_object_new (PULSE_TYPE_MONITOR, NULL);

    monitor->priv->context  = pa_context_ref (context);
    monitor->priv->proplist = pa_proplist_copy (proplist);

    monitor->priv->index_source = index_source;
    monitor->priv->index_sink_input = index_sink_input;

    return monitor;
}

gboolean
pulse_monitor_enable (PulseMonitor *monitor)
{
    g_return_val_if_fail (PULSE_IS_MONITOR (monitor), FALSE);

    if (!monitor->priv->enabled) {
        if (monitor->priv->stream == NULL)
            monitor_prepare (monitor);

        if (G_LIKELY (monitor->priv->stream != NULL))
            monitor->priv->enabled = monitor_connect_record (monitor);
    }

    return monitor->priv->enabled;
}

void
pulse_monitor_disable (PulseMonitor *monitor)
{
    g_return_if_fail (PULSE_IS_MONITOR (monitor));

    if (!monitor->priv->enabled)
        return;

    pa_stream_disconnect (monitor->priv->stream);

    monitor->priv->enabled = FALSE;
}

gboolean
pulse_monitor_is_enabled (PulseMonitor *monitor)
{
    g_return_val_if_fail (PULSE_IS_MONITOR (monitor), FALSE);

    return monitor->priv->enabled;
}

gboolean
pulse_monitor_update_index (PulseMonitor *monitor,
                            guint32       index_source,
                            guint32       index_sink_input)
{
    g_return_val_if_fail (PULSE_IS_MONITOR (monitor), FALSE);

    if (monitor->priv->index_source == index_source &&
        monitor->priv->index_sink_input == index_sink_input)
        return TRUE;

    monitor->priv->index_source = index_source;
    monitor->priv->index_sink_input = index_sink_input;

    if (pulse_monitor_is_enabled (monitor)) {
        pulse_monitor_disable (monitor);

        /* Unset the Pulse stream to let enabling recreate it */
        g_clear_pointer (&monitor->priv->stream, pa_stream_unref);

        pulse_monitor_enable (monitor);
    } else if (monitor->priv->stream) {
        /* Disabled now but was enabled before and still holds source index */
        g_clear_pointer (&monitor->priv->stream, pa_stream_unref);
    }
    return TRUE;
}

static gboolean
monitor_prepare (PulseMonitor *monitor)
{
    pa_sample_spec spec;

    spec.channels  = 1;
    spec.format    = PA_SAMPLE_FLOAT32;
    spec.rate      = 25;

    monitor->priv->stream =
        pa_stream_new_with_proplist (monitor->priv->context, "Peak detect",
                                     &spec,
                                     NULL,
                                     monitor->priv->proplist);

    if (G_UNLIKELY (monitor->priv->stream == NULL)) {
        g_warning ("Failed to create PulseAudio monitor: %s",
                   pa_strerror (pa_context_errno (monitor->priv->context)));
        return FALSE;
    }

    if (monitor->priv->index_sink_input != PA_INVALID_INDEX)
        pa_stream_set_monitor_stream (monitor->priv->stream,
                                      monitor->priv->index_sink_input);

    pa_stream_set_read_callback (monitor->priv->stream,
                                 monitor_read_cb,
                                 monitor);
    return TRUE;
}

static gboolean
monitor_connect_record (PulseMonitor *monitor)
{
    pa_buffer_attr  attr;
    gchar          *name;
    int             ret;

    attr.maxlength = (guint32) -1;
    attr.tlength   = 0;
    attr.prebuf    = 0;
    attr.minreq    = 0;
    attr.fragsize  = sizeof (gfloat);

    name = g_strdup_printf ("%u", monitor->priv->index_source);
    ret  = pa_stream_connect_record (monitor->priv->stream,
                                     name,
                                     &attr,
                                     PA_STREAM_DONT_MOVE |
                                     PA_STREAM_PEAK_DETECT |
                                     PA_STREAM_ADJUST_LATENCY |
                                     PA_STREAM_DONT_INHIBIT_AUTO_SUSPEND);
    g_free (name);

    if (ret < 0) {
        g_warning ("Failed to connect PulseAudio monitor: %s", pa_strerror (ret));
        return FALSE;
    }
    return TRUE;
}

static void
monitor_read_cb (pa_stream *stream, size_t length, void *userdata)
{
    const void *data;

    if (pa_stream_peek (stream, &data, &length) < 0)
        return;

    if (data) {
        gdouble v = ((const gfloat *) data)[length / sizeof (gfloat) - 1];

        g_signal_emit (G_OBJECT (userdata),
                       signals[VALUE],
                       0,
                       CLAMP (v, 0, 1));
    }

    /* pa_stream_drop() should not be called if the buffer is empty, but it
     * should be called if there is a hole */
    if (length)
        pa_stream_drop (stream);
}
