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

#include <string.h>
#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-client-stream.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-stream.h>

#include <pulse/pulseaudio.h>
#include <pulse/ext-stream-restore.h>

#include "pulse-connection.h"
#include "pulse-client-stream.h"
#include "pulse-ext-stream.h"
#include "pulse-helpers.h"
#include "pulse-sink.h"
#include "pulse-source.h"
#include "pulse-stream.h"

static void pulse_ext_stream_class_init (PulseExtStreamClass *klass);
static void pulse_ext_stream_init       (PulseExtStream      *ext);

G_DEFINE_TYPE (PulseExtStream, pulse_ext_stream, PULSE_TYPE_CLIENT_STREAM);

static void     pulse_ext_stream_reload     (PulseStream       *pstream);

static gboolean pulse_ext_stream_set_mute   (PulseStream       *pstream,
                                             gboolean           mute);
static gboolean pulse_ext_stream_set_volume (PulseStream       *pstream,
                                             pa_cvolume        *cvolume);
static gboolean pulse_ext_stream_set_parent (PulseClientStream *pclient,
                                             PulseStream       *parent);
static gboolean pulse_ext_stream_remove     (PulseClientStream *pclient);

static void
pulse_ext_stream_class_init (PulseExtStreamClass *klass)
{
    PulseStreamClass       *stream_class;
    PulseClientStreamClass *client_class;

    stream_class = PULSE_STREAM_CLASS (klass);

    stream_class->reload     = pulse_ext_stream_reload;
    stream_class->set_mute   = pulse_ext_stream_set_mute;
    stream_class->set_volume = pulse_ext_stream_set_volume;

    client_class = PULSE_CLIENT_STREAM_CLASS (klass);

    client_class->set_parent = pulse_ext_stream_set_parent;
    client_class->remove     = pulse_ext_stream_remove;
}

static void
pulse_ext_stream_init (PulseExtStream *ext)
{
}

PulseStream *
pulse_ext_stream_new (PulseConnection                  *connection,
                      const pa_ext_stream_restore_info *info,
                      PulseStream                      *parent)
{
    PulseStream *ext;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);

    ext = g_object_new (PULSE_TYPE_EXT_STREAM,
                        "connection", connection,
                        NULL);

    /* Consider the stream name as unchanging parameter */
    pulse_stream_update_name (ext, info->name);

    /* Other data may change at any time, so let's make a use of our update function */
    pulse_ext_stream_update (ext, info, parent);

    return ext;
}

gboolean
pulse_ext_stream_update (PulseStream                      *pstream,
                         const pa_ext_stream_restore_info *info,
                         PulseStream                      *parent)
{
    MateMixerClientStreamRole  role  = MATE_MIXER_CLIENT_STREAM_ROLE_NONE;
    MateMixerStreamFlags       flags = MATE_MIXER_STREAM_CLIENT |
                                       MATE_MIXER_STREAM_HAS_VOLUME |
                                       MATE_MIXER_STREAM_HAS_MUTE |
                                       MATE_MIXER_STREAM_CAN_SET_VOLUME;
    MateMixerClientStreamFlags client_flags =
                                       MATE_MIXER_CLIENT_STREAM_CACHED;

    PulseClientStream         *pclient;
    gchar                     *suffix;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (pstream), FALSE);
    g_return_val_if_fail (info != NULL, FALSE);

    pclient = PULSE_CLIENT_STREAM (pstream);

    suffix = strchr (info->name, ':');
    if (suffix != NULL)
        suffix++;

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (pstream));

    if (g_str_has_prefix (info->name, "sink-input"))
        flags |= MATE_MIXER_STREAM_OUTPUT;
    else if (g_str_has_prefix (info->name, "source-output"))
        flags |= MATE_MIXER_STREAM_INPUT;
    else
        g_debug ("Unknown ext-stream %s", info->name);

    if (strstr (info->name, "-by-media-role:")) {
        if (G_LIKELY (suffix != NULL))
            role = pulse_convert_media_role_name (suffix);
    }
    else if (strstr (info->name, "-by-application-name:")) {
        client_flags |= MATE_MIXER_CLIENT_STREAM_APPLICATION;

        if (G_LIKELY (suffix != NULL))
            pulse_client_stream_update_app_name (pclient, suffix);
    }
    else if (strstr (info->name, "-by-application-id:")) {
        client_flags |= MATE_MIXER_CLIENT_STREAM_APPLICATION;

        if (G_LIKELY (suffix != NULL))
            pulse_client_stream_update_app_id (pclient, suffix);
    }

    /* Flags needed before volume */
    pulse_stream_update_flags (pstream, flags);

    pulse_stream_update_channel_map (pstream, &info->channel_map);
    pulse_stream_update_volume (pstream, &info->volume, 0);

    pulse_stream_update_mute (pstream, info->mute ? TRUE : FALSE);

    pulse_client_stream_update_flags (pclient, client_flags);
    pulse_client_stream_update_role (pclient, role);

    if (parent != NULL)
        pulse_client_stream_update_parent (pclient, MATE_MIXER_STREAM (parent));
    else
        pulse_client_stream_update_parent (pclient, NULL);

    g_object_thaw_notify (G_OBJECT (pstream));
    return TRUE;
}

