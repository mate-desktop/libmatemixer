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
#include "pulse-stream.h"
#include "pulse-stream-control.h"

struct _PulseStreamControlPrivate
{
    guint32           index;
    guint             volume;
    pa_cvolume        cvolume;
    pa_volume_t       base_volume;
    pa_channel_map    channel_map;
    PulseConnection  *connection;
    PulseMonitor     *monitor;
    MateMixerAppInfo *app_info;
};

enum {
    PROP_0,
    PROP_INDEX,
    PROP_CONNECTION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void pulse_stream_control_class_init   (PulseStreamControlClass *klass);

static void pulse_stream_control_get_property (GObject                 *object,
                                               guint                    param_id,
                                               GValue                  *value,
                                               GParamSpec              *pspec);
static void pulse_stream_control_set_property (GObject                 *object,
                                               guint                    param_id,
                                               const GValue            *value,
                                               GParamSpec              *pspec);

static void pulse_stream_control_init         (PulseStreamControl      *control);
static void pulse_stream_control_dispose      (GObject                 *object);
static void pulse_stream_control_finalize     (GObject                 *object);

G_DEFINE_ABSTRACT_TYPE (PulseStreamControl, pulse_stream_control, MATE_MIXER_TYPE_STREAM_CONTROL)

static MateMixerAppInfo *       pulse_stream_control_get_app_info         (MateMixerStreamControl   *mmsc);

static gboolean                 pulse_stream_control_set_mute             (MateMixerStreamControl   *mmsc,
                                                                           gboolean                  mute);

static guint                    pulse_stream_control_get_num_channels     (MateMixerStreamControl   *mmsc);

static guint                    pulse_stream_control_get_volume           (MateMixerStreamControl   *mmsc);
static gboolean                 pulse_stream_control_set_volume           (MateMixerStreamControl   *mmsc,
                                                                           guint                     volume);

static gdouble                  pulse_stream_control_get_decibel          (MateMixerStreamControl   *mmsc);
static gboolean                 pulse_stream_control_set_decibel          (MateMixerStreamControl   *mmsc,
                                                                           gdouble                   decibel);

static guint                    pulse_stream_control_get_channel_volume   (MateMixerStreamControl   *mmsc,
                                                                           guint                     channel);
static gboolean                 pulse_stream_control_set_channel_volume   (MateMixerStreamControl   *mmsc,
                                                                           guint                     channel,
                                                                           guint                     volume);

static gdouble                  pulse_stream_control_get_channel_decibel  (MateMixerStreamControl   *mmsc,
                                                                           guint                     channel);
static gboolean                 pulse_stream_control_set_channel_decibel  (MateMixerStreamControl   *mmsc,
                                                                           guint                     channel,
                                                                           gdouble                   decibel);

static MateMixerChannelPosition pulse_stream_control_get_channel_position (MateMixerStreamControl   *mmsc,
                                                                           guint                     channel);
static gboolean                 pulse_stream_control_has_channel_position (MateMixerStreamControl   *mmsc,
                                                                           MateMixerChannelPosition  position);

static gboolean                 pulse_stream_control_set_balance          (MateMixerStreamControl   *mmsc,
                                                                           gfloat                    balance);

static gboolean                 pulse_stream_control_set_fade             (MateMixerStreamControl   *mmsc,
                                                                           gfloat                    fade);

static gboolean                 pulse_stream_control_get_monitor_enabled  (MateMixerStreamControl   *mmsc);
static gboolean                 pulse_stream_control_set_monitor_enabled  (MateMixerStreamControl   *mmsc,
                                                                           gboolean                  enabled);

static guint                    pulse_stream_control_get_min_volume       (MateMixerStreamControl   *mmsc);
static guint                    pulse_stream_control_get_max_volume       (MateMixerStreamControl   *mmsc);
static guint                    pulse_stream_control_get_normal_volume    (MateMixerStreamControl   *mmsc);
static guint                    pulse_stream_control_get_base_volume      (MateMixerStreamControl   *mmsc);

static void                     on_monitor_value (PulseMonitor       *monitor,
                                                  gdouble             value,
                                                  PulseStreamControl *control);

static void                     set_balance_fade (PulseStreamControl *control);

static gboolean                 set_cvolume      (PulseStreamControl *control,
                                                  pa_cvolume         *cvolume);

static void
pulse_stream_control_class_init (PulseStreamControlClass *klass)
{
    GObjectClass                *object_class;
    MateMixerStreamControlClass *control_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = pulse_stream_control_dispose;
    object_class->finalize     = pulse_stream_control_finalize;
    object_class->get_property = pulse_stream_control_get_property;
    object_class->set_property = pulse_stream_control_set_property;

    control_class = MATE_MIXER_STREAM_CONTROL_CLASS (klass);
    control_class->get_app_info         = pulse_stream_control_get_app_info;
    control_class->set_mute             = pulse_stream_control_set_mute;
    control_class->get_num_channels     = pulse_stream_control_get_num_channels;
    control_class->get_volume           = pulse_stream_control_get_volume;
    control_class->set_volume           = pulse_stream_control_set_volume;
    control_class->get_decibel          = pulse_stream_control_get_decibel;
    control_class->set_decibel          = pulse_stream_control_set_decibel;
    control_class->get_channel_volume   = pulse_stream_control_get_channel_volume;
    control_class->set_channel_volume   = pulse_stream_control_set_channel_volume;
    control_class->get_channel_decibel  = pulse_stream_control_get_channel_decibel;
    control_class->set_channel_decibel  = pulse_stream_control_set_channel_decibel;
    control_class->get_channel_position = pulse_stream_control_get_channel_position;
    control_class->has_channel_position = pulse_stream_control_has_channel_position;
    control_class->set_balance          = pulse_stream_control_set_balance;
    control_class->set_fade             = pulse_stream_control_set_fade;
    control_class->get_monitor_enabled  = pulse_stream_control_get_monitor_enabled;
    control_class->set_monitor_enabled  = pulse_stream_control_set_monitor_enabled;
    control_class->get_min_volume       = pulse_stream_control_get_min_volume;
    control_class->get_max_volume       = pulse_stream_control_get_max_volume;
    control_class->get_normal_volume    = pulse_stream_control_get_normal_volume;
    control_class->get_base_volume      = pulse_stream_control_get_base_volume;

    properties[PROP_INDEX] =
        g_param_spec_uint ("index",
                           "Index",
                           "Index of the stream control",
                           0,
                           G_MAXUINT,
                           0,
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

    properties[PROP_CONNECTION] =
        g_param_spec_object ("connection",
                             "Connection",
                             "PulseAudio connection",
                             PULSE_TYPE_CONNECTION,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    g_type_class_add_private (object_class, sizeof (PulseStreamControlPrivate));
}

static void
pulse_stream_control_get_property (GObject    *object,
                                   guint       param_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
    PulseStreamControl *control;

    control = PULSE_STREAM_CONTROL (object);

    switch (param_id) {
    case PROP_INDEX:
        g_value_set_uint (value, control->priv->index);
        break;
    case PROP_CONNECTION:
        g_value_set_object (value, control->priv->connection);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_stream_control_set_property (GObject      *object,
                                   guint         param_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
    PulseStreamControl *control;

    control = PULSE_STREAM_CONTROL (object);

    switch (param_id) {
    case PROP_INDEX:
        control->priv->index = g_value_get_uint (value);
        break;
    case PROP_CONNECTION:
        control->priv->connection = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_stream_control_init (PulseStreamControl *control)
{
    control->priv = G_TYPE_INSTANCE_GET_PRIVATE (control,
                                                 PULSE_TYPE_STREAM_CONTROL,
                                                 PulseStreamControlPrivate);

    /* Initialize empty volume and channel map structures, they will be used
     * if the stream does not support volume */
    pa_cvolume_init (&control->priv->cvolume);

    pa_channel_map_init (&control->priv->channel_map);
}

static void
pulse_stream_control_dispose (GObject *object)
{
    PulseStreamControl *control;

    control = PULSE_STREAM_CONTROL (object);

    g_clear_object (&control->priv->monitor);
    g_clear_object (&control->priv->connection);

    G_OBJECT_CLASS (pulse_stream_control_parent_class)->dispose (object);
}

static void
pulse_stream_control_finalize (GObject *object)
{
    PulseStreamControl *control;

    control = PULSE_STREAM_CONTROL (object);

    if (control->priv->app_info != NULL)
        _mate_mixer_app_info_free (control->priv->app_info);

    G_OBJECT_CLASS (pulse_stream_control_parent_class)->finalize (object);
}

guint32
pulse_stream_control_get_index (PulseStreamControl *control)
{
    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (control), PA_INVALID_INDEX);

    return control->priv->index;
}

guint32
pulse_stream_control_get_stream_index (PulseStreamControl *control)
{
    MateMixerStream *stream;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (control), PA_INVALID_INDEX);

    stream = mate_mixer_stream_control_get_stream (MATE_MIXER_STREAM_CONTROL (control));
    if G_UNLIKELY (stream == NULL)
        return PA_INVALID_INDEX;

    return pulse_stream_get_index (PULSE_STREAM (stream));
}

PulseConnection *
pulse_stream_control_get_connection (PulseStreamControl *control)
{
    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (control), NULL);

    return control->priv->connection;
}

PulseMonitor *
pulse_stream_control_get_monitor (PulseStreamControl *control)
{
    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (control), NULL);

    return control->priv->monitor;
}

const pa_cvolume *
pulse_stream_control_get_cvolume (PulseStreamControl *control)
{
    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (control), NULL);

    return &control->priv->cvolume;
}

const pa_channel_map *
pulse_stream_control_get_channel_map (PulseStreamControl *control)
{
    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (control), NULL);

    return &control->priv->channel_map;
}

void
pulse_stream_control_set_app_info (PulseStreamControl *control,
                                   MateMixerAppInfo   *info,
                                   gboolean            take)
{
    g_return_if_fail (PULSE_IS_STREAM_CONTROL (control));

    if G_UNLIKELY (control->priv->app_info != NULL)
        _mate_mixer_app_info_free (control->priv->app_info);

    if (take == TRUE)
        control->priv->app_info = info;
    else
        control->priv->app_info = _mate_mixer_app_info_copy (info);
}

void
pulse_stream_control_set_channel_map (PulseStreamControl *control, const pa_channel_map *map)
{
    MateMixerStreamControlFlags flags;

    g_return_if_fail (PULSE_IS_STREAM_CONTROL (control));

    flags = mate_mixer_stream_control_get_flags (MATE_MIXER_STREAM_CONTROL (control));

    if (map != NULL && pa_channel_map_valid (map)) {
        if (pa_channel_map_can_balance (map))
            flags |= MATE_MIXER_STREAM_CONTROL_CAN_BALANCE;
        else
            flags &= ~MATE_MIXER_STREAM_CONTROL_CAN_BALANCE;

        if (pa_channel_map_can_fade (map))
            flags |= MATE_MIXER_STREAM_CONTROL_CAN_FADE;
        else
            flags &= ~MATE_MIXER_STREAM_CONTROL_CAN_FADE;

        control->priv->channel_map = *map;
    } else {
        flags &= ~(MATE_MIXER_STREAM_CONTROL_CAN_BALANCE | MATE_MIXER_STREAM_CONTROL_CAN_FADE);

        /* If the channel map is not valid, create an empty channel map, which
         * also won't validate, but at least we know what it is */
        pa_channel_map_init (&control->priv->channel_map);
    }

    _mate_mixer_stream_control_set_flags (MATE_MIXER_STREAM_CONTROL (control), flags);
}

void
pulse_stream_control_set_cvolume (PulseStreamControl *control,
                                  const pa_cvolume   *cvolume,
                                  pa_volume_t         base_volume)
{
    MateMixerStreamControlFlags flags;

    g_return_if_fail (PULSE_IS_STREAM_CONTROL (control));

    /* The base volume is not a property */
    control->priv->base_volume = base_volume;

    flags = mate_mixer_stream_control_get_flags (MATE_MIXER_STREAM_CONTROL (control));

    g_object_freeze_notify (G_OBJECT (control));

    if (cvolume != NULL && pa_cvolume_valid (cvolume)) {
        /* Decibel volume and volume settability flags must be provided by
         * the implementation */
        flags |= MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE;

        if (pa_cvolume_equal (&control->priv->cvolume, cvolume) == 0) {
            control->priv->cvolume = *cvolume;
            control->priv->volume  = (guint) pa_cvolume_max (&control->priv->cvolume);

            g_object_notify (G_OBJECT (control), "volume");
        }
    } else {
        flags &= ~(MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE |
                   MATE_MIXER_STREAM_CONTROL_VOLUME_WRITABLE |
                   MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL);

        /* If the cvolume is not valid, create an empty cvolume, which also
         * won't validate, but at least we know what it is */
        pa_cvolume_init (&control->priv->cvolume);

        if (control->priv->volume != (guint) PA_VOLUME_MUTED) {
            control->priv->volume = (guint) PA_VOLUME_MUTED;

            g_object_notify (G_OBJECT (control), "volume");
        }
    }

    _mate_mixer_stream_control_set_flags (MATE_MIXER_STREAM_CONTROL (control), flags);

    /* Changing volume may change the balance and fade values as well */
    set_balance_fade (control);

    g_object_thaw_notify (G_OBJECT (control));
}

static MateMixerAppInfo *
pulse_stream_control_get_app_info (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), NULL);

    return PULSE_STREAM_CONTROL (mmsc)->priv->app_info;
}

