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

#include "pulse-connection.h"
#include "pulse-port.h"
#include "pulse-port-switch.h"
#include "pulse-sink.h"
#include "pulse-sink-switch.h"
#include "pulse-stream.h"

static void pulse_sink_switch_class_init (PulseSinkSwitchClass *klass);
static void pulse_sink_switch_init       (PulseSinkSwitch      *swtch);

G_DEFINE_TYPE (PulseSinkSwitch, pulse_sink_switch, PULSE_TYPE_PORT_SWITCH)

static gboolean pulse_sink_switch_set_active_port (PulsePortSwitch *swtch,
                                                   PulsePort       *port);

static void
pulse_sink_switch_class_init (PulseSinkSwitchClass *klass)
{
    PulsePortSwitchClass *switch_class;

    switch_class = PULSE_PORT_SWITCH_CLASS (klass);
    switch_class->set_active_port = pulse_sink_switch_set_active_port;
}

static void
pulse_sink_switch_init (PulseSinkSwitch *swtch)
{
}

PulsePortSwitch *
pulse_sink_switch_new (const gchar *name, const gchar *label, PulseSink *sink)
{
    g_return_val_if_fail (name  != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (PULSE_IS_SINK (sink), NULL);

    return g_object_new (PULSE_TYPE_SINK_SWITCH,
                         "name", name,
                         "label", label,
                         "role", MATE_MIXER_STREAM_SWITCH_ROLE_PORT,
                         "stream", sink,
                         NULL);
}

static gboolean
pulse_sink_switch_set_active_port (PulsePortSwitch *swtch, PulsePort *port)
{
    PulseStream *stream;

    g_return_val_if_fail (PULSE_IS_SINK_SWITCH (swtch), FALSE);
    g_return_val_if_fail (PULSE_IS_PORT (port), FALSE);

    stream = pulse_port_switch_get_stream (swtch);

    return pulse_connection_set_sink_port (pulse_stream_get_connection (stream),
                                           pulse_stream_get_index (stream),
                                           pulse_port_get_name (port));
}
