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
    guint             volume;
    pa_cvolume        cvolume;
    pa_channel_map    channel_map;
    MateMixerAppInfo *app_info;
    PulseConnection  *connection;
};

enum {
    PROP_0,
    PROP_CONNECTION,
    PROP_APP_INFO,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

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
static void pulse_ext_stream_dispose      (GObject             *object);
static void pulse_ext_stream_finalize     (GObject             *object);

G_DEFINE_TYPE (PulseExtStream, pulse_ext_stream, MATE_MIXER_TYPE_STORED_CONTROL)

static MateMixerAppInfo *       pulse_ext_stream_get_app_info         (MateMixerStreamControl  *mmsc);

static gboolean                 pulse_ext_stream_set_stream           (MateMixerStreamControl  *mmsc,
                                                                       MateMixerStream         *mms);

static gboolean                 pulse_ext_stream_set_mute             (MateMixerStreamControl  *mmsc,
                                                                       gboolean                 mute);

static guint                    pulse_ext_stream_get_num_channels     (MateMixerStreamControl  *mmsc);

static guint                    pulse_ext_stream_get_volume           (MateMixerStreamControl  *mmsc);
static gboolean                 pulse_ext_stream_set_volume           (MateMixerStreamControl  *mmsc,
                                                                       guint                    volume);

static guint                    pulse_ext_stream_get_channel_volume   (MateMixerStreamControl  *mmsc,
                                                                       guint                    channel);
static gboolean                 pulse_ext_stream_set_channel_volume   (MateMixerStreamControl  *mmsc,
                                                                       guint                    channel,
                                                                       guint                    volume);

static MateMixerChannelPosition pulse_ext_stream_get_channel_position (MateMixerStreamControl  *mmsc,
                                                                       guint                    channel);
static gboolean                 pulse_ext_stream_has_channel_position (MateMixerStreamControl  *mmsc,
                                                                       MateMixerChannelPosition position);

static gboolean                 pulse_ext_stream_set_balance          (MateMixerStreamControl  *mmsc,
                                                                       gfloat                   balance);

static gboolean                 pulse_ext_stream_set_fade             (MateMixerStreamControl  *mmsc,
                                                                       gfloat                   fade);

static guint                    pulse_ext_stream_get_min_volume       (MateMixerStreamControl  *mmsc);
static guint                    pulse_ext_stream_get_max_volume       (MateMixerStreamControl  *mmsc);
static guint                    pulse_ext_stream_get_normal_volume    (MateMixerStreamControl  *mmsc);
static guint                    pulse_ext_stream_get_base_volume      (MateMixerStreamControl  *mmsc);

static void                     fill_ext_stream_restore_info          (PulseExtStream             *ext,
                                                                       pa_ext_stream_restore_info *info);

static gboolean                 write_cvolume                         (PulseExtStream             *ext,
                                                                       const pa_cvolume           *cvolume);
static void                     store_cvolume                         (PulseExtStream             *ext,
                                                                       const pa_cvolume           *cvolume);

static void
pulse_ext_stream_class_init (PulseExtStreamClass *klass)
{
    GObjectClass                *object_class;
    MateMixerStreamControlClass *control_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = pulse_ext_stream_dispose;
    object_class->finalize     = pulse_ext_stream_finalize;
    object_class->get_property = pulse_ext_stream_get_property;
    object_class->set_property = pulse_ext_stream_set_property;

    control_class = MATE_MIXER_STREAM_CONTROL_CLASS (klass);
    control_class->get_app_info         = pulse_ext_stream_get_app_info;
    control_class->set_stream           = pulse_ext_stream_set_stream;
    control_class->set_mute             = pulse_ext_stream_set_mute;
    control_class->get_num_channels     = pulse_ext_stream_get_num_channels;
    control_class->get_volume           = pulse_ext_stream_get_volume;
    control_class->set_volume           = pulse_ext_stream_set_volume;
    control_class->get_channel_volume   = pulse_ext_stream_get_channel_volume;
    control_class->set_channel_volume   = pulse_ext_stream_set_channel_volume;
    control_class->get_channel_position = pulse_ext_stream_get_channel_position;
    control_class->has_channel_position = pulse_ext_stream_has_channel_position;
    control_class->set_balance          = pulse_ext_stream_set_balance;
    control_class->set_fade             = pulse_ext_stream_set_fade;
    control_class->get_min_volume       = pulse_ext_stream_get_min_volume;
    control_class->get_max_volume       = pulse_ext_stream_get_max_volume;
    control_class->get_normal_volume    = pulse_ext_stream_get_normal_volume;
    control_class->get_base_volume      = pulse_ext_stream_get_base_volume;

    properties[PROP_CONNECTION] =
        g_param_spec_object ("connection",
                             "Connection",
                             "PulseAudio connection",
                             PULSE_TYPE_CONNECTION,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_APP_INFO] =
        g_param_spec_boxed ("app-info",
                            "Application information",
                            "Application information",
                            MATE_MIXER_TYPE_APP_INFO,
                            G_PARAM_READWRITE |
                            G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

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
    case PROP_CONNECTION:
        g_value_set_object (value, ext->priv->connection);
        break;
    case PROP_APP_INFO:
        g_value_set_boxed (value, ext->priv->app_info);
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
    case PROP_CONNECTION:
        /* Construct-only object */
        ext->priv->connection = g_value_dup_object (value);
        break;
    case PROP_APP_INFO:
        /* Construct-only boxed */
        ext->priv->app_info = g_value_dup_boxed (value);
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

static void
pulse_ext_stream_dispose (GObject *object)
{
    PulseExtStream *ext;

    ext = PULSE_EXT_STREAM (object);

    g_clear_object (&ext->priv->connection);

    G_OBJECT_CLASS (pulse_ext_stream_parent_class)->dispose (object);
}

static void
pulse_ext_stream_finalize (GObject *object)
{
    PulseExtStream *ext;

    ext = PULSE_EXT_STREAM (object);

    if (ext->priv->app_info != NULL)
        _mate_mixer_app_info_free (ext->priv->app_info);

    G_OBJECT_CLASS (pulse_ext_stream_parent_class)->finalize (object);
}

PulseExtStream *
pulse_ext_stream_new (PulseConnection                  *connection,
                      const pa_ext_stream_restore_info *info,
                      PulseStream                      *parent)
{
    PulseExtStream                 *ext;
    gchar                          *suffix;
    MateMixerAppInfo               *app_info = NULL;
    MateMixerDirection              direction;
    MateMixerStreamControlFlags     flags = MATE_MIXER_STREAM_CONTROL_MUTE_READABLE |
                                            MATE_MIXER_STREAM_CONTROL_MUTE_WRITABLE |
                                            MATE_MIXER_STREAM_CONTROL_MOVABLE |
                                            MATE_MIXER_STREAM_CONTROL_STORED;
    MateMixerStreamControlRole      role  = MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN;
    MateMixerStreamControlMediaRole media_role =
                                            MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_UNKNOWN;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);

    /* The name of an ext-stream is in one of the following formats:
     *  sink-input-by-media-role: ...
     *  sink-input-by-application-name: ...
     *  sink-input-by-application-id: ...
     *  sink-input-by-media-name: ...
     *  source-output-by-media-role: ...
     *  source-output-by-application-name: ...
     *  source-output-by-application-id: ...
     *  source-output-by-media-name: ...
     */
    if (g_str_has_prefix (info->name, "sink-input"))
        direction = MATE_MIXER_DIRECTION_OUTPUT;
    else if (g_str_has_prefix (info->name, "source-output"))
        direction = MATE_MIXER_DIRECTION_INPUT;
    else
        direction = MATE_MIXER_DIRECTION_UNKNOWN;

    suffix = strchr (info->name, ':');
    if (suffix != NULL)
        suffix++;

    if (strstr (info->name, "-by-media-role:")) {
        if G_LIKELY (suffix != NULL)
            media_role = pulse_convert_media_role_name (suffix);
    }
    else if (strstr (info->name, "-by-application-name:")) {
        role = MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION;

        /* Make sure an application ext-stream always has a MateMixerAppInfo
         * structure available, even in the case no application info is
         * available */
        if (app_info == NULL)
            app_info = _mate_mixer_app_info_new ();

        if G_LIKELY (suffix != NULL)
            _mate_mixer_app_info_set_name (app_info, suffix);
    }
    else if (strstr (info->name, "-by-application-id:")) {
        role = MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION;

        /* Make sure an application ext-stream always has a MateMixerAppInfo
         * structure available, even in the case no application info is
         * available */
        if (app_info == NULL)
            app_info = _mate_mixer_app_info_new ();

        if G_LIKELY (suffix != NULL)
            _mate_mixer_app_info_set_id (app_info, suffix);
    }

    ext = g_object_new (PULSE_TYPE_EXT_STREAM,
                        "flags", flags,
                        "role", role,
                        "media-role", media_role,
                        "name", info->name,
                        "direction", direction,
                        "stream", parent,
                        "connection", connection,
                        "app-info", app_info,
                        NULL);

    if (app_info != NULL)
        _mate_mixer_app_info_free (app_info);

    /* Store values which are expected to be changed */
    pulse_ext_stream_update (ext, info, parent);

    return ext;
}

void
pulse_ext_stream_update (PulseExtStream                   *ext,
                         const pa_ext_stream_restore_info *info,
                         PulseStream                      *parent)
{
    MateMixerStreamControlFlags flags;
    gboolean                    volume_changed = FALSE;

    g_return_if_fail (PULSE_IS_EXT_STREAM (ext));
    g_return_if_fail (info != NULL);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (ext));

    _mate_mixer_stream_control_set_mute (MATE_MIXER_STREAM_CONTROL (ext),
                                         info->mute ? TRUE : FALSE);

    flags = mate_mixer_stream_control_get_flags (MATE_MIXER_STREAM_CONTROL (ext));

    if (pa_channel_map_valid (&info->channel_map) != 0) {
        if (pa_channel_map_can_balance (&info->channel_map) != 0)
            flags |= MATE_MIXER_STREAM_CONTROL_CAN_BALANCE;
        else
            flags &= ~MATE_MIXER_STREAM_CONTROL_CAN_BALANCE;

        if (pa_channel_map_can_fade (&info->channel_map) != 0)
            flags |= MATE_MIXER_STREAM_CONTROL_CAN_FADE;
        else
            flags &= ~MATE_MIXER_STREAM_CONTROL_CAN_FADE;

        ext->priv->channel_map = info->channel_map;
    } else {
        flags &= ~(MATE_MIXER_STREAM_CONTROL_CAN_BALANCE | MATE_MIXER_STREAM_CONTROL_CAN_FADE);

        /* If the channel map is not valid, create an empty channel map, which
         * also won't validate, but at least we know what it is */
        pa_channel_map_init (&ext->priv->channel_map);
    }

    if (pa_cvolume_valid (&info->volume) != 0) {
        flags |= MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE |
                 MATE_MIXER_STREAM_CONTROL_VOLUME_WRITABLE;

        if (pa_cvolume_equal (&ext->priv->cvolume, &info->volume) == 0)
            volume_changed = TRUE;
    } else {
        flags &= ~(MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE |
                   MATE_MIXER_STREAM_CONTROL_VOLUME_WRITABLE);

        if (ext->priv->volume != (guint) PA_VOLUME_MUTED)
            volume_changed = TRUE;
    }

    if (volume_changed == TRUE)
        store_cvolume (ext, &info->volume);

    _mate_mixer_stream_control_set_flags (MATE_MIXER_STREAM_CONTROL (ext), flags);

    /* Also set initially, but may change at any time */
    if (parent != NULL)
        _mate_mixer_stream_control_set_stream (MATE_MIXER_STREAM_CONTROL (ext),
                                               MATE_MIXER_STREAM (parent));
    else
        _mate_mixer_stream_control_set_stream (MATE_MIXER_STREAM_CONTROL (ext),
                                               NULL);

    g_object_thaw_notify (G_OBJECT (ext));
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
    PulseExtStream            *ext;
    pa_ext_stream_restore_info info;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), FALSE);
    g_return_val_if_fail (mms == NULL || PULSE_IS_STREAM (mms), FALSE);

    ext = PULSE_EXT_STREAM (mmsc);

    fill_ext_stream_restore_info (ext, &info);
    if (mms != NULL)
        info.device = mate_mixer_stream_get_name (mms);
    else
        info.device = NULL;

    return pulse_connection_write_ext_stream (ext->priv->connection, &info);
}

