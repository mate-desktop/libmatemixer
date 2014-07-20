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
#include <string.h>

#include <libmatemixer/matemixer-client-stream.h>
#include <libmatemixer/matemixer-stream.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-client-stream.h"
#include "pulse-helpers.h"
#include "pulse-monitor.h"
#include "pulse-stream.h"
#include "pulse-source.h"
#include "pulse-source-output.h"

static void pulse_source_output_class_init (PulseSourceOutputClass *klass);
static void pulse_source_output_init       (PulseSourceOutput      *output);

G_DEFINE_TYPE (PulseSourceOutput, pulse_source_output, PULSE_TYPE_CLIENT_STREAM);

static void          pulse_source_output_reload         (PulseStream       *pstream);

static gboolean      pulse_source_output_set_mute       (PulseStream       *pstream,
                                                         gboolean           mute);
static gboolean      pulse_source_output_set_volume     (PulseStream       *pstream,
                                                         pa_cvolume        *cvolume);

static gboolean      pulse_source_output_set_parent     (PulseClientStream *pclient,
                                                         PulseStream       *parent);
static gboolean      pulse_source_output_remove         (PulseClientStream *pclient);

static PulseMonitor *pulse_source_output_create_monitor (PulseStream       *pstream);

static void
pulse_source_output_class_init (PulseSourceOutputClass *klass)
{
    PulseStreamClass       *stream_class;
    PulseClientStreamClass *client_class;

    stream_class = PULSE_STREAM_CLASS (klass);

    stream_class->reload         = pulse_source_output_reload;
    stream_class->set_mute       = pulse_source_output_set_mute;
    stream_class->set_volume     = pulse_source_output_set_volume;
    stream_class->create_monitor = pulse_source_output_create_monitor;

    client_class = PULSE_CLIENT_STREAM_CLASS (klass);

    client_class->set_parent     = pulse_source_output_set_parent;
    client_class->remove         = pulse_source_output_remove;
}

static void
pulse_source_output_init (PulseSourceOutput *output)
{
}

PulseStream *
pulse_source_output_new (PulseConnection             *connection,
                         const pa_source_output_info *info,
                         PulseStream                 *parent)
{
    PulseSourceOutput *output;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);

    /* Consider the sink input index as unchanging parameter */
    output = g_object_new (PULSE_TYPE_SOURCE_OUTPUT,
                           "connection", connection,
                           "index", info->index,
                           NULL);

    /* Other data may change at any time, so let's make a use of our update function */
    pulse_source_output_update (PULSE_STREAM (output), info, parent);

    return PULSE_STREAM (output);
}

gboolean
pulse_source_output_update (PulseStream                 *pstream,
                            const pa_source_output_info *info,
                            PulseStream                 *parent)
{
    MateMixerStreamFlags flags = MATE_MIXER_STREAM_INPUT |
                                 MATE_MIXER_STREAM_CLIENT;
    PulseClientStream   *pclient;
    const gchar         *prop;
    const gchar         *description = NULL;
    gchar               *name;

    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (pstream), FALSE);
    g_return_val_if_fail (info != NULL, FALSE);

    pclient = PULSE_CLIENT_STREAM (pstream);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (pstream));

    /* Many other mixer applications query the Pulse client list and use the
     * client name here, but we use the name only as an identifier, so let's avoid
     * this unnecessary overhead and use a custom name.
     * Also make sure to make the name unique by including the Pulse index. */
    name = g_strdup_printf ("pulse-stream-client-input-%lu", (gulong) info->index);

    pulse_stream_update_name (pstream, name);
    g_free (name);

    prop = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_NAME);
    if (prop != NULL)
        pulse_client_stream_update_app_name (pclient, prop);

    prop = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_ID);
    if (prop != NULL)
        pulse_client_stream_update_app_id (pclient, prop);

    prop = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_VERSION);
    if (prop != NULL)
        pulse_client_stream_update_app_version (pclient, prop);

    prop = pa_proplist_gets (info->proplist, PA_PROP_APPLICATION_ICON_NAME);
    if (prop != NULL)
        pulse_client_stream_update_app_icon (pclient, prop);

    prop = pa_proplist_gets (info->proplist, PA_PROP_MEDIA_ROLE);
    if (prop != NULL) {
        MateMixerClientStreamRole role = pulse_convert_media_role_name (prop);

        if (role == MATE_MIXER_CLIENT_STREAM_ROLE_EVENT) {
            /* The event description seems to provide much better readable
             * description for event streams */
            prop = pa_proplist_gets (info->proplist, PA_PROP_EVENT_DESCRIPTION);

            if (G_LIKELY (prop != NULL))
                description = prop;
        }
        pulse_client_stream_update_role (pclient, role);
    } else
        pulse_client_stream_update_role (pclient, MATE_MIXER_CLIENT_STREAM_ROLE_NONE);

    if (description == NULL)
        description = info->name;

    pulse_stream_update_description (pstream, description);

    if (info->client != PA_INVALID_INDEX)
        pulse_client_stream_update_flags (pclient, MATE_MIXER_CLIENT_STREAM_APPLICATION);
    else
        pulse_client_stream_update_flags (pclient, MATE_MIXER_CLIENT_STREAM_NO_FLAGS);

    if (G_LIKELY (parent != NULL)) {
        pulse_client_stream_update_parent (pclient, MATE_MIXER_STREAM (parent));
        flags |= MATE_MIXER_STREAM_HAS_MONITOR;
    } else
        pulse_client_stream_update_parent (pclient, NULL);

