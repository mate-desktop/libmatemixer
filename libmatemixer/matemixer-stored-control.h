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
#include <libmatemixer/matemixer-types.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_STORED_CONTROL              \
        (mate_mixer_stored_control_get_type ())
#define MATE_MIXER_STORED_CONTROL(o)                \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_STORED_CONTROL, MateMixerStoredControl))
#define MATE_MIXER_IS_STORED_CONTROL(o)             \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_STORED_CONTROL))
#define MATE_MIXER_STORED_CONTROL_GET_INTERFACE(o)  \
        (G_TYPE_INSTANCE_GET_INTERFACE ((o), MATE_MIXER_TYPE_STORED_CONTROL, MateMixerStoredControlInterface))

typedef struct _MateMixerStoredControlInterface  MateMixerStoredControlInterface;

/**
 * MateMixerStoredControlInterface:
 *
 * The interface structure of #MateMixerStoredControl.
 */
struct _MateMixerStoredControlInterface
{
    GTypeInterface parent_iface;

    /*< private >*/
    MateMixerDirection (*get_direction) (MateMixerStoredControl *control);
};

GType              mate_mixer_stored_control_get_type      (void) G_GNUC_CONST;

MateMixerDirection mate_mixer_stored_control_get_direction (MateMixerStoredControl *control);

G_END_DECLS

#endif /* MATEMIXER_STORED_CONTROL_H */
