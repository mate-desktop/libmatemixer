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

#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

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
#define BACKEND_PRIORITY  100
#define BACKEND_FLAGS     (MATE_MIXER_BACKEND_HAS_APPLICATION_CONTROLS |        \
                           MATE_MIXER_BACKEND_HAS_STORED_CONTROLS |             \
                           MATE_MIXER_BACKEND_CAN_SET_DEFAULT_INPUT_STREAM |    \
                           MATE_MIXER_BACKEND_CAN_SET_DEFAULT_OUTPUT_STREAM)

struct _PulseBackendPrivate
{
    guint             connect_tag;
    gboolean          connected_once;
    GHashTable       *devices;
    GHashTable       *sinks;
    GHashTable       *sources;
    GHashTable       *sink_input_map;
    GHashTable       *source_output_map;
    GHashTable       *ext_streams;
    GList            *devices_list;
    GList            *streams_list;
    GList            *ext_streams_list;
    MateMixerAppInfo *app_info;
    gchar            *server_address;
    PulseConnection  *connection;
};

#define PULSE_CHANGE_STATE(p, s)        \
    (_mate_mixer_backend_set_state (MATE_MIXER_BACKEND (p), (s)))
#define PULSE_GET_DEFAULT_SINK(p)       \
    (mate_mixer_backend_get_default_output_stream (MATE_MIXER_BACKEND (p)))
#define PULSE_GET_DEFAULT_SOURCE(p)     \
    (mate_mixer_backend_get_default_input_stream (MATE_MIXER_BACKEND (p)))
#define PULSE_SET_DEFAULT_SINK(p, s)    \
    (_mate_mixer_backend_set_default_output_stream (MATE_MIXER_BACKEND (p), MATE_MIXER_STREAM (s)))
#define PULSE_SET_DEFAULT_SOURCE(p, s)  \
    (_mate_mixer_backend_set_default_input_stream (MATE_MIXER_BACKEND (p), MATE_MIXER_STREAM (s)))

#define PULSE_GET_PENDING_SINK(p)                                       \
        (g_object_get_data (G_OBJECT (p),                               \
                            "__matemixer_pulse_pending_sink"))          \

#define PULSE_SET_PENDING_SINK(p,name)                                  \
        (g_object_set_data_full (G_OBJECT (p),                          \
                                 "__matemixer_pulse_pending_sink",      \
                                 g_strdup (name),                       \
                                 g_free))

#define PULSE_SET_PENDING_SINK_NULL(p)                                  \
        (g_object_set_data (G_OBJECT (p),                               \
                            "__matemixer_pulse_pending_sink",           \
                            NULL))

#define PULSE_GET_PENDING_SOURCE(p)                                     \
        (g_object_get_data (G_OBJECT (p),                               \
                            "__matemixer_pulse_pending_source"))        \

#define PULSE_SET_PENDING_SOURCE(p,name)                                \
        (g_object_set_data_full (G_OBJECT (p),                          \
                                 "__matemixer_pulse_pending_source",    \
                                 g_strdup (name),                       \
                                 g_free))

#define PULSE_SET_PENDING_SOURCE_NULL(p)                                \
        (g_object_set_data (G_OBJECT (p),                               \
                            "__matemixer_pulse_pending_source",         \
                            NULL))

#define PULSE_GET_HANGING(o)                                            \
        ((gboolean) GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (o), "__matemixer_pulse_hanging")))

#define PULSE_SET_HANGING(o)                                            \
        (g_object_set_data (G_OBJECT (o),                               \
                            "__matemixer_pulse_hanging",                \
                            GUINT_TO_POINTER (1)))

#define PULSE_UNSET_HANGING(o)                                          \
        (g_object_steal_data (G_OBJECT (o),                             \
                              "__matemixer_pulse_hanging"))

static void pulse_backend_class_init     (PulseBackendClass *klass);
static void pulse_backend_class_finalize (PulseBackendClass *klass);

static void pulse_backend_init           (PulseBackend      *pulse);
static void pulse_backend_dispose        (GObject           *object);
static void pulse_backend_finalize       (GObject           *object);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
G_DEFINE_DYNAMIC_TYPE (PulseBackend, pulse_backend, MATE_MIXER_TYPE_BACKEND)
#pragma clang diagnostic pop

static gboolean         pulse_backend_open                      (MateMixerBackend *backend);
static void             pulse_backend_close                     (MateMixerBackend *backend);

