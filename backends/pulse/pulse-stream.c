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

#include <libmatemixer/matemixer-device.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-stream.h>
#include <libmatemixer/matemixer-port.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-helpers.h"
#include "pulse-monitor.h"
#include "pulse-stream.h"

struct _PulseStreamPrivate
{
    guint32                index;
    gchar                 *name;
    gchar                 *description;
    MateMixerDevice       *device;
    MateMixerStreamFlags   flags;
    MateMixerStreamState   state;
    gboolean               mute;
    guint                  volume;
    pa_cvolume             cvolume;
    pa_volume_t            base_volume;
    pa_channel_map         channel_map;
    gfloat                 balance;
    gfloat                 fade;
    GHashTable            *ports;
    GList                 *ports_list;
    MateMixerPort         *port;
    PulseConnection       *connection;
    PulseMonitor          *monitor;
    gchar                 *monitor_name;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_DEVICE,
    PROP_FLAGS,
    PROP_STATE,
    PROP_MUTE,
    PROP_VOLUME,
    PROP_BALANCE,
    PROP_FADE,
    PROP_ACTIVE_PORT,
    PROP_INDEX,
    PROP_CONNECTION
};

static void mate_mixer_stream_interface_init (MateMixerStreamInterface *iface);

static void pulse_stream_class_init   (PulseStreamClass *klass);

static void pulse_stream_get_property (GObject          *object,
                                       guint             param_id,
                                       GValue           *value,
                                       GParamSpec       *pspec);
static void pulse_stream_set_property (GObject          *object,
                                       guint             param_id,
                                       const GValue     *value,
                                       GParamSpec       *pspec);

static void pulse_stream_init         (PulseStream      *pstream);
static void pulse_stream_dispose      (GObject          *object);
static void pulse_stream_finalize     (GObject          *object);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PulseStream, pulse_stream, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_STREAM,
                                                         mate_mixer_stream_interface_init))

static const gchar *            pulse_stream_get_name             (MateMixerStream          *stream);
static const gchar *            pulse_stream_get_description      (MateMixerStream          *stream);
static MateMixerDevice *        pulse_stream_get_device           (MateMixerStream          *stream);
static MateMixerStreamFlags     pulse_stream_get_flags            (MateMixerStream          *stream);
static MateMixerStreamState     pulse_stream_get_state            (MateMixerStream          *stream);

static gboolean                 pulse_stream_get_mute             (MateMixerStream          *stream);
static gboolean                 pulse_stream_set_mute             (MateMixerStream          *stream,
                                                                   gboolean                  mute);

static guint                    pulse_stream_get_num_channels     (MateMixerStream          *stream);

static guint                    pulse_stream_get_volume           (MateMixerStream          *stream);
static gboolean                 pulse_stream_set_volume           (MateMixerStream          *stream,
                                                                   guint                     volume);

static gdouble                  pulse_stream_get_decibel          (MateMixerStream          *stream);
static gboolean                 pulse_stream_set_decibel          (MateMixerStream          *stream,
                                                                   gdouble                   decibel);

static guint                    pulse_stream_get_channel_volume   (MateMixerStream          *stream,
                                                                   guint                     channel);
static gboolean                 pulse_stream_set_channel_volume   (MateMixerStream          *stream,
                                                                   guint                     channel,
                                                                   guint                     volume);

static gdouble                  pulse_stream_get_channel_decibel  (MateMixerStream          *stream,
                                                                   guint                     channel);
static gboolean                 pulse_stream_set_channel_decibel  (MateMixerStream          *stream,
                                                                   guint                     channel,
                                                                   gdouble                   decibel);

static MateMixerChannelPosition pulse_stream_get_channel_position (MateMixerStream          *stream,
                                                                   guint                     channel);
static gboolean                 pulse_stream_has_channel_position (MateMixerStream          *stream,
                                                                   MateMixerChannelPosition  position);

static gfloat                   pulse_stream_get_balance          (MateMixerStream          *stream);
static gboolean                 pulse_stream_set_balance          (MateMixerStream          *stream,
                                                                   gfloat                    balance);

static gfloat                   pulse_stream_get_fade             (MateMixerStream          *stream);
static gboolean                 pulse_stream_set_fade             (MateMixerStream          *stream,
                                                                   gfloat                    fade);

