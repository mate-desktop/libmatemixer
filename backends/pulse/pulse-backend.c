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

#include <libmatemixer/matemixer-backend.h>
#include <libmatemixer/matemixer-backend-module.h>

#include <pulse/pulseaudio.h>

#include "pulse-backend.h"
#include "pulse-connection.h"
#include "pulse-device.h"
#include "pulse-enums.h"
#include "pulse-stream.h"
#include "pulse-sink.h"
#include "pulse-sink-input.h"
#include "pulse-source.h"
#include "pulse-source-output.h"

#define BACKEND_NAME      "PulseAudio"
#define BACKEND_PRIORITY  10

struct _PulseBackendPrivate
{
    gchar           *app_name;
    gchar           *app_id;
    gchar           *app_version;
    gchar           *app_icon;
    gchar           *server_address;
    gchar           *default_sink;
    gchar           *default_source;
    GHashTable      *devices;
    GHashTable      *cards;
    GHashTable      *sinks;
    GHashTable      *sink_inputs;
    GHashTable      *sources;
    GHashTable      *source_outputs;
    MateMixerState   state;
    PulseConnection *connection;
};

enum {
    PROP_0,
    PROP_STATE,
    N_PROPERTIES
};

static void mate_mixer_backend_interface_init (MateMixerBackendInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (PulseBackend, pulse_backend,
                                G_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (MATE_MIXER_TYPE_BACKEND,
                                                               mate_mixer_backend_interface_init))

static gboolean         backend_open                      (MateMixerBackend            *backend);
static void             backend_close                     (MateMixerBackend            *backend);

static MateMixerState   backend_get_state                 (MateMixerBackend            *backend);

static void             backend_set_data                  (MateMixerBackend            *backend,
                                                           const MateMixerBackendData  *data);

static GList *          backend_list_devices              (MateMixerBackend            *backend);
static GList *          backend_list_streams              (MateMixerBackend            *backend);

static MateMixerStream *backend_get_default_input_stream  (MateMixerBackend            *backend);
static gboolean         backend_set_default_input_stream  (MateMixerBackend            *backend,
                                                           MateMixerStream             *stream);

static MateMixerStream *backend_get_default_output_stream (MateMixerBackend            *backend);
static gboolean         backend_set_default_output_stream (MateMixerBackend            *backend,
                                                           MateMixerStream             *stream);

static void             backend_connection_state_cb       (PulseConnection             *connection,
                                                           GParamSpec                  *pspec,
                                                           PulseBackend                *pulse);

static void             backend_server_info_cb            (PulseConnection             *connection,
                                                           const pa_server_info        *info,
                                                           PulseBackend                *pulse);

static void             backend_card_info_cb              (PulseConnection             *connection,
                                                           const pa_card_info          *info,
                                                           PulseBackend                *pulse);
static void             backend_card_removed_cb           (PulseConnection             *connection,
                                                           guint                        index,
                                                           PulseBackend                *pulse);
static void             backend_sink_info_cb              (PulseConnection             *connection,
                                                           const pa_sink_info          *info,
                                                           PulseBackend                *pulse);
static void             backend_sink_removed_cb           (PulseConnection             *connection,
                                                           guint                        index,
                                                           PulseBackend                *pulse);
static void             backend_sink_input_info_cb        (PulseConnection             *connection,
                                                           const pa_sink_input_info    *info,
                                                           PulseBackend                *pulse);
static void             backend_sink_input_removed_cb     (PulseConnection             *connection,
                                                           guint                        index,
                                                           PulseBackend                *pulse);
static void             backend_source_info_cb            (PulseConnection             *connection,
                                                           const pa_source_info        *info,
                                                           PulseBackend                *pulse);
static void             backend_source_removed_cb         (PulseConnection             *connection,
                                                           guint                        index,
                                                           PulseBackend                *pulse);
static void             backend_source_output_info_cb     (PulseConnection             *connection,
                                                           const pa_source_output_info *info,
                                                           PulseBackend                *pulse);
static void             backend_source_output_removed_cb  (PulseConnection             *connection,
                                                           guint                        index,
                                                           PulseBackend                *pulse);

static gint             backend_compare_devices           (gconstpointer                a,
                                                           gconstpointer                b);
static gint             backend_compare_streams           (gconstpointer                a,
                                                           gconstpointer                b);

static MateMixerBackendInfo info;

void
backend_module_init (GTypeModule *module)
{
    pulse_backend_register_type (module);

    info.name         = BACKEND_NAME;
    info.priority     = BACKEND_PRIORITY;
    info.g_type       = PULSE_TYPE_BACKEND;
    info.backend_type = MATE_MIXER_BACKEND_PULSE;
}

const MateMixerBackendInfo *
backend_module_get_info (void)
{
    return &info;
}

static void
mate_mixer_backend_interface_init (MateMixerBackendInterface *iface)
{
    iface->open                      = backend_open;
    iface->close                     = backend_close;
    iface->get_state                 = backend_get_state;
    iface->set_data                  = backend_set_data;
    iface->list_devices              = backend_list_devices;
    iface->list_streams              = backend_list_streams;
    iface->get_default_input_stream  = backend_get_default_input_stream;
    iface->set_default_input_stream  = backend_set_default_input_stream;
    iface->get_default_output_stream = backend_get_default_output_stream;
    iface->set_default_output_stream = backend_set_default_output_stream;
}

static void
pulse_backend_get_property (GObject    *object,
                            guint       param_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
    PulseBackend *pulse;

    pulse = PULSE_BACKEND (object);

    switch (param_id) {
    case PROP_STATE:
        g_value_set_enum (value, pulse->priv->state);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_backend_init (PulseBackend *pulse)
{
    pulse->priv = G_TYPE_INSTANCE_GET_PRIVATE (pulse,
                                               PULSE_TYPE_BACKEND,
                                               PulseBackendPrivate);
    pulse->priv->devices =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               g_object_unref);
    pulse->priv->cards =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               g_object_unref);
    pulse->priv->sinks =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               g_object_unref);
    pulse->priv->sink_inputs =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               g_object_unref);
    pulse->priv->sources =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               g_object_unref);
    pulse->priv->source_outputs =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               g_object_unref);
}