static void             pulse_backend_set_app_info              (MateMixerBackend *backend,
                                                                 MateMixerAppInfo *info);

static void             pulse_backend_set_server_address        (MateMixerBackend *backend,
                                                                 const gchar      *address);

static const GList *    pulse_backend_list_devices              (MateMixerBackend *backend);
static const GList *    pulse_backend_list_streams              (MateMixerBackend *backend);
static const GList *    pulse_backend_list_stored_controls      (MateMixerBackend *backend);

static gboolean         pulse_backend_set_default_input_stream  (MateMixerBackend *backend,
                                                                 MateMixerStream  *stream);

static gboolean         pulse_backend_set_default_output_stream (MateMixerBackend *backend,
                                                                 MateMixerStream  *stream);

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

static gboolean         source_try_connect                  (PulseBackend                     *pulse);

static void             check_pending_sink                  (PulseBackend                     *pulse,
                                                             PulseStream                      *stream);
static void             check_pending_source                (PulseBackend                     *pulse,
                                                             PulseStream                      *stream);

static void             remove_sink_input                   (PulseBackend                     *backend,
                                                             PulseSink                        *sink,
                                                             guint                             index);
static void             remove_source_output                (PulseBackend                     *backend,
                                                             PulseSource                      *source,
                                                             guint                             index);

static void             free_list_devices                   (PulseBackend                     *pulse);
static void             free_list_streams                   (PulseBackend                     *pulse);
static void             free_list_ext_streams               (PulseBackend                     *pulse);

static gboolean         compare_stream_names                (gpointer                          key,
                                                             gpointer                          value,
                                                             gpointer                          user_data);

static MateMixerBackendInfo info;

void
backend_module_init (GTypeModule *module)
{
    pulse_backend_register_type (module);

    info.name          = BACKEND_NAME;
    info.priority      = BACKEND_PRIORITY;
    info.g_type        = PULSE_TYPE_BACKEND;
    info.backend_flags = BACKEND_FLAGS;
    info.backend_type  = MATE_MIXER_BACKEND_PULSEAUDIO;
}

const MateMixerBackendInfo *backend_module_get_info (void)
{
    return &info;
}

static void
pulse_backend_class_init (PulseBackendClass *klass)
{
    GObjectClass          *object_class;
    MateMixerBackendClass *backend_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose  = pulse_backend_dispose;
    object_class->finalize = pulse_backend_finalize;

    backend_class = MATE_MIXER_BACKEND_CLASS (klass);
    backend_class->set_app_info              = pulse_backend_set_app_info;
    backend_class->set_server_address        = pulse_backend_set_server_address;
    backend_class->open                      = pulse_backend_open;
    backend_class->close                     = pulse_backend_close;
    backend_class->list_devices              = pulse_backend_list_devices;
    backend_class->list_streams              = pulse_backend_list_streams;
    backend_class->list_stored_controls      = pulse_backend_list_stored_controls;
    backend_class->set_default_input_stream  = pulse_backend_set_default_input_stream;
    backend_class->set_default_output_stream = pulse_backend_set_default_output_stream;

    g_type_class_add_private (object_class, sizeof (PulseBackendPrivate));
}

/* Called in the code generated by G_DEFINE_DYNAMIC_TYPE() */
static void
pulse_backend_class_finalize (PulseBackendClass *klass)
{
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
    pulse->priv->sinks =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               g_object_unref);
    pulse->priv->sources =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               g_object_unref);

    pulse->priv->ext_streams =
        g_hash_table_new_full (g_str_hash,
                               g_str_equal,
                               g_free,
                               g_object_unref);

    pulse->priv->sink_input_map =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               g_object_unref);
    pulse->priv->source_output_map =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               g_object_unref);
}

static void
pulse_backend_dispose (GObject *object)
{
    MateMixerBackend *backend;
    MateMixerState    state;

    backend = MATE_MIXER_BACKEND (object);

    state = mate_mixer_backend_get_state (backend);
    if (state != MATE_MIXER_STATE_IDLE)
        pulse_backend_close (backend);

    G_OBJECT_CLASS (pulse_backend_parent_class)->dispose (object);
}

