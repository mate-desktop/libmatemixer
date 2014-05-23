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

G_DEFINE_INTERFACE (MateMixerBackend, mate_mixer_backend, G_TYPE_OBJECT)

static void
mate_mixer_backend_default_init (MateMixerBackendInterface *iface)
{
}

gboolean
mate_mixer_backend_open (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    return MATE_MIXER_BACKEND_GET_INTERFACE (backend)->open (backend);
}

void
mate_mixer_backend_close (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    MATE_MIXER_BACKEND_GET_INTERFACE (backend)->close (backend);
}

const GList *
mate_mixer_backend_list_devices (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    return MATE_MIXER_BACKEND_GET_INTERFACE (backend)->list_devices (backend);
}

const GList *
mate_mixer_backend_list_tracks (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    return MATE_MIXER_BACKEND_GET_INTERFACE (backend)->list_tracks (backend);
}
