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
#include <string.h>

#include <libmatemixer/matemixer-device.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-port.h>
#include <libmatemixer/matemixer-profile.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-device.h"
#include "pulse-stream.h"

struct _PulseDevicePrivate
{
    guint32           index;
    gchar            *name;
    gchar            *description;
    GList            *profiles;
    GList            *ports;
    GList            *streams;
    gboolean          streams_sorted;
    gchar            *icon;
    PulseConnection  *connection;
    MateMixerProfile *profile;
};

enum
{
    PROP_0,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_ICON,
    PROP_PORTS,
    PROP_PROFILES,
    PROP_ACTIVE_PROFILE,
    PROP_INDEX,
    PROP_CONNECTION,
    N_PROPERTIES
};

static void mate_mixer_device_interface_init (MateMixerDeviceInterface *iface);

static void pulse_device_class_init   (PulseDeviceClass *klass);

static void pulse_device_get_property (GObject          *object,
                                       guint             param_id,
                                       GValue           *value,
                                       GParamSpec       *pspec);
static void pulse_device_set_property (GObject          *object,
                                       guint             param_id,
                                       const GValue     *value,
                                       GParamSpec       *pspec);

static void pulse_device_init         (PulseDevice      *device);
static void pulse_device_dispose      (GObject          *object);
static void pulse_device_finalize     (GObject          *object);

G_DEFINE_TYPE_WITH_CODE (PulseDevice, pulse_device, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_DEVICE,
                                                mate_mixer_device_interface_init))

static const gchar *     device_get_name           (MateMixerDevice *device);
static const gchar *     device_get_description    (MateMixerDevice *device);
static const gchar *     device_get_icon           (MateMixerDevice *device);

static const GList *     device_list_ports         (MateMixerDevice *device);
static const GList *     device_list_profiles      (MateMixerDevice *device);

static MateMixerProfile *device_get_active_profile (MateMixerDevice *device);
static gboolean          device_set_active_profile (MateMixerDevice *device,
                                                    const gchar     *profile);

static gint              device_compare_ports      (gconstpointer    a,
                                                    gconstpointer    b);
static gint              device_compare_profiles   (gconstpointer    a,
                                                    gconstpointer    b);

static void              device_free_ports         (PulseDevice     *device);
static void              device_free_profiles      (PulseDevice     *device);

static void
mate_mixer_device_interface_init (MateMixerDeviceInterface *iface)
{
    iface->get_name           = device_get_name;
    iface->get_description    = device_get_description;
    iface->get_icon           = device_get_icon;
    iface->list_ports         = device_list_ports;
    iface->list_profiles      = device_list_profiles;
    iface->get_active_profile = device_get_active_profile;
    iface->set_active_profile = device_set_active_profile;
}

