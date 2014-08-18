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

#include <glib.h>
#include <glib-object.h>

#include "matemixer-enums.h"
#include "matemixer-enum-types.h"
#include "matemixer-stream-control.h"
#include "matemixer-stored-control.h"

G_DEFINE_INTERFACE (MateMixerStoredControl, mate_mixer_stored_control, MATE_MIXER_TYPE_STREAM_CONTROL)

static void
mate_mixer_stored_control_default_init (MateMixerStoredControlInterface *iface)
{
    g_object_interface_install_property (iface,
                                         g_param_spec_enum ("direction",
                                                            "Direction",
                                                            "Direction of the stored control",
                                                            MATE_MIXER_TYPE_DIRECTION,
                                                            MATE_MIXER_DIRECTION_UNKNOWN,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT_ONLY |
                                                            G_PARAM_STATIC_STRINGS));
}

/**
 * mate_mixer_stored_control_get_direction:
 * @control: a #MateMixerStoredControl
 */
MateMixerDirection
mate_mixer_stored_control_get_direction (MateMixerStoredControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_STORED_CONTROL (control), MATE_MIXER_DIRECTION_UNKNOWN);

    return MATE_MIXER_STORED_CONTROL_GET_INTERFACE (control)->get_direction (control);
}
