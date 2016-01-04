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
#include "pulse-source.h"
#include "pulse-source-control.h"
#include "pulse-source-output.h"
#include "pulse-source-switch.h"

struct _PulseSourcePrivate
{
    GHashTable         *outputs;
    GList              *outputs_list;
    PulsePortSwitch    *pswitch;
    GList              *pswitch_list;
    PulseSourceControl *control;
};

static void pulse_source_class_init (PulseSourceClass *klass);
static void pulse_source_init       (PulseSource      *source);
static void pulse_source_dispose    (GObject          *object);
static void pulse_source_finalize   (GObject          *object);

G_DEFINE_TYPE (PulseSource, pulse_source, PULSE_TYPE_STREAM);

static const GList *pulse_source_list_controls (MateMixerStream *mms);
static const GList *pulse_source_list_switches (MateMixerStream *mms);

static void         free_list_controls         (PulseSource     *source);

static void
pulse_source_class_init (PulseSourceClass *klass)
{
    GObjectClass         *object_class;
    MateMixerStreamClass *stream_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose  = pulse_source_dispose;
    object_class->finalize = pulse_source_finalize;

    stream_class = MATE_MIXER_STREAM_CLASS (klass);
    stream_class->list_controls = pulse_source_list_controls;
    stream_class->list_switches = pulse_source_list_switches;

    g_type_class_add_private (klass, sizeof (PulseSourcePrivate));
}

static void
pulse_source_init (PulseSource *source)
{
    source->priv = G_TYPE_INSTANCE_GET_PRIVATE (source,
                                                PULSE_TYPE_SOURCE,
                                                PulseSourcePrivate);

    source->priv->outputs = g_hash_table_new_full (g_direct_hash,
                                                   g_direct_equal,
                                                   NULL,
                                                   g_object_unref);
}

static void
pulse_source_dispose (GObject *object)
{
    PulseSource *source;

    source = PULSE_SOURCE (object);

    g_hash_table_remove_all (source->priv->outputs);

    g_clear_object (&source->priv->control);
    g_clear_object (&source->priv->pswitch);

    free_list_controls (source);

    if (source->priv->pswitch_list != NULL) {
        g_list_free (source->priv->pswitch_list);
        source->priv->pswitch_list = NULL;
    }
    G_OBJECT_CLASS (pulse_source_parent_class)->dispose (object);
}

static void
pulse_source_finalize (GObject *object)
{
    PulseSource *source;

    source = PULSE_SOURCE (object);

    g_hash_table_unref (source->priv->outputs);

    G_OBJECT_CLASS (pulse_source_parent_class)->finalize (object);
}

PulseSource *
pulse_source_new (PulseConnection      *connection,
                  const pa_source_info *info,
                  PulseDevice          *device)
{
    PulseSource *source;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);
    g_return_val_if_fail (device == NULL || PULSE_IS_DEVICE (device), NULL);

    source = g_object_new (PULSE_TYPE_SOURCE,
                           "name", info->name,
                           "label", info->description,
                           "device", device,
                           "direction", MATE_MIXER_DIRECTION_INPUT,
                           "connection", connection,
                           "index", info->index,
                           NULL);

    source->priv->control = pulse_source_control_new (connection, info, source);

    if (info->n_ports > 0) {
        pa_source_port_info **ports = info->ports;

        /* Create the port switch */
        source->priv->pswitch = pulse_source_switch_new ("port", _("Connector"), source);

        while (*ports != NULL) {
            pa_source_port_info *p = *ports++;
            PulsePort           *port;
            const gchar         *icon = NULL;

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

            pulse_port_switch_add_port (source->priv->pswitch, port);

            if (p == info->active_port)
                pulse_port_switch_set_active_port (source->priv->pswitch, port);
        }
        source->priv->pswitch_list = g_list_prepend (NULL, source->priv->pswitch);

        g_debug ("Created port list for source %s", info->name);
    }

    pulse_source_update (source, info);

    _mate_mixer_stream_set_default_control (MATE_MIXER_STREAM (source),
                                            MATE_MIXER_STREAM_CONTROL (source->priv->control));
    return source;
}

gboolean
pulse_source_add_output (PulseSource *source, const pa_source_output_info *info)
{
    PulseSourceOutput *output;

    g_return_val_if_fail (PULSE_IS_SOURCE (source), FALSE);
    g_return_val_if_fail (info != NULL, FALSE);

    /* This function is used for both creating and refreshing source outputs */
    output = g_hash_table_lookup (source->priv->outputs, GUINT_TO_POINTER (info->index));
    if (output == NULL) {
        const gchar *name;
        PulseConnection *connection;

        connection = pulse_stream_get_connection (PULSE_STREAM (source));
        output = pulse_source_output_new (connection,
                                          info,
                                          source);
        g_hash_table_insert (source->priv->outputs,
                             GUINT_TO_POINTER (info->index),
                             output);

        free_list_controls (source);

        name = mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (output));
        g_signal_emit_by_name (G_OBJECT (source),
                               "control-added",
                               name);
        return TRUE;
    }

    pulse_source_output_update (output, info);
    return FALSE;
}

void
pulse_source_remove_output (PulseSource *source, guint32 index)
{
    PulseSourceOutput *output;
    gchar             *name;

    g_return_if_fail (PULSE_IS_SOURCE (source));

    output = g_hash_table_lookup (source->priv->outputs, GUINT_TO_POINTER (index));
    if G_UNLIKELY (output == NULL)
        return;

    name = g_strdup (mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (output)));

    g_hash_table_remove (source->priv->outputs, GUINT_TO_POINTER (index));

    free_list_controls (source);
    g_signal_emit_by_name (G_OBJECT (source),
                           "control-removed",
                           name);
    g_free (name);
}

void
pulse_source_update (PulseSource          *source,
                     const pa_source_info *info)
{
    g_return_if_fail (PULSE_IS_SOURCE (source));
    g_return_if_fail (info != NULL);

    /* The switch doesn't allow being unset, PulseAudio should always include
     * the active port name if the are any ports available */
    if (info->active_port != NULL)
        pulse_port_switch_set_active_port_by_name (source->priv->pswitch,
                                                   info->active_port->name);

    pulse_source_control_update (source->priv->control, info);
}

static const GList *
pulse_source_list_controls (MateMixerStream *mms)
{
    PulseSource *source;

    g_return_val_if_fail (PULSE_IS_SOURCE (mms), NULL);

    source = PULSE_SOURCE (mms);

    if (source->priv->outputs_list == NULL) {
        source->priv->outputs_list = g_hash_table_get_values (source->priv->outputs);
        if (source->priv->outputs_list != NULL)
            g_list_foreach (source->priv->outputs_list, (GFunc) g_object_ref, NULL);

        source->priv->outputs_list = g_list_prepend (source->priv->outputs_list,
                                                     g_object_ref (source->priv->control));
    }
    return source->priv->outputs_list;
}

static const GList *
pulse_source_list_switches (MateMixerStream *mms)
{
    g_return_val_if_fail (PULSE_IS_SOURCE (mms), NULL);

    return PULSE_SOURCE (mms)->priv->pswitch_list;
}

static void
free_list_controls (PulseSource *source)
{
    if (source->priv->outputs_list == NULL)
        return;

    g_list_free_full (source->priv->outputs_list, g_object_unref);

    source->priv->outputs_list = NULL;
}
