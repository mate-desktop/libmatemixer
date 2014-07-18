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
#include "matemixer-port.h"

/**
 * SECTION:matemixer-device
 * @short_description: Hardware or software device in the sound system
 * @include: libmatemixer/matemixer.h
 */

G_DEFINE_INTERFACE (MateMixerDevice, mate_mixer_device, G_TYPE_OBJECT)

static void
mate_mixer_device_default_init (MateMixerDeviceInterface *iface)
{
    g_object_interface_install_property (iface,
                                         g_param_spec_string ("name",
                                                              "Name",
                                                              "Name of the device",
                                                              NULL,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_string ("description",
                                                              "Description",
                                                              "Description of the device",
                                                              NULL,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_string ("icon",
                                                              "Icon",
                                                              "Name of the sound device icon",
                                                              NULL,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_object ("active-profile",
                                                              "Active profile",
                                                              "The currently active profile of the device",
                                                              MATE_MIXER_TYPE_DEVICE_PROFILE,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));
}

/**
 * mate_mixer_device_get_name:
 * @device: a #MateMixerDevice
 */
const gchar *
mate_mixer_device_get_name (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

    /* Implementation required */
    return MATE_MIXER_DEVICE_GET_INTERFACE (device)->get_name (device);
}

/**
 * mate_mixer_device_get_description:
 * @device: a #MateMixerDevice
 */
const gchar *
mate_mixer_device_get_description (MateMixerDevice *device)
{
    MateMixerDeviceInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

    iface = MATE_MIXER_DEVICE_GET_INTERFACE (device);

    if (iface->get_description)
        return iface->get_description (device);

    return NULL;
}

/**
 * mate_mixer_device_get_icon:
 * @device: a #MateMixerDevice
 */
const gchar *
mate_mixer_device_get_icon (MateMixerDevice *device)
{
    MateMixerDeviceInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

    iface = MATE_MIXER_DEVICE_GET_INTERFACE (device);

    if (iface->get_icon)
        return iface->get_icon (device);

    return NULL;
}

/**
 * mate_mixer_device_get_port:
 * @device: a #MateMixerDevice
 * @name: a port name
 */
MateMixerPort *
mate_mixer_device_get_port (MateMixerDevice *device, const gchar *name)
{
    MateMixerDeviceInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    iface = MATE_MIXER_DEVICE_GET_INTERFACE (device);

    if (iface->get_port)
        return iface->get_port (device, name);

    return NULL;
}

/**
 * mate_mixer_device_get_profile:
 * @device: a #MateMixerDevice
 * @name: a profile name
 */
MateMixerDeviceProfile *
mate_mixer_device_get_profile (MateMixerDevice *device, const gchar *name)
{
    MateMixerDeviceInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    iface = MATE_MIXER_DEVICE_GET_INTERFACE (device);

    if (iface->get_profile)
        return iface->get_profile (device, name);

    return NULL;
}

/**
 * mate_mixer_device_list_ports:
 * @device: a #MateMixerDevice
 */
const GList *
mate_mixer_device_list_ports (MateMixerDevice *device)
{
    MateMixerDeviceInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

    iface = MATE_MIXER_DEVICE_GET_INTERFACE (device);

    if (iface->list_ports)
        return iface->list_ports (device);

    return NULL;
}

/**
 * mate_mixer_device_list_profiles:
 * @device: a #MateMixerDevice
 */
const GList *
mate_mixer_device_list_profiles (MateMixerDevice *device)
{
    MateMixerDeviceInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

    iface = MATE_MIXER_DEVICE_GET_INTERFACE (device);

    if (iface->list_profiles)
        return iface->list_profiles (device);

    return NULL;
}

/**
 * mate_mixer_device_get_active_profile:
 * @device: a #MateMixerDevice
 */
MateMixerDeviceProfile *
mate_mixer_device_get_active_profile (MateMixerDevice *device)
{
    MateMixerDeviceInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

    iface = MATE_MIXER_DEVICE_GET_INTERFACE (device);

    if (iface->get_active_profile)
        return iface->get_active_profile (device);

    return NULL;
}

/**
 * mate_mixer_device_set_active_profile:
 * @device: a #MateMixerDevice
 * @profile: a #MateMixerDeviceProfile
 */
gboolean
mate_mixer_device_set_active_profile (MateMixerDevice        *device,
                                      MateMixerDeviceProfile *profile)
{
    MateMixerDeviceInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), FALSE);

    iface = MATE_MIXER_DEVICE_GET_INTERFACE (device);

    if (iface->set_active_profile)
        return iface->set_active_profile (device, profile);

    return FALSE;
}
