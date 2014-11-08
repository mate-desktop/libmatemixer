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

#ifndef PULSE_PORT_SWITCH_H
#define PULSE_PORT_SWITCH_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_PORT_SWITCH                  \
        (pulse_port_switch_get_type ())
#define PULSE_PORT_SWITCH(o)                    \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_PORT_SWITCH, PulsePortSwitch))
#define PULSE_IS_PORT_SWITCH(o)                 \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_PORT_SWITCH))
#define PULSE_PORT_SWITCH_CLASS(k)              \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_PORT_SWITCH, PulsePortSwitchClass))
#define PULSE_IS_PORT_SWITCH_CLASS(k)           \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_PORT_SWITCH))
#define PULSE_PORT_SWITCH_GET_CLASS(o)          \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_PORT_SWITCH, PulsePortSwitchClass))

typedef struct _PulsePortSwitchClass    PulsePortSwitchClass;
typedef struct _PulsePortSwitchPrivate  PulsePortSwitchPrivate;

struct _PulsePortSwitch
{
    MateMixerStreamSwitch parent;

    /*< private >*/
    PulsePortSwitchPrivate *priv;
};

struct _PulsePortSwitchClass
{
    MateMixerStreamSwitchClass parent_class;

    /*< private >*/
    gboolean (*set_active_port) (PulsePortSwitch *swtch,
                                 PulsePort       *port);
};

GType        pulse_port_switch_get_type                (void) G_GNUC_CONST;

PulseStream *pulse_port_switch_get_stream              (PulsePortSwitch *swtch);

void         pulse_port_switch_add_port                (PulsePortSwitch *swtch,
                                                        PulsePort       *port);

void         pulse_port_switch_set_active_port         (PulsePortSwitch *swtch,
                                                        PulsePort       *port);

void         pulse_port_switch_set_active_port_by_name (PulsePortSwitch *swtch,
                                                        const gchar     *name);

G_END_DECLS

#endif /* PULSE_PORT_SWITCH_H */
