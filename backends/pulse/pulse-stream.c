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

#include <libmatemixer/matemixer-device.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-stream.h>
#include <libmatemixer/matemixer-port.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-helpers.h"
#include "pulse-stream.h"

struct _PulseStreamPrivate
{
    guint32                index;
    guint32                index_device;
    gchar                 *name;
    gchar                 *description;
    gchar                 *icon;
    MateMixerDevice       *device;
    MateMixerStreamFlags   flags;
    MateMixerStreamStatus  status;
    gboolean               mute;
    pa_cvolume             volume;
    pa_volume_t            volume_base;
    guint32                volume_steps;
    pa_channel_map         channel_map;
    gdouble                balance;
    gdouble                fade;
    GList                 *ports;
    MateMixerPort         *port;
    PulseConnection       *connection;
};

enum
{
    PROP_0,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_ICON,
    PROP_DEVICE,
    PROP_FLAGS,
    PROP_STATUS,
    PROP_MUTE,
    PROP_NUM_CHANNELS,
    PROP_VOLUME,
    PROP_VOLUME_DB,
    PROP_BALANCE,
    PROP_FADE,
    PROP_ACTIVE_PORT,
    PROP_INDEX,
    PROP_CONNECTION,
    N_PROPERTIES
};

static void mate_mixer_stream_interface_init (MateMixerStreamInterface  *iface);
static void pulse_stream_class_init          (PulseStreamClass          *klass);
static void pulse_stream_init                (PulseStream               *stream);
static void pulse_stream_dispose             (GObject                   *object);
static void pulse_stream_finalize            (GObject                   *object);

/* Interface implementation */
static const gchar *            stream_get_name               (MateMixerStream          *stream);
static const gchar *            stream_get_description        (MateMixerStream          *stream);
static const gchar *            stream_get_icon               (MateMixerStream          *stream);
static MateMixerDevice *        stream_get_device             (MateMixerStream          *stream);
static MateMixerStreamFlags     stream_get_flags              (MateMixerStream          *stream);
static MateMixerStreamStatus    stream_get_status             (MateMixerStream          *stream);
static gboolean                 stream_get_mute               (MateMixerStream          *stream);
static gboolean                 stream_set_mute               (MateMixerStream          *stream,
                                                               gboolean                  mute);
static guint                    stream_get_num_channels       (MateMixerStream          *stream);
static gint64                   stream_get_volume             (MateMixerStream          *stream);
static gboolean                 stream_set_volume             (MateMixerStream          *stream,
                                                               gint64                    volume);
static gdouble                  stream_get_volume_db          (MateMixerStream          *stream);
static gboolean                 stream_set_volume_db          (MateMixerStream          *stream,
                                                               gdouble                   volume_db);
static MateMixerChannelPosition stream_get_channel_position   (MateMixerStream          *stream,
                                                               guint                     channel);
static gint64                   stream_get_channel_volume     (MateMixerStream          *stream,
                                                               guint                     channel);
static gboolean                 stream_set_channel_volume     (MateMixerStream          *stream,
                                                               guint                     channel,
                                                               gint64                    volume);
static gdouble                  stream_get_channel_volume_db  (MateMixerStream          *stream,
                                                               guint                     channel);
static gboolean                 stream_set_channel_volume_db  (MateMixerStream          *stream,
                                                               guint                     channel,
                                                               gdouble                   volume_db);
static gboolean                 stream_has_position           (MateMixerStream          *stream,
                                                               MateMixerChannelPosition  position);
static gint64                   stream_get_position_volume    (MateMixerStream          *stream,
                                                               MateMixerChannelPosition  position);
static gboolean                 stream_set_position_volume    (MateMixerStream          *stream,
                                                               MateMixerChannelPosition  position,
                                                               gint64                    volume);
static gdouble                  stream_get_position_volume_db (MateMixerStream          *stream,
                                                               MateMixerChannelPosition  position);
static gboolean                 stream_set_position_volume_db (MateMixerStream          *stream,
                                                               MateMixerChannelPosition  position,
                                                               gdouble                   volume_db);
