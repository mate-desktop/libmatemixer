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

#include <libmatemixer/matemixer-device.h>
#include <libmatemixer/matemixer-device-profile.h>
#include <libmatemixer/matemixer-device-profile-private.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-port.h>
#include <libmatemixer/matemixer-port-private.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-device.h"

struct _PulseDevicePrivate
{
    guint32                 index;
    gchar                  *name;
    gchar                  *description;
    gchar                  *icon;
    GHashTable             *ports;
    GList                  *ports_list;
    GHashTable             *profiles;
    GList                  *profiles_list;
    PulseConnection        *connection;
    MateMixerDeviceProfile *profile;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_ICON,
    PROP_ACTIVE_PROFILE,
    PROP_INDEX,
    PROP_CONNECTION
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

#if PA_CHECK_VERSION (5, 0, 0)
typedef pa_card_profile_info2 _pa_card_profile_info;
#else
typedef pa_card_profile_info  _pa_card_profile_info;
#endif

static const gchar *           pulse_device_get_name           (MateMixerDevice        *device);
static const gchar *           pulse_device_get_description    (MateMixerDevice        *device);
static const gchar *           pulse_device_get_icon           (MateMixerDevice        *device);

static MateMixerPort *         pulse_device_get_port           (MateMixerDevice        *device,
                                                                const gchar            *name);
static MateMixerDeviceProfile *pulse_device_get_profile        (MateMixerDevice        *device,
                                                                const gchar            *name);

static const GList *           pulse_device_list_ports         (MateMixerDevice        *device);
static const GList *           pulse_device_list_profiles      (MateMixerDevice        *device);

static MateMixerDeviceProfile *pulse_device_get_active_profile (MateMixerDevice        *device);
static gboolean                pulse_device_set_active_profile (MateMixerDevice        *device,
                                                                MateMixerDeviceProfile *profile);

static void                    update_port      (PulseDevice          *device,
                                                 pa_card_port_info    *p_info);
static void                    update_profile   (PulseDevice          *device,
                                                _pa_card_profile_info *p_info);

static gint                    compare_ports    (gconstpointer         a,
                                                 gconstpointer         b);
static gint                    compare_profiles (gconstpointer         a,
                                                 gconstpointer         b);

static void
mate_mixer_device_interface_init (MateMixerDeviceInterface *iface)
{
    iface->get_name           = pulse_device_get_name;
    iface->get_description    = pulse_device_get_description;
    iface->get_icon           = pulse_device_get_icon;
    iface->get_port           = pulse_device_get_port;
    iface->get_profile        = pulse_device_get_profile;
    iface->list_ports         = pulse_device_list_ports;
    iface->list_profiles      = pulse_device_list_profiles;
    iface->get_active_profile = pulse_device_get_active_profile;
    iface->set_active_profile = pulse_device_set_active_profile;
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

    device->priv->ports = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 g_object_unref);

    device->priv->profiles = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    g_object_unref);
}