static void
pulse_device_class_init (PulseDeviceClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = pulse_device_dispose;
    object_class->finalize     = pulse_device_finalize;
    object_class->get_property = pulse_device_get_property;
    object_class->set_property = pulse_device_set_property;

    g_object_class_install_property (object_class,
                                     PROP_INDEX,
                                     g_param_spec_uint ("index",
                                                        "Index",
                                                        "Device index",
                                                        0,
                                                        G_MAXUINT,
                                                        0,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class,
                                     PROP_CONNECTION,
                                     g_param_spec_object ("connection",
                                                          "Connection",
                                                          "PulseAudio connection",
                                                          PULSE_TYPE_CONNECTION,
                                                          G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));

    g_object_class_override_property (object_class, PROP_NAME, "name");
    g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (object_class, PROP_ICON, "icon");
    g_object_class_override_property (object_class, PROP_PORTS, "ports");
    g_object_class_override_property (object_class, PROP_PROFILES, "profiles");
    g_object_class_override_property (object_class, PROP_ACTIVE_PROFILE, "active-profile");

    g_type_class_add_private (object_class, sizeof (PulseDevicePrivate));
}

static void
pulse_device_get_property (GObject    *object,
                           guint       param_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    PulseDevice *device;

    device = PULSE_DEVICE (object);

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
    case PROP_PORTS:
        g_value_set_pointer (value, device->priv->ports);
        break;
    case PROP_PROFILES:
        g_value_set_pointer (value, device->priv->profiles);
        break;
    case PROP_ACTIVE_PROFILE:
        g_value_set_object (value, device->priv->profile);
        break;
    case PROP_INDEX:
        g_value_set_uint (value, device->priv->index);
        break;
    case PROP_CONNECTION:
        g_value_set_object (value, device->priv->connection);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_device_set_property (GObject      *object,
                           guint         param_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    PulseDevice *device;

    device = PULSE_DEVICE (object);

    switch (param_id) {
    case PROP_INDEX:
        device->priv->index = g_value_get_uint (value);
        break;
    case PROP_CONNECTION:
        /* Construct-only object */
        device->priv->connection = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_device_init (PulseDevice *device)
{
    device->priv = G_TYPE_INSTANCE_GET_PRIVATE (device,
                                                PULSE_TYPE_DEVICE,
                                                PulseDevicePrivate);
}

static void
pulse_device_dispose (GObject *object)
{
    PulseDevice *device;

    device = PULSE_DEVICE (object);

    device_free_ports (device);
    device_free_profiles (device);

    g_clear_object (&device->priv->connection);

    G_OBJECT_CLASS (pulse_device_parent_class)->dispose (object);
}

static void
pulse_device_finalize (GObject *object)
{
    PulseDevice *device;

    device = PULSE_DEVICE (object);

    g_free (device->priv->name);
    g_free (device->priv->description);
    g_free (device->priv->icon);

    G_OBJECT_CLASS (pulse_device_parent_class)->finalize (object);
}

PulseDevice *
pulse_device_new (PulseConnection *connection, const pa_card_info *info)
{
    PulseDevice *device;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);

    /* Consider the device index as unchanging parameter */
    device = g_object_new (PULSE_TYPE_DEVICE,
                           "connection", connection,
                           "index", info->index,
                           NULL);

    /* Other data may change at any time, so let's make a use of our update function */
    pulse_device_update (device, info);

    return device;
}

gboolean
pulse_device_update (PulseDevice *device, const pa_card_info *info)
{
    const gchar *prop;
    guint32 i;

    g_return_val_if_fail (PULSE_IS_DEVICE (device), FALSE);
    g_return_val_if_fail (info != NULL, FALSE);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (device));

    /* Name */
    if (g_strcmp0 (device->priv->name, info->name)) {
        g_free (device->priv->name);
        device->priv->name = g_strdup (info->name);

        g_object_notify (G_OBJECT (device), "name");
    }

    /* Description */
    prop = pa_proplist_gets (info->proplist, PA_PROP_DEVICE_DESCRIPTION);

    if (G_UNLIKELY (prop == NULL))
        prop = info->name;

    if (g_strcmp0 (device->priv->description, prop)) {
        g_free (device->priv->description);
        device->priv->description = g_strdup (prop);

        g_object_notify (G_OBJECT (device), "description");
    }

    /* Icon */
    prop = pa_proplist_gets (info->proplist, PA_PROP_DEVICE_ICON_NAME);

    if (G_UNLIKELY (prop == NULL))
        prop = "audio-card";

    if (g_strcmp0 (device->priv->icon, prop)) {
        g_free (device->priv->icon);
        device->priv->icon = g_strdup (prop);

        g_object_notify (G_OBJECT (device), "icon");
    }

    /* List of ports */
    device_free_ports (device);

    for (i = 0; i < info->n_ports; i++) {
        MateMixerPortFlags flags = MATE_MIXER_PORT_NO_FLAGS;

        prop = pa_proplist_gets (info->ports[i]->proplist, "device.icon_name");

#if PA_CHECK_VERSION(2, 0, 0)
        if (info->ports[i]->available == PA_PORT_AVAILABLE_YES)
            flags |= MATE_MIXER_PORT_AVAILABLE;

        if (info->ports[i]->direction & PA_DIRECTION_INPUT)
            flags |= MATE_MIXER_PORT_INPUT;
        if (info->ports[i]->direction & PA_DIRECTION_OUTPUT)
            flags |= MATE_MIXER_PORT_OUTPUT;
#endif
        device->priv->ports =
            g_list_prepend (device->priv->ports,
                            mate_mixer_port_new (info->ports[i]->name,
                                                 info->ports[i]->description,
                                                 prop,
                                                 info->ports[i]->priority,
                                                 flags));
    }
    device->priv->ports = g_list_sort (device->priv->ports, device_compare_ports);

    g_object_notify (G_OBJECT (device), "ports");

    /* List of profiles */
    device_free_profiles (device);

    for (i = 0; i < info->n_profiles; i++) {
        MateMixerProfile *profile;

#if PA_CHECK_VERSION(5, 0, 0)
        pa_card_profile_info2 *p_info = info->profiles2[i];

        /* PulseAudio 5.0 includes a new pa_card_profile_info2 which only
         * differs in the new available flag, we use it not to include profiles
         * which are unavailable */
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

        if (device->priv->profile == NULL) {
#if PA_CHECK_VERSION(5, 0, 0)
            if (!g_strcmp0 (p_info->name, info->active_profile2->name))
                device->priv->profile = g_object_ref (profile);
#else
            if (!g_strcmp0 (p_info->name, info->active_profile->name))
                device->priv->profile = g_object_ref (profile);
#endif
        }
        device->priv->profiles = g_list_prepend (device->priv->profiles, profile);
    }
    device->priv->profiles = g_list_sort (device->priv->profiles,
                                          device_compare_profiles);

    g_object_notify (G_OBJECT (device), "profiles");

    g_object_thaw_notify (G_OBJECT (device));
    return TRUE;
}

PulseConnection *
pulse_device_get_connection (PulseDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return device->priv->connection;
}

guint32
pulse_device_get_index (PulseDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), 0);

    return device->priv->index;
}

