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
#include "matemixer-enums.h"
#include "matemixer-enum-types.h"
#include "matemixer-stream.h"

enum {
    DEVICE_ADDED,
    DEVICE_CHANGED,
    DEVICE_REMOVED,
    STREAM_ADDED,
    STREAM_CHANGED,
    STREAM_REMOVED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

G_DEFINE_INTERFACE (MateMixerBackend, mate_mixer_backend, G_TYPE_OBJECT)

static void
mate_mixer_backend_default_init (MateMixerBackendInterface *iface)
{
    g_object_interface_install_property (iface,
                                         g_param_spec_enum ("state",
                                                            "State",
                                                            "Backend connection state",
                                                            MATE_MIXER_TYPE_STATE,
                                                            MATE_MIXER_STATE_IDLE,
                                                            G_PARAM_READABLE |
                                                            G_PARAM_STATIC_STRINGS));

    signals[DEVICE_ADDED] = g_signal_new ("device-added",
                                          G_TYPE_FROM_INTERFACE (iface),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (MateMixerBackendInterface, device_added),
                                          NULL,
                                          NULL,
                                          g_cclosure_marshal_VOID__STRING,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_STRING);

    signals[DEVICE_CHANGED] = g_signal_new ("device-changed",
                                            G_TYPE_FROM_INTERFACE (iface),
                                            G_SIGNAL_RUN_LAST,
                                            G_STRUCT_OFFSET (MateMixerBackendInterface, device_changed),
                                            NULL,
                                            NULL,
                                            g_cclosure_marshal_VOID__STRING,
                                            G_TYPE_NONE,
                                            1,
                                            G_TYPE_STRING);

    signals[DEVICE_REMOVED] = g_signal_new ("device-removed",
                                            G_TYPE_FROM_INTERFACE (iface),
                                            G_SIGNAL_RUN_LAST,
                                            G_STRUCT_OFFSET (MateMixerBackendInterface, device_removed),
                                            NULL,
                                            NULL,
                                            g_cclosure_marshal_VOID__STRING,
                                            G_TYPE_NONE,
                                            1,
                                            G_TYPE_STRING);

    signals[STREAM_ADDED] = g_signal_new ("stream-added",
                                          G_TYPE_FROM_INTERFACE (iface),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (MateMixerBackendInterface, stream_added),
                                          NULL,
                                          NULL,
                                          g_cclosure_marshal_VOID__STRING,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_STRING);

    signals[STREAM_CHANGED] = g_signal_new ("stream-changed",
                                            G_TYPE_FROM_INTERFACE (iface),
                                            G_SIGNAL_RUN_LAST,
                                            G_STRUCT_OFFSET (MateMixerBackendInterface, stream_changed),
                                            NULL,
                                            NULL,
                                            g_cclosure_marshal_VOID__STRING,
                                            G_TYPE_NONE,
                                            1,
                                            G_TYPE_STRING);

    signals[STREAM_REMOVED] = g_signal_new ("stream-removed",
                                            G_TYPE_FROM_INTERFACE (iface),
                                            G_SIGNAL_RUN_LAST,
                                            G_STRUCT_OFFSET (MateMixerBackendInterface, stream_removed),
                                            NULL,
                                            NULL,
                                            g_cclosure_marshal_VOID__STRING,
                                            G_TYPE_NONE,
                                            1,
                                            G_TYPE_STRING);
}

void
mate_mixer_backend_set_data (MateMixerBackend *backend, const MateMixerBackendData *data)
{
    MateMixerBackendInterface *iface;

    g_return_if_fail (MATE_MIXER_IS_BACKEND (backend));

    iface = MATE_MIXER_BACKEND_GET_INTERFACE (backend);

    if (iface->set_data)
        iface->set_data (backend, data);
}

/*
 * Required behaviour:
 * if the function returns TRUE, the state must be either MATE_MIXER_STATE_READY or
 * MATE_MIXER_STATE_CONNECTING.
 */
gboolean
mate_mixer_backend_open (MateMixerBackend *backend)
{
    MateMixerBackendInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), FALSE);

    iface = MATE_MIXER_BACKEND_GET_INTERFACE (backend);

    if (!iface->open) {
        g_critical ("Backend module does not implement the open() method");
        return FALSE;
    }
    return iface->open (backend);
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

MateMixerState
mate_mixer_backend_get_state (MateMixerBackend *backend)
{
    MateMixerBackendInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), MATE_MIXER_STATE_UNKNOWN);

    iface = MATE_MIXER_BACKEND_GET_INTERFACE (backend);

    if (!iface->get_state) {
        g_critical ("Backend module does not implement the get_state() method");
        return MATE_MIXER_STATE_UNKNOWN;
    }
    return iface->get_state (backend);
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

gboolean
mate_mixer_backend_set_default_input_stream (MateMixerBackend *backend,
                                             MateMixerStream  *stream)
{
    MateMixerBackendInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), FALSE);

    iface = MATE_MIXER_BACKEND_GET_INTERFACE (backend);

    if (iface->set_default_input_stream)
        return iface->set_default_input_stream (backend, stream);

    return FALSE;
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

gboolean
mate_mixer_backend_set_default_output_stream (MateMixerBackend *backend,
                                              MateMixerStream  *stream)
{
    MateMixerBackendInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), FALSE);

    iface = MATE_MIXER_BACKEND_GET_INTERFACE (backend);

    if (iface->set_default_output_stream)
        return iface->set_default_output_stream (backend, stream);

    return FALSE;
}
