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
#include <glib/gi18n.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-device.h"
#include "pulse-monitor.h"
#include "pulse-port.h"
#include "pulse-port-switch.h"
#include "pulse-stream.h"
#include "pulse-sink.h"
#include "pulse-sink-control.h"
#include "pulse-sink-input.h"
#include "pulse-sink-switch.h"

struct _PulseSinkPrivate
{
    guint32           monitor;
    GHashTable       *inputs;
    GList            *inputs_list;
    PulsePortSwitch  *pswitch;
    GList            *pswitch_list;
    PulseSinkControl *control;
};

static void pulse_sink_class_init (PulseSinkClass *klass);
static void pulse_sink_init       (PulseSink      *sink);
static void pulse_sink_dispose    (GObject        *object);
static void pulse_sink_finalize   (GObject        *object);

G_DEFINE_TYPE (PulseSink, pulse_sink, PULSE_TYPE_STREAM);

static const GList *pulse_sink_list_controls (MateMixerStream *mms);
static const GList *pulse_sink_list_switches (MateMixerStream *mms);

static void         free_list_controls       (PulseSink       *sink);

static void
pulse_sink_class_init (PulseSinkClass *klass)
{
    GObjectClass         *object_class;
    MateMixerStreamClass *stream_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose  = pulse_sink_dispose;
    object_class->finalize = pulse_sink_finalize;

    stream_class = MATE_MIXER_STREAM_CLASS (klass);
    stream_class->list_controls = pulse_sink_list_controls;
    stream_class->list_switches = pulse_sink_list_switches;

    g_type_class_add_private (klass, sizeof (PulseSinkPrivate));
}

static void
pulse_sink_init (PulseSink *sink)
{
    sink->priv = G_TYPE_INSTANCE_GET_PRIVATE (sink,
                                              PULSE_TYPE_SINK,
                                              PulseSinkPrivate);

    sink->priv->inputs = g_hash_table_new_full (g_direct_hash,
                                                g_direct_equal,
                                                NULL,
                                                g_object_unref);

    sink->priv->monitor = PA_INVALID_INDEX;
}

static void
pulse_sink_dispose (GObject *object)
{
    PulseSink *sink;

    sink = PULSE_SINK (object);

    g_hash_table_remove_all (sink->priv->inputs);

    g_clear_object (&sink->priv->control);
    g_clear_object (&sink->priv->pswitch);

    free_list_controls (sink);

    if (sink->priv->pswitch_list != NULL) {
        g_list_free (sink->priv->pswitch_list);
        sink->priv->pswitch_list = NULL;
    }
    G_OBJECT_CLASS (pulse_sink_parent_class)->dispose (object);
}

static void
pulse_sink_finalize (GObject *object)
{
    PulseSink *sink;

    sink = PULSE_SINK (object);

    g_hash_table_unref (sink->priv->inputs);

    G_OBJECT_CLASS (pulse_sink_parent_class)->finalize (object);
}

PulseSink *
pulse_sink_new (PulseConnection    *connection,
                const pa_sink_info *info,
                PulseDevice        *device)
{
    PulseSink *sink;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);
    g_return_val_if_fail (device == NULL || PULSE_IS_DEVICE (device), NULL);

    sink = g_object_new (PULSE_TYPE_SINK,
                         "name", info->name,
                         "label", info->description,
                         "device", device,
                         "direction", MATE_MIXER_DIRECTION_OUTPUT,
                         "connection", connection,
                         "index", info->index,
                         NULL);

    sink->priv->control = pulse_sink_control_new (connection, info, sink);

    if (info->n_ports > 0) {
        pa_sink_port_info **ports = info->ports;

        /* Create the port switch */
        sink->priv->pswitch = pulse_sink_switch_new ("port", _("Connector"), sink);

        while (*ports != NULL) {
            pa_sink_port_info *p = *ports++;
            PulsePort         *port;
            const gchar       *icon = NULL;

            /* A port may include an icon but in PulseAudio sink and source ports
             * the property is not included, for this reason ports are also read from
             * devices where the icons may be present */
            if (device != NULL) {
                port = pulse_device_get_port (PULSE_DEVICE (device), p->name);
                if (port != NULL)
                    icon = mate_mixer_switch_option_get_icon (MATE_MIXER_SWITCH_OPTION (port));
            }

            port = pulse_port_new (p->name,
                                   p->description,
                                   icon,
                                   p->priority);

            pulse_port_switch_add_port (sink->priv->pswitch, port);

            if (p == info->active_port)
                pulse_port_switch_set_active_port (sink->priv->pswitch, port);
        }
        sink->priv->pswitch_list = g_list_prepend (NULL, sink->priv->pswitch);

        g_debug ("Created port list for sink %s", info->name);
    }

    pulse_sink_update (sink, info);

    _mate_mixer_stream_set_default_control (MATE_MIXER_STREAM (sink),
                                            MATE_MIXER_STREAM_CONTROL (sink->priv->control));
    return sink;
}

