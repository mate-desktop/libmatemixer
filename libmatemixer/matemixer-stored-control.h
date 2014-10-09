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

#ifndef MATEMIXER_STORED_CONTROL_H
#define MATEMIXER_STORED_CONTROL_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-stream-control.h>
#include <libmatemixer/matemixer-types.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_STORED_CONTROL          \
        (mate_mixer_stored_control_get_type ())
#define MATE_MIXER_STORED_CONTROL(o)            \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_STORED_CONTROL, MateMixerStoredControl))
#define MATE_MIXER_IS_STORED_CONTROL(o)         \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_STORED_CONTROL))
#define MATE_MIXER_STORED_CONTROL_CLASS(k)      \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_STORED_CONTROL, MateMixerStoredControlClass))
#define MATE_MIXER_IS_STORED_CONTROL_CLASS(k)   \
        (G_TYPE_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_STORED_CONTROL))
#define MATE_MIXER_STORED_CONTROL_GET_CLASS(o)  \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_STORED_CONTROL, MateMixerStoredControlClass))

typedef struct _MateMixerStoredControlClass    MateMixerStoredControlClass;
typedef struct _MateMixerStoredControlPrivate  MateMixerStoredControlPrivate;

/**
 * MateMixerStoredControl:
 *
 * The #MateMixerStoredControl structure contains only private data and should only
 * be accessed using the provided API.
 */
struct _MateMixerStoredControl
{
    MateMixerStreamControl object;

    /*< private >*/
    MateMixerStoredControlPrivate *priv;
};

/**
 * MateMixerStoredControlClass:
 * @parent_class: The parent class.
 *
 * The class structure for #MateMixerStoredControl.
 */
struct _MateMixerStoredControlClass
{
    MateMixerStreamControlClass parent_class;
};

GType              mate_mixer_stored_control_get_type      (void) G_GNUC_CONST;

MateMixerDirection mate_mixer_stored_control_get_direction (MateMixerStoredControl *control);

G_END_DECLS

#endif /* MATEMIXER_STORED_CONTROL_H */
