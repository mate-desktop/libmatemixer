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
#include <libmatemixer/matemixer-stream.h>

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
    gboolean         connected_once;
    GSource         *connect_source;
    MateMixerStream *default_sink;
    MateMixerStream *default_source;
    GHashTable      *devices;
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
    PROP_DEFAULT_INPUT,
    PROP_DEFAULT_OUTPUT,
    N_PROPERTIES
};

static void mate_mixer_backend_interface_init (MateMixerBackendInterface *iface);

static void pulse_backend_class_init     (PulseBackendClass *klass);
static void pulse_backend_class_finalize (PulseBackendClass *klass);

static void pulse_backend_get_property   (GObject           *object,
                                          guint              param_id,
                                          GValue            *value,
                                          GParamSpec        *pspec);

static void pulse_backend_init           (PulseBackend      *pulse);
static void pulse_backend_dispose        (GObject           *object);
static void pulse_backend_finalize       (GObject           *object);

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

static gboolean         backend_try_reconnect             (PulseBackend                *pulse);
static void             backend_remove_connect_source     (PulseBackend                *pulse);

static void             backend_mark_hanging              (PulseBackend                *pulse);
static void             backend_mark_hanging_hash         (GHashTable                  *hash);

static void             backend_remove_hanging            (PulseBackend                *pulse);
static void             backend_remove_hanging_hash       (PulseBackend                *pulse,
                                                           GHashTable                  *hash);

static void             backend_remove_device             (PulseBackend                *pulse,
                                                           PulseDevice                 *device);
static void             backend_remove_stream             (PulseBackend                *pulse,
                                                           GHashTable                  *hash,
                                                           PulseStream                 *stream);

static void             backend_change_state              (PulseBackend                *backend,
                                                           MateMixerState               state);

static gint             backend_compare_devices           (gconstpointer                a,
                                                           gconstpointer                b);
static gint             backend_compare_streams           (gconstpointer                a,
                                                           gconstpointer                b);
static gboolean         backend_compare_stream_names      (gpointer                     key,
                                                           gpointer                     value,
                                                           gpointer                     user_data);

static MateMixerBackendInfo info;

void
backend_module_init (GTypeModule *module)
{
    pulse_backend_register_type (module);

    info.name         = BACKEND_NAME;
    info.priority     = BACKEND_PRIORITY;
    info.g_type       = PULSE_TYPE_BACKEND;
    info.backend_type = MATE_MIXER_BACKEND_PULSEAUDIO;
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
pulse_backend_class_init (PulseBackendClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = pulse_backend_dispose;
    object_class->finalize     = pulse_backend_finalize;
    object_class->get_property = pulse_backend_get_property;

    g_object_class_override_property (object_class, PROP_STATE, "state");
    g_object_class_override_property (object_class, PROP_DEFAULT_INPUT, "default-input");
    g_object_class_override_property (object_class, PROP_DEFAULT_OUTPUT, "default-output");

    g_type_class_add_private (object_class, sizeof (PulseBackendPrivate));
}

/* Called in the code generated by G_DEFINE_DYNAMIC_TYPE_EXTENDED() */
static void
pulse_backend_class_finalize (PulseBackendClass *klass)
{
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
    case PROP_DEFAULT_INPUT:
        g_value_set_object (value, pulse->priv->default_source);
        break;
    case PROP_DEFAULT_OUTPUT:
        g_value_set_object (value, pulse->priv->default_sink);
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

    /* These hash tables store PulseDevice and PulseStream instances, the key
     * is the PulseAudio index which is not unique across different stream
     * types, hence the separate hash tables */
    pulse->priv->devices =
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
    backend_close (MATE_MIXER_BACKEND (object));

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

    g_hash_table_destroy (pulse->priv->devices);
    g_hash_table_destroy (pulse->priv->sinks);
    g_hash_table_destroy (pulse->priv->sink_inputs);
    g_hash_table_destroy (pulse->priv->sources);
    g_hash_table_destroy (pulse->priv->source_outputs);

    G_OBJECT_CLASS (pulse_backend_parent_class)->finalize (object);
}

static gboolean
backend_open (MateMixerBackend *backend)
{
    PulseBackend    *pulse;
    PulseConnection *connection;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), FALSE);

    pulse = PULSE_BACKEND (backend);

    if (G_UNLIKELY (pulse->priv->connection != NULL)) {
        g_warn_if_reached ();
        return TRUE;
    }

    connection = pulse_connection_new (pulse->priv->app_name,
                                       pulse->priv->app_id,
                                       pulse->priv->app_version,
                                       pulse->priv->app_icon,
                                       pulse->priv->server_address);

    /* No connection attempt is made during the construction of the connection,
     * but it sets up the PulseAudio structures, which might fail in an
     * unlikely case */
    if (G_UNLIKELY (connection == NULL)) {
        backend_change_state (pulse, MATE_MIXER_STATE_FAILED);
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

    /* Connect to the PulseAudio server, this might fail either instantly or
     * asynchronously, for example when remote connection timeouts */
    if (!pulse_connection_connect (connection, FALSE)) {
        g_object_unref (connection);
        backend_change_state (pulse, MATE_MIXER_STATE_FAILED);
        return FALSE;
    }

    pulse->priv->connection = connection;

    backend_change_state (pulse, MATE_MIXER_STATE_CONNECTING);
    return TRUE;
}

