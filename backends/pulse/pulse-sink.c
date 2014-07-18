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

#include <libmatemixer/matemixer-port.h>
#include <libmatemixer/matemixer-port-private.h>
#include <libmatemixer/matemixer-stream.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-device.h"
#include "pulse-monitor.h"
#include "pulse-stream.h"
#include "pulse-sink.h"

struct _PulseSinkPrivate
{
    guint32 index_monitor;
};

static void pulse_sink_class_init (PulseSinkClass *klass);
static void pulse_sink_init       (PulseSink      *sink);

G_DEFINE_TYPE (PulseSink, pulse_sink, PULSE_TYPE_STREAM);

static void          pulse_sink_reload          (PulseStream        *pstream);

static gboolean      pulse_sink_set_mute        (PulseStream        *pstream,
                                                 gboolean            mute);
static gboolean      pulse_sink_set_volume      (PulseStream        *pstream,
                                                 pa_cvolume         *cvolume);
static gboolean      pulse_sink_set_active_port (PulseStream        *pstream,
                                                 MateMixerPort      *port);

static gboolean      pulse_sink_suspend         (PulseStream        *pstream);
static gboolean      pulse_sink_resume          (PulseStream        *pstream);

static PulseMonitor *pulse_sink_create_monitor  (PulseStream        *pstream);

static void          update_ports               (PulseStream        *pstream,
                                                 pa_sink_port_info **ports,
                                                 pa_sink_port_info  *active);

static void
pulse_sink_class_init (PulseSinkClass *klass)
{
    PulseStreamClass *stream_class;

    stream_class = PULSE_STREAM_CLASS (klass);

    stream_class->reload          = pulse_sink_reload;
    stream_class->set_mute        = pulse_sink_set_mute;
    stream_class->set_volume      = pulse_sink_set_volume;
    stream_class->set_active_port = pulse_sink_set_active_port;
    stream_class->suspend         = pulse_sink_suspend;
    stream_class->resume          = pulse_sink_resume;
    stream_class->create_monitor  = pulse_sink_create_monitor;

    g_type_class_add_private (klass, sizeof (PulseSinkPrivate));
}

static void
pulse_sink_init (PulseSink *sink)
{
    sink->priv = G_TYPE_INSTANCE_GET_PRIVATE (sink,
                                              PULSE_TYPE_SINK,
                                              PulseSinkPrivate);

    sink->priv->index_monitor = PA_INVALID_INDEX;
}

PulseStream *
pulse_sink_new (PulseConnection    *connection,
                const pa_sink_info *info,
                PulseDevice        *device)
{
    PulseStream *stream;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);

    /* Consider the sink index as unchanging parameter */
    stream = g_object_new (PULSE_TYPE_SINK,
                           "connection", connection,
                           "index", info->index,
                           NULL);

    /* Other data may change at any time, so let's make a use of our update function */
    pulse_sink_update (stream, info, device);

    return stream;
}

guint32
pulse_sink_get_monitor_index (PulseStream *pstream)
{
    g_return_val_if_fail (PULSE_IS_SINK (pstream), PA_INVALID_INDEX);

    return PULSE_SINK (pstream)->priv->index_monitor;
}

gboolean
pulse_sink_update (PulseStream *pstream, const pa_sink_info *info, PulseDevice *device)
{
    MateMixerStreamFlags flags = MATE_MIXER_STREAM_OUTPUT |
                                 MATE_MIXER_STREAM_HAS_MUTE |
                                 MATE_MIXER_STREAM_HAS_VOLUME |
                                 MATE_MIXER_STREAM_CAN_SET_VOLUME |
                                 MATE_MIXER_STREAM_CAN_SUSPEND;
    PulseSink *sink;

    g_return_val_if_fail (PULSE_IS_SINK (pstream), FALSE);
    g_return_val_if_fail (info != NULL, FALSE);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (pstream));

    pulse_stream_update_name (pstream, info->name);
    pulse_stream_update_description (pstream, info->description);
    pulse_stream_update_mute (pstream, info->mute ? TRUE : FALSE);

    /* Stream state */
    switch (info->state) {
    case PA_SINK_RUNNING:
        pulse_stream_update_state (pstream, MATE_MIXER_STREAM_STATE_RUNNING);
        break;
    case PA_SINK_IDLE:
        pulse_stream_update_state (pstream, MATE_MIXER_STREAM_STATE_IDLE);
        break;
    case PA_SINK_SUSPENDED:
        pulse_stream_update_state (pstream, MATE_MIXER_STREAM_STATE_SUSPENDED);
        break;
    default:
        pulse_stream_update_state (pstream, MATE_MIXER_STREAM_STATE_UNKNOWN);
        break;
    }

    /* Build the flag list */
    if (info->flags & PA_SINK_DECIBEL_VOLUME)
        flags |= MATE_MIXER_STREAM_HAS_DECIBEL_VOLUME;
    if (info->flags & PA_SINK_FLAT_VOLUME)
        flags |= MATE_MIXER_STREAM_HAS_FLAT_VOLUME;

    sink = PULSE_SINK (pstream);

    if (sink->priv->index_monitor == PA_INVALID_INDEX)
        sink->priv->index_monitor = info->monitor_source;

    if (sink->priv->index_monitor != PA_INVALID_INDEX)
        flags |= MATE_MIXER_STREAM_HAS_MONITOR;

    /* Flags must be updated before volume */
    pulse_stream_update_flags (pstream, flags);

    pulse_stream_update_channel_map (pstream, &info->channel_map);
    pulse_stream_update_volume (pstream, &info->volume, info->base_volume);

    pulse_stream_update_device (pstream, MATE_MIXER_DEVICE (device));

    /* Ports must be updated after device */
    if (info->ports != NULL) {
        update_ports (pstream, info->ports, info->active_port);
    }

    g_object_thaw_notify (G_OBJECT (pstream));
    return TRUE;
}

