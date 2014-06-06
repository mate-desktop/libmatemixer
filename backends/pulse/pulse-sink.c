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

struct _MateMixerPulseSinkPrivate
{
    guint32 index_monitor;
};

G_DEFINE_TYPE (MateMixerPulseSink, mate_mixer_pulse_sink, MATE_MIXER_TYPE_PULSE_STREAM);

static void
mate_mixer_pulse_sink_init (MateMixerPulseSink *sink)
{
    sink->priv = G_TYPE_INSTANCE_GET_PRIVATE (sink,
                                              MATE_MIXER_TYPE_PULSE_SINK,
                                              MateMixerPulseSinkPrivate);
}

static void
mate_mixer_pulse_sink_class_init (MateMixerPulseSinkClass *klass)
{
    MateMixerPulseStreamClass *stream_class;

    stream_class = MATE_MIXER_PULSE_STREAM_CLASS (klass);

    stream_class->set_volume = mate_mixer_pulse_sink_set_volume;
    stream_class->set_mute = mate_mixer_pulse_sink_set_mute;

    g_type_class_add_private (G_OBJECT (klass), sizeof (MateMixerPulseSinkPrivate));
}

MateMixerPulseStream *
mate_mixer_pulse_sink_new (MateMixerPulseConnection *connection, const pa_sink_info *info)
{
    MateMixerPulseStream *stream;
    GList *ports = NULL;
    int i;

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

    if (ports)
        ports = g_list_reverse (ports);

    stream = g_object_new (MATE_MIXER_TYPE_PULSE_STREAM,
                           "connection", connection,
                           "index", info->index,
                           "name", info->name,
                           "description", info->description,
                           "channels", info->channel_map.channels,
                           "mute", info->mute ? TRUE : FALSE,
                           NULL);

    return stream;
}

gboolean
mate_mixer_pulse_sink_set_volume (MateMixerStream *stream, guint32 volume)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_STREAM (stream), FALSE);

/*
    return mate_mixer_pulse_connection_set_sink_volume (mate_mixer_pulse_stream_get_connection (MATE_MIXER_PULSE_STREAM (stream)),
                                                        volume);
*/
    return TRUE;
}

gboolean
mate_mixer_pulse_sink_set_mute (MateMixerStream *stream, gboolean mute)
{
    MateMixerPulseStream *pulse_stream;

    g_return_val_if_fail (MATE_MIXER_IS_PULSE_STREAM (stream), FALSE);

    pulse_stream = MATE_MIXER_PULSE_STREAM (stream);

    return mate_mixer_pulse_connection_set_sink_mute (mate_mixer_pulse_stream_get_connection (pulse_stream),
                                                      mate_mixer_pulse_stream_get_index (pulse_stream),
                                                      mute);
}
