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

#include <libmatemixer/matemixer-stream.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-device.h"
#include "pulse-monitor.h"
#include "pulse-stream.h"
#include "pulse-sink.h"

struct _PulseSinkPrivate
{
    guint32 index_monitor;
};

static void pulse_sink_class_init (PulseSinkClass *klass);
static void pulse_sink_init       (PulseSink      *sink);

G_DEFINE_TYPE (PulseSink, pulse_sink, PULSE_TYPE_STREAM);

static gboolean      sink_set_mute        (MateMixerStream *stream,
                                           gboolean         mute);
static gboolean      sink_set_volume      (MateMixerStream *stream,
                                           pa_cvolume      *volume);
static gboolean      sink_set_active_port (MateMixerStream *stream,
                                           const gchar     *port);
static gboolean      sink_suspend         (MateMixerStream *stream);
static gboolean      sink_resume          (MateMixerStream *stream);
static PulseMonitor *sink_create_monitor  (MateMixerStream *stream);

static void
pulse_sink_class_init (PulseSinkClass *klass)
{
    PulseStreamClass *stream_class;

    stream_class = PULSE_STREAM_CLASS (klass);

    stream_class->set_mute        = sink_set_mute;
    stream_class->set_volume      = sink_set_volume;
    stream_class->set_active_port = sink_set_active_port;
    stream_class->suspend         = sink_suspend;
    stream_class->resume          = sink_resume;
    stream_class->create_monitor  = sink_create_monitor;

    g_type_class_add_private (klass, sizeof (PulseSinkPrivate));
}

static void
pulse_sink_init (PulseSink *sink)
{
    sink->priv = G_TYPE_INSTANCE_GET_PRIVATE (sink,
                                              PULSE_TYPE_SINK,
                                              PulseSinkPrivate);

    sink->priv->index_monitor = PA_INVALID_INDEX;
}

PulseStream *
pulse_sink_new (PulseConnection    *connection,
                const pa_sink_info *info,
                PulseDevice        *device)
{
    PulseStream *stream;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);

    /* Consider the sink index as unchanging parameter */
    stream = g_object_new (PULSE_TYPE_SINK,
                           "connection", connection,
                           "index", info->index,
                           NULL);

    /* Other data may change at any time, so let's make a use of our update function */
    pulse_sink_update (stream, info, device);

    return stream;
}

guint32
pulse_sink_get_monitor_index (PulseStream *stream)
{
    g_return_val_if_fail (PULSE_IS_SINK (stream), PA_INVALID_INDEX);

    return PULSE_SINK (stream)->priv->index_monitor;
}

gboolean
pulse_sink_update (PulseStream *stream, const pa_sink_info *info, PulseDevice *device)
{
    MateMixerStreamFlags flags = MATE_MIXER_STREAM_OUTPUT |
                                 MATE_MIXER_STREAM_HAS_MUTE |
                                 MATE_MIXER_STREAM_HAS_VOLUME |
                                 MATE_MIXER_STREAM_HAS_MONITOR |
                                 MATE_MIXER_STREAM_CAN_SET_VOLUME |
                                 MATE_MIXER_STREAM_CAN_SUSPEND;
    PulseSink *sink;
    GList     *ports = NULL;
    guint32    i;

    g_return_val_if_fail (PULSE_IS_SINK (stream), FALSE);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (stream));

    pulse_stream_update_name (stream, info->name);
    pulse_stream_update_description (stream, info->description);
    pulse_stream_update_mute (stream, info->mute ? TRUE : FALSE);

    /* List of ports */
    for (i = 0; i < info->n_ports; i++) {
        MateMixerPortFlags flags = MATE_MIXER_PORT_NO_FLAGS;

#if PA_CHECK_VERSION(2, 0, 0)
        if (info->ports[i]->available == PA_PORT_AVAILABLE_YES)
            flags |= MATE_MIXER_PORT_AVAILABLE;
#endif
        ports = g_list_prepend (ports,
                                mate_mixer_port_new (info->ports[i]->name,
                                                     info->ports[i]->description,
                                                     NULL,
                                                     info->ports[i]->priority,
                                                     flags));
    }
    pulse_stream_update_ports (stream, ports);

    /* Active port */
    if (info->active_port)
        pulse_stream_update_active_port (stream, info->active_port->name);
    else
        pulse_stream_update_active_port (stream, NULL);

    /* Stream state */
    switch (info->state) {
    case PA_SINK_RUNNING:
        pulse_stream_update_state (stream, MATE_MIXER_STREAM_RUNNING);
        break;
    case PA_SINK_IDLE:
        pulse_stream_update_state (stream, MATE_MIXER_STREAM_IDLE);
        break;
    case PA_SINK_SUSPENDED:
        pulse_stream_update_state (stream, MATE_MIXER_STREAM_SUSPENDED);
        break;
    default:
        pulse_stream_update_state (stream, MATE_MIXER_STREAM_UNKNOWN_STATE);
        break;
    }

    /* Build the flag list */
    if (info->flags & PA_SINK_DECIBEL_VOLUME)
        flags |= MATE_MIXER_STREAM_HAS_DECIBEL_VOLUME;
    if (info->flags & PA_SINK_FLAT_VOLUME)
        flags |= MATE_MIXER_STREAM_HAS_FLAT_VOLUME;

    if (pa_channel_map_can_balance (&info->channel_map))
        flags |= MATE_MIXER_STREAM_CAN_BALANCE;
    if (pa_channel_map_can_fade (&info->channel_map))
        flags |= MATE_MIXER_STREAM_CAN_FADE;

    /* Flags must be updated before volume */
    pulse_stream_update_flags (stream, flags);

    pulse_stream_update_volume (stream,
                                &info->volume,
                                &info->channel_map,
                                info->base_volume);

    pulse_stream_update_device (stream, MATE_MIXER_DEVICE (device));

    sink = PULSE_SINK (stream);

    /* Handle change of monitoring source index */
    // XXX probably call this each time to validate
    if (sink->priv->index_monitor != info->monitor_source) {
        PulseMonitor *monitor;

        monitor = pulse_stream_get_monitor (PULSE_STREAM (stream));

        if (monitor)
            pulse_monitor_update_index (monitor,
                                        info->monitor_source,
                                        PA_INVALID_INDEX);

        sink->priv->index_monitor = info->monitor_source;
    }

    g_object_thaw_notify (G_OBJECT (stream));
    return TRUE;
}