static void
pulse_backend_finalize (GObject *object)
{
    PulseBackend *pulse;

    pulse = PULSE_BACKEND (object);

    if (pulse->priv->app_info != NULL)
        _mate_mixer_app_info_free (pulse->priv->app_info);

    g_hash_table_unref (pulse->priv->devices);
    g_hash_table_unref (pulse->priv->sinks);
    g_hash_table_unref (pulse->priv->sources);
    g_hash_table_unref (pulse->priv->ext_streams);
    g_hash_table_unref (pulse->priv->sink_input_map);
    g_hash_table_unref (pulse->priv->source_output_map);

    G_OBJECT_CLASS (pulse_backend_parent_class)->finalize (object);
}

#define PULSE_APP_NAME(p)    (mate_mixer_app_info_get_name (p->priv->app_info))
#define PULSE_APP_ID(p)      (mate_mixer_app_info_get_id (p->priv->app_info))
#define PULSE_APP_VERSION(p) (mate_mixer_app_info_get_version (p->priv->app_info))
#define PULSE_APP_ICON(p)    (mate_mixer_app_info_get_icon (p->priv->app_info))

static gboolean
pulse_backend_open (MateMixerBackend *backend)
{
    PulseBackend    *pulse;
    PulseConnection *connection;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), FALSE);

    pulse = PULSE_BACKEND (backend);

    if G_UNLIKELY (pulse->priv->connection != NULL) {
        g_warn_if_reached ();
        return TRUE;
    }

    connection = pulse_connection_new (PULSE_APP_NAME (pulse),
                                       PULSE_APP_ID (pulse),
                                       PULSE_APP_VERSION (pulse),
                                       PULSE_APP_ICON (pulse),
                                       pulse->priv->server_address);

    /* No connection attempt is made during the construction of the connection,
     * but it sets up the PulseAudio structures, which might fail in an
     * unlikely case */
    if G_UNLIKELY (connection == NULL) {
        PULSE_CHANGE_STATE (pulse, MATE_MIXER_STATE_FAILED);
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

    PULSE_CHANGE_STATE (backend, MATE_MIXER_STATE_CONNECTING);

    /* Connect to the PulseAudio server, this might fail either instantly or
     * asynchronously, for example when remote connection timeouts */
    if (pulse_connection_connect (connection, FALSE) == FALSE) {
        g_object_unref (connection);
        PULSE_CHANGE_STATE (pulse, MATE_MIXER_STATE_FAILED);
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

    if (pulse->priv->connect_tag != 0) {
        g_source_remove (pulse->priv->connect_tag);
        pulse->priv->connect_tag = 0;
    }

    if (pulse->priv->connection != NULL) {
        g_signal_handlers_disconnect_by_data (G_OBJECT (pulse->priv->connection),
                                              pulse);

        g_clear_object (&pulse->priv->connection);
    }

    free_list_devices (pulse);
    free_list_streams (pulse);
    free_list_ext_streams (pulse);

    g_hash_table_remove_all (pulse->priv->devices);
    g_hash_table_remove_all (pulse->priv->sinks);
    g_hash_table_remove_all (pulse->priv->sources);
    g_hash_table_remove_all (pulse->priv->ext_streams);
    g_hash_table_remove_all (pulse->priv->sink_input_map);
    g_hash_table_remove_all (pulse->priv->source_output_map);

    pulse->priv->connected_once = FALSE;

    PULSE_CHANGE_STATE (pulse, MATE_MIXER_STATE_IDLE);
}

static void
pulse_backend_set_app_info (MateMixerBackend *backend, MateMixerAppInfo *info)
{
    PulseBackend *pulse;

    g_return_if_fail (PULSE_IS_BACKEND (backend));
    g_return_if_fail (info != NULL);

    pulse = PULSE_BACKEND (backend);

    if (pulse->priv->app_info != NULL)
        _mate_mixer_app_info_free (pulse->priv->app_info);

    pulse->priv->app_info = _mate_mixer_app_info_copy (info);
}

static void
pulse_backend_set_server_address (MateMixerBackend *backend, const gchar *address)
{
    g_return_if_fail (PULSE_IS_BACKEND (backend));

    g_free (PULSE_BACKEND (backend)->priv->server_address);

    PULSE_BACKEND (backend)->priv->server_address = g_strdup (address);
}

static const GList *
pulse_backend_list_devices (MateMixerBackend *backend)
{
    PulseBackend *pulse;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);

    pulse = PULSE_BACKEND (backend);

    if (pulse->priv->devices_list == NULL) {
        pulse->priv->devices_list = g_hash_table_get_values (pulse->priv->devices);
        if (pulse->priv->devices_list != NULL)
            g_list_foreach (pulse->priv->devices_list, (GFunc) g_object_ref, NULL);
    }
    return pulse->priv->devices_list;
}