#if PA_CHECK_VERSION(1, 0, 0)
    if (info->has_volume) {
        flags |= MATE_MIXER_STREAM_HAS_VOLUME;

        if (info->volume_writable)
            flags |= MATE_MIXER_STREAM_CAN_SET_VOLUME;
    }

    flags |= MATE_MIXER_STREAM_HAS_MUTE;

    /* Flags needed before volume */
    pulse_stream_update_flags (pstream, flags);
    pulse_stream_update_channel_map (pstream, &info->channel_map);
    pulse_stream_update_mute (pstream, info->mute ? TRUE : FALSE);

    if (info->has_volume)
        pulse_stream_update_volume (pstream, &info->volume, 0);
    else
        pulse_stream_update_volume (pstream, NULL, 0);
#else
    /* Flags needed before volume */
    pulse_stream_update_flags (pstream, flags);

    pulse_stream_update_channel_map (pstream, &info->channel_map);
    pulse_stream_update_volume (pstream, NULL, 0);
#endif

    // XXX needs to fix monitor if parent changes

    g_object_thaw_notify (G_OBJECT (pstream));
    return TRUE;
}

static void
pulse_source_output_reload (PulseStream *pstream)
{
    g_return_if_fail (PULSE_IS_SOURCE_OUTPUT (pstream));

    pulse_connection_load_source_output_info (pulse_stream_get_connection (pstream),
                                              pulse_stream_get_index (pstream));
}

static gboolean
pulse_source_output_set_mute (PulseStream *pstream, gboolean mute)
{
    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (pstream), FALSE);

    return pulse_connection_set_source_output_mute (pulse_stream_get_connection (pstream),
                                                    pulse_stream_get_index (pstream),
                                                    mute);
}

static gboolean
pulse_source_output_set_volume (PulseStream *pstream, pa_cvolume *cvolume)
{
    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (pstream), FALSE);
    g_return_val_if_fail (cvolume != NULL, FALSE);

    return pulse_connection_set_source_output_volume (pulse_stream_get_connection (pstream),
                                                      pulse_stream_get_index (pstream),
                                                      cvolume);
}

static gboolean
pulse_source_output_set_parent (PulseClientStream *pclient, PulseStream *parent)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (pclient), FALSE);

    pstream = PULSE_STREAM (pclient);

    return pulse_connection_move_sink_input (pulse_stream_get_connection (pstream),
                                             pulse_stream_get_index (pstream),
                                             pulse_stream_get_index (parent));
}

static gboolean
pulse_source_output_remove (PulseClientStream *pclient)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (pclient), FALSE);

    pstream = PULSE_STREAM (pclient);

    return pulse_connection_kill_source_output (pulse_stream_get_connection (pstream),
                                                pulse_stream_get_index (pstream));
}

static PulseMonitor *
pulse_source_output_create_monitor (PulseStream *pstream)
{
    MateMixerStream *parent;

    g_return_val_if_fail (PULSE_IS_SOURCE_OUTPUT (pstream), NULL);

    parent = mate_mixer_client_stream_get_parent (MATE_MIXER_CLIENT_STREAM (pstream));
    if (G_UNLIKELY (parent == NULL)) {
        g_debug ("Not creating monitor for client stream %s: not available",
                 mate_mixer_stream_get_name (MATE_MIXER_STREAM (pstream)));
        return NULL;
    }

    return pulse_connection_create_monitor (pulse_stream_get_connection (pstream),
                                            pulse_stream_get_index (PULSE_STREAM (parent)),
                                            PA_INVALID_INDEX);
}
