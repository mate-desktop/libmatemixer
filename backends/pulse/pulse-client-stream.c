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

#include <pulse/pulseaudio.h>

#include "pulse-client-stream.h"
#include "pulse-stream.h"

struct _PulseClientStreamPrivate
{
    MateMixerStream *parent;
};

enum
{
    PROP_0,
    PROP_PARENT,
    N_PROPERTIES
};

static void mate_mixer_client_stream_interface_init (MateMixerClientStreamInterface  *iface);
static void pulse_client_stream_class_init          (PulseClientStreamClass          *klass);
static void pulse_client_stream_init                (PulseClientStream               *client);
static void pulse_client_stream_dispose             (GObject                         *object);

/* Interface implementation */
static MateMixerStream *stream_client_get_parent (MateMixerClientStream *client);
static gboolean         stream_client_set_parent (MateMixerClientStream *client,
                                                  MateMixerStream       *parent);
static gboolean         stream_client_remove     (MateMixerClientStream *client);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PulseClientStream, pulse_client_stream, PULSE_TYPE_STREAM,
                                  G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_CLIENT_STREAM,
                                                         mate_mixer_client_stream_interface_init))

static void
mate_mixer_client_stream_interface_init (MateMixerClientStreamInterface *iface)
{
    iface->get_parent = stream_client_get_parent;
    iface->set_parent = stream_client_set_parent;
    iface->remove     = stream_client_remove;
}

static void
pulse_client_stream_get_property (GObject    *object,
                                  guint       param_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
    PulseClientStream *client;

    client = PULSE_CLIENT_STREAM (object);

    switch (param_id) {
    case PROP_PARENT:
        g_value_set_object (value, client->priv->parent);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_client_stream_class_init (PulseClientStreamClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = pulse_client_stream_dispose;
    object_class->get_property = pulse_client_stream_get_property;

    g_object_class_install_property (object_class,
                                     PROP_PARENT,
                                     g_param_spec_object ("parent",
                                                          "Parent",
                                                          "Parent stream of the client stream",
                                                          MATE_MIXER_TYPE_STREAM,
                                                          G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));

    g_type_class_add_private (object_class, sizeof (PulseClientStreamPrivate));
}

static void
pulse_client_stream_init (PulseClientStream *client)
{
    client->priv = G_TYPE_INSTANCE_GET_PRIVATE (client,
                                                PULSE_TYPE_CLIENT_STREAM,
                                                PulseClientStreamPrivate);
}

static void
pulse_client_stream_dispose (GObject *object)
{
    PulseClientStream *client;

    client = PULSE_CLIENT_STREAM (object);

    g_clear_object (&client->priv->parent);

    G_OBJECT_CLASS (pulse_client_stream_parent_class)->dispose (object);
}

static MateMixerStream *
stream_client_get_parent (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), NULL);

    return PULSE_CLIENT_STREAM (client)->priv->parent;
}

static gboolean
stream_client_set_parent (MateMixerClientStream *client, MateMixerStream *parent)
{
    // TODO
    return TRUE;
}

static gboolean
stream_client_remove (MateMixerClientStream *client)
{
    // TODO
    return TRUE;
}
