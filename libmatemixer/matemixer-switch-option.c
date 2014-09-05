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

#include "matemixer-switch-option.h"
#include "matemixer-switch-option-private.h"

/**
 * SECTION:matemixer-switch-option
 * @include: libmatemixer/matemixer.h
 */

struct _MateMixerSwitchOptionPrivate
{
    gchar *name;
    gchar *label;
    gchar *icon;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_LABEL,
    PROP_ICON,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void mate_mixer_switch_option_class_init   (MateMixerSwitchOptionClass *klass);

static void mate_mixer_switch_option_get_property (GObject                    *object,
                                                   guint                       param_id,
                                                   GValue                     *value,
                                                   GParamSpec                 *pspec);
static void mate_mixer_switch_option_set_property (GObject                    *object,
                                                   guint                       param_id,
                                                   const GValue               *value,
                                                   GParamSpec                 *pspec);

static void mate_mixer_switch_option_init         (MateMixerSwitchOption      *option);
static void mate_mixer_switch_option_finalize     (GObject                    *object);

G_DEFINE_TYPE (MateMixerSwitchOption, mate_mixer_switch_option, G_TYPE_OBJECT)

static void
mate_mixer_switch_option_class_init (MateMixerSwitchOptionClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = mate_mixer_switch_option_finalize;
    object_class->get_property = mate_mixer_switch_option_get_property;
    object_class->set_property = mate_mixer_switch_option_set_property;

    /**
     * MateMixerSwitchOption:name:
     *
     * The name of the switch option. The name serves as a unique identifier
     * and in most cases it is not in a user-readable form.
     */
    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "Name of the switch option",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    /**
     * MateMixerSwitchOption:label:
     *
     * The label of the switch option. This is a potentially translated string
     * that should be presented to users in the user interface.
     */
    properties[PROP_LABEL] =
        g_param_spec_string ("label",
                             "Label",
                             "Label of the switch option",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    /**
     * MateMixerSwitchOption:icon:
     *
     * The XDG icon name of the switch option.
     */
    properties[PROP_ICON] =
        g_param_spec_string ("icon",
                             "Icon",
                             "Icon of the switch option",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    g_type_class_add_private (object_class, sizeof (MateMixerSwitchOptionPrivate));
}

static void
mate_mixer_switch_option_get_property (GObject    *object,
                                       guint       param_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    MateMixerSwitchOption *option;

    option = MATE_MIXER_SWITCH_OPTION (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, option->priv->name);
        break;
    case PROP_LABEL:
        g_value_set_string (value, option->priv->label);
        break;
    case PROP_ICON:
        g_value_set_string (value, option->priv->icon);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_switch_option_set_property (GObject      *object,
                                       guint         param_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    MateMixerSwitchOption *option;

    option = MATE_MIXER_SWITCH_OPTION (object);

    switch (param_id) {
    case PROP_NAME:
        /* Construct-only string */
        option->priv->name = g_value_dup_string (value);
        break;
    case PROP_LABEL:
        /* Construct-only string */
        option->priv->label = g_value_dup_string (value);
        break;
    case PROP_ICON:
        /* Construct-only string */
        option->priv->icon = g_value_dup_string (value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_switch_option_init (MateMixerSwitchOption *option)
{
    option->priv = G_TYPE_INSTANCE_GET_PRIVATE (option,
                                                MATE_MIXER_TYPE_SWITCH_OPTION,
                                                MateMixerSwitchOptionPrivate);
}

static void
mate_mixer_switch_option_finalize (GObject *object)
{
    MateMixerSwitchOption *option;

    option = MATE_MIXER_SWITCH_OPTION (object);

    g_free (option->priv->name);
    g_free (option->priv->label);
    g_free (option->priv->icon);

    G_OBJECT_CLASS (mate_mixer_switch_option_parent_class)->finalize (object);
}

/**
 * mate_mixer_switch_option_get_name:
 * @option: a #MateMixerSwitchOption
 *
 * Gets the name of the switch option. The name serves as a unique identifier
 * and in most cases it is not in a user-readable form.
 *
 * The returned name is guaranteed to be unique across all the switch options
 * of a particular #MateMixerSwitch and may be used to get the #MateMixerSwitchOption
 * using mate_mixer_switch_get_option().
 *
 * Returns: the name of the switch option.
 */
const gchar *
mate_mixer_switch_option_get_name (MateMixerSwitchOption *option)
{
    g_return_val_if_fail (MATE_MIXER_IS_SWITCH_OPTION (option), NULL);

    return option->priv->name;
}

/**
 * mate_mixer_switch_option_get_label:
 * @option: a #MateMixerSwitchOption
 *
 * Gets the label of the switch option. This is a potentially translated string
 * that should be presented to users in the user interface.
 *
 * Returns: the label of the switch option.
 */
const gchar *
mate_mixer_switch_option_get_label (MateMixerSwitchOption *option)
{
    g_return_val_if_fail (MATE_MIXER_IS_SWITCH_OPTION (option), NULL);

    return option->priv->label;
}

/**
 * mate_mixer_switch_option_get_icon:
 * @option: a #MateMixerSwitchOption
 *
 * Gets the XDG icon name of the switch option.
 *
 * Returns: the icon name or %NULL.
 */
const gchar *
mate_mixer_switch_option_get_icon (MateMixerSwitchOption *option)
{
    g_return_val_if_fail (MATE_MIXER_IS_SWITCH_OPTION (option), NULL);

    return option->priv->icon;
}

MateMixerSwitchOption *
_mate_mixer_switch_option_new (const gchar *name,
                               const gchar *label,
                               const gchar *icon)
{
    return g_object_new (MATE_MIXER_TYPE_SWITCH_OPTION,
                         "name", name,
                         "label", label,
                         "icon", icon,
                         NULL);
}
