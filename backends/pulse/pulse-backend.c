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
#include <pulse/ext-stream-restore.h>

#include "pulse-backend.h"
#include "pulse-connection.h"
#include "pulse-device.h"
#include "pulse-enums.h"
#include "pulse-ext-stream.h"
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
    GHashTable      *streams;
    GHashTable      *ext_streams;
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

static gboolean         pulse_backend_open                      (MateMixerBackend          *backend);
static void             pulse_backend_close                     (MateMixerBackend          *backend);

static MateMixerState   pulse_backend_get_state                 (MateMixerBackend          *backend);

static void             pulse_backend_set_data                  (MateMixerBackend          *backend,
                                                                const MateMixerBackendData *data);

static GList *          pulse_backend_list_devices              (MateMixerBackend          *backend);
static GList *          pulse_backend_list_streams              (MateMixerBackend          *backend);
static GList *          pulse_backend_list_cached_streams       (MateMixerBackend          *backend);

static MateMixerStream *pulse_backend_get_default_input_stream  (MateMixerBackend          *backend);
static gboolean         pulse_backend_set_default_input_stream  (MateMixerBackend          *backend,
                                                                 MateMixerStream           *stream);

static MateMixerStream *pulse_backend_get_default_output_stream (MateMixerBackend          *backend);
static gboolean         pulse_backend_set_default_output_stream (MateMixerBackend          *backend,
                                                                 MateMixerStream           *stream);

static void             on_connection_state_notify          (PulseConnection                  *connection,
                                                             GParamSpec                       *pspec,
                                                             PulseBackend                     *pulse);

static void             on_connection_server_info           (PulseConnection                  *connection,
                                                             const pa_server_info             *info,
                                                             PulseBackend                     *pulse);

static void             on_connection_card_info             (PulseConnection                  *connection,
                                                             const pa_card_info               *info,
                                                             PulseBackend                     *pulse);
static void             on_connection_card_removed          (PulseConnection                  *connection,
                                                             guint                             index,
                                                             PulseBackend                     *pulse);
static void             on_connection_sink_info             (PulseConnection                  *connection,
                                                             const pa_sink_info               *info,
                                                             PulseBackend                     *pulse);
static void             on_connection_sink_removed          (PulseConnection                  *connection,
                                                             guint                             index,
                                                             PulseBackend                     *pulse);
static void             on_connection_sink_input_info       (PulseConnection                  *connection,
                                                             const pa_sink_input_info         *info,
                                                             PulseBackend                     *pulse);
static void             on_connection_sink_input_removed    (PulseConnection                  *connection,
                                                             guint                             index,
                                                             PulseBackend                     *pulse);
static void             on_connection_source_info           (PulseConnection                  *connection,
                                                             const pa_source_info             *info,
                                                             PulseBackend                     *pulse);
static void             on_connection_source_removed        (PulseConnection                  *connection,
                                                             guint                             index,
                                                             PulseBackend                     *pulse);
static void             on_connection_source_output_info    (PulseConnection                  *connection,
                                                             const pa_source_output_info      *info,
                                                             PulseBackend                     *pulse);
static void             on_connection_source_output_removed (PulseConnection                  *connection,
                                                             guint                             index,
                                                             PulseBackend                     *pulse);
static void             on_connection_ext_stream_loading    (PulseConnection                  *connection,
                                                             PulseBackend                     *pulse);
static void             on_connection_ext_stream_loaded     (PulseConnection                  *connection,
                                                             PulseBackend                     *pulse);
static void             on_connection_ext_stream_info       (PulseConnection                  *connection,
                                                             const pa_ext_stream_restore_info *info,
                                                             PulseBackend                     *pulse);

static gboolean         connect_source_reconnect            (PulseBackend                     *pulse);
static void             connect_source_remove               (PulseBackend                     *pulse);

static void             check_pending_sink                  (PulseBackend                     *pulse,
                                                             PulseStream                      *stream);
static void             check_pending_source                (PulseBackend                     *pulse,
                                                             PulseStream                      *stream);

static void             mark_hanging                        (PulseBackend                     *pulse);
static void             mark_hanging_hash                   (GHashTable                       *hash);

static void             unmark_hanging                      (PulseBackend                     *pulse,
                                                             GObject                          *object);

static void             remove_hanging                      (PulseBackend                     *pulse);
static void             remove_device                       (PulseBackend                     *pulse,
                                                             PulseDevice                      *device);
static void             remove_stream                       (PulseBackend                     *pulse,
                                                             PulseStream                      *stream);

