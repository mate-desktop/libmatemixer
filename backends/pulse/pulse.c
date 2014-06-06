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

#include <libmatemixer/matemixer-backend.h>
#include <libmatemixer/matemixer-backend-module.h>

#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>

#include "pulse.h"
#include "pulse-connection.h"
#include "pulse-device.h"
#include "pulse-stream.h"
#include "pulse-sink.h"

#define BACKEND_NAME      "PulseAudio"
#define BACKEND_PRIORITY   0

struct _MateMixerPulsePrivate
{
    GHashTable               *devices;
    gboolean                  lists_loaded;
    GHashTable               *cards;
    GHashTable               *sinks;
    GHashTable               *sink_inputs;
    GHashTable               *sources;
    GHashTable               *source_outputs;
    MateMixerPulseConnection *connection;
};

/* Support function for dynamic loading of the backend module */
void  backend_module_init (GTypeModule *module);

const MateMixerBackendInfo *backend_module_get_info (void);

static void mate_mixer_backend_interface_init (MateMixerBackendInterface *iface);

static void pulse_card_cb (MateMixerPulseConnection *connection,
                            const pa_card_info *info,
                            MateMixerPulse *pulse);

static void pulse_sink_cb (MateMixerPulseConnection *connection,
                            const pa_sink_info *info,
                            MateMixerPulse *pulse);

static void pulse_sink_input_cb (MateMixerPulseConnection *connection,
                            const pa_sink_input_info *info,
                            MateMixerPulse *pulse);

static void pulse_source_cb (MateMixerPulseConnection *connection,
                            const pa_source_info *info,
                            MateMixerPulse *pulse);

static void pulse_source_output_cb (MateMixerPulseConnection *connection,
                            const pa_source_output_info *info,
                            MateMixerPulse *pulse);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (MateMixerPulse, mate_mixer_pulse,
                                G_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (MATE_MIXER_TYPE_BACKEND,
                                                               mate_mixer_backend_interface_init))

static MateMixerBackendInfo info;

void
backend_module_init (GTypeModule *module)
{
    mate_mixer_pulse_register_type (module);

    info.name         = BACKEND_NAME;
    info.priority     = BACKEND_PRIORITY;
    info.g_type       = MATE_MIXER_TYPE_PULSE;
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
    iface->open = mate_mixer_pulse_open;
    iface->close = mate_mixer_pulse_close;
    iface->list_devices = mate_mixer_pulse_list_devices;
}

static void
mate_mixer_pulse_init (MateMixerPulse *pulse)
{
    pulse->priv = G_TYPE_INSTANCE_GET_PRIVATE (
        pulse,
        MATE_MIXER_TYPE_PULSE,
        MateMixerPulsePrivate);

    pulse->priv->devices = g_hash_table_new_full (
        g_direct_hash,
        g_direct_equal,
        NULL,
        g_object_unref);

    pulse->priv->cards = g_hash_table_new_full (
        g_direct_hash,
        g_direct_equal,
        NULL,
        g_object_unref);
    pulse->priv->sinks = g_hash_table_new_full (
        g_direct_hash,
        g_direct_equal,
        NULL,
        g_object_unref);
    pulse->priv->sink_inputs = g_hash_table_new_full (
        g_direct_hash,
        g_direct_equal,
        NULL,
        g_object_unref);
    pulse->priv->sources = g_hash_table_new_full (
        g_direct_hash,
        g_direct_equal,
        NULL,
        g_object_unref);
    pulse->priv->source_outputs = g_hash_table_new_full (
        g_direct_hash,
        g_direct_equal,
        NULL,
        g_object_unref);
}

static void
mate_mixer_pulse_dispose (GObject *object)
{
    MateMixerPulse *pulse;

    pulse = MATE_MIXER_PULSE (object);

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

    G_OBJECT_CLASS (mate_mixer_pulse_parent_class)->dispose (object);
}

static void
mate_mixer_pulse_class_init (MateMixerPulseClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = mate_mixer_pulse_dispose;

    g_type_class_add_private (object_class, sizeof (MateMixerPulsePrivate));
}

/* Called in the code generated by G_DEFINE_DYNAMIC_TYPE_EXTENDED() */
static void
mate_mixer_pulse_class_finalize (MateMixerPulseClass *klass)
{
}

