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
#include <sys/types.h>
#include <unistd.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"

struct _MateMixerPulseConnectionPrivate
{
    gchar                 *server;
    gboolean               reconnect;
    gboolean               connected;
    pa_context            *context;
    pa_threaded_mainloop  *mainloop;
};

enum {
    PROP_0,
    PROP_SERVER,
    PROP_RECONNECT,
    PROP_CONNECTED,
    N_PROPERTIES
};

enum {
    LIST_ITEM_CARD,
    LIST_ITEM_SINK,
    LIST_ITEM_SOURCE,
    LIST_ITEM_SINK_INPUT,
    LIST_ITEM_SOURCE_OUTPUT,
    CARD_ADDED,
    CARD_REMOVED,
    CARD_CHANGED,
    SINK_ADDED,
    SINK_REMOVED,
    SINK_CHANGED,
    SOURCE_ADDED,
    SOURCE_REMOVED,
    SOURCE_CHANGED,
    N_SIGNALS
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static guint signals[N_SIGNALS] = { 0, };

G_DEFINE_TYPE (MateMixerPulseConnection, mate_mixer_pulse_connection, G_TYPE_OBJECT);

static gchar *pulse_connection_get_name (void);

static gboolean pulse_connection_process_operation (MateMixerPulseConnection *connection,
                                                    pa_operation *o);

static void pulse_connection_state_cb (pa_context *c, void *userdata);

static void pulse_connection_subscribe_cb (pa_context *c,
                                           pa_subscription_event_type_t t,
                                            uint32_t idx,
                                            void *userdata);

static void pulse_connection_card_info_cb (pa_context *c,
                                           const pa_card_info *info,
                                           int eol,
                                           void *userdata);

static void pulse_connection_sink_info_cb (pa_context *c,
                                            const pa_sink_info *info,
                                            int eol,
                                            void *userdata);

static void pulse_connection_source_info_cb (pa_context *c,
                                             const pa_source_info *info,
                                             int eol,
                                             void *userdata);

static void pulse_connection_sink_input_info_cb (pa_context *c,
                                                const pa_sink_input_info *info,
                                                int eol,
                                                void *userdata);

static void pulse_connection_source_output_info_cb (pa_context *c,
                                                    const pa_source_output_info *info,
                                                    int eol,
                                                    void *userdata);

static void
mate_mixer_pulse_connection_init (MateMixerPulseConnection *connection)
{
    connection->priv = G_TYPE_INSTANCE_GET_PRIVATE (
        connection,
        MATE_MIXER_TYPE_PULSE_CONNECTION,
        MateMixerPulseConnectionPrivate);
}

static void
mate_mixer_pulse_connection_get_property (GObject     *object,
                                          guint        param_id,
                                          GValue      *value,
                                          GParamSpec  *pspec)
{
    MateMixerPulseConnection *connection;

    connection = MATE_MIXER_PULSE_CONNECTION (object);

    switch (param_id) {
    case PROP_SERVER:
        g_value_set_string (value, connection->priv->server);
        break;
    case PROP_RECONNECT:
        g_value_set_boolean (value, connection->priv->reconnect);
        break;
    case PROP_CONNECTED:
        g_value_set_boolean (value, connection->priv->connected);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_pulse_connection_set_property (GObject       *object,
                                          guint          param_id,
                                          const GValue  *value,
                                          GParamSpec    *pspec)
{
    MateMixerPulseConnection *connection;

    connection = MATE_MIXER_PULSE_CONNECTION (object);

    switch (param_id) {
    case PROP_SERVER:
        connection->priv->server = g_strdup (g_value_get_string (value));
        break;
    case PROP_RECONNECT:
        connection->priv->reconnect = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_pulse_connection_finalize (GObject *object)
{
    MateMixerPulseConnection *connection;

    connection = MATE_MIXER_PULSE_CONNECTION (object);

    g_free (connection->priv->server);

    G_OBJECT_CLASS (mate_mixer_pulse_connection_parent_class)->finalize (object);
}

static void
mate_mixer_pulse_connection_class_init (MateMixerPulseConnectionClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = mate_mixer_pulse_connection_finalize;
    object_class->get_property = mate_mixer_pulse_connection_get_property;
    object_class->set_property = mate_mixer_pulse_connection_set_property;

    properties[PROP_SERVER] = g_param_spec_string (
        "server",
        "Server",
        "PulseAudio server to connect to",
        NULL,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_RECONNECT] = g_param_spec_boolean (
        "reconnect",
        "Reconnect",
        "Try to reconnect when connection to PulseAudio server is lost",
        TRUE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_CONNECTED] = g_param_spec_boolean (
        "connected",
        "Connected",
        "Connected to a PulseAudio server or not",
        FALSE,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    signals[LIST_ITEM_CARD] = g_signal_new (
        "list-item-card",
        G_TYPE_FROM_CLASS (object_class),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET (MateMixerPulseConnectionClass, list_item_card),
        NULL,
        NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE,
        1,
        G_TYPE_POINTER);

    signals[LIST_ITEM_SINK] = g_signal_new (
        "list-item-sink",
        G_TYPE_FROM_CLASS (object_class),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET (MateMixerPulseConnectionClass, list_item_sink),
        NULL,
        NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE,
        1,
        G_TYPE_POINTER);

    signals[LIST_ITEM_SINK_INPUT] = g_signal_new (
        "list-item-sink-input",
        G_TYPE_FROM_CLASS (object_class),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET (MateMixerPulseConnectionClass, list_item_sink_input),
        NULL,
        NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE,
        1,
        G_TYPE_POINTER);

    signals[LIST_ITEM_SOURCE] = g_signal_new (
        "list-item-source",
        G_TYPE_FROM_CLASS (object_class),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET (MateMixerPulseConnectionClass, list_item_source),
        NULL,
        NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE,
        1,
        G_TYPE_POINTER);

    signals[LIST_ITEM_SOURCE_OUTPUT] = g_signal_new (
        "list-item-source-output",
        G_TYPE_FROM_CLASS (object_class),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET (MateMixerPulseConnectionClass, list_item_source_output),
        NULL,
        NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE,
        1,
        G_TYPE_POINTER);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    g_type_class_add_private (object_class, sizeof (MateMixerPulseConnectionPrivate));
}

// XXX: pass more info about application, provide API

MateMixerPulseConnection *
mate_mixer_pulse_connection_new (const gchar *server, const gchar *app_name)
{
    pa_threaded_mainloop      *mainloop;
    pa_context                *context;
    MateMixerPulseConnection  *connection;

    mainloop = pa_threaded_mainloop_new ();
    if (G_UNLIKELY (mainloop == NULL)) {
        g_warning ("Failed to create PulseAudio main loop");
        return NULL;
    }

    if (app_name != NULL) {
        context = pa_context_new (
            pa_threaded_mainloop_get_api (mainloop),
            app_name);
    } else {
        gchar *name = pulse_connection_get_name ();

        context = pa_context_new (
            pa_threaded_mainloop_get_api (mainloop),
            name);

        g_free (name);
    }

    if (G_UNLIKELY (context == NULL)) {
        g_warning ("Failed to create PulseAudio context");

        pa_threaded_mainloop_free (mainloop);
        return NULL;
    }

    connection = g_object_new (MATE_MIXER_TYPE_PULSE_CONNECTION,
        "server",    server,
        "reconnect", TRUE,
        NULL);

    connection->priv->mainloop = mainloop;
    connection->priv->context = context;

    return connection;
}

gboolean
mate_mixer_pulse_connection_connect (MateMixerPulseConnection *connection)
{
    int ret;
    pa_operation *o;

    g_return_val_if_fail (MATE_MIXER_IS_PULSE_CONNECTION (connection), FALSE);

    if (connection->priv->connected)
        return TRUE;

    /* Initiate a connection, this call does not guarantee the connection
     * to be established and usable */
    ret = pa_context_connect (connection->priv->context, NULL, PA_CONTEXT_NOFLAGS, NULL);
    if (ret < 0) {
        g_warning ("Failed to connect to PulseAudio server: %s", pa_strerror (ret));
        return FALSE;
    }

    pa_threaded_mainloop_lock (connection->priv->mainloop);

    /* Set callback for connection status changes; the callback is not really
     * used when connecting the first time, it is only needed to signal
     * a status change */
    pa_context_set_state_callback (connection->priv->context,
        pulse_connection_state_cb,
        connection);

    ret = pa_threaded_mainloop_start (connection->priv->mainloop);
    if (ret < 0) {
        g_warning ("Failed to start PulseAudio main loop: %s", pa_strerror (ret));

        pa_context_disconnect (connection->priv->context);
        pa_threaded_mainloop_unlock (connection->priv->mainloop);
        return FALSE;
    }

    while (TRUE) {
        /* Wait for a connection state which tells us whether the connection
         * has been established or has failed */
        pa_context_state_t state =
            pa_context_get_state (connection->priv->context);

        if (state == PA_CONTEXT_READY)
            break;

        if (state == PA_CONTEXT_FAILED ||
            state == PA_CONTEXT_TERMINATED) {
            g_warning ("Failed to connect to PulseAudio server: %s",
                pa_strerror (pa_context_errno (connection->priv->context)));

            pa_context_disconnect (connection->priv->context);
            pa_threaded_mainloop_unlock (connection->priv->mainloop);
            return FALSE;
        }
        pa_threaded_mainloop_wait (connection->priv->mainloop);
    }

    pa_context_set_subscribe_callback (connection->priv->context,
        pulse_connection_subscribe_cb,
        connection);

    // XXX don't want notifications before the initial lists are downloaded

    o = pa_context_subscribe (connection->priv->context,
        PA_SUBSCRIPTION_MASK_CARD |
        PA_SUBSCRIPTION_MASK_SINK |
        PA_SUBSCRIPTION_MASK_SOURCE |
        PA_SUBSCRIPTION_MASK_SINK_INPUT |
        PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT,
        NULL, NULL);
    if (o == NULL)
        g_warning ("Failed to subscribe to PulseAudio notifications: %s",
            pa_strerror (pa_context_errno (connection->priv->context)));
    else
        pa_operation_unref (o);

    pa_threaded_mainloop_unlock (connection->priv->mainloop);

    connection->priv->connected = TRUE;

    g_object_notify_by_pspec (G_OBJECT (connection), properties[PROP_CONNECTED]);
    return TRUE;
}

void
mate_mixer_pulse_connection_disconnect (MateMixerPulseConnection *connection)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_CONNECTION (connection), FALSE);

    if (!connection->priv->connected)
        return;

    pa_context_disconnect (connection->priv->context);

    connection->priv->connected = FALSE;

    g_object_notify_by_pspec (G_OBJECT (connection), properties[PROP_CONNECTED]);
}

gboolean
mate_mixer_pulse_connection_get_server_info (MateMixerPulseConnection *connection)
{
    // TODO
    return TRUE;
}

gboolean
mate_mixer_pulse_connection_get_card_list (MateMixerPulseConnection *connection)
{
    pa_operation *o;
    gboolean ret;

    g_return_val_if_fail (MATE_MIXER_IS_PULSE_CONNECTION (connection), FALSE);

    pa_threaded_mainloop_lock (connection->priv->mainloop);

    o = pa_context_get_card_info_list (
        connection->priv->context,
        pulse_connection_card_info_cb,
        connection);

    ret = pulse_connection_process_operation (connection, o);

    pa_threaded_mainloop_unlock (connection->priv->mainloop);
    return ret;
}

gboolean
mate_mixer_pulse_connection_get_sink_list (MateMixerPulseConnection *connection)
{
    pa_operation *o;
    gboolean ret;

    g_return_val_if_fail (MATE_MIXER_IS_PULSE_CONNECTION (connection), FALSE);

    pa_threaded_mainloop_lock (connection->priv->mainloop);

    o = pa_context_get_sink_info_list (
        connection->priv->context,
        pulse_connection_sink_info_cb,
        connection);

    ret = pulse_connection_process_operation (connection, o);

    pa_threaded_mainloop_unlock (connection->priv->mainloop);
    return ret;
}

gboolean
mate_mixer_pulse_connection_get_sink_input_list (MateMixerPulseConnection *connection)
{
    pa_operation *o;
    gboolean ret;

    g_return_val_if_fail (MATE_MIXER_IS_PULSE_CONNECTION (connection), FALSE);

    pa_threaded_mainloop_lock (connection->priv->mainloop);

    o = pa_context_get_sink_input_info_list (
        connection->priv->context,
        pulse_connection_sink_input_info_cb,
        connection);

    ret = pulse_connection_process_operation (connection, o);

    pa_threaded_mainloop_unlock (connection->priv->mainloop);
    return ret;
}

gboolean
mate_mixer_pulse_connection_get_source_list (MateMixerPulseConnection *connection)
{
    pa_operation  *o;
    gboolean       ret;

    g_return_val_if_fail (MATE_MIXER_IS_PULSE_CONNECTION (connection), FALSE);

    pa_threaded_mainloop_lock (connection->priv->mainloop);

    o = pa_context_get_source_info_list (
        connection->priv->context,
        pulse_connection_source_info_cb,
        connection);

    ret = pulse_connection_process_operation (connection, o);

    pa_threaded_mainloop_unlock (connection->priv->mainloop);
    return ret;
}

gboolean
mate_mixer_pulse_connection_get_source_output_list (MateMixerPulseConnection *connection)
{
    pa_operation *o;
    gboolean ret;

    g_return_val_if_fail (MATE_MIXER_IS_PULSE_CONNECTION (connection), FALSE);

    pa_threaded_mainloop_lock (connection->priv->mainloop);

    o = pa_context_get_source_output_info_list (
        connection->priv->context,
        pulse_connection_source_output_info_cb,
        connection);

    ret = pulse_connection_process_operation (connection, o);

    pa_threaded_mainloop_unlock (connection->priv->mainloop);
    return ret;
}

gboolean
mate_mixer_pulse_connection_set_card_profile (MateMixerPulseConnection *connection,
                                              const gchar              *card,
                                              const gchar              *profile)
{
    pa_operation *o;
    gboolean ret;

    g_return_val_if_fail (MATE_MIXER_IS_PULSE_CONNECTION (connection), FALSE);

    pa_threaded_mainloop_lock (connection->priv->mainloop);

    o = pa_context_set_card_profile_by_name (
        connection->priv->context,
        card,
        profile,
        NULL, NULL);

    // XXX maybe shouldn't wait for the completion

    ret = pulse_connection_process_operation (connection, o);

    pa_threaded_mainloop_unlock (connection->priv->mainloop);
    return ret;
}

gboolean
mate_mixer_pulse_connection_set_sink_mute (MateMixerPulseConnection *connection,
                                           guint32 index,
                                           gboolean mute)
{
    pa_operation *o;
    gboolean ret;

    g_return_val_if_fail (MATE_MIXER_IS_PULSE_CONNECTION (connection), FALSE);

    pa_threaded_mainloop_lock (connection->priv->mainloop);

    o = pa_context_set_sink_mute_by_index (
        connection->priv->context,
        index,
        (int) mute,
        NULL, NULL);

    // XXX maybe shouldn't wait for the completion

    ret = pulse_connection_process_operation (connection, o);

    pa_threaded_mainloop_unlock (connection->priv->mainloop);
    return ret;
}

static gboolean
pulse_connection_process_operation (MateMixerPulseConnection *connection,
                                    pa_operation *o)
{
    if (o == NULL) {
        g_warning ("Failed to process PulseAudio operation: %s",
            pa_strerror (pa_context_errno (connection->priv->context)));

        return FALSE;
    }

    while (pa_operation_get_state (o) == PA_OPERATION_RUNNING)
        pa_threaded_mainloop_wait (connection->priv->mainloop);

    pa_operation_unref (o);
    return TRUE;
}

static gchar *
pulse_connection_get_name (void)
{
    const char *name_app;
    char        name_buf[256];

    /* Inspired by GStreamer's pulse plugin */
    name_app = g_get_application_name ();
    if (name_app != NULL)
        return g_strdup (name_app);

    if (pa_get_binary_name (name_buf, sizeof (name_buf)) != NULL)
        return g_strdup (name_buf);

    return g_strdup_printf ("libmatemixer-%lu", (gulong) getpid ());
}

static void
pulse_connection_state_cb (pa_context *c, void *userdata)
{
    MateMixerPulseConnection  *connection;
    pa_context_state_t         state;

    connection = MATE_MIXER_PULSE_CONNECTION (userdata);

    state = pa_context_get_state (c);
    switch (state) {
    case PA_CONTEXT_READY:
        /* The connection is established, the context is ready to
         * execute operations. */
        if (!connection->priv->connected) {
            connection->priv->connected = TRUE;

            g_object_notify_by_pspec (
                G_OBJECT (connection),
                properties[PROP_CONNECTED]);
        }
        break;

    case PA_CONTEXT_TERMINATED:
        /* The connection was terminated cleanly. */
        if (connection->priv->connected) {
            connection->priv->connected = FALSE;

            g_object_notify_by_pspec (
                G_OBJECT (connection),
                properties[PROP_CONNECTED]);

            pa_context_disconnect (connection->priv->context);
        }
        break;

    case PA_CONTEXT_FAILED:
        break;

    default:
        break;
    }

    pa_threaded_mainloop_signal (connection->priv->mainloop, 0);
}

static void
pulse_connection_subscribe_cb (pa_context *c,
                               pa_subscription_event_type_t t,
                               uint32_t idx,
                               void *userdata)
{
    // TODO
}

static void
pulse_connection_card_info_cb (pa_context *c,
                               const pa_card_info *info,
                               int eol,
                               void *userdata)
{
    MateMixerPulseConnection *connection;

    connection = MATE_MIXER_PULSE_CONNECTION (userdata);

    if (!eol)
        g_signal_emit (G_OBJECT (connection),
            signals[LIST_ITEM_CARD],
            0,
            info);

    pa_threaded_mainloop_signal (connection->priv->mainloop, 0);
}

static void
pulse_connection_sink_info_cb (pa_context *c,
                               const pa_sink_info *info,
                               int eol,
                               void *userdata)
{
    MateMixerPulseConnection *connection;

    connection = MATE_MIXER_PULSE_CONNECTION (userdata);

    if (!eol)
        g_signal_emit (G_OBJECT (connection),
            signals[LIST_ITEM_SINK],
            0,
            info);

    pa_threaded_mainloop_signal (connection->priv->mainloop, 0);
}

static void
pulse_connection_sink_input_info_cb (pa_context *c,
                                     const pa_sink_input_info *info,
                                     int eol,
                                     void *userdata)
{
    MateMixerPulseConnection *connection;

    connection = MATE_MIXER_PULSE_CONNECTION (userdata);

    if (!eol)
        g_signal_emit (G_OBJECT (connection),
            signals[LIST_ITEM_SINK_INPUT],
            0,
            info);

    pa_threaded_mainloop_signal (connection->priv->mainloop, 0);
}

static void
pulse_connection_source_info_cb (pa_context *c,
                                 const pa_source_info *info,
                                 int eol,
                                 void *userdata)
{
    MateMixerPulseConnection *connection;

    connection = MATE_MIXER_PULSE_CONNECTION (userdata);

    if (!eol)
        g_signal_emit (G_OBJECT (connection),
            signals[LIST_ITEM_SOURCE],
            0,
            info);

    pa_threaded_mainloop_signal (connection->priv->mainloop, 0);
}

static void
pulse_connection_source_output_info_cb (pa_context *c,
                                        const pa_source_output_info *info,
                                        int eol,
                                        void *userdata)
{
    MateMixerPulseConnection *connection;

    connection = MATE_MIXER_PULSE_CONNECTION (userdata);

    if (!eol)
        g_signal_emit (G_OBJECT (connection),
            signals[LIST_ITEM_SOURCE_OUTPUT],
            0,
            info);

    pa_threaded_mainloop_signal (connection->priv->mainloop, 0);
}
