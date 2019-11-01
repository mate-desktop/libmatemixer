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
#include "pulse-source.h"
#include "pulse-source-switch.h"
#include "pulse-stream.h"

G_DEFINE_TYPE (PulseSourceSwitch, pulse_source_switch, PULSE_TYPE_PORT_SWITCH)

static gboolean pulse_source_switch_set_active_port (PulsePortSwitch *swtch,
                                                     PulsePort       *port);

static void
pulse_source_switch_class_init (PulseSourceSwitchClass *klass)
{
    PulsePortSwitchClass *switch_class;

    switch_class = PULSE_PORT_SWITCH_CLASS (klass);
    switch_class->set_active_port = pulse_source_switch_set_active_port;
}

static void
pulse_source_switch_init (PulseSourceSwitch *swtch)
{
}

PulsePortSwitch *
pulse_source_switch_new (const gchar *name, const gchar *label, PulseSource *source)
{
    g_return_val_if_fail (name  != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (PULSE_IS_SOURCE (source), NULL);

    return g_object_new (PULSE_TYPE_SOURCE_SWITCH,
                         "name", name,
                         "label", label,
                         "role", MATE_MIXER_STREAM_SWITCH_ROLE_PORT,
                         "stream", source,
                         NULL);
}

static gboolean
pulse_source_switch_set_active_port (PulsePortSwitch *swtch, PulsePort *port)
{
    PulseStream *stream;

    g_return_val_if_fail (PULSE_IS_SOURCE_SWITCH (swtch), FALSE);
    g_return_val_if_fail (PULSE_IS_PORT (port), FALSE);

    stream = pulse_port_switch_get_stream (swtch);

    return pulse_connection_set_source_port (pulse_stream_get_connection (stream),
                                             pulse_stream_get_index (stream),
                                             pulse_port_get_name (port));
}
