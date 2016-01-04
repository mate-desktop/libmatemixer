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

#include <unistd.h>
#include <sys/types.h>
#include <glib.h>
#include <glib-object.h>

#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>
#include <pulse/ext-stream-restore.h>

#include "pulse-connection.h"
#include "pulse-enums.h"
#include "pulse-enum-types.h"
#include "pulse-monitor.h"

struct _PulseConnectionPrivate
{
    gchar               *server;
    guint                outstanding;
    pa_context          *context;
    pa_proplist         *proplist;
    pa_glib_mainloop    *mainloop;
    gboolean             ext_streams_loading;
    gboolean             ext_streams_dirty;
    PulseConnectionState state;
};

enum {
    PROP_0,
    PROP_SERVER,
    PROP_STATE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
    SERVER_INFO,
    CARD_INFO,
    CARD_REMOVED,
    SINK_INFO,
    SINK_REMOVED,
    SOURCE_INFO,
    SOURCE_REMOVED,
    SINK_INPUT_INFO,
    SINK_INPUT_REMOVED,
    SOURCE_OUTPUT_INFO,
    SOURCE_OUTPUT_REMOVED,
    EXT_STREAM_LOADING,
    EXT_STREAM_LOADED,
    EXT_STREAM_INFO,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void pulse_connection_class_init   (PulseConnectionClass *klass);

static void pulse_connection_get_property (GObject              *object,
                                           guint                 param_id,
                                           GValue               *value,
                                           GParamSpec           *pspec);
static void pulse_connection_set_property (GObject              *object,
                                           guint                 param_id,
                                           const GValue         *value,
                                           GParamSpec           *pspec);

static void pulse_connection_init         (PulseConnection      *connection);
static void pulse_connection_finalize     (GObject              *object);

G_DEFINE_TYPE (PulseConnection, pulse_connection, G_TYPE_OBJECT);

static gchar    *create_app_name             (void);

static gboolean  load_lists                  (PulseConnection                  *connection);
static gboolean  load_list_finished          (PulseConnection                  *connection);

static void      pulse_state_cb              (pa_context                       *c,
                                              void                             *userdata);
static void      pulse_subscribe_cb          (pa_context                       *c,
                                              pa_subscription_event_type_t      t,
                                              uint32_t                          idx,
                                              void                             *userdata);

static void      pulse_restore_subscribe_cb  (pa_context                       *c,
                                              void                             *userdata);
static void      pulse_server_info_cb        (pa_context                       *c,
                                              const pa_server_info             *info,
                                              void                             *userdata);
static void      pulse_card_info_cb          (pa_context                       *c,
                                              const pa_card_info               *info,
                                              int                               eol,
                                              void                             *userdata);
static void      pulse_sink_info_cb          (pa_context                       *c,
                                              const pa_sink_info               *info,
                                              int                               eol,
                                              void                             *userdata);
static void      pulse_source_info_cb        (pa_context                       *c,
                                              const pa_source_info             *info,
                                              int                               eol,
                                              void                             *userdata);
static void      pulse_sink_input_info_cb    (pa_context                       *c,
                                              const pa_sink_input_info         *info,
                                              int                               eol,
                                              void                             *userdata);
static void      pulse_source_output_info_cb (pa_context                       *c,
                                              const pa_source_output_info      *info,
                                              int                               eol,
                                              void                             *userdata);
static void      pulse_ext_stream_restore_cb (pa_context                       *c,
                                              const pa_ext_stream_restore_info *info,
                                              int                               eol,
                                              void                             *userdata);

static void      change_state                (PulseConnection                  *connection,
                                              PulseConnectionState              state);

static gboolean  process_pulse_operation     (PulseConnection                  *connection,
                                              pa_operation                     *op);

static void
pulse_connection_class_init (PulseConnectionClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = pulse_connection_finalize;
    object_class->get_property = pulse_connection_get_property;
    object_class->set_property = pulse_connection_set_property;

    properties[PROP_SERVER] =
        g_param_spec_string ("server",
                             "Server",
                             "PulseAudio server to connect to",
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_STATE] =
        g_param_spec_enum ("state",
                           "State",
                           "Connection state",
                           PULSE_TYPE_CONNECTION_STATE,
                           PULSE_CONNECTION_DISCONNECTED,
                           G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    signals[SERVER_INFO] =
        g_signal_new ("server-info",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, server_info),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_POINTER);

    signals[CARD_INFO] =
        g_signal_new ("card-info",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, card_info),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_POINTER);

