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

#ifndef PULSE_SOURCE_SWITCH_H
#define PULSE_SOURCE_SWITCH_H

#include <glib.h>
#include <glib-object.h>

#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_SOURCE_SWITCH                \
        (pulse_source_switch_get_type ())
#define PULSE_SOURCE_SWITCH(o)                  \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_SOURCE_SWITCH, PulseSourceSwitch))
#define PULSE_IS_SOURCE_SWITCH(o)               \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_SOURCE_SWITCH))
#define PULSE_SOURCE_SWITCH_CLASS(k)            \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_SOURCE_SWITCH, PulseSourceSwitchClass))
#define PULSE_IS_SOURCE_SWITCH_CLASS(k)         \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_SOURCE_SWITCH))
#define PULSE_SOURCE_SWITCH_GET_CLASS(o)        \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_SOURCE_SWITCH, PulseSourceSwitchClass))

typedef struct _PulseSourceSwitchClass    PulseSourceSwitchClass;
typedef struct _PulseSourceSwitchPrivate  PulseSourceSwitchPrivate;

struct _PulseSourceSwitch
{
    PulsePortSwitch parent;
};

struct _PulseSourceSwitchClass
{
    PulsePortSwitchClass parent_class;
};

GType            pulse_source_switch_get_type (void) G_GNUC_CONST;

PulsePortSwitch *pulse_source_switch_new      (const gchar *name,
                                               const gchar *label,
                                               PulseSource *source);

G_END_DECLS

#endif /* PULSE_SOURCE_SWITCH_H */
