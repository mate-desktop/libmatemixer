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

#ifndef MATEMIXER_TOGGLE_H
#define MATEMIXER_TOGGLE_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-types.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_TOGGLE                  \
        (mate_mixer_toggle_get_type ())
#define MATE_MIXER_TOGGLE(o)                    \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_TOGGLE, MateMixerToggle))
#define MATE_MIXER_IS_TOGGLE(o)                 \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_TOGGLE))
#define MATE_MIXER_TOGGLE_CLASS(k)              \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_TOGGLE, MateMixerToggleClass))
#define MATE_MIXER_IS_TOGGLE_CLASS(k)           \
        (G_TYPE_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_TOGGLE))
#define MATE_MIXER_TOGGLE_GET_CLASS(o)          \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_TOGGLE, MateMixerToggleClass))

typedef struct _MateMixerToggleClass    MateMixerToggleClass;
typedef struct _MateMixerTogglePrivate  MateMixerTogglePrivate;

/**
 * MateMixerToggle:
 *
 * The #MateMixerToggle structure contains only private data and should only
 * be accessed using the provided API.
 */
struct _MateMixerToggle
{
    MateMixerSwitch object;

    /*< private >*/
    MateMixerTogglePrivate *priv;
};

/**
 * MateMixerToggleClass:
 * @parent_class: The parent class.
 *
 * The class structure for #MateMixerToggle.
 */
struct _MateMixerToggleClass
{
    MateMixerSwitchClass parent_class;
};

GType                  mate_mixer_toggle_get_type         (void) G_GNUC_CONST;

gboolean               mate_mixer_toggle_get_state        (MateMixerToggle *toggle);
gboolean               mate_mixer_toggle_set_state        (MateMixerToggle *toggle,
                                                           gboolean         state);

MateMixerSwitchOption *mate_mixer_toggle_get_state_option (MateMixerToggle *toggle,
                                                           gboolean         state);

G_END_DECLS

#endif /* MATEMIXER_TOGGLE_H */