static void
backend_close (MateMixerBackend *backend)
{
    PulseBackend *pulse;

    g_return_if_fail (PULSE_IS_BACKEND (backend));

    pulse = PULSE_BACKEND (backend);

    backend_remove_connect_source (pulse);

    // XXX disconnect from notifies
    g_clear_object (&pulse->priv->connection);
    g_clear_object (&pulse->priv->default_sink);
    g_clear_object (&pulse->priv->default_source);

    g_hash_table_remove_all (pulse->priv->devices);
    g_hash_table_remove_all (pulse->priv->sinks);
    g_hash_table_remove_all (pulse->priv->sink_inputs);
    g_hash_table_remove_all (pulse->priv->sources);
    g_hash_table_remove_all (pulse->priv->source_outputs);

    backend_change_state (pulse, MATE_MIXER_STATE_IDLE);
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

    g_return_if_fail (PULSE_IS_BACKEND (backend));
    g_return_if_fail (data != NULL);

    pulse = PULSE_BACKEND (backend);

    g_free (pulse->priv->app_name);
    g_free (pulse->priv->app_id);
    g_free (pulse->priv->app_version);
    g_free (pulse->priv->app_icon);
    g_free (pulse->priv->server_address);

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

    /* Always create a new current list, caching is done in the main library */
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

    /* Always create a new current list, caching is done in the main library */
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
    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);

    return PULSE_BACKEND (backend)->priv->default_source;
}

static gboolean
backend_set_default_input_stream (MateMixerBackend *backend, MateMixerStream *stream)
{
    PulseBackend *pulse;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), FALSE);
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pulse = PULSE_BACKEND (backend);

    if (G_UNLIKELY (!PULSE_IS_SOURCE (stream))) {
        g_warn_if_reached ();
        return FALSE;
    }

    return pulse_connection_set_default_source (pulse->priv->connection,
                                                mate_mixer_stream_get_name (stream));
}

static MateMixerStream *
backend_get_default_output_stream (MateMixerBackend *backend)
{
    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);

    return PULSE_BACKEND (backend)->priv->default_sink;
}