static gboolean                 pulse_stream_suspend              (MateMixerStream          *stream);
static gboolean                 pulse_stream_resume               (MateMixerStream          *stream);

static gboolean                 pulse_stream_monitor_start        (MateMixerStream          *stream);
static void                     pulse_stream_monitor_stop         (MateMixerStream          *stream);
static gboolean                 pulse_stream_monitor_is_running   (MateMixerStream          *stream);
static gboolean                 pulse_stream_monitor_set_name     (MateMixerStream          *stream,
                                                                   const gchar              *name);

static const GList *            pulse_stream_list_ports           (MateMixerStream          *stream);

static MateMixerPort *          pulse_stream_get_active_port      (MateMixerStream          *stream);
static gboolean                 pulse_stream_set_active_port      (MateMixerStream          *stream,
                                                                   MateMixerPort            *port);

static guint                    pulse_stream_get_min_volume       (MateMixerStream          *stream);
static guint                    pulse_stream_get_max_volume       (MateMixerStream          *stream);
static guint                    pulse_stream_get_normal_volume    (MateMixerStream          *stream);
static guint                    pulse_stream_get_base_volume      (MateMixerStream          *stream);

static void                     on_monitor_value    (PulseMonitor *monitor,
                                                     gdouble       value,
                                                     PulseStream  *pstream);

static gboolean                 update_balance_fade (PulseStream  *pstream);

static gboolean                 set_cvolume         (PulseStream  *pstream,
                                                     pa_cvolume   *cvolume);

static gint                     compare_ports       (gconstpointer a,
                                                     gconstpointer b);

static void
mate_mixer_stream_interface_init (MateMixerStreamInterface *iface)
{
    iface->get_name             = pulse_stream_get_name;
    iface->get_description      = pulse_stream_get_description;
    iface->get_device           = pulse_stream_get_device;
    iface->get_flags            = pulse_stream_get_flags;
    iface->get_state            = pulse_stream_get_state;
    iface->get_mute             = pulse_stream_get_mute;
    iface->set_mute             = pulse_stream_set_mute;
    iface->get_num_channels     = pulse_stream_get_num_channels;
    iface->get_volume           = pulse_stream_get_volume;
    iface->set_volume           = pulse_stream_set_volume;
    iface->get_decibel          = pulse_stream_get_decibel;
    iface->set_decibel          = pulse_stream_set_decibel;
    iface->get_channel_volume   = pulse_stream_get_channel_volume;
    iface->set_channel_volume   = pulse_stream_set_channel_volume;
    iface->get_channel_decibel  = pulse_stream_get_channel_decibel;
    iface->set_channel_decibel  = pulse_stream_set_channel_decibel;
    iface->get_channel_position = pulse_stream_get_channel_position;
    iface->has_channel_position = pulse_stream_has_channel_position;
    iface->get_balance          = pulse_stream_get_balance;
    iface->set_balance          = pulse_stream_set_balance;
    iface->get_fade             = pulse_stream_get_fade;
    iface->set_fade             = pulse_stream_set_fade;
    iface->suspend              = pulse_stream_suspend;
    iface->resume               = pulse_stream_resume;
    iface->monitor_start        = pulse_stream_monitor_start;
    iface->monitor_stop         = pulse_stream_monitor_stop;
    iface->monitor_is_running   = pulse_stream_monitor_is_running;
    iface->monitor_set_name     = pulse_stream_monitor_set_name;
    iface->list_ports           = pulse_stream_list_ports;
    iface->get_active_port      = pulse_stream_get_active_port;
    iface->set_active_port      = pulse_stream_set_active_port;
    iface->get_min_volume       = pulse_stream_get_min_volume;
    iface->get_max_volume       = pulse_stream_get_max_volume;
    iface->get_normal_volume    = pulse_stream_get_normal_volume;
    iface->get_base_volume      = pulse_stream_get_base_volume;
}

