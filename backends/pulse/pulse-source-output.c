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
#include "pulse-source.h"
#include "pulse-source-output.h"
#include "pulse-stream.h"
#include "pulse-stream-control.h"

enum {
    STREAM_CHANGED_BY_REQUEST,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void pulse_source_output_class_init (PulseSourceOutputClass *klass);
static void pulse_source_output_init       (PulseSourceOutput      *output);

G_DEFINE_TYPE (PulseSourceOutput, pulse_source_output, PULSE_TYPE_STREAM_CONTROL);

static gboolean      pulse_source_output_set_stream     (MateMixerStreamControl *mmsc,
                                                         MateMixerStream        *mms);
static guint         pulse_source_output_get_max_volume (MateMixerStreamControl *mmsc);

static gboolean      pulse_source_output_set_mute       (PulseStreamControl     *psc,
                                                         gboolean                mute);
static gboolean      pulse_source_output_set_volume     (PulseStreamControl     *psc,
                                                         pa_cvolume             *cvolume);
static PulseMonitor *pulse_source_output_create_monitor (PulseStreamControl     *psc);

static void
pulse_source_output_class_init (PulseSourceOutputClass *klass)
{
    MateMixerStreamControlClass *mmsc_class;
    PulseStreamControlClass     *control_class;

    mmsc_class = MATE_MIXER_STREAM_CONTROL_CLASS (klass);
    mmsc_class->set_stream        = pulse_source_output_set_stream;
    mmsc_class->get_max_volume    = pulse_source_output_get_max_volume;

    control_class = PULSE_STREAM_CONTROL_CLASS (klass);
    control_class->set_mute       = pulse_source_output_set_mute;
    control_class->set_volume     = pulse_source_output_set_volume;
    control_class->create_monitor = pulse_source_output_create_monitor;

    signals[STREAM_CHANGED_BY_REQUEST] =
        g_signal_new ("stream-changed-by-request",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseSourceOutputClass, stream_changed_by_request),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_OBJECT);
}

static void
pulse_source_output_init (PulseSourceOutput *output)
{
}

PulseSourceOutput *
pulse_source_output_new (PulseConnection             *connection,
                         const pa_source_output_info *info,
                         PulseSource                 *parent)
{
    PulseSourceOutput *output;
    gchar             *name;
    const gchar       *prop;
    MateMixerAppInfo  *app_info = NULL;

    MateMixerStreamControlFlags flags = MATE_MIXER_STREAM_CONTROL_MUTE_READABLE |
                                        MATE_MIXER_STREAM_CONTROL_MUTE_WRITABLE |
                                        MATE_MIXER_STREAM_CONTROL_MOVABLE |
                                        MATE_MIXER_STREAM_CONTROL_HAS_MONITOR;
    MateMixerStreamControlRole  role  = MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN;

    MateMixerStreamControlMediaRole media_role = MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_UNKNOWN;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);
    g_return_val_if_fail (PULSE_IS_SOURCE (parent), NULL);

    /* Many mixer applications query the Pulse client list and use the client
     * name here, but we use the name only as an identifier, so let's avoid
     * this unnecessary overhead and use a custom name.
     * Also make sure to make the name unique by including the PulseAudio index. */
    name = g_strdup_printf ("pulse-input-control-%lu", (gulong) info->index);

    if (info->has_volume) {
        flags |=
            MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE |
            MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL;

        if (info->volume_writable)
            flags |= MATE_MIXER_STREAM_CONTROL_VOLUME_WRITABLE;
    }

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
    if (prop != NULL)
        media_role = pulse_convert_media_role_name (prop);

    output = g_object_new (PULSE_TYPE_SOURCE_OUTPUT,
                          "name", name,
                          "label", info->name,
                          "flags", flags,
                          "role", role,
                          "media-role", media_role,
                          "stream", parent,
                          "connection", connection,
                          "index", info->index,
                          NULL);
    g_free (name);

    if (app_info != NULL) {
        /* Takes ownership of app_info */
        pulse_stream_control_set_app_info (PULSE_STREAM_CONTROL (output),
                                           app_info,
                                           TRUE);
    }

    /* Read the rest of the output's values, parent was already given during
     * the construction */
    pulse_source_output_update (output, info, NULL);
    return output;
}

void
pulse_source_output_update (PulseSourceOutput           *output,
                            const pa_source_output_info *info,
                            PulseSource                 *parent)
{
    g_return_if_fail (PULSE_IS_SOURCE_OUTPUT (output));
    g_return_if_fail (parent == NULL || PULSE_IS_SOURCE (parent));

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (output));

    if (info != NULL) {
        _mate_mixer_stream_control_set_mute (MATE_MIXER_STREAM_CONTROL (output),
                                             info->mute ? TRUE : FALSE);

        pulse_stream_control_set_channel_map (PULSE_STREAM_CONTROL (output),
                                              &info->channel_map);
        if (info->has_volume)
            pulse_stream_control_set_cvolume (PULSE_STREAM_CONTROL (output),
                                              &info->volume,
                                              0);
        else
            pulse_stream_control_set_cvolume (PULSE_STREAM_CONTROL (output),
                                              NULL,
                                              0);
    }

    /* Source output must always have a parent source, but it is possible to
     * pass a NULL parent to indicate that it has not changed */
    if (parent != NULL)
        _mate_mixer_stream_control_set_stream (MATE_MIXER_STREAM_CONTROL (output),
                                               MATE_MIXER_STREAM (parent));

    g_object_thaw_notify (G_OBJECT (output));
}

static gboolean
pulse_source_output_set_stream (MateMixerStreamControl *mmsc, MateMixerStream *mms)
{
    PulseStreamControl *psc;
    gboolean            ret;

    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (mmsc), FALSE);
    g_return_val_if_fail (PULSE_IS_SOURCE (mms), FALSE);

    psc = PULSE_STREAM_CONTROL (mmsc);
    ret = pulse_connection_move_source_output (pulse_stream_control_get_connection (psc),
                                               pulse_stream_control_get_index (psc),
                                               pulse_stream_get_index (PULSE_STREAM (mms)));
    if (ret == TRUE)
        g_signal_emit (G_OBJECT (psc),
                       signals[STREAM_CHANGED_BY_REQUEST],
                       0,
                       mms);

    return ret;
}

static guint
pulse_source_output_get_max_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (mmsc), (guint) PA_VOLUME_MUTED);

    /* Do not extend the volume to PA_VOLUME_UI_MAX as PulseStreamControl does */
    return (guint) PA_VOLUME_NORM;
}

static gboolean
pulse_source_output_set_mute (PulseStreamControl *psc, gboolean mute)
{
    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (psc), FALSE);

    return pulse_connection_set_source_output_mute (pulse_stream_control_get_connection (psc),
                                                    pulse_stream_control_get_index (psc),
                                                    mute);
}

static gboolean
pulse_source_output_set_volume (PulseStreamControl *psc, pa_cvolume *cvolume)
{
    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (psc), FALSE);
    g_return_val_if_fail (cvolume != NULL, FALSE);

    return pulse_connection_set_source_output_volume (pulse_stream_control_get_connection (psc),
                                                      pulse_stream_control_get_index (psc),
                                                      cvolume);
}

static PulseMonitor *
pulse_source_output_create_monitor (PulseStreamControl *psc)
{
    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (psc), NULL);

    return pulse_connection_create_monitor (pulse_stream_control_get_connection (psc),
                                            pulse_stream_control_get_stream_index (psc),
                                            PA_INVALID_INDEX);
}
