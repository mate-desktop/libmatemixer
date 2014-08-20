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

#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include <pulse/pulseaudio.h>
#include <pulse/ext-stream-restore.h>

#include "pulse-connection.h"
#include "pulse-ext-stream.h"
#include "pulse-helpers.h"
#include "pulse-stream.h"
#include "pulse-stream-control.h"

struct _PulseExtStreamPrivate
{
    MateMixerAppInfo  *app_info;
    MateMixerDirection direction;
};

enum {
    PROP_0,
    PROP_DIRECTION
};

static void mate_mixer_stored_control_interface_init (MateMixerStoredControlInterface *iface);

static void pulse_ext_stream_class_init   (PulseExtStreamClass *klass);

static void pulse_ext_stream_get_property (GObject             *object,
                                           guint                param_id,
                                           GValue              *value,
                                           GParamSpec          *pspec);
static void pulse_ext_stream_set_property (GObject             *object,
                                           guint                param_id,
                                           const GValue        *value,
                                           GParamSpec          *pspec);

static void pulse_ext_stream_init         (PulseExtStream      *ext);

G_DEFINE_TYPE_WITH_CODE (PulseExtStream, pulse_ext_stream, PULSE_TYPE_STREAM_CONTROL,
                         G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_STORED_CONTROL,
                                                mate_mixer_stored_control_interface_init))

static MateMixerDirection pulse_ext_stream_get_direction (MateMixerStoredControl     *mmsc);

static MateMixerAppInfo * pulse_ext_stream_get_app_info  (MateMixerStreamControl     *mmsc);

static gboolean           pulse_ext_stream_set_stream    (MateMixerStreamControl     *mmsc,
                                                          MateMixerStream            *mms);

static gboolean           pulse_ext_stream_set_mute      (PulseStreamControl         *control,
                                                          gboolean                    mute);
static gboolean           pulse_ext_stream_set_volume    (PulseStreamControl         *control,
                                                          pa_cvolume                 *cvolume);

static void               fill_ext_stream_restore_info   (PulseStreamControl         *control,
                                                          pa_ext_stream_restore_info *info);

static void
mate_mixer_stored_control_interface_init (MateMixerStoredControlInterface *iface)
{
    iface->get_direction = pulse_ext_stream_get_direction;
}

static void
pulse_ext_stream_class_init (PulseExtStreamClass *klass)
{
    GObjectClass                *object_class;
    MateMixerStreamControlClass *control_class;
    PulseStreamControlClass     *pulse_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->get_property = pulse_ext_stream_get_property;
    object_class->set_property = pulse_ext_stream_set_property;

    control_class = MATE_MIXER_STREAM_CONTROL_CLASS (klass);
    control_class->get_app_info = pulse_ext_stream_get_app_info;
    control_class->set_stream   = pulse_ext_stream_set_stream;

    pulse_class = PULSE_STREAM_CONTROL_CLASS (klass);
    pulse_class->set_mute   = pulse_ext_stream_set_mute;
    pulse_class->set_volume = pulse_ext_stream_set_volume;

    g_object_class_override_property (object_class, PROP_DIRECTION, "direction");

    g_type_class_add_private (object_class, sizeof (PulseExtStreamPrivate));
}