    signals[CARD_REMOVED] =
        g_signal_new ("card-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, card_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__UINT,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_UINT);

    signals[SINK_INFO] =
        g_signal_new ("sink-info",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, sink_info),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_POINTER);

    signals[SINK_REMOVED] =
        g_signal_new ("sink-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, sink_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__UINT,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_UINT);

    signals[SINK_INPUT_INFO] =
        g_signal_new ("sink-input-info",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, sink_input_info),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_POINTER);

    signals[SINK_INPUT_REMOVED] =
        g_signal_new ("sink-input-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, sink_input_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__UINT,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_UINT);

    signals[SOURCE_INFO] =
        g_signal_new ("source-info",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, source_info),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_POINTER);

    signals[SOURCE_REMOVED] =
        g_signal_new ("source-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, source_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__UINT,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_UINT);

    signals[SOURCE_OUTPUT_INFO] =
        g_signal_new ("source-output-info",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, source_output_info),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_POINTER);

    signals[SOURCE_OUTPUT_REMOVED] =
        g_signal_new ("source-output-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, source_output_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__UINT,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_UINT);

    signals[EXT_STREAM_LOADING] =
        g_signal_new ("ext-stream-loading",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, ext_stream_loading),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0,
                      G_TYPE_NONE);

    signals[EXT_STREAM_LOADED] =
        g_signal_new ("ext-stream-loaded",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, ext_stream_loaded),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0,
                      G_TYPE_NONE);

    signals[EXT_STREAM_INFO] =
        g_signal_new ("ext-stream-info",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseConnectionClass, ext_stream_info),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_POINTER);

    g_type_class_add_private (object_class, sizeof (PulseConnectionPrivate));
}

