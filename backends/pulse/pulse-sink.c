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
#include <libmatemixer/matemixer-port.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-stream.h"
#include "pulse-sink.h"

struct _PulseSinkPrivate
{
    guint32 index_monitor;
};

enum {
    PROP_0,
    N_PROPERTIES
};

static gboolean sink_set_mute        (MateMixerStream *stream,
                                      gboolean         mute);
static gboolean sink_set_volume      (MateMixerStream *stream,
                                      pa_cvolume      *volume);
static gboolean sink_set_active_port (MateMixerStream *stream,
                                      const gchar     *port_name);

G_DEFINE_TYPE (PulseSink, pulse_sink, PULSE_TYPE_STREAM);

static void
pulse_sink_init (PulseSink *sink)
{
    sink->priv = G_TYPE_INSTANCE_GET_PRIVATE (sink,
                                              PULSE_TYPE_SINK,
                                              PulseSinkPrivate);
}

static void
pulse_sink_class_init (PulseSinkClass *klass)
{
    PulseStreamClass *stream_class;

    stream_class = PULSE_STREAM_CLASS (klass);

    stream_class->set_mute        = sink_set_mute;
    stream_class->set_volume      = sink_set_volume;
    stream_class->set_active_port = sink_set_active_port;

    g_type_class_add_private (klass, sizeof (PulseSinkPrivate));
}

PulseStream *
pulse_sink_new (PulseConnection *connection, const pa_sink_info *info)
{
    PulseSink *sink;
    GList     *ports = NULL;
    int        i;

    /* Convert the list of sink ports to a GList of MateMixerPorts */
    for (i = 0; i < info->n_ports; i++) {
        MateMixerPort       *port;
        MateMixerPortStatus  status = MATE_MIXER_PORT_UNKNOWN_STATUS;
        pa_sink_port_info   *p_info = info->ports[i];

#if PA_CHECK_VERSION(2, 0, 0)
        switch (p_info->available) {
        case PA_PORT_AVAILABLE_YES:
            status = MATE_MIXER_PORT_AVAILABLE;
            break;
        case PA_PORT_AVAILABLE_NO:
            status = MATE_MIXER_PORT_UNAVAILABLE;
            break;
        default:
            break;
        }
#endif
        port = mate_mixer_port_new (p_info->name,
                                    p_info->description,
                                    NULL,
                                    p_info->priority,
                                    status);

        ports = g_list_prepend (ports, port);
    }

    /* Consider the sink index as unchanging parameter */
    sink = g_object_new (PULSE_TYPE_SINK,
                         "connection", connection,
                         "index", info->index,
                         NULL);

    /* According to the PulseAudio code, the list of sink port never changes with
     * updates.
     * This may be not future-proof, but checking and validating the list of ports on
     * each update would be an expensive operation, so let's set the list only during
     * the construction */
    pulse_stream_update_ports (PULSE_STREAM (sink), g_list_reverse (ports));

    /* Other data may change at any time, so let's make a use of our update function */
    pulse_sink_update (PULSE_STREAM (sink), info);

    return PULSE_STREAM (sink);
}

gboolean
pulse_sink_update (PulseStream *stream, const pa_sink_info *info)
{
    PulseSink            *sink;
    MateMixerStreamFlags  flags =   MATE_MIXER_STREAM_OUTPUT |
                                    MATE_MIXER_STREAM_HAS_MUTE |
                                    MATE_MIXER_STREAM_HAS_VOLUME |
                                    MATE_MIXER_STREAM_CAN_SET_VOLUME;

    g_return_val_if_fail (PULSE_IS_SINK (stream), FALSE);

    sink = PULSE_SINK (stream);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (stream));

    pulse_stream_update_name (stream, info->name);
    pulse_stream_update_description (stream, info->description);
    pulse_stream_update_mute (stream, info->mute ? TRUE : FALSE);
    pulse_stream_update_channel_map (stream, &info->channel_map);
    pulse_stream_update_volume_extended (stream,
                                         &info->volume,
                                         info->base_volume,
                                         info->n_volume_steps);
    if (info->active_port)
        pulse_stream_update_active_port (stream, info->active_port->name);

    switch (info->state) {
    case PA_SINK_RUNNING:
        pulse_stream_update_status (stream, MATE_MIXER_STREAM_RUNNING);
        break;
    case PA_SINK_IDLE:
        pulse_stream_update_status (stream, MATE_MIXER_STREAM_IDLE);
        break;
    case PA_SINK_SUSPENDED:
        pulse_stream_update_status (stream, MATE_MIXER_STREAM_SUSPENDED);
        break;
    default:
        pulse_stream_update_status (stream, MATE_MIXER_STREAM_UNKNOWN_STATUS);
        break;
    }

    /* Build the flag list */
    if (info->flags & PA_SINK_DECIBEL_VOLUME)
        flags |= MATE_MIXER_STREAM_HAS_DECIBEL_VOLUME;
    if (info->flags & PA_SINK_FLAT_VOLUME)
        flags |= MATE_MIXER_STREAM_HAS_FLAT_VOLUME;

    if (info->monitor_source_name)
        flags |= MATE_MIXER_STREAM_OUTPUT_MONITOR;

    if (pa_channel_map_can_balance (&info->channel_map))
        flags |= MATE_MIXER_STREAM_CAN_BALANCE;
    if (pa_channel_map_can_fade (&info->channel_map))
        flags |= MATE_MIXER_STREAM_CAN_FADE;

    pulse_stream_update_flags (stream, flags);

    if (sink->priv->index_monitor != info->monitor_source) {
        sink->priv->index_monitor = info->monitor_source;

        // TODO: provide a property
        // g_object_notify (G_OBJECT (stream), "monitor");
    }

    g_object_thaw_notify (G_OBJECT (stream));
    return TRUE;
}

static gboolean
sink_set_mute (MateMixerStream *stream, gboolean mute)
{
    g_return_val_if_fail (PULSE_IS_SINK (stream), FALSE);

    return pulse_connection_set_sink_mute (pulse_stream_get_connection (PULSE_STREAM (stream)),
                                           pulse_stream_get_index (PULSE_STREAM (stream)),
                                           mute);
}

static gboolean
sink_set_volume (MateMixerStream *stream, pa_cvolume *volume)
{
    g_return_val_if_fail (PULSE_IS_SINK (stream), FALSE);
    g_return_val_if_fail (volume != NULL, FALSE);

    return pulse_connection_set_sink_volume (pulse_stream_get_connection (PULSE_STREAM (stream)),
                                             pulse_stream_get_index (PULSE_STREAM (stream)),
                                             volume);
}

static gboolean
sink_set_active_port (MateMixerStream *stream, const gchar *port_name)
{
    g_return_val_if_fail (PULSE_IS_SINK (stream), FALSE);
    g_return_val_if_fail (port_name != NULL, FALSE);

    return pulse_connection_set_sink_port (pulse_stream_get_connection (PULSE_STREAM (stream)),
                                           pulse_stream_get_index (PULSE_STREAM (stream)),
                                           port_name);
}