static gdouble                  stream_get_balance            (MateMixerStream          *stream);
static gboolean                 stream_set_balance            (MateMixerStream          *stream,
                                                               gdouble                   balance);
static gdouble                  stream_get_fade               (MateMixerStream          *stream);
static gboolean                 stream_set_fade               (MateMixerStream          *stream,
                                                               gdouble                   fade);
static gboolean                 stream_suspend                (MateMixerStream          *stream);
static gboolean                 stream_resume                 (MateMixerStream          *stream);
static const GList *            stream_list_ports             (MateMixerStream          *stream);
static MateMixerPort *          stream_get_active_port        (MateMixerStream          *stream);
static gboolean                 stream_set_active_port        (MateMixerStream          *stream,
                                                               const gchar              *port);
static gint64                   stream_get_min_volume         (MateMixerStream          *stream);
static gint64                   stream_get_max_volume         (MateMixerStream          *stream);
static gint64                   stream_get_normal_volume      (MateMixerStream          *stream);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PulseStream, pulse_stream, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_STREAM,
                                                         mate_mixer_stream_interface_init))

static void
mate_mixer_stream_interface_init (MateMixerStreamInterface *iface)
{
    iface->get_name                 = stream_get_name;
    iface->get_description          = stream_get_description;
    iface->get_icon                 = stream_get_icon;
    iface->get_device               = stream_get_device;
    iface->get_flags                = stream_get_flags;
    iface->get_status               = stream_get_status;
    iface->get_mute                 = stream_get_mute;
    iface->set_mute                 = stream_set_mute;
    iface->get_num_channels         = stream_get_num_channels;
    iface->get_volume               = stream_get_volume;
    iface->set_volume               = stream_set_volume;
    iface->get_volume_db            = stream_get_volume_db;
    iface->set_volume_db            = stream_set_volume_db;
    iface->get_channel_position     = stream_get_channel_position;
    iface->get_channel_volume       = stream_get_channel_volume;
    iface->set_channel_volume       = stream_set_channel_volume;
    iface->get_channel_volume_db    = stream_get_channel_volume_db;
    iface->set_channel_volume_db    = stream_set_channel_volume_db;
    iface->has_position             = stream_has_position;
    iface->get_position_volume      = stream_get_position_volume;
    iface->set_position_volume      = stream_set_position_volume;
    iface->get_position_volume_db   = stream_get_position_volume_db;
    iface->set_position_volume_db   = stream_set_position_volume_db;
    iface->get_balance              = stream_get_balance;
    iface->set_balance              = stream_set_balance;
    iface->get_fade                 = stream_get_fade;
    iface->set_fade                 = stream_set_fade;
    iface->suspend                  = stream_suspend;
    iface->resume                   = stream_resume;
    iface->list_ports               = stream_list_ports;
    iface->get_active_port          = stream_get_active_port;
    iface->set_active_port          = stream_set_active_port;
    iface->get_min_volume           = stream_get_min_volume;
    iface->get_max_volume           = stream_get_max_volume;
    iface->get_normal_volume        = stream_get_normal_volume;
}

