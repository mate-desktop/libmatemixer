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

#include <libmatemixer/matemixer-enums.h>
#include <pulse/pulseaudio.h>

#include "pulse-helpers.h"

typedef struct {
    MateMixerChannelPosition mm_position;
    pa_channel_position_t    pa_position;
} PositionMap;

static PositionMap const position_map[] = {
    { MATE_MIXER_CHANNEL_UNKNOWN_POSITION,      PA_CHANNEL_POSITION_INVALID },
    { MATE_MIXER_CHANNEL_MONO,                  PA_CHANNEL_POSITION_MONO },
    { MATE_MIXER_CHANNEL_FRONT_LEFT,            PA_CHANNEL_POSITION_FRONT_LEFT },
    { MATE_MIXER_CHANNEL_FRONT_RIGHT,           PA_CHANNEL_POSITION_FRONT_RIGHT },
    { MATE_MIXER_CHANNEL_FRONT_CENTER,          PA_CHANNEL_POSITION_FRONT_CENTER },
    { MATE_MIXER_CHANNEL_LFE,                   PA_CHANNEL_POSITION_LFE },
    { MATE_MIXER_CHANNEL_BACK_LEFT,             PA_CHANNEL_POSITION_REAR_LEFT },
    { MATE_MIXER_CHANNEL_BACK_RIGHT,            PA_CHANNEL_POSITION_REAR_RIGHT },
    { MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER,     PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER },
    { MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER,    PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER },
    { MATE_MIXER_CHANNEL_BACK_CENTER,           PA_CHANNEL_POSITION_REAR_CENTER },
    { MATE_MIXER_CHANNEL_SIDE_LEFT,             PA_CHANNEL_POSITION_SIDE_LEFT },
    { MATE_MIXER_CHANNEL_SIDE_RIGHT,            PA_CHANNEL_POSITION_SIDE_RIGHT },
    { MATE_MIXER_CHANNEL_TOP_FRONT_LEFT,        PA_CHANNEL_POSITION_TOP_FRONT_LEFT },
    { MATE_MIXER_CHANNEL_TOP_FRONT_RIGHT,       PA_CHANNEL_POSITION_TOP_FRONT_RIGHT },
    { MATE_MIXER_CHANNEL_TOP_FRONT_CENTER,      PA_CHANNEL_POSITION_TOP_FRONT_CENTER },
    { MATE_MIXER_CHANNEL_TOP_CENTER,            PA_CHANNEL_POSITION_TOP_CENTER },
    { MATE_MIXER_CHANNEL_TOP_BACK_LEFT,         PA_CHANNEL_POSITION_TOP_REAR_LEFT },
    { MATE_MIXER_CHANNEL_TOP_BACK_RIGHT,        PA_CHANNEL_POSITION_TOP_REAR_RIGHT },
    { MATE_MIXER_CHANNEL_TOP_BACK_CENTER,       PA_CHANNEL_POSITION_TOP_REAR_CENTER },
};

MateMixerChannelPosition
pulse_convert_position_from_pulse (pa_channel_position_t position)
{
    int i;

    for (i = 0; i < G_N_ELEMENTS (position_map); i++) {
        if (position == position_map[i].pa_position)
            return position_map[i].mm_position;
    }
    return MATE_MIXER_CHANNEL_UNKNOWN_POSITION;
}

pa_channel_position_t
pulse_convert_position_to_pulse (MateMixerChannelPosition position)
{
    int i;

    for (i = 0; i < G_N_ELEMENTS (position_map); i++) {
        if (position == position_map[i].mm_position)
            return position_map[i].pa_position;
    }
    return PA_CHANNEL_POSITION_INVALID;
}
