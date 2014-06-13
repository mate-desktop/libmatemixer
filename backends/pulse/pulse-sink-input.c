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

#include <libmatemixer/matemixer-client-stream.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-client-stream.h"
#include "pulse-sink-input.h"
#include "pulse-stream.h"

struct _PulseSinkInputPrivate
{
    guint32 index_monitor;
};

static gboolean sink_input_set_mute   (MateMixerStream       *stream,
                                       gboolean               mute);
static gboolean sink_input_set_volume (MateMixerStream       *stream,
                                       pa_cvolume            *volume);
static gboolean sink_input_set_parent (MateMixerClientStream *stream,
                                       MateMixerStream       *parent);

static gboolean sink_input_remove     (MateMixerClientStream *stream);

G_DEFINE_TYPE (PulseSinkInput, pulse_sink_input, PULSE_TYPE_CLIENT_STREAM);

static void
pulse_sink_input_init (PulseSinkInput *input)
{
    input->priv = G_TYPE_INSTANCE_GET_PRIVATE (input,
                                               PULSE_TYPE_SINK_INPUT,
                                               PulseSinkInputPrivate);
}

static void
pulse_sink_input_class_init (PulseSinkInputClass *klass)
{
    PulseStreamClass       *stream_class;
    PulseClientStreamClass *client_class;

    stream_class = PULSE_STREAM_CLASS (klass);

    stream_class->set_mute   = sink_input_set_mute;
    stream_class->set_volume = sink_input_set_volume;

    client_class = PULSE_CLIENT_STREAM_CLASS (klass);

    client_class->set_parent = sink_input_set_parent;
    client_class->remove     = sink_input_remove;

    g_type_class_add_private (klass, sizeof (PulseSinkInputPrivate));
}

PulseStream *
pulse_sink_input_new (PulseConnection *connection, const pa_sink_input_info *info)
{
    PulseSinkInput *input;

    /* Consider the sink input index as unchanging parameter */
    input = g_object_new (PULSE_TYPE_SINK_INPUT,
                          "connection", connection,
                          "index", info->index,
                          NULL);

    /* Other data may change at any time, so let's make a use of our update function */
    pulse_sink_input_update (PULSE_STREAM (input), info);

    return PULSE_STREAM (input);
}

gboolean
pulse_sink_input_update (PulseStream *stream, const pa_sink_input_info *info)
{
    MateMixerStreamFlags flags =    MATE_MIXER_STREAM_OUTPUT |
                                    MATE_MIXER_STREAM_CLIENT |
                                    MATE_MIXER_STREAM_HAS_MUTE;

    g_return_val_if_fail (PULSE_IS_SINK_INPUT (stream), FALSE);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (stream));

    pulse_stream_update_name (stream, info->name);
    // pulse_stream_update_description (stream, info->description);
    pulse_stream_update_mute (stream, info->mute ? TRUE : FALSE);
    pulse_stream_update_channel_map (stream, &info->channel_map);

    /* Build the flag list */
    if (info->has_volume) {
        flags |= MATE_MIXER_STREAM_HAS_VOLUME;
        pulse_stream_update_volume (stream, &info->volume);
    }
    if (info->volume_writable)
        flags |= MATE_MIXER_STREAM_CAN_SET_VOLUME;

    if (info->client != PA_INVALID_INDEX)
        flags |= MATE_MIXER_STREAM_APPLICATION;

    if (pa_channel_map_can_balance (&info->channel_map))
        flags |= MATE_MIXER_STREAM_CAN_BALANCE;
    if (pa_channel_map_can_fade (&info->channel_map))
        flags |= MATE_MIXER_STREAM_CAN_FADE;

    pulse_stream_update_flags (stream, flags);

    g_object_thaw_notify (G_OBJECT (stream));
    return TRUE;
}

static gboolean
sink_input_set_mute (MateMixerStream *stream, gboolean mute)
{
    PulseStream *ps;

    g_return_val_if_fail (PULSE_IS_SINK_INPUT (stream), FALSE);

    ps = PULSE_STREAM (stream);

    return pulse_connection_set_sink_input_mute (pulse_stream_get_connection (ps),
                                                 pulse_stream_get_index (ps),
                                                 mute);
}

static gboolean
sink_input_set_volume (MateMixerStream *stream, pa_cvolume *volume)
{
    PulseStream *ps;

    g_return_val_if_fail (PULSE_IS_SINK_INPUT (stream), FALSE);
    g_return_val_if_fail (volume != NULL, FALSE);

    ps = PULSE_STREAM (stream);

    return pulse_connection_set_sink_input_volume (pulse_stream_get_connection (ps),
                                                   pulse_stream_get_index (ps),
                                                   volume);
}

static gboolean
sink_input_set_parent (MateMixerClientStream *stream, MateMixerStream *parent)
{
    // TODO
    return TRUE;
}

static gboolean
sink_input_remove (MateMixerClientStream *stream)
{
    // TODO
    return TRUE;
}
