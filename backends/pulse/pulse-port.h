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

#ifndef PULSE_PORT_H
#define PULSE_PORT_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_PORT                         \
        (pulse_port_get_type ())
#define PULSE_PORT(o)                           \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_PORT, PulsePort))
#define PULSE_IS_PORT(o)                        \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_PORT))
#define PULSE_PORT_CLASS(k)                     \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_PORT, PulsePortClass))
#define PULSE_IS_PORT_CLASS(k)                  \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_PORT))
#define PULSE_PORT_GET_CLASS(o)                 \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_IS_PORT, PulsePortClass))

typedef struct _PulsePortClass    PulsePortClass;
typedef struct _PulsePortPrivate  PulsePortPrivate;

struct _PulsePort
{
    MateMixerSwitchOption parent;

    /*< private >*/
    PulsePortPrivate *priv;
};

struct _PulsePortClass
{
    MateMixerSwitchOptionClass parent;
};

GType        pulse_port_get_type     (void) G_GNUC_CONST;

PulsePort *  pulse_port_new          (const gchar *name,
                                      const gchar *label,
                                      const gchar *icon,
                                      guint        priority);

const gchar *pulse_port_get_name     (PulsePort   *port);
guint        pulse_port_get_priority (PulsePort   *port);

G_END_DECLS

#endif /* PULSE_PORT_H */