static gboolean
pulse_stream_control_set_mute (MateMixerStreamControl *mmsc, gboolean mute)
{
    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), FALSE);

    return PULSE_STREAM_CONTROL_GET_CLASS (mmsc)->set_mute (PULSE_STREAM_CONTROL (mmsc), mute);
}

static guint
pulse_stream_control_get_num_channels (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), 0);

    return PULSE_STREAM_CONTROL (mmsc)->priv->channel_map.channels;
}

static guint
pulse_stream_control_get_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), (guint) PA_VOLUME_MUTED);

    return PULSE_STREAM_CONTROL (mmsc)->priv->volume;
}

static gboolean
pulse_stream_control_set_volume (MateMixerStreamControl *mmsc, guint volume)
{
    PulseStreamControl *control;
    pa_cvolume          cvolume;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), FALSE);

    control = PULSE_STREAM_CONTROL (mmsc);
    cvolume = control->priv->cvolume;

    if (pa_cvolume_scale (&cvolume, (pa_volume_t) volume) == NULL)
        return FALSE;

    return set_cvolume (control, &cvolume);
}

static gdouble
pulse_stream_control_get_decibel (MateMixerStreamControl *mmsc)
{
    gdouble value;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), -MATE_MIXER_INFINITY);

    value = pa_sw_volume_to_dB (pulse_stream_control_get_volume (mmsc));

    /* PA_VOLUME_MUTED is converted to PA_DECIBEL_MININFTY */
    return (value == PA_DECIBEL_MININFTY) ? -MATE_MIXER_INFINITY : value;
}

