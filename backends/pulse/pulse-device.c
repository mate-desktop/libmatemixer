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

#include <libmatemixer/matemixer-device.h>
#include <libmatemixer/matemixer-device-port.h>
#include <libmatemixer/matemixer-device-profile.h>

#include <pulse/pulseaudio.h>

#include "pulse-device.h"

struct _MateMixerPulseDevicePrivate
{
    guint32   index;
    GList    *profiles;
    GList    *ports;
    gchar    *identifier;
    gchar    *name;
    gchar    *icon;

    MateMixerDeviceProfile *active_profile;
};

enum
{
    PROP_0,
    PROP_IDENTIFIER,
    PROP_NAME,
    PROP_ICON,
    PROP_ACTIVE_PROFILE,
    N_PROPERTIES
};

static void mate_mixer_device_interface_init (MateMixerDeviceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MateMixerPulseDevice, mate_mixer_pulse_device, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_DEVICE,
                                                mate_mixer_device_interface_init))

static void
mate_mixer_device_interface_init (MateMixerDeviceInterface *iface)
{
    iface->list_tracks = mate_mixer_pulse_device_list_tracks;
    iface->get_ports = mate_mixer_pulse_device_get_ports;
    iface->get_profiles = mate_mixer_pulse_device_get_profiles;
    iface->get_active_profile = mate_mixer_pulse_device_get_active_profile;
    iface->set_active_profile = mate_mixer_pulse_device_set_active_profile;
}

static void
mate_mixer_pulse_device_init (MateMixerPulseDevice *device)
{
    device->priv = G_TYPE_INSTANCE_GET_PRIVATE (
        device,
        MATE_MIXER_TYPE_PULSE_DEVICE,
        MateMixerPulseDevicePrivate);
}

