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
#include "pulse-stream.h"
#include "pulse-source-output.h"

struct _PulseSourceOutputPrivate
{
    guint32 index_monitor;
};

static gboolean source_output_set_mute        (MateMixerStream *stream, gboolean mute);
static gboolean source_output_set_volume      (MateMixerStream *stream, pa_cvolume *volume);

G_DEFINE_TYPE (PulseSourceOutput, pulse_source_output, PULSE_TYPE_STREAM);

static void
pulse_source_output_init (PulseSourceOutput *output)
{
    output->priv = G_TYPE_INSTANCE_GET_PRIVATE (output,
                                                PULSE_TYPE_SOURCE_OUTPUT,
                                                PulseSourceOutputPrivate);
}

static void
pulse_source_output_class_init (PulseSourceOutputClass *klass)
{
    PulseStreamClass *stream_class;

    stream_class = PULSE_STREAM_CLASS (klass);

    stream_class->set_mute   = source_output_set_mute;
    stream_class->set_volume = source_output_set_volume;

    g_type_class_add_private (klass, sizeof (PulseSourceOutputPrivate));
}

PulseStream *
pulse_source_output_new (PulseConnection *connection, const pa_source_output_info *info)
{
    PulseSourceOutput *output;

    /* Consider the sink input index as unchanging parameter */
    output = g_object_new (PULSE_TYPE_SOURCE_OUTPUT,
                           "connection", connection,
                           "index", info->index,
                           NULL);

    /* Other data may change at any time, so let's make a use of our update function */
    pulse_source_output_update (PULSE_STREAM (output), info);

    return PULSE_STREAM (output);
}

gboolean
pulse_source_output_update (PulseStream *stream, const pa_source_output_info *info)
{
    MateMixerStreamFlags flags =    MATE_MIXER_STREAM_INPUT |
                                    MATE_MIXER_STREAM_CLIENT |
                                    MATE_MIXER_STREAM_HAS_MUTE;

    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (stream), FALSE);

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
source_output_set_mute (MateMixerStream *stream, gboolean mute)
{
    PulseStream *ps;

    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (stream), FALSE);

    ps = PULSE_STREAM (stream);

    return pulse_connection_set_source_output_mute (pulse_stream_get_connection (ps),
                                                    pulse_stream_get_index (ps),
                                                    mute);
}

static gboolean
source_output_set_volume (MateMixerStream *stream, pa_cvolume *volume)
{
    PulseStream *ps;

    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (stream), FALSE);
    g_return_val_if_fail (volume != NULL, FALSE);

    ps = PULSE_STREAM (stream);

    return pulse_connection_set_source_output_volume (pulse_stream_get_connection (ps),
                                                      pulse_stream_get_index (ps),
                                                      volume);
}