static void             change_state                        (PulseBackend                     *backend,
                                                             MateMixerState                    state);

static gint             compare_devices                     (gconstpointer                     a,
                                                             gconstpointer                     b);
static gint             compare_streams                     (gconstpointer                     a,
                                                             gconstpointer                     b);
static gboolean         compare_stream_names                (gpointer                          key,
                                                             gpointer                          value,
                                                             gpointer                          user_data);

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
    iface->open                      = pulse_backend_open;
    iface->close                     = pulse_backend_close;
    iface->get_state                 = pulse_backend_get_state;
    iface->set_data                  = pulse_backend_set_data;
    iface->list_devices              = pulse_backend_list_devices;
    iface->list_streams              = pulse_backend_list_streams;
    iface->list_cached_streams       = pulse_backend_list_cached_streams;
    iface->get_default_input_stream  = pulse_backend_get_default_input_stream;
    iface->set_default_input_stream  = pulse_backend_set_default_input_stream;
    iface->get_default_output_stream = pulse_backend_get_default_output_stream;
    iface->set_default_output_stream = pulse_backend_set_default_output_stream;
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

    /* These hash tables store PulseDevice and PulseStream instances */
    pulse->priv->devices =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               g_object_unref);
    pulse->priv->streams =
        g_hash_table_new_full (g_int64_hash,
                               g_int64_equal,
                               g_free,
                               g_object_unref);
    pulse->priv->ext_streams =
        g_hash_table_new_full (g_str_hash,
                               g_str_equal,
                               g_free,
                               g_object_unref);
}

static void
pulse_backend_dispose (GObject *object)
{
    pulse_backend_close (MATE_MIXER_BACKEND (object));

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
    g_hash_table_destroy (pulse->priv->streams);
    g_hash_table_destroy (pulse->priv->ext_streams);

    G_OBJECT_CLASS (pulse_backend_parent_class)->finalize (object);
}

static gboolean
pulse_backend_open (MateMixerBackend *backend)
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
        change_state (pulse, MATE_MIXER_STATE_FAILED);
        return FALSE;
    }

    g_signal_connect (G_OBJECT (connection),
                      "notify::state",
                      G_CALLBACK (on_connection_state_notify),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "server-info",
                      G_CALLBACK (on_connection_server_info),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "card-info",
                      G_CALLBACK (on_connection_card_info),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "card-removed",
                      G_CALLBACK (on_connection_card_removed),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "sink-info",
                      G_CALLBACK (on_connection_sink_info),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "sink-removed",
                      G_CALLBACK (on_connection_sink_removed),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "sink-input-info",
                      G_CALLBACK (on_connection_sink_input_info),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "sink-input-removed",
                      G_CALLBACK (on_connection_sink_input_removed),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "source-info",
                      G_CALLBACK (on_connection_source_info),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "source-removed",
                      G_CALLBACK (on_connection_source_removed),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "source-output-info",
                      G_CALLBACK (on_connection_source_output_info),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "source-output-removed",
                      G_CALLBACK (on_connection_source_output_removed),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "ext-stream-loading",
                      G_CALLBACK (on_connection_ext_stream_loading),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "ext-stream-loaded",
                      G_CALLBACK (on_connection_ext_stream_loaded),
                      pulse);
    g_signal_connect (G_OBJECT (connection),
                      "ext-stream-info",
                      G_CALLBACK (on_connection_ext_stream_info),
                      pulse);

    change_state (pulse, MATE_MIXER_STATE_CONNECTING);

    /* Connect to the PulseAudio server, this might fail either instantly or
     * asynchronously, for example when remote connection timeouts */
    if (pulse_connection_connect (connection, FALSE) == FALSE) {
        g_object_unref (connection);
        change_state (pulse, MATE_MIXER_STATE_FAILED);
        return FALSE;
    }

    pulse->priv->connection = connection;
    return TRUE;
}

static void
pulse_backend_close (MateMixerBackend *backend)
{
    PulseBackend *pulse;

    g_return_if_fail (PULSE_IS_BACKEND (backend));

    pulse = PULSE_BACKEND (backend);

    connect_source_remove (pulse);

    if (pulse->priv->connection != NULL) {
        g_signal_handlers_disconnect_by_data (G_OBJECT (pulse->priv->connection),
                                              pulse);

        g_clear_object (&pulse->priv->connection);
    }

    g_clear_object (&pulse->priv->default_sink);
    g_clear_object (&pulse->priv->default_source);

    g_hash_table_remove_all (pulse->priv->devices);
    g_hash_table_remove_all (pulse->priv->streams);
    g_hash_table_remove_all (pulse->priv->ext_streams);

    pulse->priv->connected_once = FALSE;

    change_state (pulse, MATE_MIXER_STATE_IDLE);
}