static gboolean
pulse_ext_stream_set_mute (MateMixerStreamControl *mmsc, gboolean mute)
{
    PulseExtStream            *ext;
    pa_ext_stream_restore_info info;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), FALSE);

    ext = PULSE_EXT_STREAM (mmsc);

    fill_ext_stream_restore_info (ext, &info);
    info.mute = mute;

    return pulse_connection_write_ext_stream (ext->priv->connection, &info);
}

static guint
pulse_ext_stream_get_num_channels (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), 0);

    return PULSE_EXT_STREAM (mmsc)->priv->channel_map.channels;
}

static guint
pulse_ext_stream_get_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), (guint) PA_VOLUME_MUTED);

    return PULSE_EXT_STREAM (mmsc)->priv->volume;
}

static gboolean
pulse_ext_stream_set_volume (MateMixerStreamControl *mmsc, guint volume)
{
    PulseExtStream *ext;
    pa_cvolume      cvolume;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), FALSE);

    ext = PULSE_EXT_STREAM (mmsc);
    cvolume = ext->priv->cvolume;

    /* Modify a temporary cvolume structure as the change may be irreversible */
    if (pa_cvolume_scale (&cvolume, (pa_volume_t) volume) == NULL)
        return FALSE;

    return write_cvolume (ext, &cvolume);
}