static void
pulse_device_dispose (GObject *object)
{
    PulseDevice *device;

    device = PULSE_DEVICE (object);

    if (device->priv->ports_list != NULL) {
        g_list_free_full (device->priv->ports_list, g_object_unref);
        device->priv->ports_list = NULL;
    }
    g_hash_table_remove_all (device->priv->ports);

    if (device->priv->profiles_list != NULL) {
        g_list_free_full (device->priv->profiles_list, g_object_unref);
        device->priv->profiles_list = NULL;
    }
    g_hash_table_remove_all (device->priv->profiles);

    g_clear_object (&device->priv->profile);
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

    g_hash_table_destroy (device->priv->ports);
    g_hash_table_destroy (device->priv->profiles);

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
    MateMixerDeviceProfile *profile = NULL;
    const gchar *prop;
    guint32 i;

    g_return_val_if_fail (PULSE_IS_DEVICE (device), FALSE);
    g_return_val_if_fail (info != NULL, FALSE);

    /* Let all the information update before emitting notify signals */
    g_object_freeze_notify (G_OBJECT (device));

    /* Name */
    if (g_strcmp0 (device->priv->name, info->name) != 0) {
        g_free (device->priv->name);
        device->priv->name = g_strdup (info->name);

        g_object_notify (G_OBJECT (device), "name");
    }

    /* Description */
    prop = pa_proplist_gets (info->proplist, PA_PROP_DEVICE_DESCRIPTION);

    if (G_UNLIKELY (prop == NULL))
        prop = info->name;

    if (g_strcmp0 (device->priv->description, prop) != 0) {
        g_free (device->priv->description);
        device->priv->description = g_strdup (prop);

        g_object_notify (G_OBJECT (device), "description");
    }

    /* Icon */
    prop = pa_proplist_gets (info->proplist, PA_PROP_DEVICE_ICON_NAME);

    if (G_UNLIKELY (prop == NULL))
        prop = "audio-card";

    if (g_strcmp0 (device->priv->icon, prop) != 0) {
        g_free (device->priv->icon);
        device->priv->icon = g_strdup (prop);

        g_object_notify (G_OBJECT (device), "icon");
    }

#if PA_CHECK_VERSION (2, 0, 0)
    /* List of ports */
    for (i = 0; i < info->n_ports; i++) {
        update_port (device, info->ports[i]);
    }
#endif

    /* List of profiles */
    for (i = 0; i < info->n_profiles; i++) {
#if PA_CHECK_VERSION (5, 0, 0)
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
        update_profile (device, p_info);
    }

    /* Figure out whether the currently active profile has changed */
    profile = NULL;

#if PA_CHECK_VERSION (5, 0, 0)
    if (G_LIKELY (info->active_profile2 != NULL))
        profile = g_hash_table_lookup (device->priv->profiles, info->active_profile2->name);
#else
    if (G_LIKELY (info->active_profile != NULL))
        profile = g_hash_table_lookup (device->priv->profiles, info->active_profile->name);
#endif

    if (profile != device->priv->profile) {
        g_clear_object (&device->priv->profile);

        if (G_LIKELY (profile != NULL))
            device->priv->profile = g_object_ref (profile);

        g_object_notify (G_OBJECT (device), "active-profile");
    }

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
pulse_device_get_name (MateMixerDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return PULSE_DEVICE (device)->priv->name;
}

static const gchar *
pulse_device_get_description (MateMixerDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return PULSE_DEVICE (device)->priv->description;
}

static const gchar *
pulse_device_get_icon (MateMixerDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return PULSE_DEVICE (device)->priv->icon;
}

static MateMixerPort *
pulse_device_get_port (MateMixerDevice *device, const gchar *name)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_hash_table_lookup (PULSE_DEVICE (device)->priv->ports, name);
}

static MateMixerDeviceProfile *
pulse_device_get_profile (MateMixerDevice *device, const gchar *name)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_hash_table_lookup (PULSE_DEVICE (device)->priv->profiles, name);
}

static const GList *
pulse_device_list_ports (MateMixerDevice *device)
{
    PulseDevice *pulse;

    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    pulse = PULSE_DEVICE (device);

    if (pulse->priv->ports_list == NULL) {
        GList *list = g_hash_table_get_values (pulse->priv->ports);

        if (list != NULL) {
            g_list_foreach (list, (GFunc) g_object_ref, NULL);

            pulse->priv->ports_list = g_list_sort (list, compare_ports);
        }
    }

    return (const GList *) pulse->priv->ports_list;
}

static const GList *
pulse_device_list_profiles (MateMixerDevice *device)
{
    PulseDevice *pulse;

    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    pulse = PULSE_DEVICE (device);

    if (pulse->priv->profiles_list == NULL) {
        GList *list = g_hash_table_get_values (pulse->priv->profiles);

        if (list != NULL) {
            g_list_foreach (list, (GFunc) g_object_ref, NULL);

            pulse->priv->profiles_list = g_list_sort (list, compare_profiles);
        }
    }

    return (const GList *) pulse->priv->profiles_list;
}

static MateMixerDeviceProfile *
pulse_device_get_active_profile (MateMixerDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return PULSE_DEVICE (device)->priv->profile;
}