static void
pulse_connection_get_property (GObject    *object,
                               guint       param_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
    PulseConnection *connection;

    connection = PULSE_CONNECTION (object);

    switch (param_id) {
    case PROP_SERVER:
        g_value_set_string (value, connection->priv->server);
        break;
    case PROP_STATE:
        g_value_set_enum (value, connection->priv->state);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_connection_set_property (GObject      *object,
                               guint         param_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    PulseConnection *connection;

    connection = PULSE_CONNECTION (object);

    switch (param_id) {
    case PROP_SERVER:
        /* Construct-only string */
        connection->priv->server = g_strdup (g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_connection_init (PulseConnection *connection)
{
    connection->priv = G_TYPE_INSTANCE_GET_PRIVATE (connection,
                                                    PULSE_TYPE_CONNECTION,
                                                    PulseConnectionPrivate);
}

static void
pulse_connection_finalize (GObject *object)
{
    PulseConnection *connection;

    connection = PULSE_CONNECTION (object);

    g_free (connection->priv->server);

    if (connection->priv->context != NULL)
        pa_context_unref (connection->priv->context);

    pa_proplist_free (connection->priv->proplist);
    pa_glib_mainloop_free (connection->priv->mainloop);

    G_OBJECT_CLASS (pulse_connection_parent_class)->finalize (object);
}

PulseConnection *
pulse_connection_new (const gchar *app_name,
                      const gchar *app_id,
                      const gchar *app_version,
                      const gchar *app_icon,
                      const gchar *server_address)
{
    pa_glib_mainloop *mainloop;
    pa_proplist      *proplist;
    PulseConnection  *connection;

    mainloop = pa_glib_mainloop_new (g_main_context_get_thread_default ());
    if G_UNLIKELY (mainloop == NULL) {
        g_warning ("Failed to create PulseAudio main loop");
        return NULL;
    }

    /* Create a property list to hold information about the application,
     * the list will be kept with the connection as it will be reused later
     * when creating PulseAudio contexts and streams */
    proplist = pa_proplist_new ();
    if (app_name != NULL) {
        pa_proplist_sets (proplist, PA_PROP_APPLICATION_NAME, app_name);
    } else {
        /* Set a sensible default name when application does not provide one */
        gchar *name = create_app_name ();

        pa_proplist_sets (proplist, PA_PROP_APPLICATION_NAME, name);
        g_free (name);
    }
    if (app_id != NULL)
        pa_proplist_sets (proplist, PA_PROP_APPLICATION_ID, app_id);
    if (app_icon != NULL)
        pa_proplist_sets (proplist, PA_PROP_APPLICATION_ICON_NAME, app_icon);
    if (app_version != NULL)
        pa_proplist_sets (proplist, PA_PROP_APPLICATION_VERSION, app_version);

    connection = g_object_new (PULSE_TYPE_CONNECTION,
                               "server", server_address,
                               NULL);

    connection->priv->mainloop = mainloop;
    connection->priv->proplist = proplist;

    return connection;
}

gboolean
pulse_connection_connect (PulseConnection *connection, gboolean wait_for_daemon)
{
    pa_context         *context;
    pa_context_flags_t  flags = PA_CONTEXT_NOFLAGS;
    pa_mainloop_api    *mainloop;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_DISCONNECTED)
        return TRUE;

    mainloop = pa_glib_mainloop_get_api (connection->priv->mainloop);
    context  = pa_context_new_with_proplist (mainloop,
                                             NULL,
                                             connection->priv->proplist);
    if G_UNLIKELY (context == NULL) {
        g_warning ("Failed to create PulseAudio context");
        return FALSE;
    }

    /* Set function to monitor status changes */
    pa_context_set_state_callback (context,
                                   pulse_state_cb,
                                   connection);
    if (wait_for_daemon == TRUE)
        flags = PA_CONTEXT_NOFAIL;

    /* Initiate a connection, state changes will be delivered asynchronously */
    if (pa_context_connect (context,
                            connection->priv->server,
                            flags,
                            NULL) == 0) {
        connection->priv->context = context;
        change_state (connection, PULSE_CONNECTION_CONNECTING);
        return TRUE;
    }

    pa_context_unref (context);
    return FALSE;
}

void
pulse_connection_disconnect (PulseConnection *connection)
{
    g_return_if_fail (PULSE_IS_CONNECTION (connection));

    if (connection->priv->state == PULSE_CONNECTION_DISCONNECTED)
        return;

    if (connection->priv->context)
        pa_context_unref (connection->priv->context);

    connection->priv->context = NULL;
    connection->priv->outstanding = 0;
    connection->priv->ext_streams_loading = FALSE;
    connection->priv->ext_streams_dirty = FALSE;

    change_state (connection, PULSE_CONNECTION_DISCONNECTED);
}

PulseConnectionState
pulse_connection_get_state (PulseConnection *connection)
{
    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), PULSE_CONNECTION_DISCONNECTED);

    return connection->priv->state;
}

gboolean
pulse_connection_load_server_info (PulseConnection *connection)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_LOADING &&
        connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_get_server_info (connection->priv->context,
                                     pulse_server_info_cb,
                                     connection);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_load_card_info (PulseConnection *connection, guint32 index)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_LOADING &&
        connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    if (index == PA_INVALID_INDEX)
        op = pa_context_get_card_info_by_index (connection->priv->context,
                                                index,
                                                pulse_card_info_cb,
                                                connection);
    else
        op = pa_context_get_card_info_list (connection->priv->context,
                                            pulse_card_info_cb,
                                            connection);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_load_card_info_name (PulseConnection *connection, const gchar *name)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_LOADING &&
        connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_get_card_info_by_name (connection->priv->context,
                                           name,
                                           pulse_card_info_cb,
                                           connection);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_load_sink_info (PulseConnection *connection, guint32 index)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_LOADING &&
        connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    if (index == PA_INVALID_INDEX)
        op = pa_context_get_sink_info_by_index (connection->priv->context,
                                                index,
                                                pulse_sink_info_cb,
                                                connection);
    else
        op = pa_context_get_sink_info_list (connection->priv->context,
                                            pulse_sink_info_cb,
                                            connection);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_load_sink_info_name (PulseConnection *connection, const gchar *name)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_LOADING &&
        connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_get_sink_info_by_name (connection->priv->context,
                                           name,
                                           pulse_sink_info_cb,
                                           connection);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_load_sink_input_info (PulseConnection *connection, guint32 index)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_LOADING &&
        connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    if (index == PA_INVALID_INDEX)
        op = pa_context_get_sink_input_info (connection->priv->context,
                                             index,
                                             pulse_sink_input_info_cb,
                                             connection);
    else
        op = pa_context_get_sink_input_info_list (connection->priv->context,
                                                  pulse_sink_input_info_cb,
                                                  connection);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_load_source_info (PulseConnection *connection, guint32 index)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_LOADING &&
        connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    if (index == PA_INVALID_INDEX)
        op = pa_context_get_source_info_by_index (connection->priv->context,
                                                  index,
                                                  pulse_source_info_cb,
                                                  connection);
    else
        op = pa_context_get_source_info_list (connection->priv->context,
                                              pulse_source_info_cb,
                                              connection);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_load_source_info_name (PulseConnection *connection, const gchar *name)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_LOADING &&
        connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_get_source_info_by_name (connection->priv->context,
                                             name,
                                             pulse_source_info_cb,
                                             connection);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_load_source_output_info (PulseConnection *connection, guint32 index)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_LOADING &&
        connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    if (index == PA_INVALID_INDEX)
        op = pa_context_get_source_output_info (connection->priv->context,
                                                index,
                                                pulse_source_output_info_cb,
                                                connection);
    else
        op = pa_context_get_source_output_info_list (connection->priv->context,
                                                     pulse_source_output_info_cb,
                                                     connection);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_load_ext_stream_info (PulseConnection *connection)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_LOADING &&
        connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    /* When we receive a request to load the list of ext-streams, see if
     * loading is already in progress and if it is, wait until the current
     * loading finishes.
     * The PulseBackend class relies on this behaviour to ensure it always
     * contains a correct list of ext-streams, also PulseAudio always sends
     * a list of all streams in the database and these requests may arrive
     * very often, so this also optimizaes the amount of traffic. */
    if (connection->priv->ext_streams_loading == TRUE) {
        connection->priv->ext_streams_dirty = TRUE;
        return TRUE;
    }

    connection->priv->ext_streams_dirty = FALSE;
    connection->priv->ext_streams_loading = TRUE;
    g_signal_emit (G_OBJECT (connection),
                   signals[EXT_STREAM_LOADING],
                   0);

    op = pa_ext_stream_restore_read (connection->priv->context,
                                     pulse_ext_stream_restore_cb,
                                     connection);

    if (process_pulse_operation (connection, op) == FALSE) {
        connection->priv->ext_streams_loading = FALSE;

        g_signal_emit (G_OBJECT (connection),
                       signals[EXT_STREAM_LOADED],
                       0);
        return FALSE;
    }
    return TRUE;
}

PulseMonitor *
pulse_connection_create_monitor (PulseConnection *connection,
                                 guint32          index_source,
                                 guint32          index_sink_input)
{
    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return NULL;

    return pulse_monitor_new (connection->priv->context,
                              connection->priv->proplist,
                              index_source,
                              index_sink_input);
}

gboolean
pulse_connection_set_default_sink (PulseConnection *connection,
                                   const gchar     *name)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_default_sink (connection->priv->context,
                                      name,
                                      NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_set_default_source (PulseConnection *connection,
                                     const gchar     *name)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_default_source (connection->priv->context,
                                        name,
                                        NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_set_card_profile (PulseConnection *connection,
                                   const gchar     *card,
                                   const gchar     *profile)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (card != NULL, FALSE);
    g_return_val_if_fail (profile != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_card_profile_by_name (connection->priv->context,
                                              card,
                                              profile,
                                              NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_set_sink_mute (PulseConnection *connection,
                                guint32          index,
                                gboolean         mute)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_sink_mute_by_index (connection->priv->context,
                                            index,
                                            (int) mute,
                                            NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_set_sink_volume (PulseConnection  *connection,
                                  guint32           index,
                                  const pa_cvolume *volume)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (volume != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_sink_volume_by_index (connection->priv->context,
                                              index,
                                              volume,
                                              NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_set_sink_port (PulseConnection *connection,
                                guint32          index,
                                const gchar     *port)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (port != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_sink_port_by_index (connection->priv->context,
                                            index,
                                            port,
                                            NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_set_sink_input_mute (PulseConnection  *connection,
                                      guint32           index,
                                      gboolean          mute)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_sink_input_mute (connection->priv->context,
                                         index,
                                         (int) mute,
                                         NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_set_sink_input_volume (PulseConnection  *connection,
                                        guint32           index,
                                        const pa_cvolume *volume)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (volume != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_sink_input_volume (connection->priv->context,
                                           index,
                                           volume,
                                           NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_set_source_mute (PulseConnection *connection,
                                  guint32          index,
                                  gboolean         mute)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_source_mute_by_index (connection->priv->context,
                                              index,
                                              (int) mute,
                                              NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_set_source_volume (PulseConnection  *connection,
                                    guint32           index,
                                    const pa_cvolume *volume)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (volume != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_source_volume_by_index (connection->priv->context,
                                                index,
                                                volume,
                                                NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_set_source_port (PulseConnection *connection,
                                  guint32          index,
                                  const gchar     *port)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (port != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_source_port_by_index (connection->priv->context,
                                              index,
                                              port,
                                              NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_set_source_output_mute (PulseConnection *connection,
                                         guint32          index,
                                         gboolean         mute)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_source_output_mute (connection->priv->context,
                                            index,
                                            (int) mute,
                                            NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_set_source_output_volume (PulseConnection  *connection,
                                           guint32           index,
                                           const pa_cvolume *volume)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (volume != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_set_source_output_volume (connection->priv->context,
                                              index,
                                              volume,
                                              NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_suspend_sink (PulseConnection *connection,
                               guint32          index,
                               gboolean         suspend)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_suspend_sink_by_index (connection->priv->context,
                                           index,
                                           (int) suspend,
                                           NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_suspend_source (PulseConnection *connection,
                                 guint32          index,
                                 gboolean         suspend)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_suspend_source_by_index (connection->priv->context,
                                             index,
                                             (int) suspend,
                                             NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_move_sink_input (PulseConnection *connection,
                                  guint32          index,
                                  guint32          sink_index)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_move_sink_input_by_index (connection->priv->context,
                                              index,
                                              sink_index,
                                              NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_move_source_output (PulseConnection *connection,
                                     guint32          index,
                                     guint32          source_index)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_move_source_output_by_index (connection->priv->context,
                                                 index,
                                                 source_index,
                                                 NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_kill_sink_input (PulseConnection *connection,
                                  guint32          index)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_kill_sink_input (connection->priv->context,
                                     index,
                                     NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_kill_source_output (PulseConnection *connection,
                                     guint32          index)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_context_kill_source_output (connection->priv->context,
                                        index,
                                        NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_write_ext_stream (PulseConnection                  *connection,
                                   const pa_ext_stream_restore_info *info)
{
    pa_operation *op;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (info != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    op = pa_ext_stream_restore_write (connection->priv->context,
                                      PA_UPDATE_REPLACE,
                                      info, 1,
                                      TRUE,
                                      NULL, NULL);

    return process_pulse_operation (connection, op);
}

gboolean
pulse_connection_delete_ext_stream (PulseConnection *connection,
                                    const gchar     *name)
{
    pa_operation *op;
    gchar       **names;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    if (connection->priv->state != PULSE_CONNECTION_CONNECTED)
        return FALSE;

    names    = g_new (gchar *, 2);
    names[0] = (gchar *) name;
    names[1] = NULL;

    op = pa_ext_stream_restore_delete (connection->priv->context,
                                       (const char * const *) names,
                                       NULL, NULL);

    g_strfreev (names);

    return process_pulse_operation (connection, op);
}

static gchar *
create_app_name (void)
{
    const gchar *name_app;
    char         name_buf[256];

    /* Inspired by GStreamer's pulse plugin */
    name_app = g_get_application_name ();
    if (name_app != NULL)
        return g_strdup (name_app);

    if (pa_get_binary_name (name_buf, sizeof (name_buf)) != NULL)
        return g_strdup (name_buf);

    return g_strdup_printf ("libmatemixer-%lu", (gulong) getpid ());
}

static gboolean
load_lists (PulseConnection *connection)
{
    GSList       *ops = NULL;
    pa_operation *op;

    if G_UNLIKELY (connection->priv->outstanding > 0) {
        g_warn_if_reached ();
        return FALSE;
    }

    op = pa_context_get_card_info_list (connection->priv->context,
                                        pulse_card_info_cb,
                                        connection);
    if G_UNLIKELY (op == NULL)
        goto error;

    ops = g_slist_prepend (ops, op);

    op = pa_context_get_sink_info_list (connection->priv->context,
                                        pulse_sink_info_cb,
                                        connection);
    if G_UNLIKELY (op == NULL)
        goto error;

    ops = g_slist_prepend (ops, op);

    op = pa_context_get_sink_input_info_list (connection->priv->context,
                                              pulse_sink_input_info_cb,
                                              connection);
    if G_UNLIKELY (op == NULL)
        goto error;

    ops = g_slist_prepend (ops, op);

    op = pa_context_get_source_info_list (connection->priv->context,
                                          pulse_source_info_cb,
                                          connection);
    if G_UNLIKELY (op == NULL)
        goto error;

    ops = g_slist_prepend (ops, op);

    op = pa_context_get_source_output_info_list (connection->priv->context,
                                                 pulse_source_output_info_cb,
                                                 connection);
    if G_UNLIKELY (op == NULL)
        goto error;

    ops = g_slist_prepend (ops, op);

    connection->priv->outstanding = 5;

    /* This might not always be supported */
    op = pa_ext_stream_restore_read (connection->priv->context,
                                     pulse_ext_stream_restore_cb,
                                     connection);
    if (op != NULL) {
        ops = g_slist_prepend (ops, op);
        connection->priv->outstanding++;
    }

    g_slist_foreach (ops, (GFunc) pa_operation_unref, NULL);
    g_slist_free (ops);

    return TRUE;

error:
    g_slist_foreach (ops, (GFunc) pa_operation_cancel, NULL);
    g_slist_foreach (ops, (GFunc) pa_operation_unref, NULL);
    g_slist_free (ops);
    return FALSE;
}

static gboolean
load_list_finished (PulseConnection *connection)
{
    /* Decrement the number of outstanding requests as a list has just been
     * downloaded; when the number reaches 0, server information is requested
     * as the final step in the connection process */
    connection->priv->outstanding--;

    if G_UNLIKELY (connection->priv->outstanding < 0) {
        g_warn_if_reached ();
        connection->priv->outstanding = 0;
    }

    if (connection->priv->outstanding == 0) {
        gboolean ret = pulse_connection_load_server_info (connection);

        if G_UNLIKELY (ret == FALSE) {
            pulse_connection_disconnect (connection);
            return FALSE;
        }
    }

    return TRUE;
}

static void
pulse_state_cb (pa_context *c, void *userdata)
{
    PulseConnection    *connection;
    pa_context_state_t  state;

    connection = PULSE_CONNECTION (userdata);

    state = pa_context_get_state (c);

    if (state == PA_CONTEXT_READY) {
        pa_operation *op;

        if (connection->priv->state == PULSE_CONNECTION_LOADING ||
            connection->priv->state == PULSE_CONNECTION_CONNECTED) {
            g_warn_if_reached ();
            return;
        }

        /* We are connected, let's subscribe to notifications and load the
         * initial lists */
        pa_context_set_subscribe_callback (connection->priv->context,
                                           pulse_subscribe_cb,
                                           connection);
        pa_ext_stream_restore_set_subscribe_cb (connection->priv->context,
                                                pulse_restore_subscribe_cb,
                                                connection);

        op = pa_ext_stream_restore_subscribe (connection->priv->context,
                                              TRUE,
                                              NULL, NULL);

        /* Keep going if this operation fails */
        process_pulse_operation (connection, op);

        op = pa_context_subscribe (connection->priv->context,
                                   PA_SUBSCRIPTION_MASK_SERVER |
                                   PA_SUBSCRIPTION_MASK_CARD |
                                   PA_SUBSCRIPTION_MASK_SINK |
                                   PA_SUBSCRIPTION_MASK_SOURCE |
                                   PA_SUBSCRIPTION_MASK_SINK_INPUT |
                                   PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT,
                                   NULL, NULL);

        if (process_pulse_operation (connection, op) == TRUE) {
            change_state (connection, PULSE_CONNECTION_LOADING);

            if (load_lists (connection) == FALSE)
                state = PA_CONTEXT_FAILED;
        } else
            state = PA_CONTEXT_FAILED;
    }

    if (state == PA_CONTEXT_TERMINATED || state == PA_CONTEXT_FAILED) {
        /* We do not distinguish between failure and clean connection termination */
        pulse_connection_disconnect (connection);
        return;
    }

    if (state == PA_CONTEXT_CONNECTING)
        change_state (connection, PULSE_CONNECTION_CONNECTING);
    else if (state == PA_CONTEXT_AUTHORIZING ||
             state == PA_CONTEXT_SETTING_NAME)
        change_state (connection, PULSE_CONNECTION_AUTHORIZING);
}

static void
pulse_subscribe_cb (pa_context                   *c,
                    pa_subscription_event_type_t  t,
                    uint32_t                      idx,
                    void                         *userdata)
{
    PulseConnection *connection;

    connection = PULSE_CONNECTION (userdata);

    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
    case PA_SUBSCRIPTION_EVENT_SERVER:
        pulse_connection_load_server_info (connection);
        break;

    case PA_SUBSCRIPTION_EVENT_CARD:
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
            g_signal_emit (G_OBJECT (connection),
                           signals[CARD_REMOVED],
                           0,
                           idx);
        else
            pulse_connection_load_card_info (connection, idx);
        break;

    case PA_SUBSCRIPTION_EVENT_SINK:
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
            g_signal_emit (G_OBJECT (connection),
                           signals[SINK_REMOVED],
                           0,
                           idx);
        else
            pulse_connection_load_sink_info (connection, idx);
        break;

    case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
            g_signal_emit (G_OBJECT (connection),
                           signals[SINK_INPUT_REMOVED],
                           0,
                           idx);
        else
            pulse_connection_load_sink_input_info (connection, idx);
        break;

    case PA_SUBSCRIPTION_EVENT_SOURCE:
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
            g_signal_emit (G_OBJECT (connection),
                           signals[SOURCE_REMOVED],
                           0,
                           idx);
        else
            pulse_connection_load_source_info (connection, idx);
        break;

    case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
            g_signal_emit (G_OBJECT (connection),
                           signals[SOURCE_OUTPUT_REMOVED],
                           0,
                           idx);
        else
            pulse_connection_load_source_output_info (connection, idx);
        break;
    }
}

static void
pulse_restore_subscribe_cb (pa_context *c, void *userdata)
{
    PulseConnection *connection;

    connection = PULSE_CONNECTION (userdata);

    pulse_connection_load_ext_stream_info (connection);
}

static void
pulse_server_info_cb (pa_context           *c,
                      const pa_server_info *info,
                      void                 *userdata)
{
    PulseConnection *connection;

    connection = PULSE_CONNECTION (userdata);

    g_signal_emit (G_OBJECT (connection),
                   signals[SERVER_INFO],
                   0,
                   info);

    /* This notification may arrive at any time, but it also finalizes the
     * connection process */
    if (connection->priv->state == PULSE_CONNECTION_LOADING)
        change_state (connection, PULSE_CONNECTION_CONNECTED);
}

static void
pulse_card_info_cb (pa_context         *c,
                    const pa_card_info *info,
                    int                 eol,
                    void               *userdata)
{
    PulseConnection *connection;

    connection = PULSE_CONNECTION (userdata);

    if (eol) {
        if (connection->priv->state == PULSE_CONNECTION_LOADING)
            load_list_finished (connection);
        return;
    }

    g_signal_emit (G_OBJECT (connection),
                   signals[CARD_INFO],
                   0,
                   info);
}

static void
pulse_sink_info_cb (pa_context         *c,
                    const pa_sink_info *info,
                    int                 eol,
                    void               *userdata)
{
    PulseConnection *connection;

    connection = PULSE_CONNECTION (userdata);

    if (eol) {
        if (connection->priv->state == PULSE_CONNECTION_LOADING)
            load_list_finished (connection);
        return;
    }

    g_signal_emit (G_OBJECT (connection),
                   signals[SINK_INFO],
                   0,
                   info);
}

static void
pulse_sink_input_info_cb (pa_context               *c,
                          const pa_sink_input_info *info,
                          int                       eol,
                          void                     *userdata)
{
    PulseConnection *connection;

    connection = PULSE_CONNECTION (userdata);

    if (eol) {
        if (connection->priv->state == PULSE_CONNECTION_LOADING)
            load_list_finished (connection);
        return;
    }

    g_signal_emit (G_OBJECT (connection),
                   signals[SINK_INPUT_INFO],
                   0,
                   info);
}

static void
pulse_source_info_cb (pa_context           *c,
                      const pa_source_info *info,
                      int                   eol,
                      void                 *userdata)
{
    PulseConnection *connection;

    connection = PULSE_CONNECTION (userdata);

    if (eol) {
        if (connection->priv->state == PULSE_CONNECTION_LOADING)
            load_list_finished (connection);
        return;
    }

    g_signal_emit (G_OBJECT (connection),
                   signals[SOURCE_INFO],
                   0,
                   info);
}

static void
pulse_source_output_info_cb (pa_context                  *c,
                             const pa_source_output_info *info,
                             int                          eol,
                             void                        *userdata)
{
    PulseConnection *connection;

    connection = PULSE_CONNECTION (userdata);

    if (eol) {
        if (connection->priv->state == PULSE_CONNECTION_LOADING)
            load_list_finished (connection);
        return;
    }

    g_signal_emit (G_OBJECT (connection),
                   signals[SOURCE_OUTPUT_INFO],
                   0,
                   info);
}

static void
pulse_ext_stream_restore_cb (pa_context                       *c,
                             const pa_ext_stream_restore_info *info,
                             int                               eol,
                             void                             *userdata)
{
    PulseConnection *connection;

    connection = PULSE_CONNECTION (userdata);

    if (eol) {
        connection->priv->ext_streams_loading = FALSE;
        g_signal_emit (G_OBJECT (connection),
                       signals[EXT_STREAM_LOADED],
                       0);

        if (connection->priv->state == PULSE_CONNECTION_LOADING) {
            if (load_list_finished (connection) == FALSE)
                return;
        }

        if (connection->priv->ext_streams_dirty == TRUE)
            pulse_connection_load_ext_stream_info (connection);

        return;
    }

    g_signal_emit (G_OBJECT (connection),
                   signals[EXT_STREAM_INFO],
                   0,
                   info);
}

static void
change_state (PulseConnection *connection, PulseConnectionState state)
{
    if (connection->priv->state == state)
        return;

    connection->priv->state = state;

    g_object_notify_by_pspec (G_OBJECT (connection), properties[PROP_STATE]);
}

static gboolean
process_pulse_operation (PulseConnection *connection, pa_operation *op)
{
    if G_UNLIKELY (op == NULL) {
        g_warning ("PulseAudio operation failed: %s",
                   pa_strerror (pa_context_errno (connection->priv->context)));
        return FALSE;
    }

    pa_operation_unref (op);
    return TRUE;
}