static void
pulse_stream_get_property (GObject    *object,
                           guint       param_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    PulseStream *stream;

    stream = PULSE_STREAM (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, stream->priv->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, stream->priv->description);
        break;
    case PROP_ICON:
        g_value_set_string (value, stream->priv->icon);
        break;
    case PROP_DEVICE:
        g_value_set_object (value, stream->priv->device);
        break;
    case PROP_FLAGS:
        g_value_set_flags (value, stream->priv->flags);
        break;
    case PROP_STATUS:
        g_value_set_enum (value, stream->priv->status);
        break;
    case PROP_MUTE:
        g_value_set_boolean (value, stream->priv->mute);
        break;
    case PROP_NUM_CHANNELS:
        g_value_set_uint (value, stream_get_num_channels (MATE_MIXER_STREAM (stream)));
        break;
    case PROP_VOLUME:
        g_value_set_int64 (value, stream_get_volume (MATE_MIXER_STREAM (stream)));
        break;
    case PROP_VOLUME_DB:
        g_value_set_double (value, stream_get_volume_db (MATE_MIXER_STREAM (stream)));
        break;
    case PROP_BALANCE:
        g_value_set_double (value, stream->priv->balance);
        break;
    case PROP_FADE:
        g_value_set_double (value, stream->priv->fade);
        break;
    case PROP_ACTIVE_PORT:
        g_value_set_object (value, stream->priv->port);
        break;
    case PROP_INDEX:
        g_value_set_uint (value, stream->priv->index);
        break;
    case PROP_CONNECTION:
        g_value_set_object (value, stream->priv->connection);
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
    PulseStream *stream;

    stream = PULSE_STREAM (object);

    switch (param_id) {
    case PROP_NAME:
        stream->priv->name = g_strdup (g_value_dup_string (value));
        break;
    case PROP_DESCRIPTION:
        stream->priv->description = g_strdup (g_value_get_string (value));
        break;
    case PROP_ICON:
        stream->priv->icon = g_strdup (g_value_get_string (value));
        break;
    case PROP_DEVICE:
        // XXX may be NULL and the device may become known after the stream,
        // figure this out..
        // stream->priv->device = g_object_ref (g_value_get_object (value));
        break;
    case PROP_FLAGS:
        stream->priv->flags = g_value_get_flags (value);
        break;
    case PROP_STATUS:
        stream->priv->status = g_value_get_enum (value);
        break;
    case PROP_MUTE:
        stream->priv->mute = g_value_get_boolean (value);
        break;
    case PROP_INDEX:
        stream->priv->index = g_value_get_uint (value);
        break;
    case PROP_CONNECTION:
        stream->priv->connection = g_value_dup_object (value);
        break;
    case PROP_ACTIVE_PORT:
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
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
    g_object_class_override_property (object_class, PROP_ICON, "icon");
    g_object_class_override_property (object_class, PROP_DEVICE, "device");
    g_object_class_override_property (object_class, PROP_FLAGS, "flags");
    g_object_class_override_property (object_class, PROP_STATUS, "status");
    g_object_class_override_property (object_class, PROP_MUTE, "mute");
    g_object_class_override_property (object_class, PROP_NUM_CHANNELS, "num-channels");
    g_object_class_override_property (object_class, PROP_VOLUME, "volume");
    g_object_class_override_property (object_class, PROP_VOLUME_DB, "volume-db");
    g_object_class_override_property (object_class, PROP_BALANCE, "balance");
    g_object_class_override_property (object_class, PROP_FADE, "fade");
    g_object_class_override_property (object_class, PROP_ACTIVE_PORT, "active-port");

    g_type_class_add_private (object_class, sizeof (PulseStreamPrivate));
}

static void
pulse_stream_init (PulseStream *stream)
{
    stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (stream,
                                                PULSE_TYPE_STREAM,
                                                PulseStreamPrivate);
}

static void
pulse_stream_dispose (GObject *object)
{
    PulseStream *stream;

    stream = PULSE_STREAM (object);

    if (stream->priv->ports) {
        g_list_free_full (stream->priv->ports, g_object_unref);
        stream->priv->ports = NULL;
    }

    g_clear_object (&stream->priv->port);
    g_clear_object (&stream->priv->device);
    g_clear_object (&stream->priv->connection);

    G_OBJECT_CLASS (pulse_stream_parent_class)->dispose (object);
}

static void
pulse_stream_finalize (GObject *object)
{
    PulseStream *stream;

    stream = PULSE_STREAM (object);

    g_free (stream->priv->name);
    g_free (stream->priv->description);
    g_free (stream->priv->icon);

    G_OBJECT_CLASS (pulse_stream_parent_class)->finalize (object);
}

guint32
pulse_stream_get_index (PulseStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return stream->priv->index;
}

PulseConnection *
pulse_stream_get_connection (PulseStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), NULL);

    return stream->priv->connection;
}

gboolean
pulse_stream_update_name (PulseStream *stream, const gchar *name)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    /* Allow the name to be NULL */
    if (g_strcmp0 (name, stream->priv->name)) {
        g_free (stream->priv->name);
        stream->priv->name = g_strdup (name);

        g_object_notify (G_OBJECT (stream), "name");
    }
    return TRUE;
}

