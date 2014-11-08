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

#ifndef PULSE_DEVICE_SWITCH_H
#define PULSE_DEVICE_SWITCH_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_DEVICE_SWITCH                \
        (pulse_device_switch_get_type ())
#define PULSE_DEVICE_SWITCH(o)                  \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_DEVICE_SWITCH, PulseDeviceSwitch))
#define PULSE_IS_DEVICE_SWITCH(o)               \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_DEVICE_SWITCH))
#define PULSE_DEVICE_SWITCH_CLASS(k)            \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_DEVICE_SWITCH, PulseDeviceSwitchClass))
#define PULSE_IS_DEVICE_SWITCH_CLASS(k)         \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_DEVICE_SWITCH))
#define PULSE_DEVICE_SWITCH_GET_CLASS(o)        \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_DEVICE_SWITCH, PulseDeviceSwitchClass))

typedef struct _PulseDeviceSwitchClass    PulseDeviceSwitchClass;
typedef struct _PulseDeviceSwitchPrivate  PulseDeviceSwitchPrivate;

struct _PulseDeviceSwitch
{
    MateMixerDeviceSwitch parent;

    /*< private >*/
    PulseDeviceSwitchPrivate *priv;
};

struct _PulseDeviceSwitchClass
{
    MateMixerDeviceSwitchClass parent_class;
};

GType              pulse_device_switch_get_type                   (void) G_GNUC_CONST;

PulseDeviceSwitch *pulse_device_switch_new                        (const gchar        *name,
                                                                   const gchar        *label,
                                                                   PulseDevice        *device);

void               pulse_device_switch_add_profile                (PulseDeviceSwitch  *swtch,
                                                                   PulseDeviceProfile *profile);

void               pulse_device_switch_set_active_profile         (PulseDeviceSwitch  *swtch,
                                                                   PulseDeviceProfile *profile);

void               pulse_device_switch_set_active_profile_by_name (PulseDeviceSwitch  *swtch,
                                                                   const gchar        *name);

G_END_DECLS

#endif /* PULSE_DEVICE_SWITCH_H */
