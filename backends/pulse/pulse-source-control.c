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
#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-monitor.h"
#include "pulse-stream-control.h"
#include "pulse-source.h"
#include "pulse-source-control.h"

static void pulse_source_control_class_init (PulseSourceControlClass *klass);
static void pulse_source_control_init       (PulseSourceControl      *control);

G_DEFINE_TYPE (PulseSourceControl, pulse_source_control, PULSE_TYPE_STREAM_CONTROL);

static gboolean      pulse_source_control_set_mute        (PulseStreamControl *psc,
                                                           gboolean            mute);
static gboolean      pulse_source_control_set_volume      (PulseStreamControl *psc,
                                                           pa_cvolume         *cvolume);
static PulseMonitor *pulse_source_control_create_monitor  (PulseStreamControl *psc);

static void
pulse_source_control_class_init (PulseSourceControlClass *klass)
{
    PulseStreamControlClass *control_class;

    control_class = PULSE_STREAM_CONTROL_CLASS (klass);
    control_class->set_mute       = pulse_source_control_set_mute;
    control_class->set_volume     = pulse_source_control_set_volume;
    control_class->create_monitor = pulse_source_control_create_monitor;
}

static void
pulse_source_control_init (PulseSourceControl *control)
{
}

PulseSourceControl *
pulse_source_control_new (PulseConnection      *connection,
                          const pa_source_info *info,
                          PulseSource          *parent)
{
    PulseSourceControl         *control;
    MateMixerStreamControlFlags flags = MATE_MIXER_STREAM_CONTROL_MUTE_READABLE |
                                        MATE_MIXER_STREAM_CONTROL_MUTE_WRITABLE |
                                        MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE |
                                        MATE_MIXER_STREAM_CONTROL_VOLUME_WRITABLE |
                                        MATE_MIXER_STREAM_CONTROL_HAS_MONITOR;
    MateMixerStreamControlRole  role;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);
    g_return_val_if_fail (PULSE_IS_SOURCE (parent), NULL);

    if (info->active_port != NULL)
        role = MATE_MIXER_STREAM_CONTROL_ROLE_PORT;
    else
        role = MATE_MIXER_STREAM_CONTROL_ROLE_MASTER;

    /* Build the flag list */
    if (info->flags & PA_SOURCE_DECIBEL_VOLUME)
        flags |= MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL;

    control = g_object_new (PULSE_TYPE_SOURCE_CONTROL,
                            "name", info->name,
                            "label", info->description,
                            "flags", flags,
                            "role", role,
                            "stream", parent,
                            "connection", connection,
                            NULL);

    pulse_source_control_update (control, info);
    return control;
}

void
pulse_source_control_update (PulseSourceControl *control, const pa_source_info *info)
{
    g_return_if_fail (PULSE_IS_SOURCE_CONTROL (control));
    g_return_if_fail (info != NULL);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (control));

    _mate_mixer_stream_control_set_mute (MATE_MIXER_STREAM_CONTROL (control),
                                         info->mute ? TRUE : FALSE);

    pulse_stream_control_set_channel_map (PULSE_STREAM_CONTROL (control),
                                          &info->channel_map);

    pulse_stream_control_set_cvolume (PULSE_STREAM_CONTROL (control),
                                      &info->volume,
                                      info->base_volume);

    g_object_thaw_notify (G_OBJECT (control));
}

static gboolean
pulse_source_control_set_mute (PulseStreamControl *psc, gboolean mute)
{
    g_return_val_if_fail (PULSE_IS_SOURCE_CONTROL (psc), FALSE);

    return pulse_connection_set_source_mute (pulse_stream_control_get_connection (psc),
                                             pulse_stream_control_get_stream_index (psc),
                                             mute);
}

static gboolean
pulse_source_control_set_volume (PulseStreamControl *psc, pa_cvolume *cvolume)
{
    g_return_val_if_fail (PULSE_IS_SOURCE_CONTROL (psc), FALSE);
    g_return_val_if_fail (cvolume != NULL, FALSE);

    return pulse_connection_set_source_volume (pulse_stream_control_get_connection (psc),
                                               pulse_stream_control_get_stream_index (psc),
                                               cvolume);
}

static PulseMonitor *
pulse_source_control_create_monitor (PulseStreamControl *psc)
{
    guint32 index;

    g_return_val_if_fail (PULSE_IS_SOURCE_CONTROL (psc), NULL);

    index = pulse_stream_control_get_stream_index (psc);
    if G_UNLIKELY (index == PA_INVALID_INDEX) {
        g_debug ("Monitor of stream control %s is not available",
                 mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (psc)));
        return NULL;
    }

    return pulse_connection_create_monitor (pulse_stream_control_get_connection (psc),
                                            index,
                                            PA_INVALID_INDEX);
}
