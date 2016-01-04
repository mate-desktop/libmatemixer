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
#include "matemixer-device.h"
#include "matemixer-enums.h"
#include "matemixer-enum-types.h"
#include "matemixer-stream.h"
#include "matemixer-stream-control.h"
#include "matemixer-stored-control.h"

struct _MateMixerBackendPrivate
{
    GHashTable           *devices;
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
    STORED_CONTROL_ADDED,
    STORED_CONTROL_REMOVED,
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
static void mate_mixer_backend_finalize     (GObject               *object);

G_DEFINE_ABSTRACT_TYPE (MateMixerBackend, mate_mixer_backend, G_TYPE_OBJECT)

static void device_added          (MateMixerBackend *backend,
                                   const gchar      *name);
static void device_removed        (MateMixerBackend *backend,
                                   const gchar      *name);

static void device_stream_added   (MateMixerBackend *backend,
                                   const gchar      *name);
static void device_stream_removed (MateMixerBackend *backend,
                                   const gchar      *name);

static void
mate_mixer_backend_class_init (MateMixerBackendClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = mate_mixer_backend_dispose;
    object_class->finalize     = mate_mixer_backend_finalize;
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
                      G_SIGNAL_RUN_FIRST,
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
                      G_SIGNAL_RUN_FIRST,
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
                      G_SIGNAL_RUN_FIRST,
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
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MateMixerBackendClass, stream_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    signals[STORED_CONTROL_ADDED] =
        g_signal_new ("stored-control-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MateMixerBackendClass, stored_control_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    signals[STORED_CONTROL_REMOVED] =
        g_signal_new ("stored-control-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MateMixerBackendClass, stored_control_removed),
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

    backend->priv->devices = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    g_object_unref);

    g_signal_connect (G_OBJECT (backend),
                      "device-added",
                      G_CALLBACK (device_added),
                      NULL);

    g_signal_connect (G_OBJECT (backend),
                      "device-removed",
                      G_CALLBACK (device_removed),
                      NULL);
}

static void
mate_mixer_backend_dispose (GObject *object)
{
    MateMixerBackend *backend;

    backend = MATE_MIXER_BACKEND (object);

    g_clear_object (&backend->priv->default_input);
    g_clear_object (&backend->priv->default_output);

    g_hash_table_remove_all (backend->priv->devices);

    G_OBJECT_CLASS (mate_mixer_backend_parent_class)->dispose (object);
}

static void
mate_mixer_backend_finalize (GObject *object)
{
    MateMixerBackend *backend;

    backend = MATE_MIXER_BACKEND (object);

    g_hash_table_unref (backend->priv->devices);

    G_OBJECT_CLASS (mate_mixer_backend_parent_class)->finalize (object);
}

void
mate_mixer_backend_set_app_info (MateMixerBackend *backend, MateMixerAppInfo *info)
{
    MateMixerBackendClass *klass;

    g_return_if_fail (MATE_MIXER_IS_BACKEND (backend));

    klass = MATE_MIXER_BACKEND_GET_CLASS (backend);

    if (klass->set_app_info != NULL)
        klass->set_app_info (backend, info);
}

void
mate_mixer_backend_set_server_address (MateMixerBackend *backend, const gchar *address)
{
    MateMixerBackendClass *klass;

    g_return_if_fail (MATE_MIXER_IS_BACKEND (backend));

    klass = MATE_MIXER_BACKEND_GET_CLASS (backend);

    if (klass->set_server_address != NULL)
        klass->set_server_address (backend, address);
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

MateMixerDevice *
mate_mixer_backend_get_device (MateMixerBackend *backend, const gchar *name)
{
    const GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = mate_mixer_backend_list_devices (backend);
    while (list != NULL) {
        MateMixerDevice *device = MATE_MIXER_DEVICE (list->data);

        if (strcmp (name, mate_mixer_device_get_name (device)) == 0)
            return device;

        list = list->next;
    }
    return NULL;
}

MateMixerStream *
mate_mixer_backend_get_stream (MateMixerBackend *backend, const gchar *name)
{
    const GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = mate_mixer_backend_list_streams (backend);
    while (list != NULL) {
        MateMixerStream *stream = MATE_MIXER_STREAM (list->data);

        if (strcmp (name, mate_mixer_stream_get_name (stream)) == 0)
            return stream;

        list = list->next;
    }
    return NULL;
}

MateMixerStoredControl *
mate_mixer_backend_get_stored_control (MateMixerBackend *backend, const gchar *name)
{
    const GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = mate_mixer_backend_list_stored_controls (backend);
    while (list != NULL) {
        MateMixerStreamControl *control = MATE_MIXER_STREAM_CONTROL (list->data);

        if (strcmp (name, mate_mixer_stream_control_get_name (control)) == 0)
            return MATE_MIXER_STORED_CONTROL (control);

        list = list->next;
    }
    return NULL;
}

const GList *
mate_mixer_backend_list_devices (MateMixerBackend *backend)
{
    MateMixerBackendClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    klass = MATE_MIXER_BACKEND_GET_CLASS (backend);

    if (klass->list_devices != NULL)
        return klass->list_devices (backend);

    return NULL;
}

const GList *
mate_mixer_backend_list_streams (MateMixerBackend *backend)
{
    MateMixerBackendClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    klass = MATE_MIXER_BACKEND_GET_CLASS (backend);

    if (klass->list_streams != NULL)
        return klass->list_streams (backend);

    return NULL;
}

const GList *
mate_mixer_backend_list_stored_controls (MateMixerBackend *backend)
{
    MateMixerBackendClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    klass = MATE_MIXER_BACKEND_GET_CLASS (backend);

    if (klass->list_stored_controls != NULL)
        return klass->list_stored_controls (backend);

    return NULL;
}

MateMixerStream *
mate_mixer_backend_get_default_input_stream (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    return backend->priv->default_input;
}

gboolean
mate_mixer_backend_set_default_input_stream (MateMixerBackend *backend,
                                             MateMixerStream  *stream)
{
    MateMixerBackendClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    klass = MATE_MIXER_BACKEND_GET_CLASS (backend);
    if (klass->set_default_input_stream == NULL)
        return FALSE;

    if (backend->priv->default_input != stream) {
        if (mate_mixer_stream_get_direction (stream) != MATE_MIXER_DIRECTION_INPUT) {
            g_warning ("Unable to set non-input stream as the default input stream");
            return FALSE;
        }

        if (klass->set_default_input_stream (backend, stream) == FALSE)
            return FALSE;

        _mate_mixer_backend_set_default_input_stream (backend, stream);
    }
    return TRUE;
}

MateMixerStream *
mate_mixer_backend_get_default_output_stream (MateMixerBackend *backend)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), NULL);