static guint
pulse_ext_stream_get_channel_volume (MateMixerStreamControl *mmsc, guint channel)
{
    PulseExtStream *ext;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), (guint) PA_VOLUME_MUTED);

    ext = PULSE_EXT_STREAM (mmsc);

    if (channel >= ext->priv->cvolume.channels)
        return (guint) PA_VOLUME_MUTED;

    return (guint) ext->priv->cvolume.values[channel];
}

static gboolean
pulse_ext_stream_set_channel_volume (MateMixerStreamControl *mmsc, guint channel, guint volume)
{
    PulseExtStream *ext;
    pa_cvolume      cvolume;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), FALSE);

    ext = PULSE_EXT_STREAM (mmsc);

    if (channel >= ext->priv->cvolume.channels)
        return FALSE;

    /* Modify a temporary cvolume structure as the change may be irreversible */
    cvolume = ext->priv->cvolume;
    cvolume.values[channel] = (pa_volume_t) volume;

    return write_cvolume (ext, &cvolume);
}

static MateMixerChannelPosition
pulse_ext_stream_get_channel_position (MateMixerStreamControl *mmsc, guint channel)
{
    PulseExtStream *ext;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), MATE_MIXER_CHANNEL_UNKNOWN);

    ext = PULSE_EXT_STREAM (mmsc);

    if (channel >= ext->priv->channel_map.channels)
        return MATE_MIXER_CHANNEL_UNKNOWN;

    if (ext->priv->channel_map.map[channel] == PA_CHANNEL_POSITION_INVALID)
        return MATE_MIXER_CHANNEL_UNKNOWN;

    return pulse_channel_map_from[ext->priv->channel_map.map[channel]];
}