static MateMixerState
pulse_backend_get_state (MateMixerBackend *backend)
{
    g_return_val_if_fail (PULSE_IS_BACKEND (backend), MATE_MIXER_STATE_UNKNOWN);

    return PULSE_BACKEND (backend)->priv->state;
}

static void
pulse_backend_set_data (MateMixerBackend *backend, const MateMixerBackendData *data)
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
pulse_backend_list_devices (MateMixerBackend *backend)
{
    GList *list;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);

    /* Convert the hash table to a sorted linked list, this list is expected
     * to be cached in the main library */
    list = g_hash_table_get_values (PULSE_BACKEND (backend)->priv->devices);
    if (list != NULL) {
        g_list_foreach (list, (GFunc) g_object_ref, NULL);

        return g_list_sort (list, compare_devices);
    }
    return NULL;
}

static GList *
pulse_backend_list_streams (MateMixerBackend *backend)
{
    GList *list;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);

    /* Convert the hash table to a sorted linked list, this list is expected
     * to be cached in the main library */
    list = g_hash_table_get_values (PULSE_BACKEND (backend)->priv->streams);
    if (list != NULL) {
        g_list_foreach (list, (GFunc) g_object_ref, NULL);

        return g_list_sort (list, compare_streams);
    }
    return NULL;
}

static GList *
pulse_backend_list_cached_streams (MateMixerBackend *backend)
{
    GList *list;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);

    /* Convert the hash table to a sorted linked list, this list is expected
     * to be cached in the main library */
    list = g_hash_table_get_values (PULSE_BACKEND (backend)->priv->ext_streams);
    if (list != NULL) {
        g_list_foreach (list, (GFunc) g_object_ref, NULL);

        return g_list_sort (list, compare_streams);
    }
    return NULL;
}

static MateMixerStream *
pulse_backend_get_default_input_stream (MateMixerBackend *backend)
{
    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);

    return PULSE_BACKEND (backend)->priv->default_source;
}

static gboolean
pulse_backend_set_default_input_stream (MateMixerBackend *backend,
                                        MateMixerStream  *stream)
{
    PulseBackend *pulse;
    const gchar  *name;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), FALSE);
    g_return_val_if_fail (PULSE_IS_SOURCE (stream), FALSE);

    pulse = PULSE_BACKEND (backend);

    name = mate_mixer_stream_get_name (stream);
    if (pulse_connection_set_default_source (pulse->priv->connection, name) == FALSE)
        return FALSE;

    if (pulse->priv->default_source != NULL)
        g_object_unref (pulse->priv->default_source);

    pulse->priv->default_source = g_object_ref (stream);

    /* We might be in the process of setting a default source for which the details
     * are not yet known, make sure the change does not happen */
    g_object_set_data (G_OBJECT (pulse),
                       "backend-pending-source",
                       NULL);

    g_object_notify (G_OBJECT (pulse), "default-input");
    return TRUE;
}

static MateMixerStream *
pulse_backend_get_default_output_stream (MateMixerBackend *backend)
{
    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);

    return PULSE_BACKEND (backend)->priv->default_sink;
}

static gboolean
pulse_backend_set_default_output_stream (MateMixerBackend *backend,
                                         MateMixerStream  *stream)
{
    PulseBackend *pulse;
    const gchar  *name;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), FALSE);
    g_return_val_if_fail (PULSE_IS_SINK (stream), FALSE);

    pulse = PULSE_BACKEND (backend);

    name = mate_mixer_stream_get_name (stream);
    if (pulse_connection_set_default_sink (pulse->priv->connection, name) == FALSE)
        return FALSE;

    if (pulse->priv->default_sink != NULL)
        g_object_unref (pulse->priv->default_sink);

    pulse->priv->default_sink = g_object_ref (stream);

    /* We might be in the process of setting a default sink for which the details
     * are not yet known, make sure the change does not happen */
    g_object_set_data (G_OBJECT (pulse),
                       "backend-pending-sink",
                       NULL);

    g_object_notify (G_OBJECT (pulse), "default-output");
    return TRUE;
}