    return backend->priv->default_output;
}

gboolean
mate_mixer_backend_set_default_output_stream (MateMixerBackend *backend,
                                              MateMixerStream  *stream)
{
    MateMixerBackendClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_BACKEND (backend), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    klass = MATE_MIXER_BACKEND_GET_CLASS (backend);
    if (klass->set_default_output_stream == NULL)
        return FALSE;

    if (backend->priv->default_input != stream) {
        if (mate_mixer_stream_get_direction (stream) != MATE_MIXER_DIRECTION_OUTPUT) {
            g_warning ("Unable to set non-output stream as the default output stream");
            return FALSE;
        }

        if (klass->set_default_output_stream (backend, stream) == FALSE)
            return FALSE;

        _mate_mixer_backend_set_default_output_stream (backend, stream);
    }
    return TRUE;
}

static void
device_added (MateMixerBackend *backend, const gchar *name)
{
    MateMixerDevice *device;

    device = mate_mixer_backend_get_device (backend, name);
    if G_UNLIKELY (device == NULL) {
        g_warn_if_reached ();
        return;
    }

    /* Keep the device in a hash table as it won't be possible to retrieve
     * it when the remove signal is received */
    g_hash_table_insert (backend->priv->devices,
                         g_strdup (name),
                         g_object_ref (device));

    /* Connect to the stream signals from devices so we can forward them on
     * the backend */
    g_signal_connect_swapped (G_OBJECT (device),
                              "stream-added",
                              G_CALLBACK (device_stream_added),
                              backend);
    g_signal_connect_swapped (G_OBJECT (device),
                              "stream-removed",
                              G_CALLBACK (device_stream_removed),
                              backend);
}

static void
device_removed (MateMixerBackend *backend, const gchar *name)
{
    MateMixerDevice *device;

    device = g_hash_table_lookup (backend->priv->devices, name);
    if G_UNLIKELY (device == NULL) {
        g_warn_if_reached ();
        return;
    }

    g_signal_handlers_disconnect_by_func (G_OBJECT (device),
                                          G_CALLBACK (device_stream_added),
                                          backend);
    g_signal_handlers_disconnect_by_func (G_OBJECT (device),
                                          G_CALLBACK (device_stream_removed),
                                          backend);

    g_hash_table_remove (backend->priv->devices, name);
}

static void
device_stream_added (MateMixerBackend *backend, const gchar *name)
{
    g_signal_emit (G_OBJECT (backend),
                   signals[STREAM_ADDED],
                   0,
                   name);
}

static void
device_stream_removed (MateMixerBackend *backend, const gchar *name)
{
    g_signal_emit (G_OBJECT (backend),
                   signals[STREAM_REMOVED],
                   0,
                   name);
}

/* Protected functions */
void
_mate_mixer_backend_set_state (MateMixerBackend *backend, MateMixerState state)
{
    g_return_if_fail (MATE_MIXER_IS_BACKEND (backend));

    if (backend->priv->state == state)
        return;

    backend->priv->state = state;

    g_object_notify_by_pspec (G_OBJECT (backend), properties[PROP_STATE]);
}

void
_mate_mixer_backend_set_default_input_stream (MateMixerBackend *backend,
                                              MateMixerStream  *stream)
{
    g_return_if_fail (MATE_MIXER_IS_BACKEND (backend));
    g_return_if_fail (stream == NULL || MATE_MIXER_IS_STREAM (stream));

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

    g_object_notify_by_pspec (G_OBJECT (backend),
                              properties[PROP_DEFAULT_INPUT_STREAM]);
}

void
_mate_mixer_backend_set_default_output_stream (MateMixerBackend *backend,
                                               MateMixerStream  *stream)
{
    g_return_if_fail (MATE_MIXER_IS_BACKEND (backend));
    g_return_if_fail (stream == NULL || MATE_MIXER_IS_STREAM (stream));

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

    g_object_notify_by_pspec (G_OBJECT (backend),
                              properties[PROP_DEFAULT_OUTPUT_STREAM]);
}
