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

#ifndef ALSA_SWITCH_OPTION_H
#define ALSA_SWITCH_OPTION_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "alsa-types.h"

G_BEGIN_DECLS

#define ALSA_TYPE_SWITCH_OPTION                 \
        (alsa_switch_option_get_type ())
#define ALSA_SWITCH_OPTION(o)                   \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), ALSA_TYPE_SWITCH_OPTION, AlsaSwitchOption))
#define ALSA_IS_SWITCH_OPTION(o)                \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ALSA_TYPE_SWITCH_OPTION))
#define ALSA_SWITCH_OPTION_CLASS(k)             \
        (G_TYPE_CHECK_CLASS_CAST ((k), ALSA_TYPE_SWITCH_OPTION, AlsaSwitchOptionClass))
#define ALSA_IS_SWITCH_OPTION_CLASS(k)          \
        (G_TYPE_CHECK_CLASS_TYPE ((k), ALSA_TYPE_SWITCH_OPTION))
#define ALSA_SWITCH_OPTION_GET_CLASS(o)         \
        (G_TYPE_INSTANCE_GET_CLASS ((o), ALSA_TYPE_SWITCH_OPTION, AlsaSwitchOptionClass))

typedef struct _AlsaSwitchOptionClass    AlsaSwitchOptionClass;
typedef struct _AlsaSwitchOptionPrivate  AlsaSwitchOptionPrivate;

struct _AlsaSwitchOption
{
    MateMixerSwitchOption parent;

    /*< private >*/
    AlsaSwitchOptionPrivate *priv;
};

struct _AlsaSwitchOptionClass
{
    MateMixerSwitchOptionClass parent_class;
};

GType             alsa_switch_option_get_type (void) G_GNUC_CONST;

AlsaSwitchOption *alsa_switch_option_new      (const gchar      *name,
                                               const gchar      *label,
                                               const gchar      *icon,
                                               guint             id);

guint             alsa_switch_option_get_id   (AlsaSwitchOption *option);

G_END_DECLS

#endif /* ALSA_SWITCH_OPTION_H */
