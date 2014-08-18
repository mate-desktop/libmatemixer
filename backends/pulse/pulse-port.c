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

#include <string.h>
#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-port.h"

struct _PulsePortPrivate
{
    guint priority;
};

static void pulse_port_class_init (PulsePortClass *klass);
static void pulse_port_init       (PulsePort      *port);

G_DEFINE_TYPE (PulsePort, pulse_port, MATE_MIXER_TYPE_SWITCH_OPTION)

static void
pulse_port_class_init (PulsePortClass *klass)
{
    g_type_class_add_private (klass, sizeof (PulsePortPrivate));
}

static void
pulse_port_init (PulsePort *port)
{
    port->priv = G_TYPE_INSTANCE_GET_PRIVATE (port,
                                              PULSE_TYPE_PORT,
                                              PulsePortPrivate);
}

PulsePort *
pulse_port_new (const gchar *name,
                const gchar *label,
                const gchar *icon,
                guint        priority)
{
    PulsePort *port;

    g_return_val_if_fail (name  != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);

    port = g_object_new (PULSE_TYPE_PORT,
                         "name", name,
                         "label", label,
                         "icon", icon,
                         NULL);

    port->priv->priority = priority;
    return port;
}

const gchar *
pulse_port_get_name (PulsePort *port)
{
    g_return_val_if_fail (PULSE_IS_PORT (port), NULL);

    return mate_mixer_switch_option_get_name (MATE_MIXER_SWITCH_OPTION (port));
}

guint
pulse_port_get_priority (PulsePort *port)
{
    g_return_val_if_fail (PULSE_IS_PORT (port), 0);

    return port->priv->priority;
}
