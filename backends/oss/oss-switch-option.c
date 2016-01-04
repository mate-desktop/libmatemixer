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
#include <libmatemixer/matemixer.h>

#include "oss-switch-option.h"

struct _OssSwitchOptionPrivate
{
    guint devnum;
};

static void oss_switch_option_class_init (OssSwitchOptionClass *klass);
static void oss_switch_option_init       (OssSwitchOption      *option);

G_DEFINE_TYPE (OssSwitchOption, oss_switch_option, MATE_MIXER_TYPE_SWITCH_OPTION)

static void
oss_switch_option_class_init (OssSwitchOptionClass *klass)
{
    g_type_class_add_private (G_OBJECT_CLASS (klass), sizeof (OssSwitchOptionPrivate));
}

static void
oss_switch_option_init (OssSwitchOption *option)
{
    option->priv = G_TYPE_INSTANCE_GET_PRIVATE (option,
                                                OSS_TYPE_SWITCH_OPTION,
                                                OssSwitchOptionPrivate);
}

OssSwitchOption *
oss_switch_option_new (const gchar *name,
                       const gchar *label,
                       const gchar *icon,
                       guint        devnum)
{
    OssSwitchOption *option;

    g_return_val_if_fail (name  != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);

    option = g_object_new (OSS_TYPE_SWITCH_OPTION,
                           "name", name,
                           "label", label,
                           "icon", icon,
                           NULL);

    option->priv->devnum = devnum;
    return option;
}

guint
oss_switch_option_get_devnum (OssSwitchOption *option)
{
    g_return_val_if_fail (OSS_IS_SWITCH_OPTION (option), 0);

    return option->priv->devnum;
}