gboolean
pulse_stream_update_description (PulseStream *stream, const gchar *description)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    /* Allow the description to be NULL */
    if (g_strcmp0 (description, stream->priv->description)) {
        g_free (stream->priv->description);
        stream->priv->description = g_strdup (description);

        g_object_notify (G_OBJECT (stream), "description");
    }
    return TRUE;
}

gboolean
pulse_stream_update_flags (PulseStream *stream, MateMixerStreamFlags flags)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    if (stream->priv->flags != flags) {
        stream->priv->flags = flags;
        g_object_notify (G_OBJECT (stream), "flags");
    }
    return TRUE;
}

gboolean
pulse_stream_update_status (PulseStream *stream, MateMixerStreamStatus status)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    if (stream->priv->status != status) {
        stream->priv->status = status;
        g_object_notify (G_OBJECT (stream), "status");
    }
    return TRUE;
}

gboolean
pulse_stream_update_mute (PulseStream *stream, gboolean mute)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    if (stream->priv->mute != mute) {
        stream->priv->mute = mute;
        g_object_notify (G_OBJECT (stream), "mute");
    }
    return TRUE;
}

gboolean
pulse_stream_update_volume (PulseStream *stream, const pa_cvolume *volume)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    if (!pa_cvolume_equal (&stream->priv->volume, volume)) {
        stream->priv->volume = *volume;

        g_object_notify (G_OBJECT (stream), "volume");

        // XXX probably should notify about volume-db too but the flags may
        // be known later
    }
    return TRUE;
}

gboolean
pulse_stream_update_volume_extended (PulseStream      *stream,
                                     const pa_cvolume *volume,
                                     pa_volume_t       volume_base,
                                     guint32           volume_steps)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    // XXX use volume_base and volume_steps

    if (!pa_cvolume_equal (&stream->priv->volume, volume)) {
        stream->priv->volume = *volume;

        g_object_notify (G_OBJECT (stream), "volume");

        // XXX probably should notify about volume-db too but the flags may
        // be known later
    }

    stream->priv->volume_base  = volume_base;
    stream->priv->volume_steps = volume_steps;
    return TRUE;
}

gboolean
pulse_stream_update_channel_map (PulseStream *stream, const pa_channel_map *map)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    if (!pa_channel_map_equal (&stream->priv->channel_map, map))
        stream->priv->channel_map = *map;

    return TRUE;
}

gboolean
pulse_stream_update_ports (PulseStream *stream, GList *ports)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    /* Right now we do not change the list of ports during update */
    g_warn_if_fail (stream->priv->ports == NULL);

    stream->priv->ports = ports;
    return TRUE;
}

gboolean
pulse_stream_update_active_port (PulseStream *stream, const gchar *port_name)
{
    GList         *list;
    MateMixerPort *port = NULL;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    list = stream->priv->ports;
    while (list) {
        port = MATE_MIXER_PORT (list->data);

        if (!g_strcmp0 (mate_mixer_port_get_name (port), port_name))
            break;

        port = NULL;
        list = list->next;
    }

    if (stream->priv->port != port) {
        if (stream->priv->port)
            g_clear_object (&stream->priv->port);
        if (port)
            stream->priv->port = g_object_ref (port);

        g_object_notify (G_OBJECT (stream), "active-port");
    }
    return TRUE;
}

static const gchar *
stream_get_name (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return PULSE_STREAM (stream)->priv->name;
}

static const gchar *
stream_get_description (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return PULSE_STREAM (stream)->priv->description;
}

static const gchar *
stream_get_icon (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return PULSE_STREAM (stream)->priv->icon;
}

static MateMixerDevice *
stream_get_device (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return PULSE_STREAM (stream)->priv->device;
}

static MateMixerStreamFlags
stream_get_flags (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return PULSE_STREAM (stream)->priv->flags;
}

static MateMixerStreamStatus
stream_get_status (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return PULSE_STREAM (stream)->priv->status;
}

static gboolean
stream_get_mute (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return PULSE_STREAM (stream)->priv->mute;
}