static const GList *
pulse_backend_list_streams (MateMixerBackend *backend)
{
    PulseBackend *pulse;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);

    pulse = PULSE_BACKEND (backend);

    if (pulse->priv->streams_list == NULL) {
        GList *sinks;
        GList *sources;

        sinks = g_hash_table_get_values (pulse->priv->sinks);
        if (sinks != NULL)
            g_list_foreach (sinks, (GFunc) g_object_ref, NULL);

        sources = g_hash_table_get_values (pulse->priv->sources);
        if (sources != NULL)
            g_list_foreach (sources, (GFunc) g_object_ref, NULL);

        pulse->priv->streams_list = g_list_concat (sinks, sources);
    }
    return pulse->priv->streams_list;
}

static const GList *
pulse_backend_list_stored_controls (MateMixerBackend *backend)
{
    PulseBackend *pulse;

    g_return_val_if_fail (PULSE_IS_BACKEND (backend), NULL);

    pulse = PULSE_BACKEND (backend);

    if (pulse->priv->ext_streams_list == NULL) {
        pulse->priv->ext_streams_list = g_hash_table_get_values (pulse->priv->ext_streams);
        if (pulse->priv->ext_streams_list != NULL)
            g_list_foreach (pulse->priv->ext_streams_list, (GFunc) g_object_ref, NULL);
    }
    return pulse->priv->ext_streams_list;
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

    /* We might be in the process of setting a default source for which the details
     * are not yet known, make sure the change does not happen */
    PULSE_SET_PENDING_SOURCE_NULL (pulse);
    PULSE_SET_DEFAULT_SOURCE (pulse, stream);
    return TRUE;
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

    /* We might be in the process of setting a default sink for which the details
     * are not yet known, make sure the change does not happen */
    PULSE_SET_PENDING_SINK_NULL (pulse);
    PULSE_SET_DEFAULT_SINK (pulse, stream);
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
            PULSE_CHANGE_STATE (pulse, MATE_MIXER_STATE_CONNECTING);

            if G_UNLIKELY (pulse->priv->connect_tag != 0)
                break;

            if (pulse_connection_connect (connection, TRUE) == FALSE) {
                GSource *source;

                source = g_timeout_source_new (200);
                g_source_set_callback (source,
                                       (GSourceFunc) source_try_connect,
                                       pulse,
                                       NULL);
                pulse->priv->connect_tag =
                    g_source_attach (source, g_main_context_get_thread_default ());

                g_source_unref (source);
            }
            break;
        }

        /* First connection attempt has failed */
        PULSE_CHANGE_STATE (pulse, MATE_MIXER_STATE_FAILED);
        break;

    case PULSE_CONNECTION_CONNECTING:
    case PULSE_CONNECTION_AUTHORIZING:
    case PULSE_CONNECTION_LOADING:
        PULSE_CHANGE_STATE (pulse, MATE_MIXER_STATE_CONNECTING);
        break;

    case PULSE_CONNECTION_CONNECTED:
        pulse->priv->connected_once = TRUE;

        PULSE_CHANGE_STATE (pulse, MATE_MIXER_STATE_READY);
        break;
    }
}