static gboolean
pulse_stream_control_set_decibel (MateMixerStreamControl *mmsc, gdouble decibel)
{
    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), FALSE);

    return pulse_stream_control_set_volume (mmsc,
                                            pa_sw_volume_from_dB (decibel));
}

static guint
pulse_stream_control_get_channel_volume (MateMixerStreamControl *mmsc, guint channel)
{
    PulseStreamControl *control;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), (guint) PA_VOLUME_MUTED);

    control = PULSE_STREAM_CONTROL (mmsc);

    if (channel >= control->priv->cvolume.channels)
        return (guint) PA_VOLUME_MUTED;

    return (guint) control->priv->cvolume.values[channel];
}

static gboolean
pulse_stream_control_set_channel_volume (MateMixerStreamControl *mmsc, guint channel, guint volume)
{
    PulseStreamControl *control;
    pa_cvolume          cvolume;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), FALSE);

    control = PULSE_STREAM_CONTROL (mmsc);

    if (channel >= control->priv->cvolume.channels)
        return FALSE;

    /* This is safe, because the cvolume is validated by set_cvolume() */
    cvolume = control->priv->cvolume;
    cvolume.values[channel] = (pa_volume_t) volume;

    return set_cvolume (control, &cvolume);
}

static gdouble
pulse_stream_control_get_channel_decibel (MateMixerStreamControl *mmsc, guint channel)
{
    PulseStreamControl *control;
    gdouble             value;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), -MATE_MIXER_INFINITY);

    control = PULSE_STREAM_CONTROL (mmsc);

    if (channel >= control->priv->cvolume.channels)
        return -MATE_MIXER_INFINITY;

    value = pa_sw_volume_to_dB (control->priv->cvolume.values[channel]);

    return (value == PA_DECIBEL_MININFTY) ? -MATE_MIXER_INFINITY : value;
}