static void
pulse_backend_dispose (GObject *object)
{
    PulseBackend *pulse;

    pulse = PULSE_BACKEND (object);

    if (pulse->priv->devices) {
        g_hash_table_destroy (pulse->priv->devices);
        pulse->priv->devices = NULL;
    }

    if (pulse->priv->cards) {
        g_hash_table_destroy (pulse->priv->cards);
        pulse->priv->cards = NULL;
    }

    if (pulse->priv->sinks) {
        g_hash_table_destroy (pulse->priv->sinks);
        pulse->priv->devices = NULL;
    }

    if (pulse->priv->sink_inputs) {
        g_hash_table_destroy (pulse->priv->sink_inputs);
        pulse->priv->devices = NULL;
    }

    if (pulse->priv->sources) {
        g_hash_table_destroy (pulse->priv->sources);
        pulse->priv->devices = NULL;
    }

    if (pulse->priv->source_outputs) {
        g_hash_table_destroy (pulse->priv->source_outputs);
        pulse->priv->source_outputs = NULL;
    }

    g_clear_object (&pulse->priv->connection);

    G_OBJECT_CLASS (pulse_backend_parent_class)->dispose (object);
}

static void
pulse_backend_finalize (GObject *object)
{
    PulseBackend *pulse;

    pulse = PULSE_BACKEND (object);

    g_free (pulse->priv->app_name);
    g_free (pulse->priv->app_id);
    g_free (pulse->priv->app_version);
    g_free (pulse->priv->app_icon);
    g_free (pulse->priv->server_address);

    G_OBJECT_CLASS (pulse_backend_parent_class)->finalize (object);
}

static void
pulse_backend_class_init (PulseBackendClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = pulse_backend_dispose;
    object_class->finalize     = pulse_backend_finalize;
    object_class->get_property = pulse_backend_get_property;

    g_object_class_override_property (object_class, PROP_STATE, "state");

    g_type_class_add_private (klass, sizeof (PulseBackendPrivate));
}

/* Called in the code generated by G_DEFINE_DYNAMIC_TYPE_EXTENDED() */
static void
pulse_backend_class_finalize (PulseBackendClass *klass)
{
}