static void
on_connection_server_info (PulseConnection      *connection,
                           const pa_server_info *info,
                           PulseBackend         *pulse)
{
    MateMixerStream *stream;
    const gchar     *name_source = NULL;
    const gchar     *name_sink = NULL;

    stream = PULSE_GET_DEFAULT_SOURCE (pulse);
    if (stream != NULL)
        name_source = mate_mixer_stream_get_name (stream);

    if (g_strcmp0 (name_source, info->default_source_name) != 0) {
        if (info->default_source_name != NULL) {
            MateMixerStream *stream = g_hash_table_find (pulse->priv->sources,
                                                         compare_stream_names,
                                                         (gpointer) info->default_source_name);

            /*
             * It is possible that we are unaware of the default stream as
             * the stream details might not have arrived yet.
             *
             * When this happens, remember the name of the stream and wait for
             * the stream info callback.
             */
            if (stream != NULL) {
                PULSE_SET_DEFAULT_SOURCE (pulse, stream);
                PULSE_SET_PENDING_SOURCE_NULL (pulse);
            } else {
                g_debug ("Default input stream changed to unknown stream %s",
                         info->default_source_name);

                PULSE_SET_PENDING_SOURCE (pulse, info->default_source_name);

                /* In most cases (for example changing profile) the stream info
                 * arrives by itself, but do not rely on it and request it
                 * explicitly */
                pulse_connection_load_source_info_name (pulse->priv->connection,
                                                        info->default_source_name);
            }
        } else
            PULSE_SET_DEFAULT_SOURCE (pulse, NULL);
    }

    stream = PULSE_GET_DEFAULT_SINK (pulse);
    if (stream != NULL)
        name_sink = mate_mixer_stream_get_name (stream);

    if (g_strcmp0 (name_sink, info->default_sink_name) != 0) {
        if (info->default_sink_name != NULL) {
            MateMixerStream *stream = g_hash_table_find (pulse->priv->sinks,
                                                         compare_stream_names,
                                                         (gpointer) info->default_sink_name);

            /*
             * It is possible that we are unaware of the default stream as
             * the stream details might not have arrived yet.
             *
             * When this happens, remember the name of the stream and wait for
             * the stream info callback.
             */
            if (stream != NULL) {
                PULSE_SET_DEFAULT_SINK (pulse, stream);
                PULSE_SET_PENDING_SINK_NULL (pulse);
            } else {
                g_debug ("Default output stream changed to unknown stream %s",
                         info->default_sink_name);

                PULSE_SET_PENDING_SINK (pulse, info->default_sink_name);

                /* In most cases (for example changing profile) the stream info
                 * arrives by itself, but do not rely on it and request it
                 * explicitly */
                pulse_connection_load_sink_info_name (pulse->priv->connection,
                                                      info->default_sink_name);
            }
        } else
            PULSE_SET_DEFAULT_SINK (pulse, NULL);
    }

    if (mate_mixer_backend_get_state (MATE_MIXER_BACKEND (pulse)) != MATE_MIXER_STATE_READY)
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

        g_hash_table_insert (pulse->priv->devices,
                             GUINT_TO_POINTER (info->index),
                             device);

        free_list_devices (pulse);
        g_signal_emit_by_name (G_OBJECT (pulse),
                               "device-added",
                               mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));
    } else
        pulse_device_update (device, info);
}

static void
on_connection_card_removed (PulseConnection *connection,
                            guint            index,
                            PulseBackend    *pulse)
{
    PulseDevice *device;
    gchar       *name;

    device = g_hash_table_lookup (pulse->priv->devices, GUINT_TO_POINTER (index));
    if G_UNLIKELY (device == NULL)
        return;

    name = g_strdup (mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));

    g_hash_table_remove (pulse->priv->devices, GUINT_TO_POINTER (index));

    free_list_devices (pulse);
    g_signal_emit_by_name (G_OBJECT (pulse),
                           "device-removed",
                           name);
    g_free (name);
}

static void
on_connection_sink_info (PulseConnection    *connection,
                         const pa_sink_info *info,
                         PulseBackend       *pulse)
{
    PulseDevice *device = NULL;
    PulseStream *stream;

    if (info->card != PA_INVALID_INDEX)
        device = g_hash_table_lookup (pulse->priv->devices,
                                      GUINT_TO_POINTER (info->card));

    stream = g_hash_table_lookup (pulse->priv->sinks, GUINT_TO_POINTER (info->index));
    if (stream == NULL) {
        stream = PULSE_STREAM (pulse_sink_new (connection, info, device));

        g_hash_table_insert (pulse->priv->sinks,
                             GUINT_TO_POINTER (info->index),
                             stream);

        free_list_streams (pulse);

        if (device != NULL) {
            pulse_device_add_stream (device, stream);
        } else {
            const gchar *name =
                mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream));

            /* Only emit when not a part of the device, otherwise emitted by
             * the main library */
            g_signal_emit_by_name (G_OBJECT (pulse),
                                   "stream-added",
                                   name);
        }
        /* We might be waiting for this sink to set it as the default */
        check_pending_sink (pulse, stream);
    } else
        pulse_sink_update (PULSE_SINK (stream), info);
}