static gboolean
stream_set_mute (MateMixerStream *stream, gboolean mute)
{
    PulseStream *ps;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    ps = PULSE_STREAM (stream);

    if (ps->priv->mute == mute)
        return TRUE;

    return PULSE_STREAM_GET_CLASS (stream)->set_mute (stream, mute);
}

static guint
stream_get_num_channels (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return PULSE_STREAM (stream)->priv->volume.channels;
}

static gint64
stream_get_volume (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return (gint64) pa_cvolume_max (&PULSE_STREAM (stream)->priv->volume);
}

static gboolean
stream_set_volume (MateMixerStream *stream, gint64 volume)
{
    pa_cvolume             cvolume;
    PulseStream  *ps;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    ps = PULSE_STREAM (stream);
    cvolume = ps->priv->volume;

    if (pa_cvolume_scale (&cvolume, (pa_volume_t) volume) == NULL) {
        g_warning ("Invalid PulseAudio volume value %" G_GINT64_FORMAT, volume);
        return FALSE;
    }

    /* This is the only function which passes a volume request to the real class, so
     * all the pa_cvolume validations are only done here */



    return PULSE_STREAM_GET_CLASS (stream)->set_volume (stream, &cvolume);
}

static gdouble
stream_get_volume_db (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), 0.0);

    if (!(mate_mixer_stream_get_flags (stream) & MATE_MIXER_STREAM_HAS_DECIBEL_VOLUME))
        return FALSE;

    return pa_sw_volume_to_dB (stream_get_volume (stream));
}

static gboolean
stream_set_volume_db (MateMixerStream *stream, gdouble volume_db)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    if (!(mate_mixer_stream_get_flags (stream) & MATE_MIXER_STREAM_HAS_DECIBEL_VOLUME))
        return FALSE;

    return stream_set_volume (stream, pa_sw_volume_from_dB (volume_db));
}

static MateMixerChannelPosition
stream_get_channel_position (MateMixerStream *stream, guint channel)
{
    PulseStream *ps;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    ps = PULSE_STREAM (stream);

    if (channel >= ps->priv->channel_map.channels) {
        g_warning ("Invalid channel %u of stream %s", channel, ps->priv->name);
        return MATE_MIXER_CHANNEL_UNKNOWN_POSITION;
    }
    return pulse_convert_position_to_pulse (ps->priv->channel_map.map[channel]);
}

static gint64
stream_get_channel_volume (MateMixerStream *stream, guint channel)
{
    PulseStream *ps;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    ps = PULSE_STREAM (stream);

    if (channel >= ps->priv->volume.channels) {
        g_warning ("Invalid channel %u of stream %s", channel, ps->priv->name);
        return stream_get_min_volume (stream);
    }
    return (gint64) ps->priv->volume.values[channel];
}

static gboolean
stream_set_channel_volume (MateMixerStream *stream, guint channel, gint64 volume)
{
    pa_cvolume             cvolume;
    PulseStream  *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);
    cvolume = pstream->priv->volume;

    if (channel >= pstream->priv->volume.channels) {
        g_warning ("Invalid channel %u of stream %s", channel, pstream->priv->name);
        return FALSE;
    }

    cvolume.values[channel] = (pa_volume_t) volume;

    if (!pa_cvolume_valid (&cvolume)) {
        g_warning ("Invalid PulseAudio volume value %" G_GINT64_FORMAT, volume);
        return FALSE;
    }
    return PULSE_STREAM_GET_CLASS (stream)->set_volume (stream, &cvolume);
}

static gdouble
stream_get_channel_volume_db (MateMixerStream *stream, guint channel)
{
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), 0.0);

    pstream = PULSE_STREAM (stream);

    if (channel >= pstream->priv->volume.channels) {
        g_warning ("Invalid channel %u of stream %s", channel, pstream->priv->name);
        return 0.0;
    }
    return pa_sw_volume_to_dB (pstream->priv->volume.values[channel]);
}

static gboolean
stream_set_channel_volume_db (MateMixerStream *stream,
                              guint            channel,
                              gdouble          volume_db)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return stream_set_channel_volume (stream,
                                      channel,
                                      pa_sw_volume_from_dB (volume_db));
}

