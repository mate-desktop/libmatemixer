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

#include "matemixer-backend.h"
#include "matemixer-stream.h"

G_DEFINE_INTERFACE (MateMixerBackend, mate_mixer_backend, G_TYPE_OBJECT)

static void
mate_mixer_backend_default_init (MateMixerBackendInterface *iface)
{
}

gboolean
mate_mixer_backend_open (MateMixerBackend *backend)
{
    MateMixerBackendInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), FALSE);

    iface = MATE_MIXER_BACKEND_GET_INTERFACE (backend);

    if (iface->open)
        return iface->open (backend);

    return FALSE;
}

void
mate_mixer_backend_close (MateMixerBackend *backend)
{
    MateMixerBackendInterface *iface;

    g_return_if_fail (MATE_MIXER_IS_BACKEND (backend));

    iface = MATE_MIXER_BACKEND_GET_INTERFACE (backend);

    if (iface->close)
        iface->close (backend);
}

GList *
mate_mixer_backend_list_devices (MateMixerBackend *backend)
{
    MateMixerBackendInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    iface = MATE_MIXER_BACKEND_GET_INTERFACE (backend);

    if (iface->list_devices)
        return iface->list_devices (backend);

    return NULL;
}

GList *
mate_mixer_backend_list_streams (MateMixerBackend *backend)
{
    MateMixerBackendInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    iface = MATE_MIXER_BACKEND_GET_INTERFACE (backend);

    if (iface->list_streams)
        return iface->list_streams (backend);

    return NULL;
}

MateMixerStream *
mate_mixer_backend_get_default_input_stream (MateMixerBackend *backend)
{
    MateMixerBackendInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    iface = MATE_MIXER_BACKEND_GET_INTERFACE (backend);

    if (iface->get_default_input_stream)
        return iface->get_default_input_stream (backend);

    return NULL;
}

MateMixerStream *
mate_mixer_backend_get_default_output_stream (MateMixerBackend *backend)
{
    MateMixerBackendInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    iface = MATE_MIXER_BACKEND_GET_INTERFACE (backend);

    if (iface->get_default_output_stream)
        return iface->get_default_output_stream (backend);

    return NULL;
}