static void
on_connection_state_notify (PulseConnection *connection,
                            GParamSpec      *pspec,
                            PulseBackend    *pulse)
{
    PulseConnectionState state = pulse_connection_get_state (connection);

    switch (state) {
    case PULSE_CONNECTION_DISCONNECTED:
        if (pulse->priv->connected_once == TRUE) {
            /* We managed to connect once before, try to reconnect and if it
             * fails immediately, use a timeout source.
             * All current devices and streams are marked as hanging as it is
             * unknown whether they are still available.
             * Stream callbacks will unmark available streams and remaining
             * unavailable streams will be removed when the CONNECTED state
             * is reached. */
            mark_hanging (pulse);
            change_state (pulse, MATE_MIXER_STATE_CONNECTING);

            if (pulse->priv->connect_source == NULL &&
                pulse_connection_connect (connection, TRUE) == FALSE) {
                pulse->priv->connect_source = g_timeout_source_new (200);

                g_source_set_callback (pulse->priv->connect_source,
                                       (GSourceFunc) connect_source_reconnect,
                                       pulse,
                                       (GDestroyNotify) connect_source_remove);

                g_source_attach (pulse->priv->connect_source,
                                 g_main_context_get_thread_default ());
            }
            break;
        }

        /* First connection attempt has failed */
        change_state (pulse, MATE_MIXER_STATE_FAILED);
        break;

    case PULSE_CONNECTION_CONNECTING:
    case PULSE_CONNECTION_AUTHORIZING:
    case PULSE_CONNECTION_LOADING:
        change_state (pulse, MATE_MIXER_STATE_CONNECTING);
        break;

    case PULSE_CONNECTION_CONNECTED:
        if (pulse->priv->connected_once == TRUE)
            remove_hanging (pulse);
        else
            pulse->priv->connected_once = TRUE;

        change_state (pulse, MATE_MIXER_STATE_READY);
        break;
    }
}

static void
on_connection_server_info (PulseConnection      *connection,
                           const pa_server_info *info,
                           PulseBackend         *pulse)
{
    const gchar *name_source = NULL;
    const gchar *name_sink = NULL;

    if (pulse->priv->default_source != NULL)
        name_source = mate_mixer_stream_get_name (pulse->priv->default_source);

    if (g_strcmp0 (name_source, info->default_source_name) != 0) {
        if (pulse->priv->default_source != NULL)
            g_clear_object (&pulse->priv->default_source);

        if (info->default_source_name != NULL) {
            MateMixerStream *stream = g_hash_table_find (pulse->priv->streams,
                                                         compare_stream_names,
                                                         (gpointer) info->default_source_name);

            /* It is possible that we are unaware of the default stream, either
             * because the stream details have not arrived yet, or because we chose
             * to ignore the stream.
             * When this happens, remember the name of the stream and wait for the
             * stream info callback. */
            if (stream != NULL) {
                pulse->priv->default_source = g_object_ref (stream);
                g_object_set_data (G_OBJECT (pulse),
                                   "backend-pending-source",
                                   NULL);

                g_debug ("Default input stream changed to %s", info->default_source_name);
            } else {
                g_debug ("Default input stream changed to unknown stream %s",
                         info->default_source_name);

                g_object_set_data_full (G_OBJECT (pulse),
                                        "backend-pending-source",
                                        g_strdup (info->default_source_name),
                                        g_free);

                /* In most cases (for example changing profile) the stream info
                 * arrives by itself, but do not rely on it and request it explicitely.
                 * In the meantime, keep the default stream set to NULL, which is
                 * important as we cannot guarantee that the info arrives and we use it. */
                pulse_connection_load_source_info_name (pulse->priv->connection,
                                                        info->default_source_name);
            }
        } else
            g_debug ("Default input stream unset");

        g_object_notify (G_OBJECT (pulse), "default-input");
    }

    if (pulse->priv->default_sink != NULL)
        name_sink = mate_mixer_stream_get_name (pulse->priv->default_sink);

    if (g_strcmp0 (name_sink, info->default_sink_name) != 0) {
        if (pulse->priv->default_sink != NULL)
            g_clear_object (&pulse->priv->default_sink);

        if (info->default_sink_name != NULL) {
            MateMixerStream *stream = g_hash_table_find (pulse->priv->streams,
                                                         compare_stream_names,
                                                         (gpointer) info->default_sink_name);

            /* It is possible that we are unaware of the default stream, either
             * because the stream details have not arrived yet, or because we chose
             * to ignore the stream.
             * When this happens, remember the name of the stream and wait for the
             * stream info callback. */
            if (stream != NULL) {
                pulse->priv->default_sink = g_object_ref (stream);
                g_object_set_data (G_OBJECT (pulse),
                                   "backend-pending-sink",
                                   NULL);

                g_debug ("Default output stream changed to %s", info->default_sink_name);

            } else {
                g_debug ("Default output stream changed to unknown stream %s",
                         info->default_sink_name);

                g_object_set_data_full (G_OBJECT (pulse),
                                        "backend-pending-sink",
                                        g_strdup (info->default_sink_name),
                                        g_free);

                /* In most cases (for example changing profile) the stream info
                 * arrives by itself, but do not rely on it and request it explicitely.
                 * In the meantime, keep the default stream set to NULL, which is
                 * important as we cannot guarantee that the info arrives and we use it. */
                pulse_connection_load_sink_info_name (pulse->priv->connection,
                                                      info->default_sink_name);
            }
        } else
            g_debug ("Default output stream unset");

        g_object_notify (G_OBJECT (pulse), "default-output");
    }

    if (pulse->priv->state != MATE_MIXER_STATE_READY)
        g_debug ("Sound server is %s version %s, running on %s",
                 info->server_name,
                 info->server_version,
                 info->host_name);
}