static void
on_connection_sink_removed (PulseConnection *connection,
                            guint            idx,
                            PulseBackend    *pulse)
{
    PulseStream *stream;
    PulseDevice *device;

    stream = g_hash_table_lookup (pulse->priv->sinks, GUINT_TO_POINTER (idx));
    if G_UNLIKELY (stream == NULL)
        return;

    g_object_ref (stream);

    g_hash_table_remove (pulse->priv->sinks, GUINT_TO_POINTER (idx));
    free_list_streams (pulse);

    device = pulse_stream_get_device (stream);
    if (device != NULL) {
        pulse_device_remove_stream (device, stream);
    } else {
        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-removed",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    }

    /* The removed stream might be one of the default streams, this happens
     * especially when switching profiles, after which PulseAudio removes the
     * old streams and creates new ones with different names */
    if (MATE_MIXER_STREAM (stream) == PULSE_GET_DEFAULT_SINK (pulse)) {
        PULSE_SET_DEFAULT_SINK (pulse, NULL);

        /* PulseAudio usually sends a server info update by itself when default
         * stream changes, but there is at least one case when it does not - setting
         * a card profile to off, so to be sure request an update explicitely */
        pulse_connection_load_server_info (pulse->priv->connection);
    }
    g_object_unref (stream);
}

static void
on_connection_sink_input_info (PulseConnection          *connection,
                               const pa_sink_input_info *info,
                               PulseBackend             *pulse)
{
    PulseSink *sink = NULL;
    PulseSink *prev;

    if G_LIKELY (info->sink != PA_INVALID_INDEX)
        sink = g_hash_table_lookup (pulse->priv->sinks, GUINT_TO_POINTER (info->sink));

    if G_UNLIKELY (sink == NULL) {
        prev = g_hash_table_lookup (pulse->priv->sink_input_map, GUINT_TO_POINTER (info->index));
        if (prev != NULL) {
            g_debug ("Sink input %u moved from sink %s to an unknown sink %u, removing",
                     info->index,
                     mate_mixer_stream_get_name (MATE_MIXER_STREAM (prev)),
                     info->sink);

            remove_sink_input (pulse, prev, info->index);
        } else
            g_debug ("Sink input %u created on an unknown sink %u, ignoring",
                     info->index,
                     info->sink);
        return;
    }

    /* The sink input might have moved to a different sink */
    prev = g_hash_table_lookup (pulse->priv->sink_input_map, GUINT_TO_POINTER (info->index));
    if (prev != NULL && sink != prev) {
        g_debug ("Sink input moved from sink %s to %s",
                 mate_mixer_stream_get_name (MATE_MIXER_STREAM (prev)),
                 mate_mixer_stream_get_name (MATE_MIXER_STREAM (sink)));

        remove_sink_input (pulse, prev, info->index);
    }

    if (pulse_sink_add_input (sink, info) == TRUE)
        g_hash_table_insert (pulse->priv->sink_input_map,
                             GUINT_TO_POINTER (info->index),
                             g_object_ref (sink));
}

static void
on_connection_sink_input_removed (PulseConnection *connection,
                                  guint            idx,
                                  PulseBackend    *pulse)
{
    PulseSink *sink;

    sink = g_hash_table_lookup (pulse->priv->sink_input_map, GUINT_TO_POINTER (idx));
    if G_UNLIKELY (sink == NULL)
        return;

    remove_sink_input (pulse, sink, idx);
}

static void
on_connection_source_info (PulseConnection      *connection,
                           const pa_source_info *info,
                           PulseBackend         *pulse)
{
    PulseDevice *device = NULL;
    PulseStream *stream;

    if (info->card != PA_INVALID_INDEX)
        device = g_hash_table_lookup (pulse->priv->devices,
                                      GUINT_TO_POINTER (info->card));

    stream = g_hash_table_lookup (pulse->priv->sources, GUINT_TO_POINTER (info->index));
    if (stream == NULL) {
        stream = PULSE_STREAM (pulse_source_new (connection, info, device));

        g_hash_table_insert (pulse->priv->sources,
                             GUINT_TO_POINTER (info->index),
                             stream);

        free_list_streams (pulse);

        if (device != NULL) {
            pulse_device_add_stream (device, stream);
        } else {
            const gchar *name =
                mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream));

            /* Only emit when not a part of the device, otherwise emitted by
             * the main library */
            g_signal_emit_by_name (G_OBJECT (pulse),
                                   "stream-added",
                                   name);
        }
        /* We might be waiting for this source to set it as the default */
        check_pending_source (pulse, stream);
    } else
        pulse_source_update (PULSE_SOURCE (stream), info);
}

