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

#ifndef PULSE_HELPERS_H
#define PULSE_HELPERS_H

#include <glib.h>
#include <libmatemixer/matemixer.h>

#include <pulse/pulseaudio.h>

G_BEGIN_DECLS

extern const MateMixerChannelPosition   pulse_channel_map_from[PA_CHANNEL_POSITION_MAX];
extern const pa_channel_position_t      pulse_channel_map_to[MATE_MIXER_CHANNEL_MAX];

MateMixerStreamControlMediaRole pulse_convert_media_role_name (const gchar *name);

G_END_DECLS

#endif /* PULSE_HELPERS_H */