static void
on_connection_card_info (PulseConnection    *connection,
                         const pa_card_info *info,
                         PulseBackend       *pulse)
{
    PulseDevice *device;

    device = g_hash_table_lookup (pulse->priv->devices, GUINT_TO_POINTER (info->index));
    if (device == NULL) {
        device = pulse_device_new (connection, info);
        if (G_UNLIKELY (device == NULL))
            return;

        g_hash_table_insert (pulse->priv->devices, GUINT_TO_POINTER (info->index), device);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "device-added",
                               mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));
    } else {
        pulse_device_update (device, info);

        /* The object might be hanging if reconnecting is in progress, remove the
         * hanging flag to prevent it from being removed when connected */
        unmark_hanging (pulse, G_OBJECT (device));
    }
}

static void
on_connection_card_removed (PulseConnection *connection,
                            guint            index,
                            PulseBackend    *pulse)
{
    PulseDevice *device;

    device = g_hash_table_lookup (pulse->priv->devices, GUINT_TO_POINTER (index));
    if (G_UNLIKELY (device == NULL))
        return;

    remove_device (pulse, device);
}

/* PulseAudio uses 32-bit integers as indices for sinks, sink inputs, sources and
 * source inputs, these indices are not unique among the different kinds of streams,
 * but we want to keep all of them in a single hash table. Allow this by using 64-bit
 * hash table keys, use the lower 32 bits for the PulseAudio index and some of the
 * higher bits to indicate what kind of stream it is. */
enum {
    HASH_BIT_SINK          = (1ULL << 63),
    HASH_BIT_SINK_INPUT    = (1ULL << 62),
    HASH_BIT_SOURCE        = (1ULL << 61),
    HASH_BIT_SOURCE_OUTPUT = (1ULL << 60)
};
#define HASH_ID_SINK(idx)          (((idx) & 0xffffffff) | HASH_BIT_SINK)
#define HASH_ID_SINK_INPUT(idx)    (((idx) & 0xffffffff) | HASH_BIT_SINK_INPUT)
#define HASH_ID_SOURCE(idx)        (((idx) & 0xffffffff) | HASH_BIT_SOURCE)
#define HASH_ID_SOURCE_OUTPUT(idx) (((idx) & 0xffffffff) | HASH_BIT_SOURCE_OUTPUT)

static void
on_connection_sink_info (PulseConnection    *connection,
                         const pa_sink_info *info,
                         PulseBackend       *pulse)
{
    PulseDevice *device = NULL;
    PulseStream *stream;
    gint64       index = HASH_ID_SINK (info->index);

    if (info->card != PA_INVALID_INDEX)
        device = g_hash_table_lookup (pulse->priv->devices, GUINT_TO_POINTER (info->card));

    stream = g_hash_table_lookup (pulse->priv->streams, &index);
    if (stream == NULL) {
        stream = pulse_sink_new (connection, info, device);
        if (G_UNLIKELY (stream == NULL))
            return;

        g_hash_table_insert (pulse->priv->streams, g_memdup (&index, 8), stream);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-added",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));

        /* We might be waiting for this sink to set it as the default */
        check_pending_sink (pulse, stream);
    } else {
        pulse_sink_update (stream, info, device);

        /* The object might be hanging if reconnecting is in progress, remove the
         * hanging flag to prevent it from being removed when connected */
        unmark_hanging (pulse, G_OBJECT (stream));
    }
}

static void
on_connection_sink_removed (PulseConnection *connection,
                            guint            idx,
                            PulseBackend    *pulse)
{
    PulseStream *stream;
    gint64       index = HASH_ID_SINK (idx);

    stream = g_hash_table_lookup (pulse->priv->streams, &index);
    if (G_UNLIKELY (stream == NULL))
        return;

    remove_stream (pulse, stream);
}

