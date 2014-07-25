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
    DEVICE_REMOVED,
    STREAM_ADDED,
    STREAM_REMOVED,
    CACHED_STREAM_ADDED,
    CACHED_STREAM_REMOVED,
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
                                                            "Current backend connection state",
                                                            MATE_MIXER_TYPE_STATE,
                                                            MATE_MIXER_STATE_IDLE,
                                                            G_PARAM_READABLE |
                                                            G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_object ("default-input",
                                                              "Default input",
                                                              "Default input stream",
                                                              MATE_MIXER_TYPE_STREAM,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_object ("default-output",
                                                              "Default output",
                                                              "Default output stream",
                                                              MATE_MIXER_TYPE_STREAM,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    signals[DEVICE_ADDED] =
        g_signal_new ("device-added",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerBackendInterface, device_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    signals[DEVICE_REMOVED] =
        g_signal_new ("device-removed",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerBackendInterface, device_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    signals[STREAM_ADDED] =
        g_signal_new ("stream-added",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerBackendInterface, stream_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    signals[STREAM_REMOVED] =
        g_signal_new ("stream-removed",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerBackendInterface, stream_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    signals[CACHED_STREAM_ADDED] =
        g_signal_new ("cached-stream-added",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerBackendInterface, cached_stream_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    signals[CACHED_STREAM_REMOVED] =
        g_signal_new ("cached-stream-removed",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerBackendInterface, cached_stream_removed),
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

gboolean
mate_mixer_backend_open (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), FALSE);

    /* Implementation required */
    return MATE_MIXER_BACKEND_GET_INTERFACE (backend)->open (backend);
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
    MateMixerState state = MATE_MIXER_STATE_UNKNOWN;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), MATE_MIXER_STATE_UNKNOWN);

    g_object_get (G_OBJECT (backend),
                  "state", &state,
                  NULL);

    return state;
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

GList *
mate_mixer_backend_list_cached_streams (MateMixerBackend *backend)
{
    MateMixerBackendInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    iface = MATE_MIXER_BACKEND_GET_INTERFACE (backend);

    if (iface->list_cached_streams)
        return iface->list_cached_streams (backend);

    return NULL;
}

MateMixerStream *
mate_mixer_backend_get_default_input_stream (MateMixerBackend *backend)
{
    MateMixerStream *stream = NULL;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    g_object_get (G_OBJECT (stream),
                  "default-input", &stream,
                  NULL);

    if (stream != NULL)
        g_object_unref (stream);

    return stream;
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
    MateMixerStream *stream = NULL;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    g_object_get (G_OBJECT (stream),
                  "default-output", &stream,
                  NULL);

    if (stream != NULL)
        g_object_unref (stream);

    return stream;
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
