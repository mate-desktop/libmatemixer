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

#ifndef MATEMIXER_STREAM_SWITCH_H
#define MATEMIXER_STREAM_SWITCH_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-switch.h>
#include <libmatemixer/matemixer-types.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_STREAM_SWITCH           \
        (mate_mixer_stream_switch_get_type ())
#define MATE_MIXER_STREAM_SWITCH(o)             \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_STREAM_SWITCH, MateMixerStreamSwitch))
#define MATE_MIXER_IS_STREAM_SWITCH(o)          \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_STREAM_SWITCH))
#define MATE_MIXER_STREAM_SWITCH_CLASS(k)      \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_STREAM_SWITCH, MateMixerStreamSwitchClass))
#define MATE_MIXER_IS_STREAM_SWITCH_CLASS(k)   \
        (G_TYPE_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_STREAM_SWITCH))
#define MATE_MIXER_STREAM_SWITCH_GET_CLASS(o)  \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_STREAM_SWITCH, MateMixerStreamSwitchClass))

typedef struct _MateMixerStreamSwitchClass    MateMixerStreamSwitchClass;
typedef struct _MateMixerStreamSwitchPrivate  MateMixerStreamSwitchPrivate;

/**
 * MateMixerStreamSwitch:
 *
 * The #MateMixerStreamSwitch structure contains only private data and should only
 * be accessed using the provided API.
 */
struct _MateMixerStreamSwitch
{
    MateMixerSwitch object;

    /*< private >*/
    MateMixerStreamSwitchPrivate *priv;
};

/**
 * MateMixerStreamSwitchClass:
 * @parent_class: The parent class.
 *
 * The class structure for #MateMixerStreamSwitch.
 */
struct _MateMixerStreamSwitchClass
{
    MateMixerSwitchClass parent_class;
};

GType                      mate_mixer_stream_switch_get_type   (void) G_GNUC_CONST;

MateMixerStreamSwitchFlags mate_mixer_stream_switch_get_flags  (MateMixerStreamSwitch *swtch);
MateMixerStreamSwitchRole  mate_mixer_stream_switch_get_role   (MateMixerStreamSwitch *swtch);
MateMixerStream *          mate_mixer_stream_switch_get_stream (MateMixerStreamSwitch *swtch);

G_END_DECLS

#endif /* MATEMIXER_STREAM_SWITCH_H */
