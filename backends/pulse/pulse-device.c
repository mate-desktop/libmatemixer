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
#include <libmatemixer/matemixer-port.h>
#include <libmatemixer/matemixer-profile.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-device.h"

struct _MateMixerPulseDevicePrivate
{
    guint32                   index;
    gchar                    *name;
    gchar                    *description;
    GList                    *profiles;
    GList                    *ports;
    gchar                    *icon;
    MateMixerProfile         *profile;
    MateMixerPulseConnection *connection;
};

enum
{
    PROP_0,
    PROP_NAME,
    PROP_DESCRIPTION,
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
    iface->get_name = mate_mixer_pulse_device_get_name;
    iface->get_description = mate_mixer_pulse_device_get_description;
    iface->get_icon = mate_mixer_pulse_device_get_icon;
    iface->list_streams = mate_mixer_pulse_device_list_streams;
    iface->list_ports = mate_mixer_pulse_device_list_ports;
    iface->list_profiles = mate_mixer_pulse_device_list_profiles;
    iface->get_active_profile = mate_mixer_pulse_device_get_active_profile;
    iface->set_active_profile = mate_mixer_pulse_device_set_active_profile;
}

static void
mate_mixer_pulse_device_init (MateMixerPulseDevice *device)
{
    device->priv = G_TYPE_INSTANCE_GET_PRIVATE (device,
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
    case PROP_NAME:
        g_value_set_string (value, device->priv->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, device->priv->description);
        break;
    case PROP_ICON:
        g_value_set_string (value, device->priv->icon);
        break;
    case PROP_ACTIVE_PROFILE:
        g_value_set_object (value, device->priv->profile);
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
    case PROP_NAME:
        device->priv->name = g_strdup (g_value_get_string (value));
        break;
    case PROP_DESCRIPTION:
        device->priv->description = g_strdup (g_value_get_string (value));
        break;
    case PROP_ICON:
        device->priv->icon = g_strdup (g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_pulse_device_dispose (GObject *object)
{
    MateMixerPulseDevice *device;

    device = MATE_MIXER_PULSE_DEVICE (object);

    if (device->priv->profiles != NULL) {
        g_list_free_full (device->priv->profiles, g_object_unref);
        device->priv->profiles = NULL;
    }

    if (device->priv->ports != NULL) {
        g_list_free_full (device->priv->ports, g_object_unref);
        device->priv->ports = NULL;
    }

    g_clear_object (&device->priv->profile);
    g_clear_object (&device->priv->connection);

    G_OBJECT_CLASS (mate_mixer_pulse_device_parent_class)->dispose (object);
}

static void
mate_mixer_pulse_device_finalize (GObject *object)
{
    MateMixerPulseDevice *device;

    device = MATE_MIXER_PULSE_DEVICE (object);

    g_free (device->priv->name);
    g_free (device->priv->description);
    g_free (device->priv->icon);

    G_OBJECT_CLASS (mate_mixer_pulse_device_parent_class)->finalize (object);
}

static void
mate_mixer_pulse_device_class_init (MateMixerPulseDeviceClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = mate_mixer_pulse_device_dispose;
    object_class->finalize     = mate_mixer_pulse_device_finalize;
    object_class->get_property = mate_mixer_pulse_device_get_property;
    object_class->set_property = mate_mixer_pulse_device_set_property;

    g_object_class_override_property (object_class, PROP_NAME, "name");
    g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (object_class, PROP_ICON, "icon");
    g_object_class_override_property (object_class, PROP_ACTIVE_PROFILE, "active-profile");

    g_type_class_add_private (object_class, sizeof (MateMixerPulseDevicePrivate));
}

MateMixerPulseDevice *
mate_mixer_pulse_device_new (MateMixerPulseConnection *connection, const pa_card_info *info)
{
    MateMixerPulseDevice *device;
    MateMixerProfile     *active_profile = NULL;
    GList                *profiles = NULL;
    GList                *ports = NULL;
    guint32               i;

    g_return_val_if_fail (info != NULL, NULL);

    /* Create a list of card profiles */
    for (i = 0; i < info->n_profiles; i++) {
        MateMixerProfile *profile;

#if PA_CHECK_VERSION(5, 0, 0)
        pa_card_profile_info2 *p_info = info->profiles2[i];

        /* PulseAudio 5.0 includes a new pa_card_profile_info2 which
         * only differs in the new available flag, we use it not to include
         * profiles which are unavailable */
        if (p_info->available == 0)
            continue;
#else
        /* The old profile list is an array of structs, not pointers */
        pa_card_profile_info *p_info = &info->profiles[i];
#endif
        profile = mate_mixer_profile_new (
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
        MateMixerPort       *port;
        MateMixerPortStatus  status = MATE_MIXER_PORT_UNKNOWN_STATUS;
        pa_card_port_info   *p_info = info->ports[i];

#if PA_CHECK_VERSION(2, 0, 0)
        switch (p_info->available) {
        case PA_PORT_AVAILABLE_YES:
            status = MATE_MIXER_PORT_AVAILABLE;
            break;
        case PA_PORT_AVAILABLE_NO:
            status = MATE_MIXER_PORT_UNAVAILABLE;
            break;
        default:
            break;
        }
#endif
        port = mate_mixer_port_new (p_info->name,
                                    p_info->description,
                                    pa_proplist_gets (p_info->proplist, "device.icon_name"),
                                    p_info->priority,
                                    status);

        ports = g_list_prepend (ports, port);
    }

    /* Keep the ports in the same order as in PulseAudio */
    if (ports)
        ports = g_list_reverse (ports);

    device = g_object_new (MATE_MIXER_TYPE_PULSE_DEVICE,
                           "name", info->name,
                           "description", pa_proplist_gets (info->proplist, "device.description"),
                           "icon", pa_proplist_gets (info->proplist, "device.icon_name"),
                           NULL);

    if (profiles) {
        device->priv->profiles = profiles;

        if (G_LIKELY (active_profile))
            device->priv->profile = g_object_ref (active_profile);
    }

    device->priv->index = info->index;
    device->priv->ports = ports;
    device->priv->connection = g_object_ref (connection);

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

MateMixerPulseConnection *
mate_mixer_pulse_device_get_connection (MateMixerPulseDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), NULL);

    return device->priv->connection;
}

guint32
mate_mixer_pulse_device_get_index (MateMixerPulseDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), 0);

    return device->priv->index;
}

const gchar *
mate_mixer_pulse_device_get_name (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), NULL);

    return MATE_MIXER_PULSE_DEVICE (device)->priv->name;
}