gboolean
pulse_sink_add_input (PulseSink *sink, const pa_sink_input_info *info)
{
    PulseSinkInput *input;

    g_return_val_if_fail (PULSE_IS_SINK (sink), FALSE);
    g_return_val_if_fail (info != NULL, FALSE);

    /* This function is used for both creating and refreshing sink inputs */
    input = g_hash_table_lookup (sink->priv->inputs, GUINT_TO_POINTER (info->index));
    if (input == NULL) {
        const gchar *name;
        PulseConnection *connection;

        connection = pulse_stream_get_connection (PULSE_STREAM (sink));
        input = pulse_sink_input_new (connection,
                                      info,
                                      sink);

        g_hash_table_insert (sink->priv->inputs,
                             GUINT_TO_POINTER (info->index),
                             input);

        free_list_controls (sink);

        name = mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (input));
        g_signal_emit_by_name (G_OBJECT (sink),
                               "control-added",
                               name);
        return TRUE;
    }

    pulse_sink_input_update (input, info);
    return FALSE;
}

void
pulse_sink_remove_input (PulseSink *sink, guint32 index)
{
    PulseSinkInput *input;
    gchar          *name;

    g_return_if_fail (PULSE_IS_SINK (sink));

    input = g_hash_table_lookup (sink->priv->inputs, GUINT_TO_POINTER (index));
    if G_UNLIKELY (input == NULL)
        return;

    name = g_strdup (mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (input)));

    g_hash_table_remove (sink->priv->inputs, GUINT_TO_POINTER (index));

    free_list_controls (sink);
    g_signal_emit_by_name (G_OBJECT (sink),
                           "control-removed",
                           name);
    g_free (name);
}

void
pulse_sink_update (PulseSink *sink, const pa_sink_info *info)
{
    g_return_if_fail (PULSE_IS_SINK (sink));
    g_return_if_fail (info != NULL);

    /* The switch doesn't allow being unset, PulseAudio should always include
     * the active port name if the are any ports available */
    if (info->active_port != NULL)
        pulse_port_switch_set_active_port_by_name (sink->priv->pswitch,
                                                   info->active_port->name);

    sink->priv->monitor = info->monitor_source;

    pulse_sink_control_update (sink->priv->control, info);
}

guint32
pulse_sink_get_index_monitor (PulseSink *sink)
{
    g_return_val_if_fail (PULSE_IS_SINK (sink), 0);

    return sink->priv->monitor;
}

static const GList *
pulse_sink_list_controls (MateMixerStream *mms)
{
    PulseSink *sink;

    g_return_val_if_fail (PULSE_IS_SINK (mms), NULL);

    sink = PULSE_SINK (mms);

    if (sink->priv->inputs_list == NULL) {
        sink->priv->inputs_list = g_hash_table_get_values (sink->priv->inputs);
        if (sink->priv->inputs_list != NULL)
            g_list_foreach (sink->priv->inputs_list, (GFunc) g_object_ref, NULL);

        sink->priv->inputs_list = g_list_prepend (sink->priv->inputs_list,
                                                  g_object_ref (sink->priv->control));
    }
    return sink->priv->inputs_list;
}

static const GList *
pulse_sink_list_switches (MateMixerStream *mms)
{
    g_return_val_if_fail (PULSE_IS_SINK (mms), NULL);

    return PULSE_SINK (mms)->priv->pswitch_list;
}

static void
free_list_controls (PulseSink *sink)
{
    if (sink->priv->inputs_list == NULL)
        return;

    g_list_free_full (sink->priv->inputs_list, g_object_unref);

    sink->priv->inputs_list = NULL;
}
