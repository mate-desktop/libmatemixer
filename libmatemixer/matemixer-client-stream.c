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
#include "matemixer-stream.h"

/**
 * SECTION:matemixer-client-stream
 * @short_description: An interface providing extra functionality for client streams
 * @see_also: #MateMixerStream
 * @include: libmatemixer/matemixer.h
 *
 * #MateMixerClientStream represents a special kind of stream, which belongs
 * to a parent input or output stream.
 *
 * A typical example of a client stream is a stream provided by an application.
 */

G_DEFINE_INTERFACE (MateMixerClientStream, mate_mixer_client_stream, G_TYPE_OBJECT)

static void
mate_mixer_client_stream_default_init (MateMixerClientStreamInterface *iface)
{
    g_object_interface_install_property (iface,
                                         g_param_spec_object ("parent",
                                                              "Parent",
                                                              "Parent stream of the client stream",
                                                              MATE_MIXER_TYPE_STREAM,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_string ("app-name",
                                                              "App name",
                                                              "Name of the client stream application",
                                                              NULL,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_string ("app-id",
                                                              "App ID",
                                                              "Identifier of the client stream application",
                                                              NULL,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_string ("app-version",
                                                              "App version",
                                                              "Version of the client stream application",
                                                              NULL,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_string ("app-icon",
                                                              "App icon",
                                                              "Icon name of the client stream application",
                                                              NULL,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));
}

/**
 * mate_mixer_client_stream_get_parent:
 * @client: a #MateMixerClientStream
 *
 * Gets the parent stream of the client stream.
 *
 * Returns: a #MateMixerStream or %NULL on failure.
 */
MateMixerStream *
mate_mixer_client_stream_get_parent (MateMixerClientStream *client)
{
    MateMixerClientStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), NULL);

    iface = MATE_MIXER_CLIENT_STREAM_GET_INTERFACE (client);

    if (iface->get_parent)
        return iface->get_parent (client);

    return NULL;
}

/**
 * mate_mixer_client_stream_set_parent:
 * @client: a #MateMixerClientStream
 * @parent: a #MateMixerStream
 *
 * Changes the parent stream of the client stream. The parent stream must be a
 * non-client output stream.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_client_stream_set_parent (MateMixerClientStream *client, MateMixerStream *parent)
{
    MateMixerClientStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (parent), FALSE);

    iface = MATE_MIXER_CLIENT_STREAM_GET_INTERFACE (client);

    if (iface->set_parent)
        return iface->set_parent (client, parent);

    return FALSE;
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
    MateMixerClientStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), FALSE);

    iface = MATE_MIXER_CLIENT_STREAM_GET_INTERFACE (client);

    if (iface->remove)
        return iface->remove (client);

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
    MateMixerClientStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), NULL);

    iface = MATE_MIXER_CLIENT_STREAM_GET_INTERFACE (client);

    if (iface->get_app_name)
        return iface->get_app_name (client);

    return NULL;
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
    MateMixerClientStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), NULL);

    iface = MATE_MIXER_CLIENT_STREAM_GET_INTERFACE (client);

    if (iface->get_app_id)
        return iface->get_app_id (client);

    return NULL;
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
    MateMixerClientStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), NULL);

    iface = MATE_MIXER_CLIENT_STREAM_GET_INTERFACE (client);

    if (iface->get_app_version)
        return iface->get_app_version (client);

    return NULL;
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
    MateMixerClientStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_CLIENT_STREAM (client), NULL);

    iface = MATE_MIXER_CLIENT_STREAM_GET_INTERFACE (client);

    if (iface->get_app_icon)
        return iface->get_app_icon (client);

    return NULL;
}
