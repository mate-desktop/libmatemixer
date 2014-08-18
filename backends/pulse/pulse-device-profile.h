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

#ifndef PULSE_DEVICE_PROFILE_H
#define PULSE_DEVICE_PROFILE_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_DEVICE_PROFILE               \
        (pulse_device_profile_get_type ())
#define PULSE_DEVICE_PROFILE(o)                 \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_DEVICE_PROFILE, PulseDeviceProfile))
#define PULSE_IS_DEVICE_PROFILE(o)              \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_DEVICE_PROFILE))
#define PULSE_DEVICE_PROFILE_CLASS(k)           \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_DEVICE_PROFILE, PulseDeviceProfileClass))
#define PULSE_IS_DEVICE_PROFILE_CLASS(k)        \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_DEVICE_PROFILE))
#define PULSE_DEVICE_PROFILE_GET_CLASS(o)       \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_IS_DEVICE_PROFILE, PulseDeviceProfileClass))

typedef struct _PulseDeviceProfileClass    PulseDeviceProfileClass;
typedef struct _PulseDeviceProfilePrivate  PulseDeviceProfilePrivate;

struct _PulseDeviceProfile
{
    MateMixerSwitchOption parent;

    /*< private >*/
    PulseDeviceProfilePrivate *priv;
};

struct _PulseDeviceProfileClass
{
    MateMixerSwitchOptionClass parent;
};

GType               pulse_device_profile_get_type     (void) G_GNUC_CONST;

PulseDeviceProfile *pulse_device_profile_new          (const gchar        *name,
                                                       const gchar        *label,
                                                       guint               priority);

const gchar *       pulse_device_profile_get_name     (PulseDeviceProfile *profile);
guint               pulse_device_profile_get_priority (PulseDeviceProfile *profile);

G_END_DECLS

#endif /* PULSE_DEVICE_PROFILE_H */
