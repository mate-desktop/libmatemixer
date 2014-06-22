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

#include "pulse-client-stream.h"

struct _PulseClientStreamPrivate
{
    gchar           *app_name;
    gchar           *app_id;
    gchar           *app_version;
    gchar           *app_icon;
    MateMixerStream *parent;
};

enum
{
    PROP_0,
    PROP_PARENT,
    PROP_APP_NAME,
    PROP_APP_ID,
    PROP_APP_VERSION,
    PROP_APP_ICON,
    N_PROPERTIES
};

static void mate_mixer_client_stream_interface_init (MateMixerClientStreamInterface *iface);

static void pulse_client_stream_class_init   (PulseClientStreamClass *klass);

static void pulse_client_stream_get_property (GObject                *object,
                                              guint                   param_id,
                                              GValue                 *value,
                                              GParamSpec             *pspec);

static void pulse_client_stream_init         (PulseClientStream      *client);
static void pulse_client_stream_dispose      (GObject                *object);
static void pulse_client_stream_finalize     (GObject                *object);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PulseClientStream, pulse_client_stream, PULSE_TYPE_STREAM,
                                  G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_CLIENT_STREAM,
                                                         mate_mixer_client_stream_interface_init))

static MateMixerStream *client_stream_get_parent      (MateMixerClientStream  *client);
static gboolean         client_stream_set_parent      (MateMixerClientStream  *client,
                                                       MateMixerStream        *parent);
static gboolean         client_stream_remove          (MateMixerClientStream  *client);

static const gchar *    client_stream_get_app_name    (MateMixerClientStream  *client);
static const gchar *    client_stream_get_app_id      (MateMixerClientStream  *client);
static const gchar *    client_stream_get_app_version (MateMixerClientStream  *client);
static const gchar *    client_stream_get_app_icon    (MateMixerClientStream  *client);

static void
mate_mixer_client_stream_interface_init (MateMixerClientStreamInterface *iface)
{
    iface->get_parent      = client_stream_get_parent;
    iface->set_parent      = client_stream_set_parent;
    iface->remove          = client_stream_remove;
    iface->get_app_name    = client_stream_get_app_name;
    iface->get_app_id      = client_stream_get_app_id;
    iface->get_app_version = client_stream_get_app_version;
    iface->get_app_icon    = client_stream_get_app_icon;
}

static void
pulse_client_stream_class_init (PulseClientStreamClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = pulse_client_stream_dispose;
    object_class->finalize     = pulse_client_stream_finalize;
    object_class->get_property = pulse_client_stream_get_property;

    g_object_class_override_property (object_class, PROP_PARENT, "parent");
    g_object_class_override_property (object_class, PROP_APP_NAME, "app-name");
    g_object_class_override_property (object_class, PROP_APP_ID, "app-id");
    g_object_class_override_property (object_class, PROP_APP_VERSION, "app-version");
    g_object_class_override_property (object_class, PROP_APP_ICON, "app-icon");

    g_type_class_add_private (object_class, sizeof (PulseClientStreamPrivate));
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
    case PROP_APP_NAME:
        g_value_set_string (value, client->priv->app_name);
        break;
    case PROP_APP_ID:
        g_value_set_string (value, client->priv->app_id);
        break;
    case PROP_APP_VERSION:
        g_value_set_string (value, client->priv->app_version);
        break;
    case PROP_APP_ICON:
        g_value_set_string (value, client->priv->app_icon);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
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

static void
pulse_client_stream_finalize (GObject *object)
{
    PulseClientStream *client;

    client = PULSE_CLIENT_STREAM (object);

    g_free (client->priv->app_name);
    g_free (client->priv->app_id);
    g_free (client->priv->app_version);
    g_free (client->priv->app_icon);

    G_OBJECT_CLASS (pulse_client_stream_parent_class)->finalize (object);
}

gboolean
pulse_client_stream_update_parent (PulseClientStream *client, MateMixerStream *parent)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), FALSE);

    if (client->priv->parent != parent) {
        g_clear_object (&client->priv->parent);

        if (G_LIKELY (parent != NULL))
            client->priv->parent = g_object_ref (parent);

        g_object_notify (G_OBJECT (client), "parent");
    }
    return TRUE;
}

gboolean
pulse_client_stream_update_app_name (PulseClientStream *client, const gchar *app_name)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), FALSE);

    if (g_strcmp0 (client->priv->app_name, app_name)) {
        g_free (client->priv->app_name);
        client->priv->app_name = g_strdup (app_name);

        g_object_notify (G_OBJECT (client), "app-name");
    }
    return TRUE;
}

gboolean
pulse_client_stream_update_app_id (PulseClientStream *client, const gchar *app_id)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), FALSE);

    if (g_strcmp0 (client->priv->app_id, app_id)) {
        g_free (client->priv->app_id);
        client->priv->app_id = g_strdup (app_id);

        g_object_notify (G_OBJECT (client), "app-id");
    }
    return TRUE;
}

gboolean
pulse_client_stream_update_app_version (PulseClientStream *client, const gchar *app_version)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), FALSE);

    if (g_strcmp0 (client->priv->app_version, app_version)) {
        g_free (client->priv->app_version);
        client->priv->app_version = g_strdup (app_version);

        g_object_notify (G_OBJECT (client), "app-version");
    }
    return TRUE;
}

gboolean
pulse_client_stream_update_app_icon (PulseClientStream *client, const gchar *app_icon)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), FALSE);

    if (g_strcmp0 (client->priv->app_icon, app_icon)) {
        g_free (client->priv->app_icon);
        client->priv->app_icon = g_strdup (app_icon);

        g_object_notify (G_OBJECT (client), "app-icon");
    }
    return TRUE;
}

static MateMixerStream *
client_stream_get_parent (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), NULL);

    return PULSE_CLIENT_STREAM (client)->priv->parent;
}

static gboolean
client_stream_set_parent (MateMixerClientStream *client, MateMixerStream *parent)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), FALSE);

    return PULSE_CLIENT_STREAM_GET_CLASS (client)->set_parent (client, parent);
}

static gboolean
client_stream_remove (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), FALSE);

    return PULSE_CLIENT_STREAM_GET_CLASS (client)->remove (client);
}

static const gchar *
client_stream_get_app_name (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), NULL);

    return PULSE_CLIENT_STREAM (client)->priv->app_name;
}

static const gchar *
client_stream_get_app_id (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), NULL);

    return PULSE_CLIENT_STREAM (client)->priv->app_id;
}

static const gchar *
client_stream_get_app_version (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), NULL);

    return PULSE_CLIENT_STREAM (client)->priv->app_version;
}

static const gchar *
client_stream_get_app_icon (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), NULL);

    return PULSE_CLIENT_STREAM (client)->priv->app_icon;
}
