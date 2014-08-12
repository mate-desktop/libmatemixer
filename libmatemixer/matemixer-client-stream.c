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

#include "matemixer-client-stream.h"
#include "matemixer-enums.h"
#include "matemixer-enum-types.h"
#include "matemixer-stream.h"

/**
 * SECTION:matemixer-client-stream
 * @short_description: Interface providing extra functionality for client streams
 * @see_also: #MateMixerStream
 * @include: libmatemixer/matemixer.h
 *
 * #MateMixerClientStream represents a special kind of stream, which belongs
 * to a parent input or output stream.
 *
 * A typical example of a client stream is a stream provided by an application.
 */

struct _MateMixerClientStreamPrivate
{
    gchar                     *app_name;
    gchar                     *app_id;
    gchar                     *app_version;
    gchar                     *app_icon;
    MateMixerStream           *parent;
    MateMixerClientStreamFlags client_flags;
    MateMixerClientStreamRole  client_role;
};

enum {
    PROP_0,
    PROP_CLIENT_FLAGS,
    PROP_CLIENT_ROLE,
    PROP_PARENT,
    PROP_APP_NAME,
    PROP_APP_ID,
    PROP_APP_VERSION,
    PROP_APP_ICON,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void mate_mixer_client_stream_class_init   (MateMixerClientStreamClass *klass);

static void mate_mixer_client_stream_init         (MateMixerClientStream      *client);

static void mate_mixer_client_stream_get_property (GObject                    *object,
                                                   guint                       param_id,
                                                   GValue                     *value,
                                                   GParamSpec                 *pspec);
static void mate_mixer_client_stream_set_property (GObject                    *object,
                                                   guint                       param_id,
                                                   const GValue               *value,
                                                   GParamSpec                 *pspec);

static void mate_mixer_client_stream_dispose      (GObject                    *object);
static void mate_mixer_client_stream_finalize     (GObject                    *object);

G_DEFINE_ABSTRACT_TYPE (MateMixerClientStream, mate_mixer_client_stream, MATE_MIXER_TYPE_STREAM)

static void
mate_mixer_client_stream_class_init (MateMixerClientStreamClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = mate_mixer_client_stream_dispose;
    object_class->finalize     = mate_mixer_client_stream_finalize;
    object_class->get_property = mate_mixer_client_stream_get_property;
    object_class->set_property = mate_mixer_client_stream_set_property;

    properties[PROP_CLIENT_FLAGS] =
        g_param_spec_flags ("client-flags",
                            "Client flags",
                            "Capability flags of the client stream",
                            MATE_MIXER_TYPE_CLIENT_STREAM_FLAGS,
                            MATE_MIXER_CLIENT_STREAM_NO_FLAGS,
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_STRINGS);

    properties[PROP_CLIENT_ROLE] =
        g_param_spec_enum ("role",
                           "Role",
                           "Role of the client stream",
                           MATE_MIXER_TYPE_CLIENT_STREAM_ROLE,
                           MATE_MIXER_CLIENT_STREAM_ROLE_NONE,
                           G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS);

    properties[PROP_PARENT] =
        g_param_spec_object ("parent",
                             "Parent",
                             "Parent stream of the client stream",
                             MATE_MIXER_TYPE_STREAM,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_APP_NAME] =
        g_param_spec_string ("app-name",
                             "App name",
                             "Name of the client stream application",
                             NULL,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_APP_ID] =
        g_param_spec_string ("app-id",
                             "App ID",
                             "Identifier of the client stream application",
                             NULL,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_APP_VERSION] =
        g_param_spec_string ("app-version",
                             "App version",
                             "Version of the client stream application",
                             NULL,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_APP_ICON] =
        g_param_spec_string ("app-icon",
                             "App icon",
                             "Icon name of the client stream application",
                             NULL,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
mate_mixer_client_stream_init (MateMixerClientStream *client)
{
    client->priv = G_TYPE_INSTANCE_GET_PRIVATE (client,
                                                MATE_MIXER_TYPE_CLIENT_STREAM,
                                                MateMixerClientStreamPrivate);
}

static void
mate_mixer_client_stream_get_property (GObject    *object,
                                       guint       param_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
}

static void
mate_mixer_client_stream_set_property (GObject      *object,
                                       guint         param_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
}

static void
mate_mixer_client_stream_dispose (GObject *object)
{
    MateMixerClientStream *client;

    client = MATE_MIXER_CLIENT_STREAM (object);

    g_clear_object (&client->priv->parent);

    G_OBJECT_CLASS (mate_mixer_client_stream_parent_class)->dispose (object);
}

static void
mate_mixer_client_stream_finalize (GObject *object)
{
    MateMixerClientStream *client;

    client = MATE_MIXER_CLIENT_STREAM (object);

    g_free (client->priv->app_name);
    g_free (client->priv->app_id);
    g_free (client->priv->app_version);
    g_free (client->priv->app_icon);

    G_OBJECT_CLASS (mate_mixer_client_stream_parent_class)->finalize (object);
}

/**
 * mate_mixer_client_stream_get_flags:
 * @client: a #MateMixerClientStream
 *
 */
MateMixerClientStreamFlags
mate_mixer_client_stream_get_flags (MateMixerClientStream *client)
{
    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), MATE_MIXER_CLIENT_STREAM_NO_FLAGS);

    return client->priv->client_flags;
}

/**
 * mate_mixer_client_stream_get_role:
 * @client: a #MateMixerClientStream
 *
 */
MateMixerClientStreamRole
mate_mixer_client_stream_get_role (MateMixerClientStream *client)
{
    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), MATE_MIXER_CLIENT_STREAM_ROLE_NONE);

    return client->priv->client_role;
}

