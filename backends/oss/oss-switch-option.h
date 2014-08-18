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

#ifndef OSS_SWITCH_OPTION_H
#define OSS_SWITCH_OPTION_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "oss-types.h"

G_BEGIN_DECLS

#define OSS_TYPE_SWITCH_OPTION                        \
        (oss_switch_option_get_type ())
#define OSS_SWITCH_OPTION(o)                          \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), OSS_TYPE_SWITCH_OPTION, OssSwitchOption))
#define OSS_IS_SWITCH_OPTION(o)                       \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OSS_TYPE_SWITCH_OPTION))
#define OSS_SWITCH_OPTION_CLASS(k)                    \
        (G_TYPE_CHECK_CLASS_CAST ((k), OSS_TYPE_SWITCH_OPTION, OssSwitchOptionClass))
#define OSS_IS_SWITCH_OPTION_CLASS(k)                 \
        (G_TYPE_CHECK_CLASS_TYPE ((k), OSS_TYPE_SWITCH_OPTION))
#define OSS_SWITCH_OPTION_GET_CLASS(o)                \
        (G_TYPE_INSTANCE_GET_CLASS ((o), OSS_TYPE_SWITCH_OPTION, OssSwitchOptionClass))

typedef struct _OssSwitchOptionClass    OssSwitchOptionClass;
typedef struct _OssSwitchOptionPrivate  OssSwitchOptionPrivate;

struct _OssSwitchOption
{
    MateMixerSwitchOption parent;

    /*< private >*/
    OssSwitchOptionPrivate *priv;
};

struct _OssSwitchOptionClass
{
    MateMixerSwitchOptionClass parent_class;
};

GType            oss_switch_option_get_type   (void) G_GNUC_CONST;

OssSwitchOption *oss_switch_option_new        (const gchar     *name,
                                               const gchar     *label,
                                               const gchar     *icon,
                                               guint            devnum);

guint            oss_switch_option_get_devnum (OssSwitchOption *option);

G_END_DECLS

#endif /* OSS_SWITCH_OPTION_H */
