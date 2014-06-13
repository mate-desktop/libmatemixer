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

G_DEFINE_INTERFACE (MateMixerClientStream, mate_mixer_client_stream, G_TYPE_OBJECT)

static void
mate_mixer_client_stream_default_init (MateMixerClientStreamInterface *iface)
{
    g_object_interface_install_property (iface,
                                         g_param_spec_object ("parent",
                                                              "Parent",
                                                              "Parent stream of the client stream",
                                                              MATE_MIXER_TYPE_STREAM,
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
}

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
