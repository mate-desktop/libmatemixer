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

#include <libmatemixer/matemixer-stream.h>
#include <libmatemixer/matemixer-port.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-stream.h"

struct _MateMixerPulseStreamPrivate
{
    guint32                   index;
    gchar                    *name;
    gchar                    *description;
    gchar                    *icon;
    guint                     channels;
    gboolean                  mute;
    GList                    *ports;
    MateMixerPort            *port;
    MateMixerPulseConnection *connection;
};

enum
{
    PROP_0,
    PROP_INDEX,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_ICON,
    PROP_CHANNELS,
    PROP_VOLUME,
    PROP_MUTE,
    N_PROPERTIES
};

static void mate_mixer_stream_interface_init   (MateMixerStreamInterface  *iface);
static void mate_mixer_pulse_stream_class_init (MateMixerPulseStreamClass *klass);
static void mate_mixer_pulse_stream_init       (MateMixerPulseStream      *stream);
static void mate_mixer_pulse_stream_dispose    (GObject                   *object);
static void mate_mixer_pulse_stream_finalize   (GObject                   *object);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (MateMixerPulseStream, mate_mixer_pulse_stream, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_STREAM,
                                                         mate_mixer_stream_interface_init))

static void
mate_mixer_stream_interface_init (MateMixerStreamInterface *iface)
{
    iface->get_name = mate_mixer_pulse_stream_get_name;
    iface->get_description = mate_mixer_pulse_stream_get_description;
    iface->get_icon = mate_mixer_pulse_stream_get_icon;
    iface->list_ports = mate_mixer_pulse_stream_list_ports;
}

static void
mate_mixer_pulse_stream_get_property (GObject     *object,
                                      guint        param_id,
                                      GValue      *value,
                                      GParamSpec  *pspec)
{
    MateMixerPulseStream *stream;

    stream = MATE_MIXER_PULSE_STREAM (object);

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
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_pulse_stream_set_property (GObject       *object,
                                      guint          param_id,
                                      const GValue  *value,
                                      GParamSpec    *pspec)
{
    MateMixerPulseStream *stream;

    stream = MATE_MIXER_PULSE_STREAM (object);

    switch (param_id) {
    case PROP_NAME:
        stream->priv->name = g_strdup (g_value_get_string (value));
        break;
    case PROP_DESCRIPTION:
        stream->priv->description = g_strdup (g_value_get_string (value));
        break;
    case PROP_ICON:
        stream->priv->icon = g_strdup (g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_pulse_stream_class_init (MateMixerPulseStreamClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = mate_mixer_pulse_stream_dispose;
    object_class->finalize     = mate_mixer_pulse_stream_finalize;
    object_class->get_property = mate_mixer_pulse_stream_get_property;
    object_class->set_property = mate_mixer_pulse_stream_set_property;

    g_object_class_override_property (object_class, PROP_NAME, "name");
    g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (object_class, PROP_ICON, "icon");

    g_type_class_add_private (object_class, sizeof (MateMixerPulseStreamPrivate));
}

static void
mate_mixer_pulse_stream_init (MateMixerPulseStream *stream)
{
    stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (
        stream,
        MATE_MIXER_TYPE_PULSE_STREAM,
        MateMixerPulseStreamPrivate);
}

static void
mate_mixer_pulse_stream_dispose (GObject *object)
{
    MateMixerPulseStream *stream;

    stream = MATE_MIXER_PULSE_STREAM (object);

    if (stream->priv->ports) {
        g_list_free_full (stream->priv->ports, g_object_unref);
        stream->priv->ports = NULL;
    }
    g_clear_object (&stream->priv->connection);

    G_OBJECT_CLASS (mate_mixer_pulse_stream_parent_class)->dispose (object);
}

static void
mate_mixer_pulse_stream_finalize (GObject *object)
{
    MateMixerPulseStream *stream;

    stream = MATE_MIXER_PULSE_STREAM (object);

    g_free (stream->priv->name);
    g_free (stream->priv->description);
    g_free (stream->priv->icon);

    G_OBJECT_CLASS (mate_mixer_pulse_stream_parent_class)->finalize (object);
}

MateMixerPulseConnection *
mate_mixer_pulse_stream_get_connection (MateMixerPulseStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_STREAM (stream), NULL);

    return stream->priv->connection;
}

guint32
mate_mixer_pulse_stream_get_index (MateMixerPulseStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_STREAM (stream), FALSE);

    return stream->priv->index;
}

const gchar *
mate_mixer_pulse_stream_get_name (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_STREAM (stream), FALSE);

    return MATE_MIXER_PULSE_STREAM (stream)->priv->name;
}

const gchar *
mate_mixer_pulse_stream_get_description (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_STREAM (stream), FALSE);

    return MATE_MIXER_PULSE_STREAM (stream)->priv->description;
}

const gchar *
mate_mixer_pulse_stream_get_icon (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_STREAM (stream), FALSE);

    return MATE_MIXER_PULSE_STREAM (stream)->priv->icon;
}

MateMixerPort *
mate_mixer_pulse_stream_get_active_port (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_STREAM (stream), FALSE);

    return MATE_MIXER_PULSE_STREAM (stream)->priv->port;
}

const GList *
mate_mixer_pulse_stream_list_ports (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_STREAM (stream), FALSE);

    return MATE_MIXER_PULSE_STREAM (stream)->priv->ports;
}

gboolean
mate_mixer_pulse_stream_set_volume (MateMixerStream *stream, guint32 volume)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_STREAM (stream), FALSE);

    return MATE_MIXER_PULSE_STREAM_GET_CLASS (stream)->set_volume (stream, volume);
}

gboolean
mate_mixer_pulse_stream_set_mute (MateMixerStream *stream, gboolean mute)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_STREAM (stream), FALSE);

    return MATE_MIXER_PULSE_STREAM_GET_CLASS (stream)->set_mute (stream, mute);
}

gboolean
mate_mixer_pulse_stream_set_active_port (MateMixerStream *stream, MateMixerPort *port)
{
    return TRUE;
}