static gboolean
pulse_stream_control_set_channel_decibel (MateMixerStreamControl *mmsc,
                                          guint                   channel,
                                          gdouble                 decibel)
{
    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), FALSE);

    return pulse_stream_control_set_channel_volume (mmsc,
                                                    channel,
                                                    pa_sw_volume_from_dB (decibel));
}

static MateMixerChannelPosition
pulse_stream_control_get_channel_position (MateMixerStreamControl *mmsc, guint channel)
{
    PulseStreamControl *control;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), MATE_MIXER_CHANNEL_UNKNOWN);

    control = PULSE_STREAM_CONTROL (mmsc);

    if (channel >= control->priv->channel_map.channels)
        return MATE_MIXER_CHANNEL_UNKNOWN;

    if (control->priv->channel_map.map[channel] == PA_CHANNEL_POSITION_INVALID)
        return MATE_MIXER_CHANNEL_UNKNOWN;

    return pulse_channel_map_from[control->priv->channel_map.map[channel]];
}

static gboolean
pulse_stream_control_has_channel_position (MateMixerStreamControl  *mmsc,
                                           MateMixerChannelPosition position)
{
    PulseStreamControl *control;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), FALSE);

    control = PULSE_STREAM_CONTROL (mmsc);

    /* Handle invalid position as a special case, otherwise this function would
     * return TRUE for e.g. unknown index in a default channel map */
    if (pulse_channel_map_to[position] == PA_CHANNEL_POSITION_INVALID)
        return FALSE;

    if (pa_channel_map_has_position (&control->priv->channel_map,
                                     pulse_channel_map_to[position]) != 0)
        return TRUE;
    else
        return FALSE;
}