static void
pulse_stream_class_init (PulseStreamClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = pulse_stream_dispose;
    object_class->finalize     = pulse_stream_finalize;
    object_class->get_property = pulse_stream_get_property;
    object_class->set_property = pulse_stream_set_property;

    g_object_class_install_property (object_class,
                                     PROP_INDEX,
                                     g_param_spec_uint ("index",
                                                        "Index",
                                                        "Stream index",
                                                        0,
                                                        G_MAXUINT,
                                                        0,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (object_class,
                                     PROP_CONNECTION,
                                     g_param_spec_object ("connection",
                                                          "Connection",
                                                          "PulseAudio connection",
                                                          PULSE_TYPE_CONNECTION,
                                                          G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));

    g_object_class_override_property (object_class, PROP_NAME, "name");
    g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (object_class, PROP_DEVICE, "device");
    g_object_class_override_property (object_class, PROP_FLAGS, "flags");
    g_object_class_override_property (object_class, PROP_STATE, "state");
    g_object_class_override_property (object_class, PROP_MUTE, "mute");
    g_object_class_override_property (object_class, PROP_VOLUME, "volume");
    g_object_class_override_property (object_class, PROP_BALANCE, "balance");
    g_object_class_override_property (object_class, PROP_FADE, "fade");
    g_object_class_override_property (object_class, PROP_ACTIVE_PORT, "active-port");

    g_type_class_add_private (object_class, sizeof (PulseStreamPrivate));
}

static void
pulse_stream_get_property (GObject    *object,
                           guint       param_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    PulseStream *pstream;

    pstream = PULSE_STREAM (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, pstream->priv->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, pstream->priv->description);
        break;
    case PROP_DEVICE:
        g_value_set_object (value, pstream->priv->device);
        break;
    case PROP_FLAGS:
        g_value_set_flags (value, pstream->priv->flags);
        break;
    case PROP_STATE:
        g_value_set_enum (value, pstream->priv->state);
        break;
    case PROP_MUTE:
        g_value_set_boolean (value, pstream->priv->mute);
        break;
    case PROP_VOLUME:
        g_value_set_uint (value, pstream->priv->volume);
        break;
    case PROP_BALANCE:
        g_value_set_float (value, pstream->priv->balance);
        break;
    case PROP_FADE:
        g_value_set_float (value, pstream->priv->fade);
        break;
    case PROP_ACTIVE_PORT:
        g_value_set_object (value, pstream->priv->port);
        break;
    case PROP_INDEX:
        g_value_set_uint (value, pstream->priv->index);
        break;
    case PROP_CONNECTION:
        g_value_set_object (value, pstream->priv->connection);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_stream_set_property (GObject      *object,
                           guint         param_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    PulseStream *pstream;

    pstream = PULSE_STREAM (object);

    switch (param_id) {
    case PROP_INDEX:
        pstream->priv->index = g_value_get_uint (value);
        break;
    case PROP_CONNECTION:
        /* Construct-only object */
        pstream->priv->connection = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_stream_init (PulseStream *pstream)
{
    pstream->priv = G_TYPE_INSTANCE_GET_PRIVATE (pstream,
                                                 PULSE_TYPE_STREAM,
                                                 PulseStreamPrivate);

    pstream->priv->ports = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  g_object_unref);

    /* Initialize empty volume and channel map structures, they will be used
     * if the stream does not support volume */
    pa_cvolume_init (&pstream->priv->cvolume);

    pa_channel_map_init (&pstream->priv->channel_map);
}

static void
pulse_stream_dispose (GObject *object)
{
    PulseStream *pstream;

    pstream = PULSE_STREAM (object);

    if (pstream->priv->ports_list != NULL) {
        g_list_free_full (pstream->priv->ports_list, g_object_unref);
        pstream->priv->ports_list = NULL;
    }
    g_hash_table_remove_all (pstream->priv->ports);

    g_clear_object (&pstream->priv->port);
    g_clear_object (&pstream->priv->device);
    g_clear_object (&pstream->priv->monitor);
    g_clear_object (&pstream->priv->connection);

    G_OBJECT_CLASS (pulse_stream_parent_class)->dispose (object);
}

static void
pulse_stream_finalize (GObject *object)
{
    PulseStream *pstream;

    pstream = PULSE_STREAM (object);

    g_free (pstream->priv->name);
    g_free (pstream->priv->description);
    g_free (pstream->priv->monitor_name);

    g_hash_table_destroy (pstream->priv->ports);

    G_OBJECT_CLASS (pulse_stream_parent_class)->finalize (object);
}

guint32
pulse_stream_get_index (PulseStream *pstream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), 0);

    return pstream->priv->index;
}

PulseConnection *
pulse_stream_get_connection (PulseStream *pstream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), NULL);

    return pstream->priv->connection;
}

