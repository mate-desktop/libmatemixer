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

#ifndef ALSA_TOGGLE_H
#define ALSA_TOGGLE_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "alsa-types.h"

G_BEGIN_DECLS

typedef enum {
    ALSA_TOGGLE_CAPTURE,
    ALSA_TOGGLE_PLAYBACK
} AlsaToggleType;

#define ALSA_TYPE_TOGGLE                        \
        (alsa_toggle_get_type ())
#define ALSA_TOGGLE(o)                          \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), ALSA_TYPE_TOGGLE, AlsaToggle))
#define ALSA_IS_TOGGLE(o)                       \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ALSA_TYPE_TOGGLE))
#define ALSA_TOGGLE_CLASS(k)                    \
        (G_TYPE_CHECK_CLASS_CAST ((k), ALSA_TYPE_TOGGLE, AlsaToggleClass))
#define ALSA_IS_TOGGLE_CLASS(k)                 \
        (G_TYPE_CHECK_CLASS_TYPE ((k), ALSA_TYPE_TOGGLE))
#define ALSA_TOGGLE_GET_CLASS(o)                \
        (G_TYPE_INSTANCE_GET_CLASS ((o), ALSA_TYPE_TOGGLE, AlsaToggleClass))

typedef struct _AlsaToggleClass    AlsaToggleClass;
typedef struct _AlsaTogglePrivate  AlsaTogglePrivate;

struct _AlsaToggle
{
    MateMixerStreamToggle parent;

    /*< private >*/
    AlsaTogglePrivate *priv;
};

struct _AlsaToggleClass
{
    MateMixerStreamToggleClass parent_class;
};

GType       alsa_toggle_get_type (void) G_GNUC_CONST;

AlsaToggle *alsa_toggle_new      (AlsaStream               *stream,
                                  const gchar              *name,
                                  const gchar              *label,
                                  MateMixerStreamSwitchRole role,
                                  AlsaToggleType            type,
                                  AlsaSwitchOption         *on,
                                  AlsaSwitchOption         *off);

G_END_DECLS

#endif /* ALSA_TOGGLE_H */