static gboolean
pulse_ext_stream_has_channel_position (MateMixerStreamControl  *mmsc,
                                       MateMixerChannelPosition position)
{
    PulseExtStream *ext;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), FALSE);

    ext = PULSE_EXT_STREAM (mmsc);

    /* Handle invalid position as a special case, otherwise this function would
     * return TRUE for e.g. unknown index in a default channel map */
    if (pulse_channel_map_to[position] == PA_CHANNEL_POSITION_INVALID)
        return FALSE;

    if (pa_channel_map_has_position (&ext->priv->channel_map,
                                     pulse_channel_map_to[position]) != 0)
        return TRUE;
    else
        return FALSE;
}

static gboolean
pulse_ext_stream_set_balance (MateMixerStreamControl *mmsc, gfloat balance)
{
    PulseExtStream *ext;
    pa_cvolume      cvolume;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), FALSE);

    ext = PULSE_EXT_STREAM (mmsc);
    cvolume = ext->priv->cvolume;

    /* Modify a temporary cvolume structure as the change may be irreversible */
    if (pa_cvolume_set_balance (&cvolume, &ext->priv->channel_map, balance) == NULL)
        return FALSE;

    return write_cvolume (ext, &cvolume);
}

static gboolean
pulse_ext_stream_set_fade (MateMixerStreamControl *mmsc, gfloat fade)
{
    PulseExtStream *ext;
    pa_cvolume      cvolume;

    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), FALSE);

    ext = PULSE_EXT_STREAM (mmsc);
    cvolume = ext->priv->cvolume;

    /* Modify a temporary cvolume structure as the change may be irreversible */
    if (pa_cvolume_set_fade (&cvolume, &ext->priv->channel_map, fade) == NULL)
        return FALSE;

    return write_cvolume (ext, &cvolume);
}