static gboolean
backend_set_default_output_stream (MateMixerBackend *backend, MateMixerStream *stream)
{
    PulseBackend *pulse;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), FALSE);
    g_return_val_if_fail (PULSE_IS_STREAM (stream), FALSE);

    pulse = PULSE_BACKEND (backend);

    if (G_UNLIKELY (!PULSE_IS_SINK (stream))) {
        g_warn_if_reached ();
        return FALSE;
    }

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
        if (pulse->priv->connected_once) {
            /* We managed to connect once before, try to reconnect and if it
             * fails immediately, use a timeout source.
             * All current devices and streams are marked as hanging as it is
             * unknown whether they are still available, stream callbacks will
             * unmark them and remaining unavailable streams will be removed
             * when the CONNECTED state is reached. */
            backend_mark_hanging (pulse);
            backend_change_state (pulse, MATE_MIXER_STATE_CONNECTING);

            if (!pulse->priv->connect_source &&
                !pulse_connection_connect (connection, TRUE)) {
                pulse->priv->connect_source = g_timeout_source_new (200);

                g_source_set_callback (pulse->priv->connect_source,
                                       (GSourceFunc) backend_try_reconnect,
                                       pulse,
                                       (GDestroyNotify) backend_remove_connect_source);

                g_source_attach (pulse->priv->connect_source,
                                 g_main_context_get_thread_default ());
            }
            break;
        }

        /* First connection attempt has failed */
        backend_change_state (pulse, MATE_MIXER_STATE_FAILED);
        break;

    case PULSE_CONNECTION_CONNECTING:
    case PULSE_CONNECTION_AUTHORIZING:
    case PULSE_CONNECTION_LOADING:
        backend_change_state (pulse, MATE_MIXER_STATE_CONNECTING);
        break;

    case PULSE_CONNECTION_CONNECTED:
        if (pulse->priv->connected_once)
            backend_remove_hanging (pulse);
        else
            pulse->priv->connected_once = TRUE;

        backend_change_state (pulse, MATE_MIXER_STATE_READY);
        break;
    }
}

static void
backend_server_info_cb (PulseConnection      *connection,
                        const pa_server_info *info,
                        PulseBackend         *pulse)
{
    const gchar *name_source = NULL;
    const gchar *name_sink = NULL;

    if (pulse->priv->default_source != NULL)
        name_source = mate_mixer_stream_get_name (pulse->priv->default_source);

    if (g_strcmp0 (name_source, info->default_source_name)) {
        if (pulse->priv->default_source != NULL)
            g_clear_object (&pulse->priv->default_source);

        if (info->default_source_name != NULL) {
            MateMixerStream *stream = g_hash_table_find (pulse->priv->sources,
                                                         backend_compare_stream_names,
                                                         (gpointer) info->default_source_name);

            /* It is theoretically possible to receive a server info notification
             * before the stream lists are fully downloaded, this should not be
             * a problem as a newer notification will arrive later after the
             * streams are read.
             * Of course this will only work if Pulse delivers notifications in
             * the correct order, but it seems it does. */
            if (G_LIKELY (stream != NULL)) {
                pulse->priv->default_source = g_object_ref (stream);
                g_debug ("Default input stream changed to %s", info->default_source_name);

                g_object_notify (G_OBJECT (pulse), "default-input");
            } else
                g_debug ("Default input stream %s not yet known",
                         info->default_source_name);
        }
    }

    if (pulse->priv->default_sink != NULL)
        name_sink = mate_mixer_stream_get_name (pulse->priv->default_sink);

    if (g_strcmp0 (name_sink, info->default_sink_name)) {
        if (pulse->priv->default_sink != NULL)
            g_clear_object (&pulse->priv->default_sink);

        if (info->default_sink_name != NULL) {
            MateMixerStream *stream = g_hash_table_find (pulse->priv->sinks,
                                                         backend_compare_stream_names,
                                                         (gpointer) info->default_sink_name);
            if (G_LIKELY (stream != NULL)) {
                pulse->priv->default_sink = g_object_ref (stream);
                g_debug ("Default output stream changed to %s", info->default_sink_name);

                g_object_notify (G_OBJECT (pulse), "default-output");
            } else
                g_debug ("Default output stream %s not yet known",
                         info->default_sink_name);
        }
    }

    if (pulse->priv->state != MATE_MIXER_STATE_READY)
        g_debug ("Sound server is %s version %s, running on %s",
                 info->server_name,
                 info->server_version,
                 info->host_name);
}

static void
backend_card_info_cb (PulseConnection    *connection,
                      const pa_card_info *info,
                      PulseBackend       *pulse)
{
    gpointer     p;
    PulseDevice *device;

    p = g_hash_table_lookup (pulse->priv->devices, GINT_TO_POINTER (info->index));
    if (p == NULL) {
        device = pulse_device_new (connection, info);

        if (G_UNLIKELY (device == NULL))
            return;

        g_hash_table_insert (pulse->priv->devices,
                             GINT_TO_POINTER (info->index),
                             device);
    } else {
        device = PULSE_DEVICE (p);
        pulse_device_update (device, info);
    }

    if (pulse->priv->connected_once) {
        /* The object might be hanging if reconnecting is in progress, remove the
         * hanging flag to prevent it from being removed when connected */
        if (pulse->priv->state != MATE_MIXER_STATE_READY)
            g_object_steal_data (G_OBJECT (device), "hanging");

        g_signal_emit_by_name (G_OBJECT (pulse),
                               (p == NULL)
                                    ? "device-added"
                                    : "device-changed",
                               mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));
    }
}