static gboolean
backend_open (MateMixerBackend *backend)
{
    PulseBackend    *pulse;
    PulseConnection *connection;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), FALSE);

    pulse = PULSE_BACKEND (backend);

    if (G_UNLIKELY (pulse->priv->connection != NULL))
        return TRUE;

    connection = pulse_connection_new (pulse->priv->app_name,
                                       pulse->priv->app_id,
                                       pulse->priv->app_version,
                                       pulse->priv->app_icon,
                                       pulse->priv->server_address);
    if (G_UNLIKELY (connection == NULL)) {
        pulse->priv->state = MATE_MIXER_STATE_FAILED;
        return FALSE;
    }

    g_signal_connect (connection,
                      "notify::state",
                      G_CALLBACK (backend_connection_state_cb),
                      pulse);
    g_signal_connect (connection,
                      "server-info",
                      G_CALLBACK (backend_server_info_cb),
                      pulse);
    g_signal_connect (connection,
                      "card-info",
                      G_CALLBACK (backend_card_info_cb),
                      pulse);
    g_signal_connect (connection,
                      "card-removed",
                      G_CALLBACK (backend_card_removed_cb),
                      pulse);
    g_signal_connect (connection,
                      "sink-info",
                      G_CALLBACK (backend_sink_info_cb),
                      pulse);
    g_signal_connect (connection,
                      "sink-removed",
                      G_CALLBACK (backend_sink_removed_cb),
                      pulse);
    g_signal_connect (connection,
                      "sink-input-info",
                      G_CALLBACK (backend_sink_input_info_cb),
                      pulse);
    g_signal_connect (connection,
                      "sink-input-removed",
                      G_CALLBACK (backend_sink_input_removed_cb),
                      pulse);
    g_signal_connect (connection,
                      "source-info",
                      G_CALLBACK (backend_source_info_cb),
                      pulse);
    g_signal_connect (connection,
                      "source-removed",
                      G_CALLBACK (backend_source_removed_cb),
                      pulse);
    g_signal_connect (connection,
                      "source-output-info",
                      G_CALLBACK (backend_source_output_info_cb),
                      pulse);
    g_signal_connect (connection,
                      "source-output-removed",
                      G_CALLBACK (backend_source_output_removed_cb),
                      pulse);

    if (!pulse_connection_connect (connection)) {
        pulse->priv->state = MATE_MIXER_STATE_FAILED;
        g_object_unref (connection);
        return FALSE;
    }
    pulse->priv->connection = connection;
    pulse->priv->state = MATE_MIXER_STATE_CONNECTING;
    return TRUE;
}

static void
backend_close (MateMixerBackend *backend)
{
    g_return_if_fail (PULSE_IS_BACKEND (backend));

    g_clear_object (&PULSE_BACKEND (backend)->priv->connection);
}

static MateMixerState
backend_get_state (MateMixerBackend *backend)
{
    g_return_val_if_fail (PULSE_IS_BACKEND (backend), MATE_MIXER_STATE_UNKNOWN);

    return PULSE_BACKEND (backend)->priv->state;
}

static void
backend_set_data (MateMixerBackend *backend, const MateMixerBackendData *data)
{
    PulseBackend *pulse;

    if (data == NULL)
        return;

    g_return_if_fail (PULSE_IS_BACKEND (backend));

    pulse = PULSE_BACKEND (backend);

    g_free (data->app_name);
    g_free (data->app_id);
    g_free (data->app_version);
    g_free (data->app_icon);
    g_free (data->server_address);

    pulse->priv->app_name = g_strdup (data->app_name);
    pulse->priv->app_id = g_strdup (data->app_id);
    pulse->priv->app_version = g_strdup (data->app_version);
    pulse->priv->app_icon = g_strdup (data->app_icon);
    pulse->priv->server_address = g_strdup (data->server_address);
}

static GList *
backend_list_devices (MateMixerBackend *backend)
{
    GList *list;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);

    list = g_hash_table_get_values (PULSE_BACKEND (backend)->priv->devices);

    g_list_foreach (list, (GFunc) g_object_ref, NULL);

    return g_list_sort (list, backend_compare_devices);
}

static GList *
backend_list_streams (MateMixerBackend *backend)
{
    GList        *list;
    PulseBackend *pulse;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);

    pulse = PULSE_BACKEND (backend);

    list = g_list_concat (g_hash_table_get_values (pulse->priv->sinks),
                          g_hash_table_get_values (pulse->priv->sink_inputs));
    list = g_list_concat (list,
                          g_hash_table_get_values (pulse->priv->sources));
    list = g_list_concat (list,
                          g_hash_table_get_values (pulse->priv->source_outputs));

    g_list_foreach (list, (GFunc) g_object_ref, NULL);

    return g_list_sort (list, backend_compare_streams);
}

static MateMixerStream *
backend_get_default_input_stream (MateMixerBackend *backend)
{
    return NULL;
}

static gboolean
backend_set_default_input_stream (MateMixerBackend *backend, MateMixerStream *stream)
{
    PulseBackend *pulse;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);
    g_return_val_if_fail (PULSE_IS_STREAM (stream), NULL);

    pulse = PULSE_BACKEND (backend);

    return pulse_connection_set_default_source (pulse->priv->connection,
                                                mate_mixer_stream_get_name (stream));
}