gboolean
mate_mixer_pulse_open (MateMixerBackend *backend)
{
    MateMixerPulse            *pulse;
    MateMixerPulseConnection  *connection;

    g_return_val_if_fail (MATE_MIXER_IS_PULSE (backend), FALSE);

    pulse = MATE_MIXER_PULSE (backend);

    g_return_val_if_fail (pulse->priv->connection == NULL, FALSE);

    connection = mate_mixer_pulse_connection_new (NULL, NULL);
    if (G_UNLIKELY (connection == NULL)) {
        g_object_unref (connection);
        return FALSE;
    }

    if (!mate_mixer_pulse_connection_connect (connection)) {
        g_object_unref (connection);
        return FALSE;
    }

    g_signal_connect (connection,
        "list-item-card",
        G_CALLBACK (pulse_card_cb),
        pulse);
    g_signal_connect (connection,
        "list-item-sink",
        G_CALLBACK (pulse_sink_cb),
        pulse);
    g_signal_connect (connection,
        "list-item-sink-input",
        G_CALLBACK (pulse_sink_input_cb),
        pulse);
    g_signal_connect (connection,
        "list-item-source",
        G_CALLBACK (pulse_source_cb),
        pulse);
    g_signal_connect (connection,
        "list-item-source-output",
        G_CALLBACK (pulse_source_output_cb),
        pulse);

    pulse->priv->connection = connection;
    return TRUE;
}

void
mate_mixer_pulse_close (MateMixerBackend *backend)
{
    MateMixerPulse *pulse;

    g_return_if_fail (MATE_MIXER_IS_PULSE (backend));

    pulse = MATE_MIXER_PULSE (backend);

    g_clear_object (&pulse->priv->connection);
}

static gboolean
pulse_load_lists (MateMixerPulse *pulse)
{
    /* The Pulse server is queried for initial lists, each of the functions
     * waits until the list is available and then continues with the next.
     *
     * One possible improvement would be to load the lists asynchronously right
     * after we connect to Pulse and when the application calls one of the
     * list_* () functions, check if the initial list is already available and
     * eventually wait until it's available. However, this would be
     * tricky with the way the Pulse API is currently used and might not
     * be beneficial at all.
     */

    // XXX figure out how to handle server reconnects, ideally everything
    // we know should be ditched and read again asynchronously and
    // the user should only be notified about actual differences
    // from the state before we were disconnected

    // mate_mixer_pulse_connection_get_server_info (pulse->priv->connection);
    mate_mixer_pulse_connection_get_card_list (pulse->priv->connection);
    mate_mixer_pulse_connection_get_sink_list (pulse->priv->connection);
    mate_mixer_pulse_connection_get_sink_input_list (pulse->priv->connection);
    mate_mixer_pulse_connection_get_source_list (pulse->priv->connection);
    mate_mixer_pulse_connection_get_source_output_list (pulse->priv->connection);

    return TRUE;
}

GList *
mate_mixer_pulse_list_devices (MateMixerBackend *backend)
{
    MateMixerPulse *pulse;
    GList          *list;

    g_return_val_if_fail (MATE_MIXER_IS_PULSE (backend), NULL);

    pulse = MATE_MIXER_PULSE (backend);

    if (!pulse->priv->lists_loaded)
        pulse_load_lists (pulse);

    list = g_hash_table_get_values (pulse->priv->devices);

    g_list_foreach (list, (GFunc) g_object_ref, NULL);
    return list;
}

GList *
mate_mixer_pulse_list_streams (MateMixerBackend *backend)
{
    // TODO
    return NULL;
}

static void
pulse_card_cb (MateMixerPulseConnection *connection,
                const pa_card_info *info,
                MateMixerPulse *pulse)
{
    MateMixerPulseDevice *device;

    device = mate_mixer_pulse_device_new (connection, info);
    if (G_LIKELY (device))
        g_hash_table_insert (
            pulse->priv->devices,
            GINT_TO_POINTER (mate_mixer_pulse_device_get_index (device)),
            device);
}

static void
pulse_sink_cb (MateMixerPulseConnection *connection,
                            const pa_sink_info *info,
                            MateMixerPulse *pulse)
{
    MateMixerPulseStream *stream;

    stream = mate_mixer_pulse_sink_new (connection, info);
    if (G_LIKELY (stream))
        g_hash_table_insert (
            pulse->priv->sinks,
            GINT_TO_POINTER (mate_mixer_pulse_stream_get_index (stream)),
            stream);
}

static void
pulse_sink_input_cb (MateMixerPulseConnection *connection,
                            const pa_sink_input_info *info,
                            MateMixerPulse *pulse)
{

}

static void
pulse_source_cb (MateMixerPulseConnection *connection,
                            const pa_source_info *info,
                            MateMixerPulse *pulse)
{

}

static void
pulse_source_output_cb (MateMixerPulseConnection *connection,
                            const pa_source_output_info *info,
                            MateMixerPulse *pulse)
{

}