static void
pulse_ext_stream_get_property (GObject      *object,
                               guint         param_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
    PulseExtStream *ext;

    ext = PULSE_EXT_STREAM (object);

    switch (param_id) {
    case PROP_DIRECTION:
        g_value_set_enum (value, ext->priv->direction);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_ext_stream_set_property (GObject      *object,
                               guint         param_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    PulseExtStream *ext;

    ext = PULSE_EXT_STREAM (object);

    switch (param_id) {
    case PROP_DIRECTION:
        ext->priv->direction = g_value_get_enum (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_ext_stream_init (PulseExtStream *ext)
{
    ext->priv = G_TYPE_INSTANCE_GET_PRIVATE (ext,
                                             PULSE_TYPE_EXT_STREAM,
                                             PulseExtStreamPrivate);
}

PulseExtStream *
pulse_ext_stream_new (PulseConnection                  *connection,
                      const pa_ext_stream_restore_info *info,
                      PulseStream                      *parent)
{
    PulseExtStream             *ext;
    gchar                      *suffix;
    MateMixerDirection          direction;
    MateMixerStreamControlFlags flags = MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE |
                                        MATE_MIXER_STREAM_CONTROL_VOLUME_WRITABLE |
                                        MATE_MIXER_STREAM_CONTROL_MUTE_READABLE |
                                        MATE_MIXER_STREAM_CONTROL_MUTE_WRITABLE;
    MateMixerStreamControlRole  role  = MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN;
    MateMixerAppInfo           *app_info;

    MateMixerStreamControlMediaRole media_role = MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_UNKNOWN;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);

    if (g_str_has_prefix (info->name, "sink-input"))
        direction = MATE_MIXER_DIRECTION_OUTPUT;
    else if (g_str_has_prefix (info->name, "source-output"))
        direction = MATE_MIXER_DIRECTION_INPUT;
    else
        direction = MATE_MIXER_DIRECTION_UNKNOWN;

    app_info = _mate_mixer_app_info_new ();

    suffix = strchr (info->name, ':');
    if (suffix != NULL)
        suffix++;

    if (strstr (info->name, "-by-media-role:")) {
        if (G_LIKELY (suffix != NULL))
            media_role = pulse_convert_media_role_name (suffix);
    }
    else if (strstr (info->name, "-by-application-name:")) {
        role = MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION;

        if (G_LIKELY (suffix != NULL))
            _mate_mixer_app_info_set_name (app_info, suffix);
    }
    else if (strstr (info->name, "-by-application-id:")) {
        role = MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION;

        if (G_LIKELY (suffix != NULL))
            _mate_mixer_app_info_set_id (app_info, suffix);
    }

    ext = g_object_new (PULSE_TYPE_EXT_STREAM,
                        "flags", flags,
                        "role", role,
                        "media-role", media_role,
                        "name", info->name,
                        "connection", connection,
                        "direction", direction,
                        "stream", parent,
                        NULL);

    if (role == MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION)
        ext->priv->app_info = app_info;
    else
        _mate_mixer_app_info_free (app_info);

    pulse_ext_stream_update (ext, info, parent);
    return ext;
}

void
pulse_ext_stream_update (PulseExtStream                   *ext,
                         const pa_ext_stream_restore_info *info,
                         PulseStream                      *parent)
{
    g_return_if_fail (PULSE_IS_EXT_STREAM (ext));
    g_return_if_fail (info != NULL);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (ext));

    _mate_mixer_stream_control_set_mute (MATE_MIXER_STREAM_CONTROL (ext),
                                         info->mute ? TRUE : FALSE);

    pulse_stream_control_set_channel_map (PULSE_STREAM_CONTROL (ext),
                                          &info->channel_map);

    pulse_stream_control_set_cvolume (PULSE_STREAM_CONTROL (ext),
                                      &info->volume,
                                      0);

    _mate_mixer_stream_control_set_stream (MATE_MIXER_STREAM_CONTROL (ext),
                                           MATE_MIXER_STREAM (parent));

    g_object_thaw_notify (G_OBJECT (ext));
}

static MateMixerDirection
pulse_ext_stream_get_direction (MateMixerStoredControl *mmsc)
{
    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), MATE_MIXER_DIRECTION_UNKNOWN);

    return PULSE_EXT_STREAM (mmsc)->priv->direction;
}

static MateMixerAppInfo *
pulse_ext_stream_get_app_info (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), NULL);

    return PULSE_EXT_STREAM (mmsc)->priv->app_info;
}

static gboolean
pulse_ext_stream_set_stream (MateMixerStreamControl *mmsc, MateMixerStream *mms)
{
    pa_ext_stream_restore_info info;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), FALSE);
    g_return_val_if_fail (mms == NULL || PULSE_IS_STREAM (mms), FALSE);

    fill_ext_stream_restore_info (PULSE_STREAM_CONTROL (mmsc), &info);

    if (mms != NULL)
        info.device = mate_mixer_stream_get_name (mms);
    else
        info.device = NULL;

    return pulse_connection_write_ext_stream (pulse_stream_control_get_connection (PULSE_STREAM_CONTROL (mmsc)),
                                              &info);
}

static gboolean
pulse_ext_stream_set_mute (PulseStreamControl *control, gboolean mute)
{
    pa_ext_stream_restore_info info;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (control), FALSE);

    fill_ext_stream_restore_info (control, &info);

    info.mute = mute;

    return pulse_connection_write_ext_stream (pulse_stream_control_get_connection (control),
                                              &info);
}

static gboolean
pulse_ext_stream_set_volume (PulseStreamControl *control, pa_cvolume *cvolume)
{
    pa_ext_stream_restore_info info;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (control), FALSE);
    g_return_val_if_fail (cvolume != NULL, FALSE);

    fill_ext_stream_restore_info (control, &info);

    info.volume = *cvolume;

    return pulse_connection_write_ext_stream (pulse_stream_control_get_connection (control),
                                              &info);
}

static void
fill_ext_stream_restore_info (PulseStreamControl         *control,
                              pa_ext_stream_restore_info *info)
{
    MateMixerStream        *stream;
    MateMixerStreamControl *mmsc;
    const pa_channel_map   *map;
    const pa_cvolume       *cvolume;

    mmsc = MATE_MIXER_STREAM_CONTROL (control);

    info->name = mate_mixer_stream_control_get_name (mmsc);
    info->mute = mate_mixer_stream_control_get_mute (mmsc);

    map = pulse_stream_control_get_channel_map (control);
    if G_LIKELY (map != NULL)
        info->channel_map = *map;
    else
        pa_channel_map_init (&info->channel_map);

    cvolume = pulse_stream_control_get_cvolume (control);
    if G_LIKELY (cvolume != NULL)
        info->volume = *cvolume;
    else
        pa_cvolume_init (&info->volume);

    stream = mate_mixer_stream_control_get_stream (mmsc);
    if (stream != NULL)
        info->device = mate_mixer_stream_get_name (stream);
    else
        info->device = NULL;
}
