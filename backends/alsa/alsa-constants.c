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
#include <glib/gi18n.h>
#include <alsa/asoundlib.h>
#include <libmatemixer/matemixer.h>

#include "alsa-constants.h"

// XXX add more and probably move them somewhere else
const AlsaControlInfo alsa_controls[] =
{
    { "Master",   N_("Master"),  MATE_MIXER_STREAM_CONTROL_ROLE_MASTER },
    { "Speaker",  N_("Speaker"), MATE_MIXER_STREAM_CONTROL_ROLE_MASTER },
    { "Capture",  N_("Capture"), MATE_MIXER_STREAM_CONTROL_ROLE_MASTER },
    { "PCM",      N_("PCM"),     MATE_MIXER_STREAM_CONTROL_ROLE_PCM },
    { "Line",     N_("Line"),    MATE_MIXER_STREAM_CONTROL_ROLE_PORT },
    { "Mic",      N_("Mic"),     MATE_MIXER_STREAM_CONTROL_ROLE_PORT },
    { NULL }
};

const MateMixerChannelPosition alsa_channel_map_from[SND_MIXER_SCHN_LAST] =
{
    [SND_MIXER_SCHN_FRONT_LEFT]             = MATE_MIXER_CHANNEL_FRONT_LEFT,
    [SND_MIXER_SCHN_FRONT_RIGHT]            = MATE_MIXER_CHANNEL_FRONT_RIGHT,
    [SND_MIXER_SCHN_REAR_LEFT]              = MATE_MIXER_CHANNEL_BACK_LEFT,
    [SND_MIXER_SCHN_REAR_RIGHT]             = MATE_MIXER_CHANNEL_BACK_RIGHT,
    [SND_MIXER_SCHN_FRONT_CENTER]           = MATE_MIXER_CHANNEL_FRONT_CENTER,
    [SND_MIXER_SCHN_WOOFER]                 = MATE_MIXER_CHANNEL_LFE,
    [SND_MIXER_SCHN_SIDE_LEFT]              = MATE_MIXER_CHANNEL_SIDE_LEFT,
    [SND_MIXER_SCHN_SIDE_RIGHT]             = MATE_MIXER_CHANNEL_SIDE_RIGHT,
    [SND_MIXER_SCHN_REAR_CENTER]            = MATE_MIXER_CHANNEL_BACK_CENTER
};

const snd_mixer_selem_channel_id_t alsa_channel_map_to[MATE_MIXER_CHANNEL_MAX] =
{
    [MATE_MIXER_CHANNEL_UNKNOWN]            = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_MONO]               = SND_MIXER_SCHN_MONO,
    [MATE_MIXER_CHANNEL_FRONT_LEFT]         = SND_MIXER_SCHN_FRONT_LEFT,
    [MATE_MIXER_CHANNEL_FRONT_RIGHT]        = SND_MIXER_SCHN_FRONT_RIGHT,
    [MATE_MIXER_CHANNEL_FRONT_CENTER]       = SND_MIXER_SCHN_FRONT_CENTER,
    [MATE_MIXER_CHANNEL_LFE]                = SND_MIXER_SCHN_WOOFER,
    [MATE_MIXER_CHANNEL_BACK_LEFT]          = SND_MIXER_SCHN_REAR_LEFT,
    [MATE_MIXER_CHANNEL_BACK_RIGHT]         = SND_MIXER_SCHN_REAR_RIGHT,
    [MATE_MIXER_CHANNEL_BACK_CENTER]        = SND_MIXER_SCHN_REAR_CENTER,
    [MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER]  = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER] = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_SIDE_LEFT]          = SND_MIXER_SCHN_SIDE_LEFT,
    [MATE_MIXER_CHANNEL_SIDE_RIGHT]         = SND_MIXER_SCHN_SIDE_RIGHT,
    [MATE_MIXER_CHANNEL_TOP_FRONT_LEFT]     = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_FRONT_RIGHT]    = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_FRONT_CENTER]   = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_CENTER]         = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_BACK_LEFT]      = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_BACK_RIGHT]     = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_BACK_CENTER]    = SND_MIXER_SCHN_UNKNOWN
};