PulseMonitor *
pulse_stream_get_monitor (PulseStream *pstream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), NULL);

    return pstream->priv->monitor;
}

const pa_cvolume *
pulse_stream_get_cvolume (PulseStream *pstream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), NULL);

    return &pstream->priv->cvolume;
}

const pa_channel_map *
pulse_stream_get_channel_map (PulseStream *pstream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), NULL);

    return &pstream->priv->channel_map;
}

GHashTable *
pulse_stream_get_ports (PulseStream *pstream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), NULL);

    return pstream->priv->ports;
}

gboolean
pulse_stream_update_name (PulseStream *pstream, const gchar *name)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), FALSE);

    /* Allow the name to be NULL */
    if (g_strcmp0 (name, pstream->priv->name) != 0) {
        g_free (pstream->priv->name);
        pstream->priv->name = g_strdup (name);

        g_object_notify (G_OBJECT (pstream), "name");
        return TRUE;
    }
    return FALSE;
}

gboolean
pulse_stream_update_description (PulseStream *pstream, const gchar *description)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), FALSE);

    /* Allow the description to be NULL */
    if (g_strcmp0 (description, pstream->priv->description) != 0) {
        g_free (pstream->priv->description);
        pstream->priv->description = g_strdup (description);

        g_object_notify (G_OBJECT (pstream), "description");
        return TRUE;
    }
    return FALSE;
}

gboolean
pulse_stream_update_device (PulseStream *pstream, MateMixerDevice *device)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), FALSE);

    if (pstream->priv->device != device) {
        g_clear_object (&pstream->priv->device);

        if (G_LIKELY (device != NULL))
            pstream->priv->device = g_object_ref (device);

        g_object_notify (G_OBJECT (pstream), "device");
        return TRUE;
    }
    return FALSE;
}

gboolean
pulse_stream_update_flags (PulseStream *pstream, MateMixerStreamFlags flags)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), FALSE);

    if (pstream->priv->flags != flags) {
        pstream->priv->flags = flags;

        g_object_notify (G_OBJECT (pstream), "flags");
        return TRUE;
    }
    return FALSE;
}

gboolean
pulse_stream_update_state (PulseStream *pstream, MateMixerStreamState state)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), FALSE);

    if (pstream->priv->state != state) {
        pstream->priv->state = state;

        g_object_notify (G_OBJECT (pstream), "state");
        return TRUE;
    }
    return FALSE;
}

gboolean
pulse_stream_update_channel_map (PulseStream *pstream, const pa_channel_map *map)
{
    MateMixerStreamFlags flags;

    g_return_val_if_fail (PULSE_IS_STREAM (pstream), FALSE);
    g_return_val_if_fail (map != NULL, FALSE);

    flags = pstream->priv->flags;

    if (pa_channel_map_valid (map)) {
        if (pa_channel_map_can_balance (map))
            flags |= MATE_MIXER_STREAM_CAN_BALANCE;
        else
            flags &= ~MATE_MIXER_STREAM_CAN_BALANCE;

        if (pa_channel_map_can_fade (map))
            flags |= MATE_MIXER_STREAM_CAN_FADE;
        else
            flags &= ~MATE_MIXER_STREAM_CAN_FADE;

        pstream->priv->channel_map = *map;
    } else {
        flags &= ~(MATE_MIXER_STREAM_CAN_BALANCE | MATE_MIXER_STREAM_CAN_FADE);

        /* If the channel map is not valid, create an empty channel map, which
         * also won't validate, but at least we know what it is */
        pa_channel_map_init (&pstream->priv->channel_map);
    }

    pulse_stream_update_flags (pstream, flags);
    return TRUE;
}