static gboolean
pulse_stream_control_set_balance (MateMixerStreamControl *mmsc, gfloat balance)
{
    PulseStreamControl *control;
    pa_cvolume          cvolume;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), FALSE);

    control = PULSE_STREAM_CONTROL (mmsc);
    cvolume = control->priv->cvolume;

    if (pa_cvolume_set_balance (&cvolume, &control->priv->channel_map, balance) == NULL)
        return FALSE;

    return set_cvolume (control, &cvolume);
}

static gboolean
pulse_stream_control_set_fade (MateMixerStreamControl *mmsc, gfloat fade)
{
    PulseStreamControl *control;
    pa_cvolume          cvolume;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), FALSE);

    control = PULSE_STREAM_CONTROL (mmsc);
    cvolume = control->priv->cvolume;

    if (pa_cvolume_set_fade (&cvolume, &control->priv->channel_map, fade) == NULL)
        return FALSE;

    return set_cvolume (control, &cvolume);
}

static gboolean
pulse_stream_control_get_monitor_enabled (MateMixerStreamControl *mmsc)
{
    PulseStreamControl *control;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), FALSE);

    control = PULSE_STREAM_CONTROL (mmsc);

    if (control->priv->monitor != NULL)
        return pulse_monitor_get_enabled (control->priv->monitor);

    return FALSE;
}