static guint
pulse_ext_stream_get_min_volume (MateMixerStreamControl *mmsc)
{
    return (guint) PA_VOLUME_MUTED;
}

static guint
pulse_ext_stream_get_max_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), (guint) PA_VOLUME_MUTED);

    return (guint) PA_VOLUME_NORM;
}

static guint
pulse_ext_stream_get_normal_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), (guint) PA_VOLUME_MUTED);

    return (guint) PA_VOLUME_NORM;
}

static guint
pulse_ext_stream_get_base_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (PULSE_IS_EXT_STREAM (mmsc), (guint) PA_VOLUME_MUTED);

    /* Base volume is not supported/used in ext-streams */
    return (guint) PA_VOLUME_NORM;
}

static void
fill_ext_stream_restore_info (PulseExtStream             *ext,
                              pa_ext_stream_restore_info *info)
{
    MateMixerStream        *mms;
    MateMixerStreamControl *mmsc;

    mmsc = MATE_MIXER_STREAM_CONTROL (ext);

    info->name = mate_mixer_stream_control_get_name (mmsc);
    info->mute = mate_mixer_stream_control_get_mute (mmsc);
    info->volume      = ext->priv->cvolume;
    info->channel_map = ext->priv->channel_map;

    mms = mate_mixer_stream_control_get_stream (mmsc);
    if (mms != NULL)
        info->device = mate_mixer_stream_get_name (mms);
    else
        info->device = NULL;
}

static gboolean
write_cvolume (PulseExtStream *ext, const pa_cvolume *cvolume)
{
    pa_ext_stream_restore_info info;

    /* Make sure to only store a valid and modified volume */
    if (pa_cvolume_valid (cvolume) == 0)
        return FALSE;
    if (pa_cvolume_equal (cvolume, &ext->priv->cvolume) != 0)
        return TRUE;

    fill_ext_stream_restore_info (ext, &info);
    info.volume = *cvolume;

    if (pulse_connection_write_ext_stream (ext->priv->connection, &info) == FALSE)
        return FALSE;

    store_cvolume (ext, cvolume);
    return TRUE;
}

static void
store_cvolume (PulseExtStream *ext, const pa_cvolume *cvolume)
{
    gfloat value;

    /* Avoid validating whether the volume has changed, it should be done by
     * the caller */
    ext->priv->cvolume = *cvolume;
    ext->priv->volume  = (guint) pa_cvolume_max (cvolume);

    g_object_notify (G_OBJECT (ext), "volume");

    /* PulseAudio returns the default 0.0f value on error, so skip checking validity
     * of the channel map and cvolume */
    value = pa_cvolume_get_balance (&ext->priv->cvolume,
                                    &ext->priv->channel_map);

    _mate_mixer_stream_control_set_balance (MATE_MIXER_STREAM_CONTROL (ext), value);

    value = pa_cvolume_get_fade (&ext->priv->cvolume,
                                 &ext->priv->channel_map);

    _mate_mixer_stream_control_set_fade (MATE_MIXER_STREAM_CONTROL (ext), value);
}
