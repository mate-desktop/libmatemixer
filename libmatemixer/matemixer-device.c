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

#include "matemixer-device.h"
#include "matemixer-device-profile.h"

G_DEFINE_INTERFACE (MateMixerDevice, mate_mixer_device, G_TYPE_OBJECT)

static void
mate_mixer_device_default_init (MateMixerDeviceInterface *iface)
{
    g_object_interface_install_property (
        iface,
        g_param_spec_string ("identifier",
                             "Identifier",
                             "Identifier of the sound device",
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    g_object_interface_install_property (
        iface,
        g_param_spec_string ("name",
                             "Name",
                             "Name of the sound device",
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    g_object_interface_install_property (
        iface,
        g_param_spec_string ("icon",
                             "Icon",
                             "Name of the sound device icon",
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    g_object_interface_install_property (
        iface,
        g_param_spec_object ("active-profile",
                             "Active profile",
                             "Identifier of the currently active profile",
                             MATE_MIXER_TYPE_DEVICE_PROFILE,
                             G_PARAM_CONSTRUCT | G_PARAM_READWRITE));
}

const GList *
mate_mixer_device_list_tracks (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

    return MATE_MIXER_DEVICE_GET_INTERFACE (device)->list_tracks (device);
}
