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

#include "matemixer-enums.h"
#include "matemixer-enum-types.h"
#include "matemixer-switch.h"
#include "matemixer-switch-private.h"
#include "matemixer-switch-option.h"

/**
 * SECTION:matemixer-switch
 * @include: libmatemixer/matemixer.h
 */

struct _MateMixerSwitchPrivate
{
    gchar                 *name;
    gchar                 *label;
    GList                 *options;
    MateMixerSwitchOption *active;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_LABEL,
    PROP_ACTIVE_OPTION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void mate_mixer_switch_class_init   (MateMixerSwitchClass *klass);

static void mate_mixer_switch_get_property (GObject              *object,
                                            guint                 param_id,
                                            GValue               *value,
                                            GParamSpec           *pspec);
static void mate_mixer_switch_set_property (GObject              *object,
                                            guint                 param_id,
                                            const GValue         *value,
                                            GParamSpec           *pspec);

static void mate_mixer_switch_init         (MateMixerSwitch      *swtch);
static void mate_mixer_switch_finalize     (GObject              *object);

G_DEFINE_ABSTRACT_TYPE (MateMixerSwitch, mate_mixer_switch, G_TYPE_OBJECT)

static MateMixerSwitchOption *mate_mixer_switch_real_get_option (MateMixerSwitch *swtch,
                                                                 const gchar     *name);

static void
mate_mixer_switch_class_init (MateMixerSwitchClass *klass)
{
    GObjectClass *object_class;

    klass->get_option = mate_mixer_switch_real_get_option;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = mate_mixer_switch_finalize;
    object_class->get_property = mate_mixer_switch_get_property;
    object_class->set_property = mate_mixer_switch_set_property;

    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "Name of the switch",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_LABEL] =
        g_param_spec_string ("label",
                             "Label",
                             "Label of the switch",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_ACTIVE_OPTION] =
        g_param_spec_object ("active-option",
                             "Active option",
                             "Active option of the switch",
                             MATE_MIXER_TYPE_SWITCH_OPTION,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    g_type_class_add_private (object_class, sizeof (MateMixerSwitchPrivate));
}

static void
mate_mixer_switch_get_property (GObject    *object,
                                guint       param_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
    MateMixerSwitch *swtch;

    swtch = MATE_MIXER_SWITCH (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, swtch->priv->name);
        break;
    case PROP_LABEL:
        g_value_set_string (value, swtch->priv->label);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_switch_set_property (GObject      *object,
                                guint         param_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
    MateMixerSwitch *swtch;

    swtch = MATE_MIXER_SWITCH (object);

    switch (param_id) {
    case PROP_NAME:
        /* Construct-only string */
        swtch->priv->name = g_value_dup_string (value);
        break;
    case PROP_LABEL:
        /* Construct-only string */
        swtch->priv->label = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_switch_init (MateMixerSwitch *swtch)
{
    swtch->priv = G_TYPE_INSTANCE_GET_PRIVATE (swtch,
                                               MATE_MIXER_TYPE_SWITCH,
                                               MateMixerSwitchPrivate);
}

static void
mate_mixer_switch_finalize (GObject *object)
{
    MateMixerSwitch *swtch;

    swtch = MATE_MIXER_SWITCH (object);

    g_free (swtch->priv->name);
    g_free (swtch->priv->label);

    G_OBJECT_CLASS (mate_mixer_switch_parent_class)->finalize (object);
}

const gchar *
mate_mixer_switch_get_name (MateMixerSwitch *swtch)
{
    g_return_val_if_fail (MATE_MIXER_IS_SWITCH (swtch), NULL);

    return swtch->priv->name;
}

const gchar *
mate_mixer_switch_get_label (MateMixerSwitch *swtch)
{
    g_return_val_if_fail (MATE_MIXER_IS_SWITCH (swtch), NULL);

    return swtch->priv->label;
}

MateMixerSwitchOption *
mate_mixer_switch_get_option (MateMixerSwitch *swtch, const gchar *name)
{
    g_return_val_if_fail (MATE_MIXER_IS_SWITCH (swtch), NULL);

    return MATE_MIXER_SWITCH_GET_CLASS (swtch)->get_option (swtch, name);
}

MateMixerSwitchOption *
mate_mixer_switch_get_active_option (MateMixerSwitch *swtch)
{
    g_return_val_if_fail (MATE_MIXER_IS_SWITCH (swtch), NULL);

    return swtch->priv->active;
}

gboolean
mate_mixer_switch_set_active_option (MateMixerSwitch       *swtch,
                                     MateMixerSwitchOption *option)
{
    MateMixerSwitchClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_SWITCH (swtch), FALSE);

    klass = MATE_MIXER_SWITCH_GET_CLASS (swtch);
    if (klass->set_active_option != NULL) {
        if (klass->set_active_option (swtch, option) == FALSE)
            return FALSE;

        _mate_mixer_switch_set_active_option (swtch, option);
        return TRUE;
    }
    return FALSE;
}

const GList *
mate_mixer_switch_list_options (MateMixerSwitch *swtch)
{
    g_return_val_if_fail (MATE_MIXER_IS_SWITCH (swtch), NULL);

    if (swtch->priv->options == NULL) {
        MateMixerSwitchClass *klass = MATE_MIXER_SWITCH_GET_CLASS (swtch);

        if (klass->list_options != NULL)
            swtch->priv->options = klass->list_options (swtch);
    }
    return (const GList *) swtch->priv->options;
}

void
_mate_mixer_switch_set_active_option (MateMixerSwitch       *swtch,
                                      MateMixerSwitchOption *option)
{
    g_return_if_fail (MATE_MIXER_IS_SWITCH (swtch));
    g_return_if_fail (MATE_MIXER_IS_SWITCH_OPTION (option));

    if (swtch->priv->active == option)
        return;

    if (swtch->priv->active != NULL)
        g_object_unref (swtch->priv->active);

    swtch->priv->active = g_object_ref (option);

    g_object_notify_by_pspec (G_OBJECT (swtch), properties[PROP_ACTIVE_OPTION]);
}

static MateMixerSwitchOption *
mate_mixer_switch_real_get_option (MateMixerSwitch *swtch, const gchar *name)
{
    const GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_SWITCH (swtch), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = mate_mixer_switch_list_options (swtch);
    while (list != NULL) {
        MateMixerSwitchOption *option = MATE_MIXER_SWITCH_OPTION (list->data);

        if (strcmp (name, mate_mixer_switch_option_get_name (option)) == 0)
            return option;

        list = list->next;
    }
    return NULL;
}
