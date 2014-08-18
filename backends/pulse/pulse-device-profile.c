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
#include "pulse-device.h"
#include "pulse-device-profile.h"

struct _PulseDeviceProfilePrivate
{
    guint priority;
};

static void pulse_device_profile_class_init (PulseDeviceProfileClass *klass);
static void pulse_device_profile_init       (PulseDeviceProfile      *profile);

G_DEFINE_TYPE (PulseDeviceProfile, pulse_device_profile, MATE_MIXER_TYPE_SWITCH_OPTION)

static void
pulse_device_profile_class_init (PulseDeviceProfileClass *klass)
{
    g_type_class_add_private (klass, sizeof (PulseDeviceProfilePrivate));
}

static void
pulse_device_profile_init (PulseDeviceProfile *profile)
{
    profile->priv = G_TYPE_INSTANCE_GET_PRIVATE (profile,
                                                 PULSE_TYPE_DEVICE_PROFILE,
                                                 PulseDeviceProfilePrivate);
}

PulseDeviceProfile *
pulse_device_profile_new (const gchar *name,
                          const gchar *label,
                          guint        priority)
{
    PulseDeviceProfile *profile;

    g_return_val_if_fail (name  != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);

    profile = g_object_new (PULSE_TYPE_DEVICE_PROFILE,
                            "name", name,
                            "label", label,
                            NULL);

    profile->priv->priority = priority;
    return profile;
}

const gchar *
pulse_device_profile_get_name (PulseDeviceProfile *profile)
{
    g_return_val_if_fail (PULSE_IS_DEVICE_PROFILE (profile), NULL);

    return mate_mixer_switch_option_get_name (MATE_MIXER_SWITCH_OPTION (profile));
}

guint
pulse_device_profile_get_priority (PulseDeviceProfile *profile)
{
    g_return_val_if_fail (PULSE_IS_DEVICE_PROFILE (profile), 0);

    return profile->priv->priority;
}