static void
on_connection_sink_input_info (PulseConnection          *connection,
                               const pa_sink_input_info *info,
                               PulseBackend             *pulse)
{
    PulseStream *stream;
    PulseStream *parent = NULL;
    gint64       index;

    if (G_LIKELY (info->sink != PA_INVALID_INDEX)) {
        index = HASH_ID_SINK (info->sink);

        parent = g_hash_table_lookup (pulse->priv->streams, &index);
        if (G_UNLIKELY (parent == NULL))
            g_debug ("Unknown parent %d of PulseAudio sink input %s",
                     info->sink,
                     info->name);
    }

    index = HASH_ID_SINK_INPUT (info->index);

    stream = g_hash_table_lookup (pulse->priv->streams, &index);
    if (stream == NULL) {
        stream = pulse_sink_input_new (connection, info, parent);
        if (G_UNLIKELY (stream == NULL))
            return;

        g_hash_table_insert (pulse->priv->streams, g_memdup (&index, 8), stream);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-added",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    } else {
        pulse_sink_input_update (stream, info, parent);

        /* The object might be hanging if reconnecting is in progress, remove the
         * hanging flag to prevent it from being removed when connected */
        unmark_hanging (pulse, G_OBJECT (stream));
    }
}

static void
on_connection_sink_input_removed (PulseConnection *connection,
                                  guint            idx,
                                  PulseBackend    *pulse)
{
    PulseStream *stream;
    gint64       index = HASH_ID_SINK_INPUT (idx);

    stream = g_hash_table_lookup (pulse->priv->streams, &index);
    if (G_UNLIKELY (stream == NULL))
        return;

    remove_stream (pulse, stream);
}

static void
on_connection_source_info (PulseConnection      *connection,
                           const pa_source_info *info,
                           PulseBackend         *pulse)
{
    PulseDevice *device = NULL;
    PulseStream *stream;
    gint64       index = HASH_ID_SOURCE (info->index);

    /* Skip monitor streams */
    if (info->monitor_of_sink != PA_INVALID_INDEX)
        return;

    if (info->card != PA_INVALID_INDEX)
        device = g_hash_table_lookup (pulse->priv->devices, GUINT_TO_POINTER (info->card));

    stream = g_hash_table_lookup (pulse->priv->streams, &index);
    if (stream == NULL) {
        stream = pulse_source_new (connection, info, device);
        if (G_UNLIKELY (stream == NULL))
            return;

        g_hash_table_insert (pulse->priv->streams, g_memdup (&index, 8), stream);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-added",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));

        /* We might be waiting for this source to set it as the default */
        check_pending_source (pulse, stream);
    } else {
        pulse_source_update (stream, info, device);

        /* The object might be hanging if reconnecting is in progress, remove the
         * hanging flag to prevent it from being removed when connected */
        unmark_hanging (pulse, G_OBJECT (stream));
    }
}

static void
on_connection_source_removed (PulseConnection *connection,
                              guint            idx,
                              PulseBackend    *pulse)
{
    PulseStream *stream;
    gint64       index = HASH_ID_SOURCE (idx);

    stream = g_hash_table_lookup (pulse->priv->streams, &index);
    if (G_UNLIKELY (stream == NULL))
        return;

    remove_stream (pulse, stream);
}

static void
on_connection_source_output_info (PulseConnection             *connection,
                                  const pa_source_output_info *info,
                                  PulseBackend                *pulse)
{
    PulseStream *stream;
    PulseStream *parent = NULL;
    gint64       index;

    if (G_LIKELY (info->source != PA_INVALID_INDEX)) {
        index = HASH_ID_SOURCE (info->source);

        /* Most likely a monitor source that we have skipped */
        parent = g_hash_table_lookup (pulse->priv->streams, &index);
        if (parent == NULL)
            return;
    }

    index = HASH_ID_SOURCE_OUTPUT (info->index);

    stream = g_hash_table_lookup (pulse->priv->streams, &index);
    if (stream == NULL) {
        stream = pulse_source_output_new (connection, info, parent);
        if (G_UNLIKELY (stream == NULL))
            return;

        g_hash_table_insert (pulse->priv->streams, g_memdup (&index, 8), stream);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-added",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    } else {
        pulse_source_output_update (stream, info, parent);

        /* The object might be hanging if reconnecting is in progress, remove the
         * hanging flag to prevent it from being removed when connected */
        unmark_hanging (pulse, G_OBJECT (stream));
    }
}