static const gchar *
device_get_name (MateMixerDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return PULSE_DEVICE (device)->priv->name;
}

static const gchar *
device_get_description (MateMixerDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return PULSE_DEVICE (device)->priv->description;
}

static const gchar *
device_get_icon (MateMixerDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return PULSE_DEVICE (device)->priv->icon;
}

static const GList *
device_list_ports (MateMixerDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return (const GList *) PULSE_DEVICE (device)->priv->ports;
}

static const GList *
device_list_profiles (MateMixerDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return (const GList *) PULSE_DEVICE (device)->priv->profiles;
}

static MateMixerProfile *
device_get_active_profile (MateMixerDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return PULSE_DEVICE (device)->priv->profile;
}

static gboolean
device_set_active_profile (MateMixerDevice *device, const gchar *profile)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), FALSE);
    g_return_val_if_fail (profile != NULL, FALSE);

    return pulse_connection_set_card_profile (PULSE_DEVICE (device)->priv->connection,
                                              PULSE_DEVICE (device)->priv->name,
                                              profile);
}

static gint
device_compare_ports (gconstpointer a, gconstpointer b)
{
    MateMixerPort *p1 = MATE_MIXER_PORT (a);
    MateMixerPort *p2 = MATE_MIXER_PORT (b);

    gint ret = (gint) (mate_mixer_port_get_priority (p2) -
                       mate_mixer_port_get_priority (p1));
    if (ret != 0)
        return ret;
    else
        return strcmp (mate_mixer_port_get_name (p1),
                       mate_mixer_port_get_name (p2));
}

static gint
device_compare_profiles (gconstpointer a, gconstpointer b)
{
    MateMixerProfile *p1 = MATE_MIXER_PROFILE (a);
    MateMixerProfile *p2 = MATE_MIXER_PROFILE (b);

    gint ret = (gint) (mate_mixer_profile_get_priority (p2) -
                       mate_mixer_profile_get_priority (p1));
    if (ret != 0)
        return ret;
    else
        return strcmp (mate_mixer_profile_get_name (p1),
                       mate_mixer_profile_get_name (p2));
}

static void
device_free_ports (PulseDevice *device)
{
    if (device->priv->ports == NULL)
        return;

    g_list_free_full (device->priv->ports, g_object_unref);

    device->priv->ports = NULL;
}

static void
device_free_profiles (PulseDevice *device)
{
    if (device->priv->profiles == NULL)
        return;

    g_list_free_full (device->priv->profiles, g_object_unref);

    g_clear_object (&device->priv->profile);

    device->priv->profiles = NULL;
}