const gchar *
mate_mixer_pulse_device_get_description (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), NULL);

    return MATE_MIXER_PULSE_DEVICE (device)->priv->description;
}

const gchar *
mate_mixer_pulse_device_get_icon (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), NULL);

    return MATE_MIXER_PULSE_DEVICE (device)->priv->icon;
}

const GList *
mate_mixer_pulse_device_list_streams (MateMixerDevice *device)
{
    // TODO
    return NULL;
}

const GList *
mate_mixer_pulse_device_list_ports (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), NULL);

    return (const GList *) MATE_MIXER_PULSE_DEVICE (device)->priv->ports;
}

const GList *
mate_mixer_pulse_device_list_profiles (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), NULL);

    return (const GList *) MATE_MIXER_PULSE_DEVICE (device)->priv->profiles;
}

MateMixerProfile *
mate_mixer_pulse_device_get_active_profile (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), NULL);

    return MATE_MIXER_PULSE_DEVICE (device)->priv->profile;
}

gboolean
mate_mixer_pulse_device_set_active_profile (MateMixerDevice *device, const gchar *name)
{
    gboolean ret;
    MateMixerPulseDevicePrivate *priv;

    g_return_val_if_fail (MATE_MIXER_IS_PULSE_DEVICE (device), FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    priv = MATE_MIXER_PULSE_DEVICE (device)->priv;
    ret  = mate_mixer_pulse_connection_set_card_profile (priv->connection,
                                                         priv->name,
                                                         name);

    // XXX decide to either confirm the change during the connection call or
    // wait for a notification from Pulse
/*
    if (ret) {
        if (priv->profile)
            g_object_unref (priv->profile);

        priv->profile = g_object_ref (profile);

        g_object_notify_by_pspec (
            G_OBJECT (device),
            properties[PROP_ACTIVE_PROFILE]);
    }
*/
    return ret;
}
