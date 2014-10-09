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

#ifndef MATEMIXER_SWITCH_OPTION_H
#define MATEMIXER_SWITCH_OPTION_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-types.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_SWITCH_OPTION           \
        (mate_mixer_switch_option_get_type ())
#define MATE_MIXER_SWITCH_OPTION(o)             \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_SWITCH_OPTION, MateMixerSwitchOption))
#define MATE_MIXER_IS_SWITCH_OPTION(o)          \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_SWITCH_OPTION))
#define MATE_MIXER_SWITCH_OPTION_CLASS(k)       \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_SWITCH_OPTION, MateMixerSwitchOptionClass))
#define MATE_MIXER_IS_SWITCH_OPTION_CLASS(k)    \
        (G_TYPE_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_SWITCH_OPTION))
#define MATE_MIXER_SWITCH_OPTION_GET_CLASS(o)   \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_SWITCH_OPTION, MateMixerSwitchOptionClass))

typedef struct _MateMixerSwitchOptionClass    MateMixerSwitchOptionClass;
typedef struct _MateMixerSwitchOptionPrivate  MateMixerSwitchOptionPrivate;

/**
 * MateMixerSwitchOption:
 *
 * The #MateMixerSwitchOption structure contains only private data and should only
 * be accessed using the provided API.
 */
struct _MateMixerSwitchOption
{
    GObject parent;

    /*< private >*/
    MateMixerSwitchOptionPrivate *priv;
};

/**
 * MateMixerSwitchOptionClass:
 * @parent_class: The parent class.
 *
 * The class structure for #MateMixerSwitchOption.
 */
struct _MateMixerSwitchOptionClass
{
    GObjectClass parent_class;
};

GType        mate_mixer_switch_option_get_type  (void) G_GNUC_CONST;

const gchar *mate_mixer_switch_option_get_name  (MateMixerSwitchOption *option);
const gchar *mate_mixer_switch_option_get_label (MateMixerSwitchOption *option);
const gchar *mate_mixer_switch_option_get_icon  (MateMixerSwitchOption *option);

G_END_DECLS

#endif /* MATEMIXER_SWITCH_OPTION_H */