gboolean
pulse_stream_update_volume (PulseStream      *pstream,
                            const pa_cvolume *cvolume,
                            pa_volume_t       base_volume)
{
    MateMixerStreamFlags flags;

    g_return_val_if_fail (PULSE_IS_STREAM (pstream), FALSE);

    /* The base volume is not a property */
    pstream->priv->base_volume = base_volume;

    flags = pstream->priv->flags;

    if (cvolume != NULL && pa_cvolume_valid (cvolume)) {
        /* Decibel volume and volume settability flags must be provided by
         * the implementation */
        flags |= MATE_MIXER_STREAM_HAS_VOLUME;

        if (pa_cvolume_equal (&pstream->priv->cvolume, cvolume) == 0) {
            pstream->priv->cvolume = *cvolume;
            pstream->priv->volume  = (guint) pa_cvolume_max (&pstream->priv->cvolume);

            g_object_notify (G_OBJECT (pstream), "volume");
        }
    } else {
        flags &= ~(MATE_MIXER_STREAM_HAS_VOLUME |
                   MATE_MIXER_STREAM_HAS_DECIBEL_VOLUME |
                   MATE_MIXER_STREAM_CAN_SET_VOLUME);

        /* If the cvolume is not valid, create an empty cvolume, which also
         * won't validate, but at least we know what it is */
        pa_cvolume_init (&pstream->priv->cvolume);

        if (pstream->priv->volume != (guint) PA_VOLUME_MUTED) {
            pstream->priv->volume = (guint) PA_VOLUME_MUTED;

            g_object_notify (G_OBJECT (pstream), "volume");
        }
    }

    pulse_stream_update_flags (pstream, flags);

    /* Changing volume may change the balance and fade values as well */
    update_balance_fade (pstream);
    return TRUE;
}

gboolean
pulse_stream_update_mute (PulseStream *pstream, gboolean mute)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), FALSE);

    if (pstream->priv->mute != mute) {
        pstream->priv->mute = mute;

        g_object_notify (G_OBJECT (pstream), "mute");
        return TRUE;
    }
    return FALSE;
}

gboolean
pulse_stream_update_active_port (PulseStream *pstream, MateMixerPort *port)
{
    g_return_val_if_fail (PULSE_IS_STREAM (pstream), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_PORT (port), FALSE);

    if (pstream->priv->port != port) {
        if (pstream->priv->port != NULL)
            g_clear_object (&pstream->priv->port);

        if (port != NULL)
            pstream->priv->port = g_object_ref (port);

        g_object_notify (G_OBJECT (pstream), "active-port");
        return TRUE;
    }
    return FALSE;
}

static const gchar *
pulse_stream_get_name (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), NULL);

    return PULSE_STREAM (stream)->priv->name;
}

static const gchar *
pulse_stream_get_description (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), NULL);

    return PULSE_STREAM (stream)->priv->description;
}

static MateMixerDevice *
pulse_stream_get_device (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), NULL);

    return PULSE_STREAM (stream)->priv->device;
}

static MateMixerStreamFlags
pulse_stream_get_flags (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), MATE_MIXER_STREAM_NO_FLAGS);

    return PULSE_STREAM (stream)->priv->flags;
}

static MateMixerStreamState
pulse_stream_get_state (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), MATE_MIXER_STREAM_STATE_UNKNOWN);

    return PULSE_STREAM (stream)->priv->state;
}

static gboolean
pulse_stream_get_mute (MateMixerStream *stream)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_HAS_MUTE))
        return FALSE;

    return pstream->priv->mute;
}

static gboolean
pulse_stream_set_mute (MateMixerStream *stream, gboolean mute)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_HAS_MUTE))
        return FALSE;

    if (pstream->priv->mute != mute) {
        PulseStreamClass *klass = PULSE_STREAM_GET_CLASS (pstream);

        if (klass->set_mute (pstream, mute) == FALSE)
            return FALSE;

        pstream->priv->mute = mute;

        g_object_notify (G_OBJECT (stream), "mute");
    }
    return TRUE;
}

static guint
pulse_stream_get_num_channels (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), 0);

    return PULSE_STREAM (stream)->priv->channel_map.channels;
}

static guint
pulse_stream_get_volume (MateMixerStream *stream)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), (guint) PA_VOLUME_MUTED);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_HAS_VOLUME))
        return (guint) PA_VOLUME_MUTED;

    return pstream->priv->volume;
}

static gboolean
pulse_stream_set_volume (MateMixerStream *stream, guint volume)
{
    PulseStream *pstream;
    pa_cvolume   cvolume;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_CAN_SET_VOLUME))
        return FALSE;

    cvolume = pstream->priv->cvolume;

    if (pa_cvolume_scale (&cvolume, (pa_volume_t) volume) == NULL)
        return FALSE;

    return set_cvolume (pstream, &cvolume);
}