static gboolean
sink_set_mute (MateMixerStream *stream, gboolean mute)
{
    PulseStream *pulse;

    g_return_val_if_fail (PULSE_IS_SINK (stream), FALSE);

    pulse = PULSE_STREAM (stream);

    return pulse_connection_set_sink_mute (pulse_stream_get_connection (pulse),
                                           pulse_stream_get_index (pulse),
                                           mute);
}

static gboolean
sink_set_volume (MateMixerStream *stream, pa_cvolume *volume)
{
    PulseStream *pulse;

    g_return_val_if_fail (PULSE_IS_SINK (stream), FALSE);
    g_return_val_if_fail (volume != NULL, FALSE);

    pulse = PULSE_STREAM (stream);

    return pulse_connection_set_sink_volume (pulse_stream_get_connection (pulse),
                                             pulse_stream_get_index (pulse),
                                             volume);
}

static gboolean
sink_set_active_port (MateMixerStream *stream, const gchar *port)
{
    PulseStream *pulse;

    g_return_val_if_fail (PULSE_IS_SINK (stream), FALSE);
    g_return_val_if_fail (port != NULL, FALSE);

    pulse = PULSE_STREAM (stream);

    return pulse_connection_set_sink_port (pulse_stream_get_connection (pulse),
                                           pulse_stream_get_index (pulse),
                                           port);
}

static gboolean
sink_suspend (MateMixerStream *stream)
{
    PulseStream *pulse;

    g_return_val_if_fail (PULSE_IS_SINK (stream), FALSE);

    pulse = PULSE_STREAM (stream);

    return pulse_connection_suspend_sink (pulse_stream_get_connection (pulse),
                                          pulse_stream_get_index (pulse),
                                          TRUE);
}

static gboolean
sink_resume (MateMixerStream *stream)
{
    PulseStream *pulse;

    g_return_val_if_fail (PULSE_IS_SINK (stream), FALSE);

    pulse = PULSE_STREAM (stream);

    return pulse_connection_suspend_sink (pulse_stream_get_connection (pulse),
                                          pulse_stream_get_index (pulse),
                                          FALSE);
}

static PulseMonitor *
sink_create_monitor (MateMixerStream *stream)
{
    PulseStream *pulse;
    guint32      index;

    g_return_val_if_fail (PULSE_IS_SINK (stream), NULL);

    pulse = PULSE_STREAM (stream);
    index = pulse_sink_get_monitor_index (pulse);

    if (G_UNLIKELY (index == PA_INVALID_INDEX)) {
        g_debug ("Not creating monitor for stream %s as it is not available",
                 mate_mixer_stream_get_name (stream));
        return NULL;
    }

    return pulse_connection_create_monitor (pulse_stream_get_connection (pulse),
                                            index,
                                            PA_INVALID_INDEX);
}
