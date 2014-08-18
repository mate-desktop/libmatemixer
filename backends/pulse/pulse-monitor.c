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
#include <glib/gi18n.h>
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
    PROP_0,
    PROP_ENABLED,
    PROP_INDEX_SOURCE,
    PROP_INDEX_SINK_INPUT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
    VALUE,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void pulse_monitor_class_init   (PulseMonitorClass *klass);

static void pulse_monitor_get_property (GObject           *object,
                                        guint              param_id,
                                        GValue            *value,
                                        GParamSpec        *pspec);
static void pulse_monitor_set_property (GObject           *object,
                                        guint              param_id,
                                        const GValue      *value,
                                        GParamSpec        *pspec);

static void pulse_monitor_init         (PulseMonitor      *monitor);
static void pulse_monitor_finalize     (GObject           *object);

G_DEFINE_TYPE (PulseMonitor, pulse_monitor, G_TYPE_OBJECT);

static gboolean stream_connect (PulseMonitor *monitor);

static void     stream_read_cb (pa_stream    *stream,
                                size_t        length,
                                void         *userdata);

static void
pulse_monitor_class_init (PulseMonitorClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = pulse_monitor_finalize;
    object_class->get_property = pulse_monitor_get_property;
    object_class->set_property = pulse_monitor_set_property;

    properties[PROP_ENABLED] =
        g_param_spec_boolean ("enabled",
                              "Enabled",
                              "Monitor enabled",
                              FALSE,
                              G_PARAM_READABLE |
                              G_PARAM_STATIC_STRINGS);

    properties[PROP_INDEX_SOURCE] =
        g_param_spec_uint ("index-source",
                           "Index of source",
                           "Index of the PulseAudio source",
                           0,
                           G_MAXUINT,
                           0,
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

    properties[PROP_INDEX_SINK_INPUT] =
        g_param_spec_uint ("index-sink-input",
                           "Index of sink input",
                           "Index of the PulseAudio sink input",
                           0,
                           G_MAXUINT,
                           0,
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

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
pulse_monitor_get_property (GObject    *object,
                            guint       param_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
    PulseMonitor *monitor;

    monitor = PULSE_MONITOR (object);

    switch (param_id) {
    case PROP_ENABLED:
        g_value_set_boolean (value, monitor->priv->enabled);
        break;
    case PROP_INDEX_SOURCE:
        g_value_set_uint (value, monitor->priv->index_source);
        break;
    case PROP_INDEX_SINK_INPUT:
        g_value_set_uint (value, monitor->priv->index_sink_input);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_monitor_set_property (GObject      *object,
                            guint         param_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    PulseMonitor *monitor;

    monitor = PULSE_MONITOR (object);

    switch (param_id) {
    case PROP_INDEX_SOURCE:
        monitor->priv->index_source = g_value_get_uint (value);
        break;
    case PROP_INDEX_SINK_INPUT:
        monitor->priv->index_sink_input = g_value_get_uint (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
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

    /* The pulse stream may exist if the monitor is running */
    if (monitor->priv->stream != NULL) {
        pa_stream_disconnect (monitor->priv->stream);
        pa_stream_unref (monitor->priv->stream);
    }

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

    g_return_val_if_fail (context  != NULL, NULL);
    g_return_val_if_fail (proplist != NULL, NULL);

    monitor = g_object_new (PULSE_TYPE_MONITOR,
                            "index-source", index_source,
                            "index-sink-input", index_sink_input,
                            NULL);

    monitor->priv->context  = pa_context_ref (context);
    monitor->priv->proplist = pa_proplist_copy (proplist);

    return monitor;
}

gboolean
pulse_monitor_get_enabled (PulseMonitor *monitor)
{
    g_return_val_if_fail (PULSE_IS_MONITOR (monitor), FALSE);

    return monitor->priv->enabled;
}

gboolean
pulse_monitor_set_enabled (PulseMonitor *monitor, gboolean enabled)
{
    g_return_val_if_fail (PULSE_IS_MONITOR (monitor), FALSE);

    if (enabled == monitor->priv->enabled)
        return TRUE;

    if (enabled) {
        monitor->priv->enabled = stream_connect (monitor);

        if (monitor->priv->enabled == FALSE)
            return FALSE;
    } else {
        pa_stream_disconnect (monitor->priv->stream);
        pa_stream_unref (monitor->priv->stream);

        monitor->priv->stream = NULL;
        monitor->priv->enabled = FALSE;
    }
    g_object_notify_by_pspec (G_OBJECT (monitor), properties[PROP_ENABLED]);

    return TRUE;
}

static gboolean
stream_connect (PulseMonitor *monitor)
{
    pa_sample_spec  spec;
    pa_buffer_attr  attr;
    gchar          *idx;
    int             ret;

    attr.maxlength = (guint32) -1;
    attr.tlength   = 0;
    attr.prebuf    = 0;
    attr.minreq    = 0;
    attr.fragsize  = sizeof (gfloat);
    spec.channels  = 1;
    spec.format    = PA_SAMPLE_FLOAT32;
    spec.rate      = 25;

    monitor->priv->stream =
        pa_stream_new_with_proplist (monitor->priv->context,
                                     _("Peak detect"),
                                     &spec,
                                     NULL,
                                     monitor->priv->proplist);

    if (G_UNLIKELY (monitor->priv->stream == NULL)) {
        g_warning ("Failed to create peak monitor: %s",
                   pa_strerror (pa_context_errno (monitor->priv->context)));
        return FALSE;
    }

    /* Set sink input index for the stream, source outputs are not supported */
    if (monitor->priv->index_sink_input != PA_INVALID_INDEX)
        pa_stream_set_monitor_stream (monitor->priv->stream,
                                      monitor->priv->index_sink_input);

    pa_stream_set_read_callback (monitor->priv->stream,
                                 stream_read_cb,
                                 monitor);

    /* Source index must be passed as a string */
    idx = g_strdup_printf ("%u", monitor->priv->index_source);
    ret = pa_stream_connect_record (monitor->priv->stream,
                                    idx,
                                    &attr,
                                    PA_STREAM_DONT_MOVE |
                                    PA_STREAM_PEAK_DETECT |
                                    PA_STREAM_ADJUST_LATENCY);
    g_free (idx);

    if (ret < 0) {
        g_warning ("Failed to connect peak monitor: %s", pa_strerror (ret));
        return FALSE;
    }
    return TRUE;
}

static void
stream_read_cb (pa_stream *stream, size_t length, void *userdata)
{
    const void *data;

    /* Read the next fragment from the buffer (for recording streams).
     *
     * If there is data at the current read index, data will point to the
     * actual data and length will contain the size of the data in bytes
     * (which can be less or more than a complete fragment).
     *
     * If there is no data at the current read index, it means that either
     * the buffer is empty or it contains a hole (that is, the write index
     * is ahead of the read index but there's no data where the read index
     * points at). If the buffer is empty, data will be NULL and length will
     * be 0. If there is a hole, data will be NULL and length will contain
     * the length of the hole. */
    if (pa_stream_peek (stream, &data, &length) < 0)
        return;

    if (data != NULL) {
        gdouble v = ((const gfloat *) data)[length / sizeof (gfloat) - 1];

        g_signal_emit (G_OBJECT (userdata),
                       signals[VALUE],
                       0,
                       CLAMP (v, 0, 1));
    }

    /* pa_stream_drop() should not be called if the buffer is empty, but it
     * should be called if there is a hole */
    if (length > 0)
        pa_stream_drop (stream);
}
