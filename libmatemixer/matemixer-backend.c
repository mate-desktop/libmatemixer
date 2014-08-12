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

#include <string.h>
#include <glib.h>
#include <glib-object.h>

#include "matemixer-backend.h"
#include "matemixer-backend-private.h"
#include "matemixer-enums.h"
#include "matemixer-enum-types.h"
#include "matemixer-stream.h"

struct _MateMixerBackendPrivate
{
    GList                *devices;
    GList                *streams;
    GList                *stored_streams;
    MateMixerStream      *default_input;
    MateMixerStream      *default_output;
    MateMixerState        state;
    MateMixerBackendFlags flags;
};

enum {
    PROP_0,
    PROP_STATE,
    PROP_DEFAULT_INPUT_STREAM,
    PROP_DEFAULT_OUTPUT_STREAM,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
    DEVICE_ADDED,
    DEVICE_REMOVED,
    STREAM_ADDED,
    STREAM_REMOVED,
    STORED_STREAM_ADDED,
    STORED_STREAM_REMOVED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void mate_mixer_backend_class_init   (MateMixerBackendClass *klass);

static void mate_mixer_backend_init         (MateMixerBackend      *backend);

static void mate_mixer_backend_get_property (GObject               *object,
                                             guint                  param_id,
                                             GValue                *value,
                                             GParamSpec            *pspec);
static void mate_mixer_backend_set_property (GObject               *object,
                                             guint                  param_id,
                                             const GValue          *value,
                                             GParamSpec            *pspec);

static void mate_mixer_backend_dispose      (GObject               *object);

G_DEFINE_ABSTRACT_TYPE (MateMixerBackend, mate_mixer_backend, G_TYPE_OBJECT)

static void device_added (MateMixerBackend *backend, const gchar *name);
static void device_removed (MateMixerBackend *backend, const gchar *name);

static void device_stream_added   (MateMixerDevice *device,
                                   const gchar     *name);
static void device_stream_removed (MateMixerDevice *device,
                                   const gchar     *name);

static void free_devices        (MateMixerBackend *backend);
static void free_streams        (MateMixerBackend *backend);
static void free_stored_streams (MateMixerBackend *backend);
static void stream_removed      (MateMixerBackend *backend,
                                 const gchar      *name);

static void
mate_mixer_backend_class_init (MateMixerBackendClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = mate_mixer_backend_dispose;
    object_class->get_property = mate_mixer_backend_get_property;
    object_class->set_property = mate_mixer_backend_set_property;

    properties[PROP_STATE] =
        g_param_spec_enum ("state",
                           "State",
                           "Current backend connection state",
                           MATE_MIXER_TYPE_STATE,
                           MATE_MIXER_STATE_IDLE,
                           G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS);

    properties[PROP_DEFAULT_INPUT_STREAM] =
        g_param_spec_object ("default-input-stream",
                             "Default input stream",
                             "Default input stream",
                             MATE_MIXER_TYPE_STREAM,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_DEFAULT_OUTPUT_STREAM] =
        g_param_spec_object ("default-output-stream",
                             "Default output stream",
                             "Default output stream",
                             MATE_MIXER_TYPE_STREAM,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    signals[DEVICE_ADDED] =
        g_signal_new ("device-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerBackendClass, device_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    signals[DEVICE_REMOVED] =
        g_signal_new ("device-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerBackendClass, device_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    signals[STREAM_ADDED] =
        g_signal_new ("stream-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerBackendClass, stream_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    signals[STREAM_REMOVED] =
        g_signal_new ("stream-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerBackendClass, stream_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    signals[STORED_STREAM_ADDED] =
        g_signal_new ("stored-stream-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerBackendClass, stored_stream_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    signals[STORED_STREAM_REMOVED] =
        g_signal_new ("stored-stream-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerBackendClass, stored_stream_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    g_type_class_add_private (object_class, sizeof (MateMixerBackendPrivate));
}

static void
mate_mixer_backend_get_property (GObject    *object,
                                 guint       param_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    MateMixerBackend *backend;

    backend = MATE_MIXER_BACKEND (object);

    switch (param_id) {
    case PROP_STATE:
        g_value_set_enum (value, backend->priv->state);
        break;

    case PROP_DEFAULT_INPUT_STREAM:
        g_value_set_object (value, backend->priv->default_input);
        break;

    case PROP_DEFAULT_OUTPUT_STREAM:
        g_value_set_object (value, backend->priv->default_output);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_backend_set_property (GObject      *object,
                                 guint         param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    MateMixerBackend *backend;

    backend = MATE_MIXER_BACKEND (object);

    switch (param_id) {
    case PROP_DEFAULT_INPUT_STREAM:
        mate_mixer_backend_set_default_input_stream (backend, g_value_get_object (value));
        break;

    case PROP_DEFAULT_OUTPUT_STREAM:
        mate_mixer_backend_set_default_output_stream (backend, g_value_get_object (value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_backend_init (MateMixerBackend *backend)
{
    backend->priv = G_TYPE_INSTANCE_GET_PRIVATE (backend,
                                                 MATE_MIXER_TYPE_BACKEND,
                                                 MateMixerBackendPrivate);

    g_signal_connect (G_OBJECT (backend),
                      "device-added",
                      G_CALLBACK (free_devices),
                      NULL);
    g_signal_connect (G_OBJECT (backend),
                      "device-added",
                      G_CALLBACK (device_added),
                      NULL);

    g_signal_connect (G_OBJECT (backend),
                      "device-removed",
                      G_CALLBACK (free_devices),
                      NULL);
    g_signal_connect (G_OBJECT (backend),
                      "device-removed",
                      G_CALLBACK (device_removed),
                      NULL);

    g_signal_connect (G_OBJECT (backend),
                      "stream-added",
                      G_CALLBACK (free_streams),
                      NULL);
    g_signal_connect (G_OBJECT (backend),
                      "stream-removed",
                      G_CALLBACK (free_streams),
                      NULL);

    g_signal_connect (G_OBJECT (backend),
                      "stored-stream-added",
                      G_CALLBACK (free_stored_streams),
                      NULL);
    g_signal_connect (G_OBJECT (backend),
                      "stored-stream-removed",
                      G_CALLBACK (free_stored_streams),
                      NULL);

    // XXX also free when changing state
}

static void
mate_mixer_backend_dispose (GObject *object)
{
    MateMixerBackend *backend;

    backend = MATE_MIXER_BACKEND (object);

    free_devices (backend);
    free_streams (backend);
    free_stored_streams (backend);

    g_clear_object (&backend->priv->default_input);
    g_clear_object (&backend->priv->default_output);

    G_OBJECT_CLASS (mate_mixer_backend_parent_class)->dispose (object);
}

void
mate_mixer_backend_set_data (MateMixerBackend *backend, MateMixerBackendData *data)
{
    MateMixerBackendClass *klass;

    g_return_if_fail (MATE_MIXER_IS_BACKEND (backend));

    klass = MATE_MIXER_BACKEND_GET_CLASS (backend);

    if (klass->set_data != NULL)
        klass->set_data (backend, data);
}

gboolean
mate_mixer_backend_open (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), FALSE);

    /* Implementation required */
    return MATE_MIXER_BACKEND_GET_CLASS (backend)->open (backend);
}

void
mate_mixer_backend_close (MateMixerBackend *backend)
{
    MateMixerBackendClass *klass;

    g_return_if_fail (MATE_MIXER_IS_BACKEND (backend));

    klass = MATE_MIXER_BACKEND_GET_CLASS (backend);

    if (klass->close != NULL)
        klass->close (backend);
}

MateMixerState
mate_mixer_backend_get_state (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), MATE_MIXER_STATE_UNKNOWN);

    return backend->priv->state;
}

MateMixerBackendFlags
mate_mixer_backend_get_flags (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), MATE_MIXER_BACKEND_NO_FLAGS);

    return backend->priv->flags;
}

const GList *
mate_mixer_backend_list_devices (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    if (backend->priv->devices == NULL) {
        MateMixerBackendClass *klass;

        klass = MATE_MIXER_BACKEND_GET_CLASS (backend);

        if (klass->list_devices != NULL)
            backend->priv->devices = klass->list_devices (backend);
    }

    return backend->priv->devices;
}

const GList *
mate_mixer_backend_list_streams (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    if (backend->priv->streams == NULL) {
        MateMixerBackendClass *klass;

        klass = MATE_MIXER_BACKEND_GET_CLASS (backend);

        if (klass->list_streams != NULL)
            backend->priv->streams = klass->list_streams (backend);
    }

    return backend->priv->streams;
}

const GList *
mate_mixer_backend_list_stored_streams (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    if (backend->priv->stored_streams == NULL) {
        MateMixerBackendClass *klass;

        klass = MATE_MIXER_BACKEND_GET_CLASS (backend);

        if (klass->list_stored_streams != NULL)
            backend->priv->stored_streams = klass->list_stored_streams (backend);
    }

    return backend->priv->stored_streams;
}

MateMixerStream *
mate_mixer_backend_get_default_input_stream (MateMixerBackend *backend)
{
    MateMixerStream *stream = NULL;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    g_object_get (G_OBJECT (backend),
                  "default-input-stream", &stream,
                  NULL);

    if (stream != NULL)
        g_object_unref (stream);

    return stream;
}

gboolean
mate_mixer_backend_set_default_input_stream (MateMixerBackend *backend,
                                             MateMixerStream  *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    if (backend->priv->default_input != stream) {
        MateMixerBackendClass *klass;

        klass = MATE_MIXER_BACKEND_GET_CLASS (backend);

        if (klass->set_default_input_stream == NULL ||
            klass->set_default_input_stream (backend, stream) == FALSE)
            return FALSE;

        _mate_mixer_backend_set_default_input_stream (backend, stream);
    }

    return TRUE;
}

MateMixerStream *
mate_mixer_backend_get_default_output_stream (MateMixerBackend *backend)
{
    MateMixerStream *stream = NULL;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    g_object_get (G_OBJECT (backend),
                  "default-output-stream", &stream,
                  NULL);

    if (stream != NULL)
        g_object_unref (stream);

    return stream;
}

gboolean
mate_mixer_backend_set_default_output_stream (MateMixerBackend *backend,
                                              MateMixerStream  *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    if (backend->priv->default_input != stream) {
        MateMixerBackendClass *klass;

        klass = MATE_MIXER_BACKEND_GET_CLASS (backend);

        if (klass->set_default_output_stream == NULL ||
            klass->set_default_output_stream (backend, stream) == FALSE)
            return FALSE;

        _mate_mixer_backend_set_default_output_stream (backend, stream);
    }

    return TRUE;
}

static void
device_added (MateMixerBackend *backend, const gchar *name)
{
    MateMixerDevice *device;

    // device = mate_mixer_backend_get_device (backend, name);

/*
    g_signal_connect (G_OBJECT (device),
                      "stream-added",
                      G_CALLBACK (device_stream_added));
                      */
}

static void
device_removed (MateMixerBackend *backend, const gchar *name)
{
}

static void
device_stream_added (MateMixerDevice *device, const gchar *name)
{
    g_signal_emit (G_OBJECT (device),
                   signals[STREAM_ADDED],
                   0,
                   name);
}

static void
device_stream_removed (MateMixerDevice *device, const gchar *name)
{
    g_signal_emit (G_OBJECT (device),
                   signals[STREAM_REMOVED],
                   0,
                   name);
}

static void
free_devices (MateMixerBackend *backend)
{
    if (backend->priv->devices == NULL)
        return;

    g_list_free_full (backend->priv->devices, g_object_unref);

    backend->priv->devices = NULL;
}

static void
free_streams (MateMixerBackend *backend)
{
    if (backend->priv->streams == NULL)
        return;

    g_list_free_full (backend->priv->streams, g_object_unref);

    backend->priv->streams = NULL;
}

static void
free_stored_streams (MateMixerBackend *backend)
{
    if (backend->priv->stored_streams == NULL)
        return;

    g_list_free_full (backend->priv->stored_streams, g_object_unref);

    backend->priv->stored_streams = NULL;
}

/* Protected */
void
_mate_mixer_backend_set_state (MateMixerBackend *backend, MateMixerState state)
{
    if (backend->priv->state == state)
        return;

    backend->priv->state = state;

    g_object_notify_by_pspec (G_OBJECT (backend), properties[PROP_STATE]);
}

void
_mate_mixer_backend_set_default_input_stream (MateMixerBackend *backend,
                                              MateMixerStream  *stream)
{
    if (backend->priv->default_input == stream)
        return;

    if (backend->priv->default_input != NULL)
        g_object_unref (backend->priv->default_input);

    if (stream != NULL)
        backend->priv->default_input = g_object_ref (stream);
    else
        backend->priv->default_input = NULL;

    g_debug ("Default input stream changed to %s",
             (stream != NULL) ? mate_mixer_stream_get_name (stream) : "none");

    g_object_notify_by_pspec (G_OBJECT (backend), properties[PROP_DEFAULT_INPUT_STREAM]);
}

void
_mate_mixer_backend_set_default_output_stream (MateMixerBackend *backend,
                                               MateMixerStream  *stream)
{
    if (backend->priv->default_output == stream)
        return;

    if (backend->priv->default_output != NULL)
        g_object_unref (backend->priv->default_output);

    if (stream != NULL)
        backend->priv->default_output = g_object_ref (stream);
    else
        backend->priv->default_output = NULL;

    g_debug ("Default output stream changed to %s",
             (stream != NULL) ? mate_mixer_stream_get_name (stream) : "none");

    g_object_notify_by_pspec (G_OBJECT (backend), properties[PROP_DEFAULT_OUTPUT_STREAM]);
}