static void
backend_card_removed_cb (PulseConnection *connection,
                         guint            index,
                         PulseBackend    *pulse)
{
    gpointer p;

    p = g_hash_table_lookup (pulse->priv->devices, GINT_TO_POINTER (index));
    if (G_UNLIKELY (p == NULL))
        return;

    backend_remove_device (pulse, PULSE_DEVICE (p));
}

static void
backend_sink_info_cb (PulseConnection    *connection,
                      const pa_sink_info *info,
                      PulseBackend       *pulse)
{
    PulseDevice *device = NULL;
    PulseStream *stream;
    gpointer     p = NULL;

    if (info->card != PA_INVALID_INDEX)
        p = g_hash_table_lookup (pulse->priv->devices, GINT_TO_POINTER (info->card));
    if (p)
        device = PULSE_DEVICE (p);

    p = g_hash_table_lookup (pulse->priv->sinks, GINT_TO_POINTER (info->index));
    if (p == NULL) {
        stream = pulse_sink_new (connection, info, device);

        if (G_UNLIKELY (stream == NULL))
            return;

        g_hash_table_insert (pulse->priv->sinks,
                             GINT_TO_POINTER (info->index),
                             stream);
    } else {
        stream = PULSE_STREAM (p);
        pulse_sink_update (stream, info, device);
    }

    if (pulse->priv->connected_once) {
        /* The object might be hanging if reconnecting is in progress, remove the
         * hanging flag to prevent it from being removed when connected */
        if (pulse->priv->state != MATE_MIXER_STATE_READY)
            g_object_steal_data (G_OBJECT (stream), "hanging");

        g_signal_emit_by_name (G_OBJECT (pulse),
                               (p == NULL)
                                    ? "stream-added"
                                    : "stream-changed",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    }
}

static void
backend_sink_removed_cb (PulseConnection *connection,
                         guint            index,
                         PulseBackend    *pulse)
{
    gpointer p;

    p = g_hash_table_lookup (pulse->priv->sinks, GINT_TO_POINTER (index));
    if (G_UNLIKELY (p == NULL))
        return;

    backend_remove_stream (pulse, pulse->priv->sinks, PULSE_STREAM (p));
}

static void
backend_sink_input_info_cb (PulseConnection          *connection,
                            const pa_sink_input_info *info,
                            PulseBackend             *pulse)
{
    PulseStream *stream;
    gpointer     p;
    gpointer     parent = NULL;

    if (G_LIKELY (info->sink)) {
        parent = g_hash_table_lookup (pulse->priv->sinks, GINT_TO_POINTER (info->sink));
        if (G_UNLIKELY (parent == NULL))
            g_debug ("Unknown parent %d of PulseAudio sink input %s",
                     info->sink,
                     info->name);
    }

    p = g_hash_table_lookup (pulse->priv->sink_inputs, GINT_TO_POINTER (info->index));
    if (p == NULL) {
        stream = pulse_sink_input_new (connection, info, parent);

        if (G_UNLIKELY (stream == NULL))
            return;

        g_hash_table_insert (pulse->priv->sink_inputs,
                             GINT_TO_POINTER (info->index),
                             stream);
    } else {
        stream = PULSE_STREAM (p);
        pulse_sink_input_update (stream, info, parent);
    }

    if (pulse->priv->connected_once) {
        /* The object might be hanging if reconnecting is in progress, remove the
         * hanging flag to prevent it from being removed when connected */
        if (pulse->priv->state != MATE_MIXER_STATE_READY)
            g_object_steal_data (G_OBJECT (stream), "hanging");

        g_signal_emit_by_name (G_OBJECT (pulse),
                               (p == NULL)
                                    ? "stream-added"
                                    : "stream-changed",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    }
}

static void
backend_sink_input_removed_cb (PulseConnection *connection,
                               guint            index,
                               PulseBackend    *pulse)
{
    gpointer p;

    p = g_hash_table_lookup (pulse->priv->sink_inputs, GINT_TO_POINTER (index));
    if (G_UNLIKELY (p == NULL))
        return;

    backend_remove_stream (pulse, pulse->priv->sink_inputs, PULSE_STREAM (p));
}

static void
backend_source_info_cb (PulseConnection      *connection,
                        const pa_source_info *info,
                        PulseBackend         *pulse)
{
    PulseDevice *device = NULL;
    PulseStream *stream;
    gpointer     p = NULL;

    /* Skip monitor streams */
    if (info->monitor_of_sink != PA_INVALID_INDEX)
        return;

    if (info->card != PA_INVALID_INDEX)
        p = g_hash_table_lookup (pulse->priv->devices, GINT_TO_POINTER (info->card));
    if (p)
        device = PULSE_DEVICE (p);

    p = g_hash_table_lookup (pulse->priv->sources, GINT_TO_POINTER (info->index));
    if (p == NULL) {
        stream = pulse_source_new (connection, info, device);

        if (G_UNLIKELY (stream == NULL))
            return;

        g_hash_table_insert (pulse->priv->sources,
                             GINT_TO_POINTER (info->index),
                             stream);
    } else {
        stream = PULSE_STREAM (p);
        pulse_source_update (stream, info, device);
    }

    if (pulse->priv->connected_once) {
        /* The object might be hanging if reconnecting is in progress, remove the
         * hanging flag to prevent it from being removed when connected */
        if (pulse->priv->state != MATE_MIXER_STATE_READY)
            g_object_steal_data (G_OBJECT (stream), "hanging");

        g_signal_emit_by_name (G_OBJECT (pulse),
                               (p == NULL)
                                    ? "stream-added"
                                    : "stream-changed",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    }
}

static void
backend_source_removed_cb (PulseConnection *connection,
                           guint            index,
                           PulseBackend    *pulse)
{
    gpointer p;

    p = g_hash_table_lookup (pulse->priv->sources, GINT_TO_POINTER (index));
    if (G_UNLIKELY (p == NULL))
        return;

    backend_remove_stream (pulse, pulse->priv->sources, PULSE_STREAM (p));
}

static void
backend_source_output_info_cb (PulseConnection             *connection,
                               const pa_source_output_info *info,
                               PulseBackend                *pulse)
{
    PulseStream *stream;
    gpointer     p;
    gpointer     parent = NULL;

    if (G_LIKELY (info->source)) {
        parent = g_hash_table_lookup (pulse->priv->sources, GINT_TO_POINTER (info->source));

        /* Probably a monitor source that we have skipped */
        if (parent == NULL)
            return;
    }

    p = g_hash_table_lookup (pulse->priv->source_outputs, GINT_TO_POINTER (info->index));
    if (p == NULL) {
        stream = pulse_source_output_new (connection, info, parent);
        if (G_UNLIKELY (stream == NULL))
            return;

        g_hash_table_insert (pulse->priv->source_outputs,
                             GINT_TO_POINTER (info->index),
                             stream);
    } else {
        stream = PULSE_STREAM (p);
        pulse_source_output_update (stream, info, parent);
    }

    if (pulse->priv->connected_once) {
        /* The object might be hanging if reconnecting is in progress, remove the
         * hanging flag to prevent it from being removed when connected */
        if (pulse->priv->state != MATE_MIXER_STATE_READY)
            g_object_steal_data (G_OBJECT (stream), "hanging");

        g_signal_emit_by_name (G_OBJECT (pulse),
                               (p == NULL)
                                    ? "stream-added"
                                    : "stream-changed",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    }
}

static void
backend_source_output_removed_cb (PulseConnection *connection,
                                  guint            index,
                                  PulseBackend    *pulse)
{
    gpointer p;

    p = g_hash_table_lookup (pulse->priv->source_outputs, GINT_TO_POINTER (index));
    if (G_UNLIKELY (p == NULL))
        return;

    backend_remove_stream (pulse, pulse->priv->source_outputs, PULSE_STREAM (p));
}

static gboolean
backend_try_reconnect (PulseBackend *pulse)
{
    /* When the connect call succeeds, return FALSE to remove the idle source
     * and wait for the connection state notifications, otherwise this function
     * will be called again */
    return !pulse_connection_connect (pulse->priv->connection, TRUE);
}

static void
backend_remove_connect_source (PulseBackend *pulse)
{
    g_clear_pointer (&pulse->priv->connect_source, g_source_unref);
}

static void
backend_mark_hanging (PulseBackend *pulse)
{
    backend_mark_hanging_hash (pulse->priv->devices);
    backend_mark_hanging_hash (pulse->priv->sinks);
    backend_mark_hanging_hash (pulse->priv->sink_inputs);
    backend_mark_hanging_hash (pulse->priv->sources);
    backend_mark_hanging_hash (pulse->priv->source_outputs);
}

static void
backend_mark_hanging_hash (GHashTable *hash)
{
    GHashTableIter iter;
    gpointer       value;

    g_hash_table_iter_init (&iter, hash);

    while (g_hash_table_iter_next (&iter, NULL, &value))
        g_object_set_data (G_OBJECT (value), "hanging", GINT_TO_POINTER (1));
}

static void
backend_remove_hanging (PulseBackend *pulse)
{
    GHashTableIter iter;
    gpointer       value;

    g_hash_table_iter_init (&iter, pulse->priv->devices);

    while (g_hash_table_iter_next (&iter, NULL, &value))
        if (g_object_get_data (G_OBJECT (value), "hanging"))
            backend_remove_device (pulse, PULSE_DEVICE (value));

    backend_remove_hanging_hash (pulse, pulse->priv->sinks);
    backend_remove_hanging_hash (pulse, pulse->priv->sink_inputs);
    backend_remove_hanging_hash (pulse, pulse->priv->sources);
    backend_remove_hanging_hash (pulse, pulse->priv->source_outputs);
}

static void
backend_remove_hanging_hash (PulseBackend *pulse, GHashTable *hash)
{
    GHashTableIter iter;
    gpointer       value;

    g_hash_table_iter_init (&iter, hash);

    while (g_hash_table_iter_next (&iter, NULL, &value))
        if (g_object_get_data (G_OBJECT (value), "hanging"))
            backend_remove_stream (pulse, hash, PULSE_STREAM (value));
}

static void
backend_remove_device (PulseBackend *pulse, PulseDevice *device)
{
    gchar *name;

    name = g_strdup (mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));

    g_hash_table_remove (pulse->priv->devices,
                         GINT_TO_POINTER (pulse_device_get_index (device)));

    g_signal_emit_by_name (G_OBJECT (pulse), "device-removed", name);
    g_free (name);
}

static void
backend_remove_stream (PulseBackend *pulse, GHashTable *hash, PulseStream *stream)
{
    gchar *name;

    /* Make sure we do not end up with invalid default streams, but this is
     * very unlikely to happen */
    if (G_UNLIKELY (MATE_MIXER_STREAM (stream) == pulse->priv->default_sink)) {
        g_clear_object (&pulse->priv->default_sink);
        g_object_notify (G_OBJECT (pulse), "default-output");
    }
    else if (G_UNLIKELY (MATE_MIXER_STREAM (stream) == pulse->priv->default_source)) {
        g_clear_object (&pulse->priv->default_source);
        g_object_notify (G_OBJECT (pulse), "default-input");
    }

    name = g_strdup (mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));

    g_hash_table_remove (hash, GINT_TO_POINTER (pulse_stream_get_index (stream)));

    g_signal_emit_by_name (G_OBJECT (pulse), "stream-removed", name);
    g_free (name);
}

static void
backend_change_state (PulseBackend *backend, MateMixerState state)
{
    if (backend->priv->state == state)
        return;

    backend->priv->state = state;

    g_object_notify (G_OBJECT (backend), "state");
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

static gboolean
backend_compare_stream_names (gpointer key, gpointer value, gpointer user_data)
{
    MateMixerStream *stream = MATE_MIXER_STREAM (value);

    return !strcmp (mate_mixer_stream_get_name (stream), (const gchar *) user_data);
}