static gdouble
pulse_stream_get_decibel (MateMixerStream *stream)
{
    PulseStream *pstream;
    gdouble      value;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), -MATE_MIXER_INFINITY);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_HAS_DECIBEL_VOLUME))
        return -MATE_MIXER_INFINITY;

    value = pa_sw_volume_to_dB (pulse_stream_get_volume (stream));

    /* PA_VOLUME_MUTED is converted to PA_DECIBEL_MININFTY */
    return (value == PA_DECIBEL_MININFTY) ? -MATE_MIXER_INFINITY : value;
}

static gboolean
pulse_stream_set_decibel (MateMixerStream *stream, gdouble decibel)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_HAS_DECIBEL_VOLUME) ||
        !(pstream->priv->flags & MATE_MIXER_STREAM_CAN_SET_VOLUME))
        return FALSE;

    return pulse_stream_set_volume (stream, pa_sw_volume_from_dB (decibel));
}

static guint
pulse_stream_get_channel_volume (MateMixerStream *stream, guint channel)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), (guint) PA_VOLUME_MUTED);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_HAS_VOLUME))
        return (guint) PA_VOLUME_MUTED;

    if (channel >= pstream->priv->cvolume.channels)
        return (guint) PA_VOLUME_MUTED;

    return (guint) pstream->priv->cvolume.values[channel];
}

static gboolean
pulse_stream_set_channel_volume (MateMixerStream *stream, guint channel, guint volume)
{
    PulseStream *pstream;
    pa_cvolume   cvolume;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_CAN_SET_VOLUME))
        return FALSE;

    if (channel >= pstream->priv->cvolume.channels)
        return FALSE;

    /* This is safe, because the cvolume is validated by set_cvolume() */
    cvolume = pstream->priv->cvolume;
    cvolume.values[channel] = (pa_volume_t) volume;

    return set_cvolume (pstream, &cvolume);
}

static gdouble
pulse_stream_get_channel_decibel (MateMixerStream *stream, guint channel)
{
    PulseStream *pstream;
    gdouble      value;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), -MATE_MIXER_INFINITY);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_HAS_DECIBEL_VOLUME))
        return -MATE_MIXER_INFINITY;

    if (channel >= pstream->priv->cvolume.channels)
        return -MATE_MIXER_INFINITY;

    value = pa_sw_volume_to_dB (pstream->priv->cvolume.values[channel]);

    return (value == PA_DECIBEL_MININFTY) ? -MATE_MIXER_INFINITY : value;
}

static gboolean
pulse_stream_set_channel_decibel (MateMixerStream *stream,
                                  guint            channel,
                                  gdouble          decibel)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_HAS_DECIBEL_VOLUME) ||
        !(pstream->priv->flags & MATE_MIXER_STREAM_CAN_SET_VOLUME))
        return FALSE;

    return pulse_stream_set_channel_volume (stream, channel, pa_sw_volume_from_dB (decibel));
}

static MateMixerChannelPosition
pulse_stream_get_channel_position (MateMixerStream *stream, guint channel)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), MATE_MIXER_CHANNEL_UNKNOWN);

    pstream = PULSE_STREAM (stream);

    if (channel >= pstream->priv->channel_map.channels)
        return MATE_MIXER_CHANNEL_UNKNOWN;

    return pulse_convert_position_to_pulse (pstream->priv->channel_map.map[channel]);
}

static gboolean
pulse_stream_has_channel_position (MateMixerStream         *stream,
                                   MateMixerChannelPosition position)
{
    PulseStream          *pstream;
    pa_channel_position_t p;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    /* Handle invalid position as a special case, otherwise this function would
     * return TRUE for e.g. unknown index in a default channel map */
    p = pulse_convert_position_to_pulse (position);

    if (p == PA_CHANNEL_POSITION_INVALID)
        return FALSE;

    if (pa_channel_map_has_position (&pstream->priv->channel_map, p) != 0)
        return TRUE;
    else
        return FALSE;
}

static gfloat
pulse_stream_get_balance (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), 0.0f);

    return PULSE_STREAM (stream)->priv->balance;
}

static gboolean
pulse_stream_set_balance (MateMixerStream *stream, gfloat balance)
{
    PulseStream *pstream;
    pa_cvolume   cvolume;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_CAN_BALANCE))
        return FALSE;

    cvolume = pstream->priv->cvolume;

    if (pa_cvolume_set_balance (&cvolume, &pstream->priv->channel_map, balance) == NULL)
        return FALSE;

    return set_cvolume (pstream, &cvolume);
}

