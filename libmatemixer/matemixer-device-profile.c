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
#include "matemixer-device-profile-private.h"

/**
 * SECTION:matemixer-device-profile
 * @short_description: Device profile
 * @include: libmatemixer/matemixer.h
 */

struct _MateMixerDeviceProfilePrivate
{
    gchar *name;
    gchar *description;
    guint  priority;
    guint  num_input_streams;
    guint  num_output_streams;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_PRIORITY,
    PROP_NUM_INPUT_STREAMS,
    PROP_NUM_OUTPUT_STREAMS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void mate_mixer_device_profile_class_init   (MateMixerDeviceProfileClass *klass);

static void mate_mixer_device_profile_get_property (GObject                     *object,
                                                    guint                        param_id,
                                                    GValue                      *value,
                                                    GParamSpec                  *pspec);
static void mate_mixer_device_profile_set_property (GObject                     *object,
                                                    guint                        param_id,
                                                    const GValue                *value,
                                                    GParamSpec                  *pspec);

static void mate_mixer_device_profile_init         (MateMixerDeviceProfile      *profile);
static void mate_mixer_device_profile_finalize     (GObject                     *object);

G_DEFINE_TYPE (MateMixerDeviceProfile, mate_mixer_device_profile, G_TYPE_OBJECT);

static void
mate_mixer_device_profile_class_init (MateMixerDeviceProfileClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = mate_mixer_device_profile_finalize;
    object_class->get_property = mate_mixer_device_profile_get_property;
    object_class->set_property = mate_mixer_device_profile_set_property;

    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "Name of the profile",
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_DESCRIPTION] =
        g_param_spec_string ("description",
                             "Description",
                             "Description of the profile",
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_PRIORITY] =
        g_param_spec_uint ("priority",
                           "Priority",
                           "Priority of the profile",
                           0,
                           G_MAXUINT,
                           0,
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS);

    properties[PROP_NUM_INPUT_STREAMS] =
        g_param_spec_uint ("num-input-streams",
                           "Number of input streams",
                           "Number of input streams in the profile",
                           0,
                           G_MAXUINT,
                           0,
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS);

    properties[PROP_NUM_OUTPUT_STREAMS] =
        g_param_spec_uint ("num-output-streams",
                           "Number of output streams",
                           "Number of output streams in the profile",
                           0,
                           G_MAXUINT,
                           0,
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    g_type_class_add_private (object_class, sizeof (MateMixerDeviceProfilePrivate));
}

static void
mate_mixer_device_profile_get_property (GObject    *object,
                                        guint       param_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
    MateMixerDeviceProfile *profile;

    profile = MATE_MIXER_DEVICE_PROFILE (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, profile->priv->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, profile->priv->description);
        break;
    case PROP_PRIORITY:
        g_value_set_uint (value, profile->priv->priority);
        break;
    case PROP_NUM_INPUT_STREAMS:
        g_value_set_uint (value, profile->priv->num_input_streams);
        break;
    case PROP_NUM_OUTPUT_STREAMS:
        g_value_set_uint (value, profile->priv->num_output_streams);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_device_profile_set_property (GObject      *object,
                                        guint         param_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
    MateMixerDeviceProfile *profile;

    profile = MATE_MIXER_DEVICE_PROFILE (object);

    switch (param_id) {
    case PROP_NAME:
        /* Construct-only string */
        profile->priv->name = g_strdup (g_value_get_string (value));
        break;
    case PROP_DESCRIPTION:
        /* Construct-only string */
        profile->priv->description = g_strdup (g_value_get_string (value));
        break;
    case PROP_PRIORITY:
        profile->priv->priority = g_value_get_uint (value);
        break;
    case PROP_NUM_INPUT_STREAMS:
        profile->priv->num_input_streams = g_value_get_uint (value);
        break;
    case PROP_NUM_OUTPUT_STREAMS:
        profile->priv->num_output_streams = g_value_get_uint (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_device_profile_init (MateMixerDeviceProfile *profile)
{
    profile->priv = G_TYPE_INSTANCE_GET_PRIVATE (profile,
                                                 MATE_MIXER_TYPE_DEVICE_PROFILE,
                                                 MateMixerDeviceProfilePrivate);
}

static void
mate_mixer_device_profile_finalize (GObject *object)
{
    MateMixerDeviceProfile *profile;

    profile = MATE_MIXER_DEVICE_PROFILE (object);

    g_free (profile->priv->name);
    g_free (profile->priv->description);

    G_OBJECT_CLASS (mate_mixer_device_profile_parent_class)->finalize (object);
}

/**
 * mate_mixer_device_profile_get_name:
 * @profile: a #MateMixerDeviceProfile
 */
const gchar *
mate_mixer_device_profile_get_name (MateMixerDeviceProfile *profile)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), NULL);

    return profile->priv->name;
}

/**
 * mate_mixer_device_profile_get_description:
 * @profile: a #MateMixerDeviceProfile
 */
const gchar *
mate_mixer_device_profile_get_description (MateMixerDeviceProfile *profile)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), NULL);

    return profile->priv->description;
}