static void
on_connection_source_removed (PulseConnection *connection,
                              guint            idx,
                              PulseBackend    *pulse)
{
    PulseDevice *device;
    PulseStream *stream;

    stream = g_hash_table_lookup (pulse->priv->sources, GUINT_TO_POINTER (idx));
    if G_UNLIKELY (stream == NULL)
        return;

    g_object_ref (stream);

    g_hash_table_remove (pulse->priv->sources, GUINT_TO_POINTER (idx));
    free_list_streams (pulse);

    device = pulse_stream_get_device (stream);
    if (device != NULL) {
        pulse_device_remove_stream (device, stream);
    } else {
        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stream-removed",
                               mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream)));
    }

    /* The removed stream might be one of the default streams, this happens
     * especially when switching profiles, after which PulseAudio removes the
     * old streams and creates new ones with different names */
    if (MATE_MIXER_STREAM (stream) == PULSE_GET_DEFAULT_SOURCE (pulse)) {
        PULSE_SET_DEFAULT_SOURCE (pulse, NULL);

        /* PulseAudio usually sends a server info update by itself when default
         * stream changes, but there is at least one case when it does not - setting
         * a card profile to off, so to be sure request an update explicitely */
        pulse_connection_load_server_info (pulse->priv->connection);
    }
    g_object_unref (stream);
}

static void
on_connection_source_output_info (PulseConnection             *connection,
                                  const pa_source_output_info *info,
                                  PulseBackend                *pulse)
{
    PulseSource *source = NULL;
    PulseSource *prev;

    if G_LIKELY (info->source != PA_INVALID_INDEX)
        source = g_hash_table_lookup (pulse->priv->sources, GUINT_TO_POINTER (info->source));

    if G_UNLIKELY (source == NULL) {
        prev = g_hash_table_lookup (pulse->priv->source_output_map, GUINT_TO_POINTER (info->index));
        if (prev != NULL) {
            g_debug ("Source output %u moved from source %s to an unknown source %u, removing",
                     info->index,
                     mate_mixer_stream_get_name (MATE_MIXER_STREAM (prev)),
                     info->source);

            remove_source_output (pulse, prev, info->index);
        } else
            g_debug ("Source output %u created on an unknown source %u, ignoring",
                     info->index,
                     info->source);
        return;
    }

    /* The source output might have moved to a different source */
    prev = g_hash_table_lookup (pulse->priv->source_output_map, GUINT_TO_POINTER (info->index));
    if (prev != NULL && source != prev) {
        g_debug ("Source output moved from source %s to %s",
                 mate_mixer_stream_get_name (MATE_MIXER_STREAM (prev)),
                 mate_mixer_stream_get_name (MATE_MIXER_STREAM (source)));

        remove_source_output (pulse, prev, info->index);
    }

    if (pulse_source_add_output (source, info) == TRUE)
        g_hash_table_insert (pulse->priv->source_output_map,
                             GUINT_TO_POINTER (info->index),
                             g_object_ref (source));
}

static void
on_connection_source_output_removed (PulseConnection *connection,
                                     guint            idx,
                                     PulseBackend    *pulse)
{
    PulseSource *source;

    source = g_hash_table_lookup (pulse->priv->source_output_map, GUINT_TO_POINTER (idx));
    if G_UNLIKELY (source == NULL)
        return;

    remove_source_output (pulse, source, idx);
}

static void
on_connection_ext_stream_info (PulseConnection                  *connection,
                               const pa_ext_stream_restore_info *info,
                               PulseBackend                     *pulse)
{
    PulseExtStream *ext;
    PulseStream    *parent = NULL;

    if (info->device != NULL) {
        parent = g_hash_table_find (pulse->priv->sinks, compare_stream_names,
                                    (gpointer) info->device);

        if (parent == NULL)
            parent = g_hash_table_find (pulse->priv->sources, compare_stream_names,
                                        (gpointer) info->device);
    }

    ext = g_hash_table_lookup (pulse->priv->ext_streams, info->name);
    if (ext == NULL) {
        ext = pulse_ext_stream_new (connection, info, parent);

        g_hash_table_insert (pulse->priv->ext_streams,
                             g_strdup (info->name),
                             ext);

        free_list_ext_streams (pulse);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stored-control-added",
                               mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (ext)));
    } else {
        pulse_ext_stream_update (ext, info, parent);

        /* The object might be hanging if ext-streams are being loaded, remove
         * the hanging flag to prevent it from being removed */
        PULSE_UNSET_HANGING (ext);
    }
}