static gboolean
stream_has_position (MateMixerStream *stream, MateMixerChannelPosition position)
{
    PulseStreamPrivate *priv;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    priv = PULSE_STREAM (stream)->priv;

    return pa_channel_map_has_position (&priv->channel_map,
                                        pulse_convert_position_to_pulse (position));
}

static gint64
stream_get_position_volume (MateMixerStream          *stream,
                            MateMixerChannelPosition  position)
{
    PulseStreamPrivate *priv;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), 0);

    priv = PULSE_STREAM (stream)->priv;

    return pa_cvolume_get_position (&priv->volume,
                                    &priv->channel_map,
                                    pulse_convert_position_to_pulse (position));
}

static gboolean
stream_set_position_volume (MateMixerStream          *stream,
                            MateMixerChannelPosition  position,
                            gint64                    volume)
{
    PulseStreamPrivate *priv;
    pa_cvolume cvolume;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    priv = PULSE_STREAM (stream)->priv;
    cvolume = priv->volume;

    if (!pa_cvolume_set_position (&cvolume,
                                  &priv->channel_map,
                                  pulse_convert_position_to_pulse (position),
                                  (pa_volume_t) volume)) {
        return FALSE;
    }
    return PULSE_STREAM_GET_CLASS (stream)->set_volume (stream, &cvolume);
}

static gdouble
stream_get_position_volume_db (MateMixerStream          *stream,
                               MateMixerChannelPosition  position)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), 0.0);

    return pa_sw_volume_to_dB (stream_get_position_volume (stream, position));
}

static gboolean
stream_set_position_volume_db (MateMixerStream          *stream,
                               MateMixerChannelPosition  position,
                               gdouble                   volume_db)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return stream_set_position_volume (stream, position, pa_sw_volume_from_dB (volume_db));
}

static gdouble
stream_get_balance (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), 0.0);

    return PULSE_STREAM (stream)->priv->balance;
}

static gboolean
stream_set_balance (MateMixerStream *stream, gdouble balance)
{
    PulseStream *pstream;
    pa_cvolume            cvolume;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);
    cvolume = pstream->priv->volume;

    if (balance == pstream->priv->balance)
        return TRUE;

    if (pa_cvolume_set_balance (&cvolume,
                                &pstream->priv->channel_map,
                                (float) balance) == NULL) {
        return FALSE;
    }
    return PULSE_STREAM_GET_CLASS (stream)->set_volume (stream, &cvolume);
}

static gdouble
stream_get_fade (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return PULSE_STREAM (stream)->priv->fade;
}

static gboolean
stream_set_fade (MateMixerStream *stream, gdouble fade)
{
    pa_cvolume   cvolume;
    PulseStream *pstream;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pstream = PULSE_STREAM (stream);
    cvolume = pstream->priv->volume;

    if (fade == pstream->priv->fade)
        return TRUE;

    if (pa_cvolume_set_fade (&cvolume,
                             &pstream->priv->channel_map,
                             (float) fade) == NULL) {
        return FALSE;
    }
    return PULSE_STREAM_GET_CLASS (stream)->set_volume (stream, &cvolume);
}

static gboolean
stream_suspend (MateMixerStream *stream)
{
    // TODO
    return TRUE;
}

static gboolean
stream_resume (MateMixerStream *stream)
{
    // TODO
    return TRUE;
}

static const GList *
stream_list_ports (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return (const GList *) PULSE_STREAM (stream)->priv->ports;
}

static MateMixerPort *
stream_get_active_port (MateMixerStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    return PULSE_STREAM (stream)->priv->port;
}

static gboolean
stream_set_active_port (MateMixerStream *stream, const gchar *port_name)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);
    g_return_val_if_fail (port_name != NULL, FALSE);

    return PULSE_STREAM_GET_CLASS (stream)->set_active_port (stream, port_name);
}

static gint64
stream_get_min_volume (MateMixerStream *stream)
{
    return (gint64) PA_VOLUME_MUTED;
}

static gint64
stream_get_max_volume (MateMixerStream *stream)
{
    return (gint64) PA_VOLUME_UI_MAX;
}

static gint64
stream_get_normal_volume (MateMixerStream *stream)
{
    return (gint64) PA_VOLUME_NORM;
}
