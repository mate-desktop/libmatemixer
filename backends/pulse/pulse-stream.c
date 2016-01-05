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
#include "pulse-device.h"
#include "pulse-helpers.h"
#include "pulse-monitor.h"
#include "pulse-port.h"
#include "pulse-stream.h"

struct _PulseStreamPrivate
{
    guint32          index;
    PulseConnection *connection;
};

enum {
    PROP_0,
    PROP_INDEX,
    PROP_CONNECTION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void pulse_stream_class_init   (PulseStreamClass *klass);

static void pulse_stream_get_property (GObject          *object,
                                       guint             param_id,
                                       GValue           *value,
                                       GParamSpec       *pspec);
static void pulse_stream_set_property (GObject          *object,
                                       guint             param_id,
                                       const GValue     *value,
                                       GParamSpec       *pspec);

static void pulse_stream_init         (PulseStream      *stream);
static void pulse_stream_dispose      (GObject          *object);

G_DEFINE_ABSTRACT_TYPE (PulseStream, pulse_stream, MATE_MIXER_TYPE_STREAM)

static void
pulse_stream_class_init (PulseStreamClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = pulse_stream_dispose;
    object_class->get_property = pulse_stream_get_property;
    object_class->set_property = pulse_stream_set_property;

    properties[PROP_INDEX] =
        g_param_spec_uint ("index",
                           "Index",
                           "Index of the stream",
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

    g_type_class_add_private (object_class, sizeof (PulseStreamPrivate));
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
    case PROP_INDEX:
        stream->priv->index = g_value_get_uint (value);
        break;
    case PROP_CONNECTION:
        stream->priv->connection = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
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

    g_clear_object (&stream->priv->connection);

    G_OBJECT_CLASS (pulse_stream_parent_class)->dispose (object);
}

guint32
pulse_stream_get_index (PulseStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), PA_INVALID_INDEX);

    return stream->priv->index;
}

PulseConnection *
pulse_stream_get_connection (PulseStream *stream)
{
    g_return_val_if_fail (PULSE_IS_STREAM (stream), NULL);

    return stream->priv->connection;
}

PulseDevice *
pulse_stream_get_device (PulseStream *stream)
{
    MateMixerDevice *device;

    g_return_val_if_fail (PULSE_IS_STREAM (stream), NULL);

    device = mate_mixer_stream_get_device (MATE_MIXER_STREAM (stream));
    if (device != NULL)
        return PULSE_DEVICE (device);

    return NULL;
}