static void
mate_mixer_pulse_device_get_property (GObject     *object,
                                      guint        param_id,
                                      GValue      *value,
                                      GParamSpec  *pspec)
{
    MateMixerPulseDevice *device;

    device = MATE_MIXER_PULSE_DEVICE (object);

    switch (param_id) {
    case PROP_IDENTIFIER:
        g_value_set_string (value, device->priv->identifier);
        break;
    case PROP_NAME:
        g_value_set_string (value, device->priv->name);
        break;
    case PROP_ICON:
        g_value_set_string (value, device->priv->icon);
        break;
    case PROP_ACTIVE_PROFILE:
        g_value_set_object (value, device->priv->active_profile);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_pulse_device_set_property (GObject       *object,
                                      guint          param_id,
                                      const GValue  *value,
                                      GParamSpec    *pspec)
{
    MateMixerPulseDevice *device;

    device = MATE_MIXER_PULSE_DEVICE (object);

    switch (param_id) {
    case PROP_IDENTIFIER:
        device->priv->identifier = g_strdup (g_value_get_string (value));
        break;
    case PROP_NAME:
        device->priv->name = g_strdup (g_value_get_string (value));
        break;
    case PROP_ICON:
        device->priv->icon = g_strdup (g_value_get_string (value));
        break;
    case PROP_ACTIVE_PROFILE:
        mate_mixer_pulse_device_set_active_profile (
            MATE_MIXER_DEVICE (device),
            g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_pulse_device_finalize (GObject *object)
{
    MateMixerPulseDevice *device;

    device = MATE_MIXER_PULSE_DEVICE (object);

    g_free (device->priv->identifier);
    g_free (device->priv->name);
    g_free (device->priv->icon);

    if (device->priv->profiles != NULL)
        g_list_free_full (device->priv->profiles, g_object_unref);

    if (device->priv->ports != NULL)
        g_list_free_full (device->priv->ports, g_object_unref);

    if (device->priv->active_profile != NULL)
        g_object_unref (device->priv->active_profile);

    G_OBJECT_CLASS (mate_mixer_pulse_device_parent_class)->finalize (object);
}

static void
mate_mixer_pulse_device_class_init (MateMixerPulseDeviceClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = mate_mixer_pulse_device_finalize;
    object_class->get_property = mate_mixer_pulse_device_get_property;
    object_class->set_property = mate_mixer_pulse_device_set_property;

    g_object_class_override_property (object_class, PROP_IDENTIFIER, "identifier");
    g_object_class_override_property (object_class, PROP_NAME, "name");
    g_object_class_override_property (object_class, PROP_ICON, "icon");
    g_object_class_override_property (object_class, PROP_ACTIVE_PROFILE, "active-profile");

    g_type_class_add_private (object_class, sizeof (MateMixerPulseDevicePrivate));
}

MateMixerPulseDevice *
mate_mixer_pulse_device_new (const pa_card_info *info)
{
    MateMixerPulseDevice    *device;
    MateMixerDeviceProfile  *active_profile = NULL;
    GList                   *profiles = NULL;
    GList                   *ports = NULL;
    guint32                  i;

    g_return_val_if_fail (info != NULL, NULL);

    /* Create a list of card profiles */
    for (i = 0; i < info->n_profiles; i++) {
        MateMixerDeviceProfile *profile;

#if PA_CHECK_VERSION(5, 0, 0)
        pa_card_profile_info2 *p_info = info->profiles2[i];

        /* PulseAudio 5.0 includes a new pa_card_profile_info2 which
         * only differs in the new available flag, we use it not to include
         * profiles which are unavailable */
        if (p_info->available == 0)
            continue;
#else
        /* The old profile list is an array of structs, not pointers */
        pa_card_profile_info  *p_info = &info->profiles[i];
#endif
        profile = mate_mixer_device_profile_new (
            p_info->name,
            p_info->description,
            p_info->priority);

#if PA_CHECK_VERSION(5, 0, 0)
        if (!g_strcmp0 (p_info->name, info->active_profile2->name))
            active_profile = g_object_ref (profile);
#else
        if (!g_strcmp0 (p_info->name, info->active_profile->name))
            active_profile = g_object_ref (profile);
#endif
        profiles = g_list_prepend (profiles, profile);
    }

    /* Keep the profiles in the same order as in PulseAudio */
    if (profiles)
        profiles = g_list_reverse (profiles);

    /* Create a list of card ports */
    for (i = 0; i < info->n_ports; i++) {
        MateMixerDevicePort          *port;
        MateMixerDevicePortDirection  direction = 0;
        MateMixerDevicePortStatus     status = 0;
        pa_card_port_info            *p_info = info->ports[i];

        if (p_info->direction & PA_DIRECTION_INPUT)
            direction |= MATE_MIXER_DEVICE_PORT_DIRECTION_INPUT;

        if (p_info->direction & PA_DIRECTION_OUTPUT)
            direction |= MATE_MIXER_DEVICE_PORT_DIRECTION_OUTPUT;

#if PA_CHECK_VERSION(2, 0, 0)
        if (p_info->available == PA_PORT_AVAILABLE_YES)
            status |= MATE_MIXER_DEVICE_PORT_STATUS_AVAILABLE;
#endif
        port = mate_mixer_device_port_new (
            p_info->name,
            p_info->description,
            pa_proplist_gets (p_info->proplist, "device.icon_name"),
            p_info->priority,
            direction,
            status,
            p_info->latency_offset);

        ports = g_list_prepend (ports, port);
    }

    /* Keep the ports in the same order as in PulseAudio */
    if (ports)
        ports = g_list_reverse (ports);

    device = g_object_new (MATE_MIXER_TYPE_PULSE_DEVICE,
        "identifier",      info->name,
        "name",            pa_proplist_gets (info->proplist, "device.description"),
        "icon",            pa_proplist_gets (info->proplist, "device.icon_name"),
        "active-profile",  active_profile,
        NULL);

    device->priv->index = info->index;
    device->priv->profiles = profiles;
    device->priv->ports = ports;

    return device;
}

gboolean
mate_mixer_pulse_device_update (MateMixerPulseDevice *device, const pa_card_info *info)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), FALSE);
    g_return_val_if_fail (info != NULL, FALSE);

    // TODO: update status, active_profile, maybe others?
    return TRUE;
}

const GList *
mate_mixer_pulse_device_list_tracks (MateMixerDevice *device)
{
    // TODO
    return NULL;
}

const GList *
mate_mixer_pulse_device_get_ports (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), NULL);

    return MATE_MIXER_PULSE_DEVICE (device)->priv->ports;
}

const GList *
mate_mixer_pulse_device_get_profiles (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), NULL);

    return MATE_MIXER_PULSE_DEVICE (device)->priv->profiles;
}

MateMixerDeviceProfile *
mate_mixer_pulse_device_get_active_profile (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), NULL);

    return MATE_MIXER_PULSE_DEVICE (device)->priv->active_profile;
}

gboolean
mate_mixer_pulse_device_set_active_profile (MateMixerDevice *device,
                                            MateMixerDeviceProfile *profile)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), FALSE);

    // TODO
    // pa_context_set_card_profile_by_index ()
    return TRUE;
}