static gboolean
pulse_stream_control_set_monitor_enabled (MateMixerStreamControl *mmsc, gboolean enabled)
{
    PulseStreamControl *control;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), FALSE);

    control = PULSE_STREAM_CONTROL (mmsc);

    if (enabled == TRUE) {
        if (control->priv->monitor == NULL) {
            control->priv->monitor =
                PULSE_STREAM_CONTROL_GET_CLASS (control)->create_monitor (control);

            if G_UNLIKELY (control->priv->monitor == NULL)
                return FALSE;

            g_signal_connect (G_OBJECT (control->priv->monitor),
                              "value",
                              G_CALLBACK (on_monitor_value),
                              control);
        }
    } else {
        if (control->priv->monitor == NULL)
            return FALSE;
    }
    return pulse_monitor_set_enabled (control->priv->monitor, enabled);
}

static guint
pulse_stream_control_get_min_volume (MateMixerStreamControl *mmsc)
{
    return (guint) PA_VOLUME_MUTED;
}

static guint
pulse_stream_control_get_max_volume (MateMixerStreamControl *mmsc)
{
    MateMixerStreamControlFlags flags;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), (guint) PA_VOLUME_MUTED);

    flags = mate_mixer_stream_control_get_flags (mmsc);

    /*
     * From PulseAudio wiki:
     * For all volumes that are > PA_VOLUME_NORM (i.e. beyond the maximum volume
     * setting of the hw) PA will do digital amplification. This works only for
     * devices that have PA_SINK_DECIBEL_VOLUME/PA_SOURCE_DECIBEL_VOLUME set. For
     * devices that lack this flag do not extend the volume slider like this, it
     * will not have any effect.
     */
    if (flags & MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL)
        return (guint) PA_VOLUME_UI_MAX;
    else
        return (guint) PA_VOLUME_NORM;
}

static guint
pulse_stream_control_get_normal_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), (guint) PA_VOLUME_MUTED);

    return (guint) PA_VOLUME_NORM;
}

static guint
pulse_stream_control_get_base_volume (MateMixerStreamControl *mmsc)
{
    PulseStreamControl *control;

    g_return_val_if_fail (PULSE_IS_STREAM_CONTROL (mmsc), (guint) PA_VOLUME_MUTED);

    control = PULSE_STREAM_CONTROL (mmsc);

    if (control->priv->base_volume > 0)
        return (guint) control->priv->base_volume;
    else
        return (guint) PA_VOLUME_NORM;
}

static void
on_monitor_value (PulseMonitor *monitor, gdouble value, PulseStreamControl *control)
{
    g_signal_emit_by_name (G_OBJECT (control),
                           "monitor-value",
                           value);
}

static void
set_balance_fade (PulseStreamControl *control)
{
    gfloat value;

    /* PulseAudio returns the default 0.0f value on error, so skip checking validity
     * of the channel map and cvolume */
    value = pa_cvolume_get_balance (&control->priv->cvolume,
                                    &control->priv->channel_map);

    _mate_mixer_stream_control_set_balance (MATE_MIXER_STREAM_CONTROL (control), value);

    value = pa_cvolume_get_fade (&control->priv->cvolume,
                                 &control->priv->channel_map);

    _mate_mixer_stream_control_set_fade (MATE_MIXER_STREAM_CONTROL (control), value);
}

static gboolean
set_cvolume (PulseStreamControl *control, pa_cvolume *cvolume)
{
    PulseStreamControlClass *klass;

    if (pa_cvolume_valid (cvolume) == 0)
        return FALSE;
    if (pa_cvolume_equal (cvolume, &control->priv->cvolume) != 0)
        return TRUE;

    klass = PULSE_STREAM_CONTROL_GET_CLASS (control);

    if (klass->set_volume (control, cvolume) == FALSE)
        return FALSE;

    control->priv->cvolume = *cvolume;
    control->priv->volume  = (guint) pa_cvolume_max (cvolume);

    g_object_notify (G_OBJECT (control), "volume");

    /* Changing volume may change the balance and fade values as well */
    set_balance_fade (control);
    return TRUE;
}
