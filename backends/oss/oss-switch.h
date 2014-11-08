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

#ifndef OSS_SWITCH_H
#define OSS_SWITCH_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "oss-types.h"

G_BEGIN_DECLS

#define OSS_TYPE_SWITCH                         \
        (oss_switch_get_type ())
#define OSS_SWITCH(o)                           \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), OSS_TYPE_SWITCH, OssSwitch))
#define OSS_IS_SWITCH(o)                        \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OSS_TYPE_SWITCH))
#define OSS_SWITCH_CLASS(k)                     \
        (G_TYPE_CHECK_CLASS_CAST ((k), OSS_TYPE_SWITCH, OssSwitchClass))
#define OSS_IS_SWITCH_CLASS(k)                  \
        (G_TYPE_CHECK_CLASS_TYPE ((k), OSS_TYPE_SWITCH))
#define OSS_SWITCH_GET_CLASS(o)                 \
        (G_TYPE_INSTANCE_GET_CLASS ((o), OSS_TYPE_SWITCH, OssSwitchClass))

typedef struct _OssSwitchClass    OssSwitchClass;
typedef struct _OssSwitchPrivate  OssSwitchPrivate;

struct _OssSwitch
{
    MateMixerStreamSwitch parent;

    /*< private >*/
    OssSwitchPrivate *priv;
};

struct _OssSwitchClass
{
    MateMixerStreamSwitchClass parent_class;
};

GType      oss_switch_get_type (void) G_GNUC_CONST;

OssSwitch *oss_switch_new      (OssStream   *stream,
                                const gchar *name,
                                const gchar *label,
                                gint         fd,
                                GList       *options);

void       oss_switch_load     (OssSwitch   *swtch);
void       oss_switch_close    (OssSwitch   *swtch);

G_END_DECLS

#endif /* OSS_SWITCH_H */
