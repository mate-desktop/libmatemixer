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
#include <alsa/asoundlib.h>
#include <libmatemixer/matemixer.h>

#include "alsa-switch-option.h"

struct _AlsaSwitchOptionPrivate
{
    guint id;
};

static void alsa_switch_option_class_init (AlsaSwitchOptionClass *klass);
static void alsa_switch_option_init       (AlsaSwitchOption      *option);

G_DEFINE_TYPE (AlsaSwitchOption, alsa_switch_option, MATE_MIXER_TYPE_SWITCH_OPTION)

static void
alsa_switch_option_class_init (AlsaSwitchOptionClass *klass)
{
    g_type_class_add_private (G_OBJECT_CLASS (klass), sizeof (AlsaSwitchOptionPrivate));
}

static void
alsa_switch_option_init (AlsaSwitchOption *option)
{
    option->priv = G_TYPE_INSTANCE_GET_PRIVATE (option,
                                                ALSA_TYPE_SWITCH_OPTION,
                                                AlsaSwitchOptionPrivate);
}

AlsaSwitchOption *
alsa_switch_option_new (const gchar *name,
                        const gchar *label,
                        const gchar *icon,
                        guint        id)
{
    AlsaSwitchOption *option;

    g_return_val_if_fail (name  != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);

    option = g_object_new (ALSA_TYPE_SWITCH_OPTION,
                           "name", name,
                           "label", label,
                           "icon", icon,
                           NULL);

    option->priv->id = id;
    return option;
}

guint
alsa_switch_option_get_id (AlsaSwitchOption *option)
{
    g_return_val_if_fail (ALSA_IS_SWITCH_OPTION (option), 0);

    return option->priv->id;
}