static void
pulse_ext_stream_reload (PulseStream *pstream)
{
    g_return_if_fail (PULSE_IS_EXT_STREAM (pstream));

    pulse_connection_load_ext_stream_info (pulse_stream_get_connection (pstream));
}

static gboolean
pulse_ext_stream_set_mute (PulseStream *pstream, gboolean mute)
{
    MateMixerStream           *parent;
    const pa_channel_map      *map;
    const pa_cvolume          *cvolume;
    pa_ext_stream_restore_info info;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (pstream), FALSE);

    info.name = mate_mixer_stream_get_name (MATE_MIXER_STREAM (pstream));
    info.mute = mute;

    map = pulse_stream_get_channel_map (pstream);
    if (map != NULL)
        info.channel_map = *map;
    else
        pa_channel_map_init (&info.channel_map);

    cvolume = pulse_stream_get_cvolume (pstream);
    if (cvolume != NULL)
        info.volume = *cvolume;
    else
        pa_cvolume_init (&info.volume);

    parent = mate_mixer_client_stream_get_parent (MATE_MIXER_CLIENT_STREAM (pstream));
    if (parent != NULL)
        info.device = mate_mixer_stream_get_name (parent);
    else
        info.device = NULL;

    return pulse_connection_write_ext_stream (pulse_stream_get_connection (pstream), &info);
}

static gboolean
pulse_ext_stream_set_volume (PulseStream *pstream, pa_cvolume *cvolume)
{
    MateMixerStream           *parent;
    const pa_channel_map      *map;
    pa_ext_stream_restore_info info;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (pstream), FALSE);
    g_return_val_if_fail (cvolume != NULL, FALSE);

    info.name = mate_mixer_stream_get_name (MATE_MIXER_STREAM (pstream));
    info.mute = mate_mixer_stream_get_mute (MATE_MIXER_STREAM (pstream));

    map = pulse_stream_get_channel_map (pstream);
    if (map != NULL)
        info.channel_map = *map;
    else
        pa_channel_map_init (&info.channel_map);

    parent = mate_mixer_client_stream_get_parent (MATE_MIXER_CLIENT_STREAM (pstream));
    if (parent != NULL)
        info.device = mate_mixer_stream_get_name (parent);
    else
        info.device = NULL;

    info.volume = *cvolume;

    return pulse_connection_write_ext_stream (pulse_stream_get_connection (pstream), &info);
}

static gboolean
pulse_ext_stream_set_parent (PulseClientStream *pclient, PulseStream *parent)
{
    MateMixerStreamFlags       flags;
    PulseStream               *pstream;
    const pa_channel_map      *map;
    const pa_cvolume          *cvolume;
    pa_ext_stream_restore_info info;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (pclient), FALSE);
    g_return_val_if_fail (PULSE_IS_STREAM (parent), FALSE);

    flags = mate_mixer_stream_get_flags (MATE_MIXER_STREAM (pclient));

    /* Validate the parent stream */
    if (flags & MATE_MIXER_STREAM_INPUT && !PULSE_IS_SOURCE (parent)) {
        g_warning ("Could not change stream parent to %s: not a parent input stream",
                   mate_mixer_stream_get_name (MATE_MIXER_STREAM (parent)));
        return FALSE;
    } else if (!PULSE_IS_SINK (parent)) {
        g_warning ("Could not change stream parent to %s: not a parent output stream",
                   mate_mixer_stream_get_name (MATE_MIXER_STREAM (parent)));
        return FALSE;
    }

    pstream = PULSE_STREAM (pclient);

    info.name = mate_mixer_stream_get_name (MATE_MIXER_STREAM (pstream));
    info.mute = mate_mixer_stream_get_mute (MATE_MIXER_STREAM (pstream));

    map = pulse_stream_get_channel_map (pstream);
    if (map != NULL)
        info.channel_map = *map;
    else
        pa_channel_map_init (&info.channel_map);

    cvolume = pulse_stream_get_cvolume (pstream);
    if (cvolume != NULL)
        info.volume = *cvolume;
    else
        pa_cvolume_init (&info.volume);

    info.device = mate_mixer_stream_get_name (MATE_MIXER_STREAM (parent));

    return pulse_connection_write_ext_stream (pulse_stream_get_connection (pstream), &info);
}

static gboolean
pulse_ext_stream_remove (PulseClientStream *pclient)
{
    PulseStream *pstream;
    const gchar *name;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (pclient), FALSE);

    pstream = PULSE_STREAM (pclient);
    name = mate_mixer_stream_get_name (MATE_MIXER_STREAM (pstream));

    return pulse_connection_delete_ext_stream (pulse_stream_get_connection (pstream), name);
}