/**
 * mate_mixer_client_stream_get_parent:
 * @client: a #MateMixerClientStream
 *
 * Gets the parent stream of the client stream.
 *
 * Returns: a #MateMixerStream or %NULL if the parent stream is not known.
 */
MateMixerStream *
mate_mixer_client_stream_get_parent (MateMixerClientStream *client)
{
    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), NULL);

    return client->priv->parent;
}

/**
 * mate_mixer_client_stream_set_parent:
 * @client: a #MateMixerClientStream
 * @parent: a #MateMixerStream
 *
 * Changes the parent stream of the client stream. The parent stream must be a
 * non-client input or output stream.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_client_stream_set_parent (MateMixerClientStream *client, MateMixerStream *parent)
{
    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (parent), FALSE);

    if (client->priv->parent != parent) {
        MateMixerClientStreamClass *klass;

        klass = MATE_MIXER_CLIENT_STREAM_GET_CLASS (client);

        if (klass->set_parent == NULL ||
            klass->set_parent (client, parent) == FALSE)
            return FALSE;

        if (client->priv->parent != NULL)
            g_object_unref (client->priv->parent);

        client->priv->parent = g_object_ref (parent);
    }

    return TRUE;
}

/**
 * mate_mixer_client_stream_remove:
 * @client: a #MateMixerClientStream
 *
 * Removes the client stream.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_client_stream_remove (MateMixerClientStream *client)
{
    MateMixerClientStreamClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), FALSE);

    klass = MATE_MIXER_CLIENT_STREAM_GET_CLASS (client);

    if (klass->remove != NULL)
        return klass->remove (client);

    return FALSE;
}

/**
 * mate_mixer_client_stream_get_app_name:
 * @client: a #MateMixerClientStream
 *
 * Gets the name of the application in case the stream is an application
 * stream.
 *
 * Returns: a string on success, or %NULL if the stream is not an application
 * stream or if the application does not provide a name.
 */
const gchar *
mate_mixer_client_stream_get_app_name (MateMixerClientStream *client)
{
    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), NULL);

    return client->priv->app_name;
}

/**
 * mate_mixer_client_stream_get_app_id:
 * @client: a #MateMixerClientStream
 *
 * Gets the identifier (e.g. org.example.app) of the application in case the
 * stream is an application stream.
 *
 * Returns: a string on success, or %NULL if the stream is not an application
 * stream or if the application does not provide an identifier.
 */
const gchar *
mate_mixer_client_stream_get_app_id (MateMixerClientStream *client)
{
    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), NULL);

    return client->priv->app_id;
}

/**
 * mate_mixer_client_stream_get_app_version:
 * @client: a #MateMixerClientStream
 *
 * Gets the version of the application in case the stream is an application
 * stream.
 *
 * Returns: a string on success, or %NULL if the stream is not an application
 * stream or if the application does not provide a version string.
 */
const gchar *
mate_mixer_client_stream_get_app_version (MateMixerClientStream *client)
{
    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), NULL);

    return client->priv->app_version;
}

/**
 * mate_mixer_client_stream_get_app_icon:
 * @client: a #MateMixerClientStream
 *
 * Gets the XDG icon name of the application in case the stream is an
 * application stream.
 *
 * Returns: a string on success, or %NULL if the stream is not an application
 * stream or if the application does not provide an icon name.
 */
const gchar *
mate_mixer_client_stream_get_app_icon (MateMixerClientStream *client)
{
    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), NULL);

    return client->priv->app_icon;
}