static MateMixerStream *
backend_get_default_output_stream (MateMixerBackend *backend)
{
    return NULL;
}

static gboolean
backend_set_default_output_stream (MateMixerBackend *backend, MateMixerStream *stream)
{
    PulseBackend *pulse;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);
    g_return_val_if_fail (PULSE_IS_STREAM (stream), NULL);

    pulse = PULSE_BACKEND (backend);

    return pulse_connection_set_default_sink (pulse->priv->connection,
                                              mate_mixer_stream_get_name (stream));
}

static void
backend_connection_state_cb (PulseConnection *connection,
                             GParamSpec      *pspec,
                             PulseBackend    *pulse)
{
    PulseConnectionState state = pulse_connection_get_state (connection);

    switch (state) {
    case PULSE_CONNECTION_DISCONNECTED:
        break;
    case PULSE_CONNECTION_CONNECTING:
        break;
    case PULSE_CONNECTION_AUTHORIZING:
        break;
    case PULSE_CONNECTION_LOADING:
        break;
    case PULSE_CONNECTION_CONNECTED:
        pulse->priv->state = MATE_MIXER_STATE_READY;

        g_object_notify (G_OBJECT (pulse), "state");
        break;
    }
}

static void
backend_server_info_cb (PulseConnection      *connection,
                        const pa_server_info *info,
                        PulseBackend         *pulse)
{
    // XXX add property

    if (g_strcmp0 (pulse->priv->default_sink, info->default_sink_name)) {
        g_free (pulse->priv->default_sink);

        pulse->priv->default_sink = g_strdup (info->default_sink_name);
        // g_object_notify (G_OBJECT (pulse), "default-output");
    }

    if (g_strcmp0 (pulse->priv->default_source, info->default_source_name)) {
        g_free (pulse->priv->default_source);

        pulse->priv->default_source = g_strdup (info->default_source_name);
        // g_object_notify (G_OBJECT (pulse), "default-input");
    }
}

static void
backend_card_info_cb (PulseConnection    *connection,
                      const pa_card_info *info,
                      PulseBackend       *pulse)
{
    gpointer p;

    p = g_hash_table_lookup (pulse->priv->devices, GINT_TO_POINTER (info->index));
    if (!p) {
        PulseDevice *device;

        device = pulse_device_new (connection, info);
        g_hash_table_insert (pulse->priv->devices,
                             GINT_TO_POINTER (pulse_device_get_index (device)),
                             device);
        g_signal_emit_by_name (G_OBJECT (pulse),
                               "device-added",
                               mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));
    } else {
        pulse_device_update (PULSE_DEVICE (p), info);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "device-changed",
                               mate_mixer_device_get_name (MATE_MIXER_DEVICE (p)));
    }
}

static void
backend_card_removed_cb (PulseConnection *connection,
                         guint            index,
                         PulseBackend    *pulse)
{
    gpointer  p;
    gchar    *name;

    p = g_hash_table_lookup (pulse->priv->devices, GINT_TO_POINTER (index));
    if (G_UNLIKELY (p == NULL))
        return;

    name = g_strdup (mate_mixer_device_get_name (MATE_MIXER_DEVICE (p)));

    g_hash_table_remove (pulse->priv->devices, GINT_TO_POINTER (index));
    if (G_LIKELY (name != NULL))
        g_signal_emit_by_name (G_OBJECT (pulse),
                               "device-removed",
                               name);
    g_free (name);
}

static void
backend_sink_info_cb (PulseConnection    *connection,
                      const pa_sink_info *info,
                      PulseBackend       *pulse)
{
    gpointer p;

    p = g_hash_table_lookup (pulse->priv->sinks, GINT_TO_POINTER (info->index));
    if (!p) {
        PulseStream *stream;

        stream = pulse_sink_new (connection, info);
        g_hash_table_insert (pulse->priv->sinks,
                             GINT_TO_POINTER (pulse_stream_get_index (stream)),
                             stream);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-added",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    } else {
        pulse_sink_update (p, info);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-changed",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (p)));
    }
}

static void
backend_sink_removed_cb (PulseConnection *connection,
                         guint            index,
                         PulseBackend    *pulse)
{
    gpointer  p;
    gchar    *name;

    p = g_hash_table_lookup (pulse->priv->sinks, GINT_TO_POINTER (index));
    if (G_UNLIKELY (p == NULL))
        return;

    name = g_strdup (mate_mixer_stream_get_name (MATE_MIXER_STREAM (p)));

    g_hash_table_remove (pulse->priv->sinks, GINT_TO_POINTER (index));
    if (G_LIKELY (name != NULL))
        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-removed",
                               name);
    g_free (name);
}

