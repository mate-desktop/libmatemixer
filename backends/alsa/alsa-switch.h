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

#ifndef ALSA_SWITCH_H
#define ALSA_SWITCH_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "alsa-types.h"

G_BEGIN_DECLS

#define ALSA_TYPE_SWITCH                        \
        (alsa_switch_get_type ())
#define ALSA_SWITCH(o)                          \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), ALSA_TYPE_SWITCH, AlsaSwitch))
#define ALSA_IS_SWITCH(o)                       \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ALSA_TYPE_SWITCH))
#define ALSA_SWITCH_CLASS(k)                    \
        (G_TYPE_CHECK_CLASS_CAST ((k), ALSA_TYPE_SWITCH, AlsaSwitchClass))
#define ALSA_IS_SWITCH_CLASS(k)                 \
        (G_TYPE_CHECK_CLASS_TYPE ((k), ALSA_TYPE_SWITCH))
#define ALSA_SWITCH_GET_CLASS(o)                \
        (G_TYPE_INSTANCE_GET_CLASS ((o), ALSA_TYPE_SWITCH, AlsaSwitchClass))

typedef struct _AlsaSwitchClass    AlsaSwitchClass;
typedef struct _AlsaSwitchPrivate  AlsaSwitchPrivate;

struct _AlsaSwitch
{
    MateMixerStreamSwitch parent;

    /*< private >*/
    AlsaSwitchPrivate *priv;
};

struct _AlsaSwitchClass
{
    MateMixerStreamSwitchClass parent_class;
};

GType       alsa_switch_get_type (void) G_GNUC_CONST;

AlsaSwitch *alsa_switch_new      (AlsaStream               *stream,
                                  const gchar              *name,
                                  const gchar              *label,
                                  MateMixerStreamSwitchRole role,
                                  GList                    *options);

G_END_DECLS

#endif /* ALSA_SWITCH_H */