static gfloat
pulse_stream_get_fade (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), 0.0f);

    return PULSE_STREAM (stream)->priv->fade;
}

static gboolean
pulse_stream_set_fade (MateMixerStream *stream, gfloat fade)
{
    PulseStream *pstream;
    pa_cvolume   cvolume;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_CAN_FADE))
        return FALSE;

    cvolume = pstream->priv->cvolume;

    if (pa_cvolume_set_fade (&cvolume, &pstream->priv->channel_map, fade) == NULL)
        return FALSE;

    return set_cvolume (pstream, &cvolume);
}

static gboolean
pulse_stream_suspend (MateMixerStream *stream)
{
    PulseStream      *pstream;
    PulseStreamClass *klass;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_CAN_SUSPEND))
        return FALSE;

    if (pstream->priv->state == MATE_MIXER_STREAM_STATE_SUSPENDED)
        return FALSE;

    klass = PULSE_STREAM_GET_CLASS (pstream);

    if (klass->suspend (pstream) == FALSE)
        return FALSE;

    pulse_stream_update_state (pstream, MATE_MIXER_STREAM_STATE_SUSPENDED);
    return TRUE;
}

static gboolean
pulse_stream_resume (MateMixerStream *stream)
{
    PulseStream      *pstream;
    PulseStreamClass *klass;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_CAN_SUSPEND))
        return FALSE;

    if (pstream->priv->state != MATE_MIXER_STREAM_STATE_SUSPENDED)
        return FALSE;

    klass = PULSE_STREAM_GET_CLASS (pstream);

    if (klass->resume (pstream) == FALSE)
        return FALSE;

    /* The state when resumed should be either RUNNING or IDLE, let's assume
     * IDLE for now and request an immediate update */
    pulse_stream_update_state (pstream, MATE_MIXER_STREAM_STATE_IDLE);

    klass->reload (pstream);
    return TRUE;
}

static gboolean
pulse_stream_monitor_start (MateMixerStream *stream)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_HAS_MONITOR))
        return FALSE;

    if (pstream->priv->monitor == NULL) {
        pstream->priv->monitor = PULSE_STREAM_GET_CLASS (pstream)->create_monitor (pstream);

        if (G_UNLIKELY (pstream->priv->monitor == NULL))
            return FALSE;

        pulse_monitor_set_name (pstream->priv->monitor,
                                pstream->priv->monitor_name);

        g_signal_connect (G_OBJECT (pstream->priv->monitor),
                          "value",
                          G_CALLBACK (on_monitor_value),
                          pstream);
    }

    return pulse_monitor_set_enabled (pstream->priv->monitor, TRUE);
}

static void
pulse_stream_monitor_stop (MateMixerStream *stream)
{
    PulseStream *pstream;

    g_return_if_fail (PULSE_IS_STREAM (stream));

    pstream = PULSE_STREAM (stream);

    if (pstream->priv->monitor != NULL)
        pulse_monitor_set_enabled (pstream->priv->monitor, FALSE);
}

static gboolean
pulse_stream_monitor_is_running (MateMixerStream *stream)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (pstream->priv->monitor != NULL)
        return pulse_monitor_get_enabled (pstream->priv->monitor);

    return FALSE;
}

static gboolean
pulse_stream_monitor_set_name (MateMixerStream *stream, const gchar *name)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);

    if (pstream->priv->monitor != NULL)
        pulse_monitor_set_name (pstream->priv->monitor, name);

    pstream->priv->monitor_name = g_strdup (name);
    return TRUE;
}

static const GList *
pulse_stream_list_ports (MateMixerStream *stream)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), NULL);

    pstream = PULSE_STREAM (stream);

    if (pstream->priv->ports_list == NULL) {
        GList *list = g_hash_table_get_values (pstream->priv->ports);

        if (list != NULL) {
            g_list_foreach (list, (GFunc) g_object_ref, NULL);

            pstream->priv->ports_list = g_list_sort (list, compare_ports);
        }
    }

    return (const GList *) pstream->priv->ports_list;
}

static MateMixerPort *
pulse_stream_get_active_port (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), NULL);

    return PULSE_STREAM (stream)->priv->port;
}