static gboolean
pulse_device_set_active_profile (MateMixerDevice *device, MateMixerDeviceProfile *profile)
{
    PulseDevice *pulse;
    const gchar *name;
    gboolean     ret;

    g_return_val_if_fail (PULSE_IS_DEVICE (device), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), FALSE);

    pulse = PULSE_DEVICE (device);

    name = mate_mixer_device_profile_get_name (profile);

    /* Make sure the profile belongs to the device */
    if (g_hash_table_lookup (pulse->priv->profiles, name) == NULL) {
        g_warning ("Profile %s does not belong to device %s", name, pulse->priv->name);
        return FALSE;
    }

    ret = pulse_connection_set_card_profile (pulse->priv->connection,
                                             pulse->priv->name,
                                             name);
    if (ret == TRUE) {
        if (pulse->priv->profile != NULL)
            g_object_unref (pulse->priv->profile);

        pulse->priv->profile = g_object_ref (profile);

        g_object_notify (G_OBJECT (device), "active-profile");
    }
    return ret;
}

static void
update_port (PulseDevice *device, pa_card_port_info *p_info)
{
    MateMixerPort     *port;
    MateMixerPortFlags flags = MATE_MIXER_PORT_NO_FLAGS;
    const gchar       *icon;

    icon = pa_proplist_gets (p_info->proplist, "device.icon_name");

    if (p_info->available == PA_PORT_AVAILABLE_YES)
        flags |= MATE_MIXER_PORT_AVAILABLE;

    if (p_info->direction & PA_DIRECTION_INPUT)
        flags |= MATE_MIXER_PORT_INPUT;
    if (p_info->direction & PA_DIRECTION_OUTPUT)
        flags |= MATE_MIXER_PORT_OUTPUT;

    port = g_hash_table_lookup (device->priv->ports, p_info->name);

    if (port != NULL) {
        /* Update existing port */
        _mate_mixer_port_update_description (port, p_info->description);
        _mate_mixer_port_update_icon (port, icon);
        _mate_mixer_port_update_priority (port, p_info->priority);
        _mate_mixer_port_update_flags (port, flags);
    } else {
        /* Add previously unknown port to the hash table */
        port = _mate_mixer_port_new (p_info->name,
                                     p_info->description,
                                     icon,
                                     p_info->priority,
                                     flags);

        g_hash_table_insert (device->priv->ports, g_strdup (p_info->name), port);
    }
}

static void
update_profile (PulseDevice *device, _pa_card_profile_info *p_info)
{
    MateMixerDeviceProfile *profile;

    profile = g_hash_table_lookup (device->priv->profiles, p_info->name);

    if (profile != NULL) {
        /* Update existing profile */
        _mate_mixer_device_profile_update_description (profile, p_info->description);
        _mate_mixer_device_profile_update_priority (profile, p_info->priority);
        _mate_mixer_device_profile_update_num_input_streams (profile, p_info->n_sources);
        _mate_mixer_device_profile_update_num_output_streams (profile, p_info->n_sinks);
    } else {
        /* Add previously unknown profile to the hash table */
        profile = _mate_mixer_device_profile_new (p_info->name,
                                                  p_info->description,
                                                  p_info->priority,
                                                  p_info->n_sources,
                                                  p_info->n_sinks);

        g_hash_table_insert (device->priv->profiles, g_strdup (p_info->name), profile);
    }
}

static gint
compare_ports (gconstpointer a, gconstpointer b)
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
compare_profiles (gconstpointer a, gconstpointer b)
{
    MateMixerDeviceProfile *p1 = MATE_MIXER_DEVICE_PROFILE (a);
    MateMixerDeviceProfile *p2 = MATE_MIXER_DEVICE_PROFILE (b);

    gint ret = (gint) (mate_mixer_device_profile_get_priority (p2) -
                       mate_mixer_device_profile_get_priority (p1));
    if (ret != 0)
        return ret;
    else
        return strcmp (mate_mixer_device_profile_get_name (p1),
                       mate_mixer_device_profile_get_name (p2));
}
