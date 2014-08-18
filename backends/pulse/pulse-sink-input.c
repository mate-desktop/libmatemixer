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
#include "pulse-helpers.h"
#include "pulse-monitor.h"
#include "pulse-sink.h"
#include "pulse-sink-input.h"
#include "pulse-stream.h"
#include "pulse-stream-control.h"

static void pulse_sink_input_class_init (PulseSinkInputClass *klass);
static void pulse_sink_input_init       (PulseSinkInput      *input);

G_DEFINE_TYPE (PulseSinkInput, pulse_sink_input, PULSE_TYPE_STREAM_CONTROL);

static gboolean      pulse_sink_input_set_mute       (PulseStreamControl *psc,
                                                      gboolean            mute);
static gboolean      pulse_sink_input_set_volume     (PulseStreamControl *psc,
                                                      pa_cvolume         *cvolume);
static PulseMonitor *pulse_sink_input_create_monitor (PulseStreamControl *psc);

static void
pulse_sink_input_class_init (PulseSinkInputClass *klass)
{
    PulseStreamControlClass *control_class;

    control_class = PULSE_STREAM_CONTROL_CLASS (klass);
    control_class->set_mute       = pulse_sink_input_set_mute;
    control_class->set_volume     = pulse_sink_input_set_volume;
    control_class->create_monitor = pulse_sink_input_create_monitor;
}

static void
pulse_sink_input_init (PulseSinkInput *input)
{
}

PulseSinkInput *
pulse_sink_input_new (PulseSink *sink, const pa_sink_input_info *info)
{
    PulseSinkInput   *input;
    gchar            *name;
    const gchar      *prop;
    const gchar      *label = NULL;
    MateMixerAppInfo *app_info = NULL;

    MateMixerStreamControlFlags flags = MATE_MIXER_STREAM_CONTROL_MUTE_READABLE |
                                        MATE_MIXER_STREAM_CONTROL_MUTE_WRITABLE |
                                        MATE_MIXER_STREAM_CONTROL_HAS_MONITOR;
    MateMixerStreamControlRole  role  = MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN;

    MateMixerStreamControlMediaRole media_role = MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_UNKNOWN;

    g_return_val_if_fail (PULSE_IS_SINK (sink), NULL);
    g_return_val_if_fail (info != NULL, NULL);

    /* Many mixer applications query the Pulse client list and use the client
     * name here, but we use the name only as an identifier, so let's avoid
     * this unnecessary overhead and use a custom name.
     * Also make sure to make the name unique by including the PulseAudio index. */
    name = g_strdup_printf ("pulse-output-control-%lu", (gulong) info->index);

#if PA_CHECK_VERSION(1, 0, 0)
    if (info->has_volume) {
        flags |=
            MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE |
            MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL;

        if (info->volume_writable)
            flags |= MATE_MIXER_STREAM_CONTROL_VOLUME_WRITABLE;
    }
#else
    /* Pre-1.0 PulseAudio does not include the has_volume and volume_writable
     * fields, but does include the volume info, so let's give it a try */
    flags |=
        MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE |
        MATE_MIXER_STREAM_CONTROL_VOLUME_WRITABLE |
        MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL;
#endif

    if (info->client != PA_INVALID_INDEX) {
        app_info = _mate_mixer_app_info_new ();

        role = MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION;

        prop = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_NAME);
        if (prop != NULL)
            _mate_mixer_app_info_set_name (app_info, prop);

        prop = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_ID);
        if (prop != NULL)
            _mate_mixer_app_info_set_id (app_info, prop);

        prop = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_VERSION);
        if (prop != NULL)
            _mate_mixer_app_info_set_version (app_info, prop);

        prop = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_ICON_NAME);
        if (prop != NULL)
            _mate_mixer_app_info_set_icon (app_info, prop);
    }

    prop = pa_proplist_gets (info->proplist, PA_PROP_MEDIA_ROLE);
    if (prop != NULL) {
        media_role = pulse_convert_media_role_name (prop);

        if (media_role == MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_EVENT) {
            /* The event description seems to provide much better readable
             * description for event streams */
            prop = pa_proplist_gets (info->proplist, PA_PROP_EVENT_DESCRIPTION);
            if (prop != NULL)
                label = prop;
        }
    }

    if (label == NULL)
        label = info->name;

    input = g_object_new (PULSE_TYPE_SINK_INPUT,
                          "name", name,
                          "label", label,
                          "flags", flags,
                          "role", role,
                          "media-role", media_role,
                          "index", info->index,
                          "stream", sink,
                          NULL);
    g_free (name);

    if (app_info != NULL)
        pulse_stream_control_set_app_info (PULSE_STREAM_CONTROL (input), app_info);

    pulse_sink_input_update (input, info);
    return input;
}

void
pulse_sink_input_update (PulseSinkInput *input, const pa_sink_input_info *info)
{
    g_return_if_fail (PULSE_IS_SINK_INPUT (input));
    g_return_if_fail (info != NULL);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (input));

    _mate_mixer_stream_control_set_mute (MATE_MIXER_STREAM_CONTROL (input),
                                         info->mute ? TRUE : FALSE);

#if PA_CHECK_VERSION(1, 0, 0)
    pulse_stream_control_set_channel_map (PULSE_STREAM_CONTROL (input), &info->channel_map);

    if (info->has_volume)
        pulse_stream_control_set_cvolume (PULSE_STREAM_CONTROL (input), &info->volume, 0);
    else
        pulse_stream_control_set_cvolume (PULSE_STREAM_CONTROL (input), NULL, 0);
#else
    pulse_stream_control_set_channel_map (PULSE_STREAM_CONTROL (input), &info->channel_map);
    pulse_stream_control_set_volume (PULSE_STREAM_CONTROL (input), &info->volume, 0);
#endif

    g_object_thaw_notify (G_OBJECT (input));
}

static gboolean
pulse_sink_input_set_mute (PulseStreamControl *psc, gboolean mute)
{
    g_return_val_if_fail (PULSE_IS_SINK_INPUT (psc), FALSE);

    return pulse_connection_set_sink_input_mute (PULSE_STREAM_CONTROL_GET_CONNECTION (psc),
                                                 pulse_stream_control_get_index (psc),
                                                 mute);
}

static gboolean
pulse_sink_input_set_volume (PulseStreamControl *psc, pa_cvolume *cvolume)
{
    g_return_val_if_fail (PULSE_IS_SINK_INPUT (psc), FALSE);
    g_return_val_if_fail (cvolume != NULL, FALSE);

    return pulse_connection_set_sink_input_volume (PULSE_STREAM_CONTROL_GET_CONNECTION (psc),
                                                   pulse_stream_control_get_index (psc),
                                                   cvolume);
}

static PulseMonitor *
pulse_sink_input_create_monitor (PulseStreamControl *psc)
{
    PulseSink *sink;
    guint32    index;

    g_return_val_if_fail (PULSE_IS_SINK_INPUT (psc), NULL);

    sink = PULSE_SINK (mate_mixer_stream_control_get_stream (MATE_MIXER_STREAM_CONTROL (psc)));

    index = pulse_sink_get_index_monitor (sink);
    if G_UNLIKELY (index == PA_INVALID_INDEX) {
        g_debug ("Monitor of stream control %s is not available",
                 mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (psc)));
        return NULL;
    }

    return pulse_connection_create_monitor (PULSE_STREAM_CONTROL_GET_CONNECTION (psc),
                                            index,
                                            pulse_stream_control_get_index (psc));
}