static void
backend_sink_input_info_cb (PulseConnection          *connection,
                            const pa_sink_input_info *info,
                            PulseBackend             *pulse)
{
    gpointer p;

    p = g_hash_table_lookup (pulse->priv->sink_inputs, GINT_TO_POINTER (info->index));
    if (!p) {
        PulseStream *stream;

        stream = pulse_sink_input_new (connection, info);
        g_hash_table_insert (pulse->priv->sink_inputs,
                             GINT_TO_POINTER (pulse_stream_get_index (stream)),
                             stream);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-added",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    } else {
        pulse_sink_input_update (p, info);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-changed",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (p)));
    }
}

static void
backend_sink_input_removed_cb (PulseConnection *connection,
                               guint            index,
                               PulseBackend    *pulse)
{
    gpointer  p;
    gchar    *name;

    p = g_hash_table_lookup (pulse->priv->sink_inputs, GINT_TO_POINTER (index));
    if (G_UNLIKELY (p == NULL))
        return;

    name = g_strdup (mate_mixer_stream_get_name (MATE_MIXER_STREAM (p)));

    g_hash_table_remove (pulse->priv->sink_inputs, GINT_TO_POINTER (index));
    if (G_LIKELY (name != NULL))
        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-removed",
                               name);
    g_free (name);
}

static void
backend_source_info_cb (PulseConnection      *connection,
                        const pa_source_info *info,
                        PulseBackend         *pulse)
{
    gpointer p;

    p = g_hash_table_lookup (pulse->priv->sources, GINT_TO_POINTER (info->index));
    if (!p) {
        PulseStream *stream;

        stream = pulse_source_new (connection, info);
        g_hash_table_insert (pulse->priv->sources,
                             GINT_TO_POINTER (pulse_stream_get_index (stream)),
                             stream);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-added",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    } else {
        pulse_source_update (p, info);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-changed",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (p)));
    }
}

static void
backend_source_removed_cb (PulseConnection *connection,
                           guint            index,
                           PulseBackend    *pulse)
{
    gpointer  p;
    gchar    *name;

    p = g_hash_table_lookup (pulse->priv->sources, GINT_TO_POINTER (index));
    if (G_UNLIKELY (p == NULL))
        return;

    name = g_strdup (mate_mixer_stream_get_name (MATE_MIXER_STREAM (p)));

    g_hash_table_remove (pulse->priv->sources, GINT_TO_POINTER (index));
    if (G_LIKELY (name != NULL))
        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-removed",
                               name);
    g_free (name);
}

static void
backend_source_output_info_cb (PulseConnection             *connection,
                               const pa_source_output_info *info,
                               PulseBackend                *pulse)
{
    gpointer p;

    p = g_hash_table_lookup (pulse->priv->source_outputs, GINT_TO_POINTER (info->index));
    if (!p) {
        PulseStream *stream;

        stream = pulse_source_output_new (connection, info);
        g_hash_table_insert (pulse->priv->source_outputs,
                             GINT_TO_POINTER (pulse_stream_get_index (stream)),
                             stream);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-added",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    } else {
        pulse_source_output_update (p, info);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-changed",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (p)));
    }
}

static void
backend_source_output_removed_cb (PulseConnection *connection,
                                  guint            index,
                                  PulseBackend    *pulse)
{
    gpointer  p;
    gchar    *name;

    p = g_hash_table_lookup (pulse->priv->source_outputs, GINT_TO_POINTER (index));
    if (G_UNLIKELY (p == NULL))
        return;

    name = g_strdup (mate_mixer_stream_get_name (MATE_MIXER_STREAM (p)));

    g_hash_table_remove (pulse->priv->source_outputs, GINT_TO_POINTER (index));
    if (G_LIKELY (name != NULL))
        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-removed",
                               name);
    g_free (name);
}

static gint
backend_compare_devices (gconstpointer a, gconstpointer b)
{
    return strcmp (mate_mixer_device_get_name (MATE_MIXER_DEVICE (a)),
                   mate_mixer_device_get_name (MATE_MIXER_DEVICE (b)));
}

static gint
backend_compare_streams (gconstpointer a, gconstpointer b)
{
    return strcmp (mate_mixer_stream_get_name (MATE_MIXER_STREAM (a)),
                   mate_mixer_stream_get_name (MATE_MIXER_STREAM (b)));
}