/**
 * mate_mixer_device_profile_get_priority:
 * @profile: a #MateMixerDeviceProfile
 */
guint
mate_mixer_device_profile_get_priority (MateMixerDeviceProfile *profile)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), 0);

    return profile->priv->priority;
}

/**
 * mate_mixer_device_profile_get_num_input_streams:
 * @profile: a #MateMixerDeviceProfile
 */
guint
mate_mixer_device_profile_get_num_input_streams (MateMixerDeviceProfile *profile)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), 0);

    return profile->priv->num_input_streams;
}

/**
 * mate_mixer_device_profile_get_num_output_streams:
 * @profile: a #MateMixerDeviceProfile
 */
guint
mate_mixer_device_profile_get_num_output_streams (MateMixerDeviceProfile *profile)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), 0);

    return profile->priv->num_output_streams;
}

MateMixerDeviceProfile *
_mate_mixer_device_profile_new (const gchar *name,
                                const gchar *description,
                                guint        priority,
                                guint        input_streams,
                                guint        output_streams)
{
    return g_object_new (MATE_MIXER_TYPE_DEVICE_PROFILE,
                         "name", name,
                         "description", description,
                         "priority", priority,
                         "num-input-streams", input_streams,
                         "num-output-streams", output_streams,
                         NULL);
}

gboolean
_mate_mixer_device_profile_update_description (MateMixerDeviceProfile *profile,
                                               const gchar            *description)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), FALSE);

    if (g_strcmp0 (profile->priv->description, description) != 0) {
        g_free (profile->priv->description);

        profile->priv->description = g_strdup (description);

        g_object_notify_by_pspec (G_OBJECT (profile), properties[PROP_DESCRIPTION]);
        return TRUE;
    }

    return FALSE;
}

gboolean
_mate_mixer_device_profile_update_priority (MateMixerDeviceProfile *profile,
                                            guint                   priority)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), FALSE);

    if (profile->priv->priority != priority) {
        profile->priv->priority = priority;

        g_object_notify_by_pspec (G_OBJECT (profile), properties[PROP_PRIORITY]);
        return TRUE;
    }

    return FALSE;
}

gboolean
_mate_mixer_device_profile_update_num_input_streams (MateMixerDeviceProfile *profile,
                                                     guint                   num)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), FALSE);

    if (profile->priv->num_input_streams != num) {
        profile->priv->num_input_streams = num;

        g_object_notify_by_pspec (G_OBJECT (profile), properties[PROP_NUM_INPUT_STREAMS]);
        return TRUE;
    }

    return FALSE;
}

gboolean
_mate_mixer_device_profile_update_num_output_streams (MateMixerDeviceProfile *profile,
                                                      guint                   num)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PROFILE (profile), FALSE);

    if (profile->priv->num_output_streams != num) {
        profile->priv->num_output_streams = num;

        g_object_notify_by_pspec (G_OBJECT (profile), properties[PROP_NUM_OUTPUT_STREAMS]);
        return TRUE;
    }

    return FALSE;
}