static void
pulse_sink_reload (PulseStream *pstream)
{
    g_return_if_fail (PULSE_IS_SINK (pstream));

    pulse_connection_load_sink_info (pulse_stream_get_connection (pstream),
                                     pulse_stream_get_index (pstream));
}

static gboolean
pulse_sink_set_mute (PulseStream *pstream, gboolean mute)
{
    g_return_val_if_fail (PULSE_IS_SINK (pstream), FALSE);

    return pulse_connection_set_sink_mute (pulse_stream_get_connection (pstream),
                                           pulse_stream_get_index (pstream),
                                           mute);
}

static gboolean
pulse_sink_set_volume (PulseStream *pstream, pa_cvolume *cvolume)
{
    g_return_val_if_fail (PULSE_IS_SINK (pstream), FALSE);
    g_return_val_if_fail (cvolume != NULL, FALSE);

    return pulse_connection_set_sink_volume (pulse_stream_get_connection (pstream),
                                             pulse_stream_get_index (pstream),
                                             cvolume);
}

static gboolean
pulse_sink_set_active_port (PulseStream *pstream, MateMixerPort *port)
{
    g_return_val_if_fail (PULSE_IS_SINK (pstream), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_PORT (port), FALSE);

    return pulse_connection_set_sink_port (pulse_stream_get_connection (pstream),
                                           pulse_stream_get_index (pstream),
                                           mate_mixer_port_get_name (port));
}

static gboolean
pulse_sink_suspend (PulseStream *pstream)
{
    g_return_val_if_fail (PULSE_IS_SINK (pstream), FALSE);

    return pulse_connection_suspend_sink (pulse_stream_get_connection (pstream),
                                          pulse_stream_get_index (pstream),
                                          TRUE);
}

static gboolean
pulse_sink_resume (PulseStream *pstream)
{
    g_return_val_if_fail (PULSE_IS_SINK (pstream), FALSE);

    return pulse_connection_suspend_sink (pulse_stream_get_connection (pstream),
                                          pulse_stream_get_index (pstream),
                                          FALSE);
}

static PulseMonitor *
pulse_sink_create_monitor (PulseStream *pstream)
{
    guint32 index;

    g_return_val_if_fail (PULSE_IS_SINK (pstream), NULL);

    index = pulse_sink_get_monitor_index (pstream);

    if (G_UNLIKELY (index == PA_INVALID_INDEX)) {
        g_debug ("Not creating monitor for stream %s: not available",
                 mate_mixer_stream_get_name (MATE_MIXER_STREAM (pstream)));
        return NULL;
    }

    return pulse_connection_create_monitor (pulse_stream_get_connection (pstream),
                                            index,
                                            PA_INVALID_INDEX);
}

static void
update_ports (PulseStream        *pstream,
              pa_sink_port_info **ports,
              pa_sink_port_info  *active)
{
    MateMixerPort   *port;
    MateMixerDevice *device;
    GHashTable      *hash;

    hash = pulse_stream_get_ports (pstream);

    while (*ports != NULL) {
        MateMixerPortFlags  flags = MATE_MIXER_PORT_NO_FLAGS;
        pa_sink_port_info  *info  = *ports;
        const gchar        *icon  = NULL;

        device = mate_mixer_stream_get_device (MATE_MIXER_STREAM (pstream));
        if (device != NULL) {
            port = mate_mixer_device_get_port (device, info->name);

            if (port != NULL) {
                flags = mate_mixer_port_get_flags (port);
                icon  = mate_mixer_port_get_icon (port);
            }
        }

#if PA_CHECK_VERSION(2, 0, 0)
        if (info->available == PA_PORT_AVAILABLE_YES)
            flags |= MATE_MIXER_PORT_AVAILABLE;
        else
            flags &= ~MATE_MIXER_PORT_AVAILABLE;
#endif

        port = g_hash_table_lookup (hash, info->name);

        if (port != NULL) {
            /* Update existing port */
            _mate_mixer_port_update_description (port, info->description);
            _mate_mixer_port_update_icon (port, icon);
            _mate_mixer_port_update_priority (port, info->priority);
            _mate_mixer_port_update_flags (port, flags);
        } else {
            /* Add previously unknown port to the hash table */
            port = _mate_mixer_port_new (info->name,
                                         info->description,
                                         icon,
                                         info->priority,
                                         flags);

            g_hash_table_insert (hash, g_strdup (info->name), port);
        }

        ports++;
    }

    /* Active port */
    if (G_LIKELY (active != NULL))
        port = g_hash_table_lookup (hash, active->name);
    else
        port = NULL;

    pulse_stream_update_active_port (pstream, port);
}
