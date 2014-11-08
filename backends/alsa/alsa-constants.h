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

#ifndef ALSA_CONSTANTS_H
#define ALSA_CONSTANTS_H

#include <glib.h>
#include <alsa/asoundlib.h>
#include <libmatemixer/matemixer.h>

G_BEGIN_DECLS

typedef struct {
    gchar                     *name;
    gchar                     *label;
    MateMixerStreamControlRole role;
    gboolean                   use_default_input;
    gboolean                   use_default_output;
} AlsaControlInfo;

typedef struct {
    gchar                     *name;
    gchar                     *label;
    MateMixerStreamSwitchRole  role;
} AlsaSwitchInfo;

typedef struct {
    gchar *name;
    gchar *label;
    gchar *icon;
} AlsaSwitchOptionInfo;

extern const AlsaControlInfo                alsa_controls[];
extern const AlsaSwitchInfo                 alsa_switches[];
extern const AlsaSwitchOptionInfo           alsa_switch_options[];

extern const MateMixerChannelPosition       alsa_channel_map_from[];
extern const snd_mixer_selem_channel_id_t   alsa_channel_map_to[];

G_END_DECLS

#endif /* ALSA_CONSTANTS_H */