static void
on_connection_ext_stream_loading (PulseConnection *connection, PulseBackend *pulse)
{
    GHashTableIter iter;
    gpointer       ext;

    g_hash_table_iter_init (&iter, pulse->priv->ext_streams);

    while (g_hash_table_iter_next (&iter, NULL, &ext) == TRUE)
        PULSE_SET_HANGING (ext);
}

static void
on_connection_ext_stream_loaded (PulseConnection *connection, PulseBackend *pulse)
{
    GHashTableIter iter;
    gpointer       name;
    gpointer       ext;

    g_hash_table_iter_init (&iter, pulse->priv->ext_streams);

    while (g_hash_table_iter_next (&iter, &name, &ext) == TRUE) {
        if (PULSE_GET_HANGING (ext) == FALSE)
            continue;

        g_hash_table_iter_remove (&iter);
        free_list_ext_streams (pulse);

        g_signal_emit_by_name (G_OBJECT (pulse),
                               "stored-control-removed",
                               name);
    }
}

static gboolean
source_try_connect (PulseBackend *pulse)
{
    /* When the connect call succeeds, return FALSE to remove the source
     * and wait for the connection state notifications, otherwise this function
     * will be called again */
    if (pulse_connection_connect (pulse->priv->connection, TRUE) == TRUE) {
        pulse->priv->connect_tag = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static void
check_pending_sink (PulseBackend *pulse, PulseStream *stream)
{
    const gchar *pending;
    const gchar *name;

    /* See if the currently added sream matches the default input stream
     * we are waiting for */
    pending = PULSE_GET_PENDING_SINK (pulse);
    if (pending == NULL)
        return;

    name = mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream));
    if (g_strcmp0 (pending, name) != 0)
        return;

    g_debug ("Setting default output stream to pending stream %s", name);

    PULSE_SET_PENDING_SINK_NULL (pulse);
    PULSE_SET_DEFAULT_SINK (pulse, stream);
}

static void
check_pending_source (PulseBackend *pulse, PulseStream *stream)
{
    const gchar *pending;
    const gchar *name;

    /* See if the currently added sream matches the default input stream
     * we are waiting for */
    pending = PULSE_GET_PENDING_SOURCE (pulse);
    if (pending == NULL)
        return;

    name = mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream));
    if (g_strcmp0 (pending, name) != 0)
        return;

    g_debug ("Setting default input stream to pending stream %s", name);

    PULSE_SET_PENDING_SOURCE_NULL (pulse);
    PULSE_SET_DEFAULT_SOURCE (pulse, stream);
}

static void
remove_sink_input (PulseBackend *pulse, PulseSink *sink, guint index)
{
    pulse_sink_remove_input (sink, index);

    g_hash_table_remove (pulse->priv->sink_input_map, GUINT_TO_POINTER (index));
}

static void
remove_source_output (PulseBackend *pulse, PulseSource *source, guint index)
{
    pulse_source_remove_output (source, index);

    g_hash_table_remove (pulse->priv->source_output_map, GUINT_TO_POINTER (index));
}

static void
free_list_devices (PulseBackend *pulse)
{
    if (pulse->priv->devices_list == NULL)
        return;

    g_list_free_full (pulse->priv->devices_list, g_object_unref);

    pulse->priv->devices_list = NULL;
}

static void
free_list_streams (PulseBackend *pulse)
{
    if (pulse->priv->streams_list == NULL)
        return;

    g_list_free_full (pulse->priv->streams_list, g_object_unref);

    pulse->priv->streams_list = NULL;
}

static void
free_list_ext_streams (PulseBackend *pulse)
{
    if (pulse->priv->ext_streams_list == NULL)
        return;

    g_list_free_full (pulse->priv->ext_streams_list, g_object_unref);

    pulse->priv->ext_streams_list = NULL;
}

static gboolean
compare_stream_names (gpointer key, gpointer value, gpointer user_data)
{
    MateMixerStream *stream = MATE_MIXER_STREAM (value);

    return strcmp (mate_mixer_stream_get_name (stream), (const gchar *) user_data) == 0;
}