static void
on_connection_source_output_removed (PulseConnection *connection,
                                     guint            idx,
                                     PulseBackend    *pulse)
{
    PulseStream *stream;
    gint64       index = HASH_ID_SOURCE_OUTPUT (idx);

    stream = g_hash_table_lookup (pulse->priv->streams, &index);
    if (G_UNLIKELY (stream == NULL))
        return;

    remove_stream (pulse, stream);
}

static void
on_connection_ext_stream_info (PulseConnection                  *connection,
                               const pa_ext_stream_restore_info *info,
                               PulseBackend                     *pulse)
{
    PulseStream *stream;
    PulseStream *parent = NULL;

    if (G_LIKELY (info->device != NULL))
        parent = g_hash_table_find (pulse->priv->streams, compare_stream_names,
                                    (gpointer) info->device);

    stream = g_hash_table_lookup (pulse->priv->ext_streams, info->name);
    if (stream == NULL) {
        stream = pulse_ext_stream_new (connection, info, parent);
        if (G_UNLIKELY (stream == NULL))
            return;

        g_hash_table_insert (pulse->priv->ext_streams, g_strdup (info->name), stream);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "cached-stream-added",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    } else {
        pulse_ext_stream_update (stream, info, parent);

        /* The object might be hanging if ext-streams are being loaded, remove
         * the hanging flag to prevent it from being removed */
        unmark_hanging (pulse, G_OBJECT (stream));
    }
}

static void
on_connection_ext_stream_loading (PulseConnection *connection, PulseBackend *pulse)
{
    mark_hanging_hash (pulse->priv->ext_streams);
}

static void
on_connection_ext_stream_loaded (PulseConnection *connection, PulseBackend *pulse)
{
    GHashTableIter iter;
    gpointer       value;

    g_hash_table_iter_init (&iter, pulse->priv->ext_streams);

    while (g_hash_table_iter_next (&iter, NULL, &value) == TRUE) {
        guint hanging =
            GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (value), "backend-hanging"));

        if (hanging == 1) {
            gchar *name = g_strdup ((const gchar *) value);

            g_hash_table_remove (pulse->priv->ext_streams, (gconstpointer) name);
            g_signal_emit_by_name (G_OBJECT (pulse),
                                   "cached-stream-removed",
                                   name);
            g_free (name);
        }
    }
}

static gboolean
connect_source_reconnect (PulseBackend *pulse)
{
    /* When the connect call succeeds, return FALSE to remove the idle source
     * and wait for the connection state notifications, otherwise this function
     * will be called again */
    if (pulse_connection_connect (pulse->priv->connection, TRUE) == TRUE) {
        connect_source_remove (pulse);
        return FALSE;
    }
    return TRUE;
}

static void
connect_source_remove (PulseBackend *pulse)
{
    g_clear_pointer (&pulse->priv->connect_source, g_source_unref);
}

static void
check_pending_sink (PulseBackend *pulse, PulseStream *stream)
{
    const gchar *pending;
    const gchar *name;

    /* See if the currently added sream matches the default input stream
     * we are waiting for */
    pending = g_object_get_data (G_OBJECT (pulse), "backend-pending-sink");
    if (pending == NULL)
        return;

    name = mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream));
    if (g_strcmp0 (pending, name) != 0)
        return;

    pulse->priv->default_sink = g_object_ref (stream);
    g_object_set_data (G_OBJECT (pulse),
                       "backend-pending-sink",
                       NULL);

    g_debug ("Default output stream changed to pending stream %s", name);

    g_object_notify (G_OBJECT (pulse), "default-output");
}

static void
check_pending_source (PulseBackend *pulse, PulseStream *stream)
{
    const gchar *pending;
    const gchar *name;

    /* See if the currently added sream matches the default input stream
     * we are waiting for */
    pending = g_object_get_data (G_OBJECT (pulse), "backend-pending-source");
    if (pending == NULL)
        return;

    name = mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream));
    if (g_strcmp0 (pending, name) != 0)
        return;

    pulse->priv->default_source = g_object_ref (stream);
    g_object_set_data (G_OBJECT (pulse),
                       "backend-pending-source",
                       NULL);

    g_debug ("Default input stream changed to pending stream %s", name);

    g_object_notify (G_OBJECT (pulse), "default-input");
}

static void
mark_hanging (PulseBackend *pulse)
{
    /* Mark devices and streams as hanging, ext-streams are handled separately */
    mark_hanging_hash (pulse->priv->devices);
    mark_hanging_hash (pulse->priv->streams);
}