static gboolean
pulse_stream_set_active_port (MateMixerStream *stream, MateMixerPort *port)
{
    PulseStream      *pstream;
    PulseStreamClass *klass;
    const gchar      *name;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_PORT (port), FALSE);

    pstream = PULSE_STREAM (stream);

    /* Make sure the port comes from this stream */
    name = mate_mixer_port_get_name (port);

    if (g_hash_table_lookup (pstream->priv->ports, name) == NULL) {
        g_warning ("Port %s does not belong to stream %s",
                   mate_mixer_port_get_name (port),
                   mate_mixer_stream_get_name (stream));
        return FALSE;
    }

    klass = PULSE_STREAM_GET_CLASS (pstream);

    /* Change the port */
    if (klass->set_active_port (pstream, port) == FALSE)
        return FALSE;

    if (pstream->priv->port != NULL)
        g_object_unref (pstream->priv->port);

    pstream->priv->port = g_object_ref (port);

    g_object_notify (G_OBJECT (stream), "active-port");
    return TRUE;
}

static guint
pulse_stream_get_min_volume (MateMixerStream *stream)
{
    return (guint) PA_VOLUME_MUTED;
}

static guint
pulse_stream_get_max_volume (MateMixerStream *stream)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), (guint) PA_VOLUME_MUTED);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_HAS_VOLUME))
        return (guint) PA_VOLUME_MUTED;

    return (guint) PA_VOLUME_UI_MAX;
}

static guint
pulse_stream_get_normal_volume (MateMixerStream *stream)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), (guint) PA_VOLUME_MUTED);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_HAS_VOLUME))
        return (guint) PA_VOLUME_MUTED;

    return (guint) PA_VOLUME_NORM;
}

static guint
pulse_stream_get_base_volume (MateMixerStream *stream)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), (guint) PA_VOLUME_MUTED);

    pstream = PULSE_STREAM (stream);

    if (!(pstream->priv->flags & MATE_MIXER_STREAM_HAS_VOLUME))
        return (guint) PA_VOLUME_MUTED;

    if (pstream->priv->base_volume > 0)
        return pstream->priv->base_volume;
    else
        return (guint) PA_VOLUME_NORM;
}

static void
on_monitor_value (PulseMonitor *monitor, gdouble value, PulseStream *pstream)
{
    g_signal_emit_by_name (G_OBJECT (pstream),
                           "monitor-value",
                           value);
}

static gboolean
update_balance_fade (PulseStream *pstream)
{
    gfloat   fade;
    gfloat   balance;
    gboolean changed = FALSE;

    /* The PulseAudio return the default 0.0f values on errors, so skip checking
     * validity of the channel map and volume */
    balance = pa_cvolume_get_balance (&pstream->priv->cvolume,
                                      &pstream->priv->channel_map);

    if (pstream->priv->balance != balance) {
        pstream->priv->balance = balance;

        g_object_notify (G_OBJECT (pstream), "balance");
        changed = TRUE;
    }

    fade = pa_cvolume_get_fade (&pstream->priv->cvolume,
                                &pstream->priv->channel_map);

    if (pstream->priv->fade != fade) {
        pstream->priv->fade = fade;

        g_object_notify (G_OBJECT (pstream), "fade");
        changed = TRUE;
    }

    return changed;
}

static gboolean
set_cvolume (PulseStream *pstream, pa_cvolume *cvolume)
{
    PulseStreamClass *klass;

    if (pa_cvolume_valid (cvolume) == 0)
        return FALSE;
    if (pa_cvolume_equal (cvolume, &pstream->priv->cvolume) != 0)
        return TRUE;

    klass = PULSE_STREAM_GET_CLASS (pstream);

    if (klass->set_volume (pstream, cvolume) == FALSE)
        return FALSE;

    pstream->priv->cvolume = *cvolume;
    pstream->priv->volume  = (guint) pa_cvolume_max (cvolume);

    g_object_notify (G_OBJECT (pstream), "volume");

    /* Changing volume may change the balance and fade values as well */
    update_balance_fade (pstream);
    return TRUE;
}

static gint
compare_ports (gconstpointer a, gconstpointer b)
{
    MateMixerPort *p1 = MATE_MIXER_PORT (a);
    MateMixerPort *p2 = MATE_MIXER_PORT (b);

    gint ret = (gint) (mate_mixer_port_get_priority (p2) -
                       mate_mixer_port_get_priority (p1));
    if (ret != 0)
        return ret;
    else
        return strcmp (mate_mixer_port_get_name (p1),
                       mate_mixer_port_get_name (p2));
}
