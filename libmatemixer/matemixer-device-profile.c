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

#include "matemixer-device-profile.h"

struct _MateMixerDeviceProfilePrivate
{
    gchar    *identifier;
    gchar    *name;
    guint32   priority;
};

enum
{
    PROP_0,
    PROP_IDENTIFIER,
    PROP_NAME,
    PROP_PRIORITY,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (MateMixerDeviceProfile, mate_mixer_device_profile, G_TYPE_OBJECT);

static void
mate_mixer_device_profile_init (MateMixerDeviceProfile *profile)
{
    profile->priv = G_TYPE_INSTANCE_GET_PRIVATE (
        profile,
        MATE_MIXER_TYPE_DEVICE_PROFILE,
        MateMixerDeviceProfilePrivate);
}

static void
mate_mixer_device_profile_get_property (GObject     *object,
                                        guint        param_id,
                                        GValue      *value,
                                        GParamSpec  *pspec)
{
    MateMixerDeviceProfile *profile;

    profile = MATE_MIXER_DEVICE_PROFILE (object);

    switch (param_id) {
    case PROP_IDENTIFIER:
        g_value_set_string (value, profile->priv->identifier);
        break;
    case PROP_NAME:
        g_value_set_string (value, profile->priv->name);
        break;
    case PROP_PRIORITY:
        g_value_set_uint (value, profile->priv->priority);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_device_profile_set_property (GObject       *object,
                                        guint          param_id,
                                        const GValue  *value,
                                        GParamSpec    *pspec)
{
    MateMixerDeviceProfile *profile;

    profile = MATE_MIXER_DEVICE_PROFILE (object);

    switch (param_id) {
    case PROP_IDENTIFIER:
        profile->priv->identifier = g_strdup (g_value_get_string (value));
        break;
    case PROP_NAME:
        profile->priv->name = g_strdup (g_value_get_string (value));
        break;
    case PROP_PRIORITY:
        profile->priv->priority = g_value_get_uint (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_device_profile_finalize (GObject *object)
{
    MateMixerDeviceProfile *profile;

    profile = MATE_MIXER_DEVICE_PROFILE (object);

    g_free (profile->priv->identifier);
    g_free (profile->priv->name);

    G_OBJECT_CLASS (mate_mixer_device_profile_parent_class)->finalize (object);
}

static void
mate_mixer_device_profile_class_init (MateMixerDeviceProfileClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = mate_mixer_device_profile_finalize;
    object_class->get_property = mate_mixer_device_profile_get_property;
    object_class->set_property = mate_mixer_device_profile_set_property;

    properties[PROP_IDENTIFIER] = g_param_spec_string (
        "identifier",
        "Identifier",
        "Identifier of the device profile",
        NULL,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    properties[PROP_NAME] = g_param_spec_string (
        "name",
        "Name",
        "Name of the device profile",
        NULL,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    properties[PROP_PRIORITY] = g_param_spec_uint (
        "priority",
        "Priority",
        "Priority of the device profile",
        0,
        G_MAXUINT32,
        0,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    g_type_class_add_private (object_class, sizeof (MateMixerDeviceProfilePrivate));
}

MateMixerDeviceProfile *
mate_mixer_device_profile_new (const gchar  *identifier,
                               const gchar  *name,
                               guint32       priority)
{
    return g_object_new (MATE_MIXER_TYPE_DEVICE_PROFILE,
        "identifier",    identifier,
        "name",          name,
        "priority",      priority,
        NULL);
}

const gchar *
mate_mixer_device_profile_get_identifier (MateMixerDeviceProfile *profile)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), NULL);

    return profile->priv->identifier;
}

const gchar *
mate_mixer_device_profile_get_name (MateMixerDeviceProfile *profile)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), NULL);

    return profile->priv->name;
}

guint32
mate_mixer_device_profile_get_priority (MateMixerDeviceProfile *profile)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), NULL);

    return profile->priv->priority;
}
