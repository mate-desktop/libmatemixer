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

#ifndef MATEMIXER_STREAM_TOGGLE_H
#define MATEMIXER_STREAM_TOGGLE_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-stream-switch.h>
#include <libmatemixer/matemixer-types.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_STREAM_TOGGLE           \
        (mate_mixer_stream_toggle_get_type ())
#define MATE_MIXER_STREAM_TOGGLE(o)             \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_STREAM_TOGGLE, MateMixerStreamToggle))
#define MATE_MIXER_IS_STREAM_TOGGLE(o)          \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_STREAM_TOGGLE))
#define MATE_MIXER_STREAM_TOGGLE_CLASS(k)       \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_STREAM_TOGGLE, MateMixerStreamToggleClass))
#define MATE_MIXER_IS_STREAM_TOGGLE_CLASS(k)    \
        (G_TYPE_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_STREAM_TOGGLE))
#define MATE_MIXER_STREAM_TOGGLE_GET_CLASS(o)   \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_STREAM_TOGGLE, MateMixerStreamToggleClass))

typedef struct _MateMixerStreamToggleClass    MateMixerStreamToggleClass;
typedef struct _MateMixerStreamTogglePrivate  MateMixerStreamTogglePrivate;

/**
 * MateMixerStreamToggle:
 *
 * The #MateMixerStreamToggle structure contains only private data and should only
 * be accessed using the provided API.
 */
struct _MateMixerStreamToggle
{
    MateMixerStreamSwitch object;

    /*< private >*/
    MateMixerStreamTogglePrivate *priv;
};

/**
 * MateMixerStreamToggleClass:
 * @parent_class: The parent class.
 *
 * The class structure for #MateMixerStreamToggle.
 */
struct _MateMixerStreamToggleClass
{
    MateMixerStreamSwitchClass parent_class;
};

GType                  mate_mixer_stream_toggle_get_type         (void) G_GNUC_CONST;

gboolean               mate_mixer_stream_toggle_get_state        (MateMixerStreamToggle *toggle);
gboolean               mate_mixer_stream_toggle_set_state        (MateMixerStreamToggle *toggle,
                                                                  gboolean               state);

MateMixerSwitchOption *mate_mixer_stream_toggle_get_state_option (MateMixerStreamToggle *toggle,
                                                                  gboolean               state);

G_END_DECLS

#endif /* MATEMIXER_STREAM_TOGGLE_H */
