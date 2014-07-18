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
#include "pulse-source.h"

static void pulse_source_class_init (PulseSourceClass *klass);
static void pulse_source_init       (PulseSource      *source);

G_DEFINE_TYPE (PulseSource, pulse_source, PULSE_TYPE_STREAM);

static void          pulse_source_reload          (PulseStream          *pstream);

static gboolean      pulse_source_set_mute        (PulseStream          *pstream,
                                                   gboolean              mute);
static gboolean      pulse_source_set_volume      (PulseStream          *pstream,
                                                   pa_cvolume           *cvolume);
static gboolean      pulse_source_set_active_port (PulseStream          *pstream,
                                                   MateMixerPort        *port);

static PulseMonitor *pulse_source_create_monitor  (PulseStream          *pstream);

static void          update_ports                 (PulseStream          *pstream,
                                                   pa_source_port_info **ports,
                                                   pa_source_port_info  *active);

static void
pulse_source_class_init (PulseSourceClass *klass)
{
    PulseStreamClass *stream_class;

    stream_class = PULSE_STREAM_CLASS (klass);

    stream_class->reload          = pulse_source_reload;
    stream_class->set_mute        = pulse_source_set_mute;
    stream_class->set_volume      = pulse_source_set_volume;
    stream_class->set_active_port = pulse_source_set_active_port;
    stream_class->create_monitor  = pulse_source_create_monitor;
}

static void
pulse_source_init (PulseSource *source)
{
}

PulseStream *
pulse_source_new (PulseConnection      *connection,
                  const pa_source_info *info,
                  PulseDevice          *device)
{
    PulseStream *stream;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);

    /* Consider the sink index as unchanging parameter */
    stream = g_object_new (PULSE_TYPE_SOURCE,
                           "connection", connection,
                           "index", info->index,
                           NULL);

    /* Other data may change at any time, so let's make a use of our update function */
    pulse_source_update (stream, info, device);

    return stream;
}

gboolean
pulse_source_update (PulseStream          *pstream,
                     const pa_source_info *info,
                     PulseDevice          *device)
{
    MateMixerStreamFlags flags = MATE_MIXER_STREAM_INPUT |
                                 MATE_MIXER_STREAM_HAS_MUTE |
                                 MATE_MIXER_STREAM_HAS_MONITOR |
                                 MATE_MIXER_STREAM_HAS_VOLUME |
                                 MATE_MIXER_STREAM_CAN_SET_VOLUME |
                                 MATE_MIXER_STREAM_CAN_SUSPEND;

    g_return_val_if_fail (PULSE_IS_SOURCE (pstream), FALSE);
    g_return_val_if_fail (info != NULL, FALSE);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (pstream));

    pulse_stream_update_name (pstream, info->name);
    pulse_stream_update_description (pstream, info->description);
    pulse_stream_update_mute (pstream, info->mute ? TRUE : FALSE);

    /* Stream state */
    switch (info->state) {
    case PA_SOURCE_RUNNING:
        pulse_stream_update_state (pstream, MATE_MIXER_STREAM_STATE_RUNNING);
        break;
    case PA_SOURCE_IDLE:
        pulse_stream_update_state (pstream, MATE_MIXER_STREAM_STATE_IDLE);
        break;
    case PA_SOURCE_SUSPENDED:
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
pulse_source_reload (PulseStream *pstream)
{
    g_return_if_fail (PULSE_IS_SOURCE (pstream));

    pulse_connection_load_source_info (pulse_stream_get_connection (pstream),
                                       pulse_stream_get_index (pstream));
}

static gboolean
pulse_source_set_mute (PulseStream *pstream, gboolean mute)
{
    g_return_val_if_fail (PULSE_IS_SOURCE (pstream), FALSE);

    return pulse_connection_set_source_mute (pulse_stream_get_connection (pstream),
                                             pulse_stream_get_index (pstream),
                                             mute);
}

static gboolean
pulse_source_set_volume (PulseStream *pstream, pa_cvolume *cvolume)
{
    g_return_val_if_fail (PULSE_IS_SOURCE (pstream), FALSE);
    g_return_val_if_fail (cvolume != NULL, FALSE);

    return pulse_connection_set_source_volume (pulse_stream_get_connection (pstream),
                                               pulse_stream_get_index (pstream),
                                               cvolume);
}

static gboolean
pulse_source_set_active_port (PulseStream *pstream, MateMixerPort *port)
{
    g_return_val_if_fail (PULSE_IS_SOURCE (pstream), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_PORT (port), FALSE);

    return pulse_connection_set_source_port (pulse_stream_get_connection (pstream),
                                             pulse_stream_get_index (pstream),
                                             mate_mixer_port_get_name (port));
}

static PulseMonitor *
pulse_source_create_monitor (PulseStream *pstream)
{
    g_return_val_if_fail (PULSE_IS_SOURCE (pstream), NULL);

    return pulse_connection_create_monitor (pulse_stream_get_connection (pstream),
                                            pulse_stream_get_index (pstream),
                                            PA_INVALID_INDEX);
}

static void
update_ports (PulseStream          *pstream,
              pa_source_port_info **ports,
              pa_source_port_info  *active)
{
    MateMixerPort   *port;
    MateMixerDevice *device;
    GHashTable      *hash;

    hash = pulse_stream_get_ports (pstream);

    while (*ports != NULL) {
        MateMixerPortFlags   flags = MATE_MIXER_PORT_NO_FLAGS;
        pa_source_port_info *info  = *ports;
        const gchar         *icon  = NULL;

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