static void
mark_hanging_hash (GHashTable *hash)
{
    GHashTableIter iter;
    gpointer       value;

    g_hash_table_iter_init (&iter, hash);

    while (g_hash_table_iter_next (&iter, NULL, &value) == TRUE)
        g_object_set_data (G_OBJECT (value), "backend-hanging", GUINT_TO_POINTER (1));
}

static void
unmark_hanging (PulseBackend *pulse, GObject *object)
{
    if (pulse->priv->connected_once == FALSE)
        return;
    if (pulse->priv->state == MATE_MIXER_STATE_READY)
        return;

    g_object_steal_data (object, "backend-hanging");
}

static void
remove_hanging (PulseBackend *pulse)
{
    GHashTableIter iter;
    gpointer       value;

    g_hash_table_iter_init (&iter, pulse->priv->devices);

    while (g_hash_table_iter_next (&iter, NULL, &value) == TRUE) {
        guint hanging =
            GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (value), "backend-hanging"));

        if (hanging == 1)
            remove_device (pulse, PULSE_DEVICE (value));
    }

    g_hash_table_iter_init (&iter, pulse->priv->streams);

    while (g_hash_table_iter_next (&iter, NULL, &value) == TRUE) {
        guint hanging =
            GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (value), "backend-hanging"));

        if (hanging == 1)
            remove_stream (pulse, PULSE_STREAM (value));
    }
}

static void
remove_device (PulseBackend *pulse, PulseDevice *device)
{
    gchar *name;

    name = g_strdup (mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));

    g_hash_table_remove (pulse->priv->devices,
                         GUINT_TO_POINTER (pulse_device_get_index (device)));

    g_signal_emit_by_name (G_OBJECT (pulse), "device-removed", name);
    g_free (name);
}

static void
remove_stream (PulseBackend *pulse, PulseStream *stream)
{
    gchar   *name;
    guint32  idx;
    gint64   index;
    gboolean reload = FALSE;

    /* The removed stream might be one of the default streams, this happens
     * especially when switching profiles, after which PulseAudio removes the
     * old streams and creates new ones with different names */
    if (MATE_MIXER_STREAM (stream) == pulse->priv->default_sink) {
        g_clear_object (&pulse->priv->default_sink);

        g_object_notify (G_OBJECT (pulse), "default-output");
        reload = TRUE;
    }
    else if (MATE_MIXER_STREAM (stream) == pulse->priv->default_source) {
        g_clear_object (&pulse->priv->default_source);

        g_object_notify (G_OBJECT (pulse), "default-input");
        reload = TRUE;
    }

    idx = pulse_stream_get_index (stream);

    if (PULSE_IS_SINK (stream))
        index = HASH_ID_SINK (idx);
    else if (PULSE_IS_SINK_INPUT (stream))
        index = HASH_ID_SINK_INPUT (idx);
    else if (PULSE_IS_SOURCE (stream))
        index = HASH_ID_SOURCE (idx);
    else
        index = HASH_ID_SOURCE_OUTPUT (idx);

    name = g_strdup (mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));

    g_hash_table_remove (pulse->priv->streams, &index);

    /* PulseAudio usually sends a server info update by itself when default
     * stream changes, but there is at least one case when it does not - setting
     * a card profile to off, so to be sure request an update explicitely */
    if (reload == TRUE)
        pulse_connection_load_server_info (pulse->priv->connection);

    g_signal_emit_by_name (G_OBJECT (pulse), "stream-removed", name);
    g_free (name);
}

static void
change_state (PulseBackend *backend, MateMixerState state)
{
    if (backend->priv->state == state)
        return;

    backend->priv->state = state;

    g_object_notify (G_OBJECT (backend), "state");
}

static gint
compare_devices (gconstpointer a, gconstpointer b)
{
    return strcmp (mate_mixer_device_get_name (MATE_MIXER_DEVICE (a)),
                   mate_mixer_device_get_name (MATE_MIXER_DEVICE (b)));
}

static gint
compare_streams (gconstpointer a, gconstpointer b)
{
    return strcmp (mate_mixer_stream_get_name (MATE_MIXER_STREAM (a)),
                   mate_mixer_stream_get_name (MATE_MIXER_STREAM (b)));
}

static gboolean
compare_stream_names (gpointer key, gpointer value, gpointer user_data)
{
    MateMixerStream *stream = MATE_MIXER_STREAM (value);

    return !strcmp (mate_mixer_stream_get_name (stream), (const gchar *) user_data);
}
