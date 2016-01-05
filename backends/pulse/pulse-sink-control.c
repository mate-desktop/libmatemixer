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
#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-monitor.h"
#include "pulse-stream-control.h"
#include "pulse-sink.h"
#include "pulse-sink-control.h"

static void pulse_sink_control_class_init (PulseSinkControlClass *klass);
static void pulse_sink_control_init       (PulseSinkControl      *control);

G_DEFINE_TYPE (PulseSinkControl, pulse_sink_control, PULSE_TYPE_STREAM_CONTROL);

static gboolean      pulse_sink_control_set_mute        (PulseStreamControl *psc,
                                                         gboolean            mute);
static gboolean      pulse_sink_control_set_volume      (PulseStreamControl *psc,
                                                         pa_cvolume         *cvolume);
static PulseMonitor *pulse_sink_control_create_monitor  (PulseStreamControl *psc);

static void
pulse_sink_control_class_init (PulseSinkControlClass *klass)
{
    PulseStreamControlClass *control_class;

    control_class = PULSE_STREAM_CONTROL_CLASS (klass);
    control_class->set_mute       = pulse_sink_control_set_mute;
    control_class->set_volume     = pulse_sink_control_set_volume;
    control_class->create_monitor = pulse_sink_control_create_monitor;
}

static void
pulse_sink_control_init (PulseSinkControl *control)
{
}

PulseSinkControl *
pulse_sink_control_new (PulseConnection    *connection,
                        const pa_sink_info *info,
                        PulseSink          *parent)
{
    PulseSinkControl           *control;
    MateMixerStreamControlFlags flags = MATE_MIXER_STREAM_CONTROL_MUTE_READABLE |
                                        MATE_MIXER_STREAM_CONTROL_MUTE_WRITABLE |
                                        MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE |
                                        MATE_MIXER_STREAM_CONTROL_VOLUME_WRITABLE;
    MateMixerStreamControlRole  role;
    guint32                     index;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);
    g_return_val_if_fail (PULSE_IS_SINK (parent), NULL);

    if (info->active_port != NULL)
        role = MATE_MIXER_STREAM_CONTROL_ROLE_PORT;
    else
        role = MATE_MIXER_STREAM_CONTROL_ROLE_MASTER;

    /* Build the flag list */
    if (info->flags & PA_SINK_DECIBEL_VOLUME)
        flags |= MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL;

    index = pulse_sink_get_index_monitor (parent);
    if (index != PA_INVALID_INDEX)
        flags |= MATE_MIXER_STREAM_CONTROL_HAS_MONITOR;

    control = g_object_new (PULSE_TYPE_SINK_CONTROL,
                            "name", info->name,
                            "label", info->description,
                            "flags", flags,
                            "role", role,
                            "stream", parent,
                            "connection", connection,
                            NULL);

    pulse_sink_control_update (control, info);
    return control;
}

void
pulse_sink_control_update (PulseSinkControl *control, const pa_sink_info *info)
{
    g_return_if_fail (PULSE_IS_SINK_CONTROL (control));
    g_return_if_fail (info != NULL);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (control));

    _mate_mixer_stream_control_set_mute (MATE_MIXER_STREAM_CONTROL (control),
                                         info->mute ? TRUE : FALSE);

    pulse_stream_control_set_channel_map (PULSE_STREAM_CONTROL (control),
                                          &info->channel_map);

    pulse_stream_control_set_cvolume (PULSE_STREAM_CONTROL (control),
                                      &info->volume,
                                      info->base_volume);

    g_object_thaw_notify (G_OBJECT (control));
}

static gboolean
pulse_sink_control_set_mute (PulseStreamControl *psc, gboolean mute)
{
    g_return_val_if_fail (PULSE_IS_SINK_CONTROL (psc), FALSE);

    return pulse_connection_set_sink_mute (pulse_stream_control_get_connection (psc),
                                           pulse_stream_control_get_stream_index (psc),
                                           mute);
}

static gboolean
pulse_sink_control_set_volume (PulseStreamControl *psc, pa_cvolume *cvolume)
{
    g_return_val_if_fail (PULSE_IS_SINK_CONTROL (psc), FALSE);
    g_return_val_if_fail (cvolume != NULL, FALSE);

    return pulse_connection_set_sink_volume (pulse_stream_control_get_connection (psc),
                                             pulse_stream_control_get_stream_index (psc),
                                             cvolume);
}

static PulseMonitor *
pulse_sink_control_create_monitor (PulseStreamControl *psc)
{
    PulseSink *sink;
    guint32    index;

    g_return_val_if_fail (PULSE_IS_SINK_CONTROL (psc), NULL);

    sink = PULSE_SINK (mate_mixer_stream_control_get_stream (MATE_MIXER_STREAM_CONTROL (psc)));

    index = pulse_sink_get_index_monitor (sink);
    if G_UNLIKELY (index == PA_INVALID_INDEX) {
        g_debug ("Monitor of stream control %s is not available",
                 mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (psc)));
        return NULL;
    }

    return pulse_connection_create_monitor (pulse_stream_control_get_connection (psc),
                                            index,
                                            PA_INVALID_INDEX);
}
