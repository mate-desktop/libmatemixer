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

#include "alsa-compat.h"
#include "alsa-constants.h"

/*
 * These lists of ALSA mixer elements are based on PulseAudio's mixer paths and own
 * observations. The intention is to provide translatable and in some cases better
 * readable labels and role assignments. The controls list is also used for selecting
 * the default controls and the selection machanism relies on the order of elements,
 * so more useful elements should be placed on the top. The last two boolean values
 * indicate whether we prefer the element to be used as a default input or output
 * control.
 *
 * Of course the lists are very incomplete and it would be great if users validated
 * and refreshed them from time to time.
 */
const AlsaControlInfo alsa_controls[] =
{
    /* Output controls */
    { "Master",                 N_("Master"),               MATE_MIXER_STREAM_CONTROL_ROLE_MASTER,  FALSE, TRUE },
    { "Hardware Master",        N_("Hardware Master"),      MATE_MIXER_STREAM_CONTROL_ROLE_MASTER,  FALSE, TRUE },
    { "PCM",                    N_("PCM"),                  MATE_MIXER_STREAM_CONTROL_ROLE_PCM,     FALSE, TRUE },
    { "Speaker",                N_("Speaker"),              MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Desktop Speaker",        N_("Desktop Speaker"),      MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Front",                  N_("Front Speaker"),        MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Front Speaker",          N_("Front Speaker"),        MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Speaker Front",          N_("Front Speaker"),        MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Headphone",              N_("Headphone"),            MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Headphone2",             N_("Headphone 2"),          MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Headset",                N_("Headset"),              MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Master Surround",        N_("Surround Speaker"),     MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Surround",               N_("Surround Speaker"),     MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Surround Speaker",       N_("Surround Speaker"),     MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Speaker Surround",       N_("Surround Speaker"),     MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Center",                 N_("Center Speaker"),       MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Center Speaker",         N_("Center Speaker"),       MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "CLFE",                   N_("CLFE Speaker"),         MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Speaker CLFE",           N_("CLFE Speaker"),         MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Digital/SPDIF",          N_("Digital"),              MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    FALSE, TRUE },
    { "Master Mono",            N_("Master Mono"),          MATE_MIXER_STREAM_CONTROL_ROLE_MASTER,  FALSE, TRUE },
    { "Master Digital",         N_("Master Digital"),       MATE_MIXER_STREAM_CONTROL_ROLE_MASTER,  FALSE, TRUE },
    { "Speaker Side",           N_("Side Speaker"),         MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Side",                   N_("Side Speaker"),         MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Rear",                   N_("Rear Speaker"),         MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, TRUE },
    { "Wave",                   N_("Wave"),                 MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, FALSE, TRUE },
    { "Phone",                  N_("Phone"),                MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, FALSE, TRUE },
    { "CD",                     N_("CD"),                   MATE_MIXER_STREAM_CONTROL_ROLE_CD,      FALSE, TRUE },
    { "Music",                  N_("Music"),                MATE_MIXER_STREAM_CONTROL_ROLE_MUSIC,   FALSE, TRUE },
    { "AC97",                   N_("AC97"),                 MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, FALSE, TRUE },
    { "LFE",                    N_("LFE Speaker"),          MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, FALSE },
    { "LFE Speaker",            N_("LFE Speaker"),          MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, FALSE },
    { "Bass Speaker",           N_("Bass Speaker"),         MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, FALSE },
    { "PC Speaker",             N_("PC Speaker"),           MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, FALSE, FALSE },
    { "Synth",                  N_("Synth"),                MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, FALSE, FALSE },
    { "MIDI",                   N_("MIDI"),                 MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, FALSE, FALSE },
    { "Synth/MIDI",             N_("Synth/MIDI"),           MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, FALSE, FALSE },
    { "Bass",                   N_("Bass"),                 MATE_MIXER_STREAM_CONTROL_ROLE_BASS,    FALSE, FALSE },
    { "Treble",                 N_("Treble"),               MATE_MIXER_STREAM_CONTROL_ROLE_TREBLE,  FALSE, FALSE },

    /* Input controls */
    { "Capture",                N_("Capture"),                      MATE_MIXER_STREAM_CONTROL_ROLE_MASTER,      TRUE, FALSE },
    { "Mic",                    N_("Microphone"),                   MATE_MIXER_STREAM_CONTROL_ROLE_MICROPHONE,  TRUE, FALSE },
    { "Mic/Line",               N_("Microphone/Line In"),           MATE_MIXER_STREAM_CONTROL_ROLE_PORT,        TRUE, FALSE },
    { "Internal Mic",           N_("Internal Microphone"),          MATE_MIXER_STREAM_CONTROL_ROLE_MICROPHONE,  TRUE, FALSE },
    { "Int Mic",                N_("Internal Microphone"),          MATE_MIXER_STREAM_CONTROL_ROLE_MICROPHONE,  TRUE, FALSE },
    { "Front Mic",              N_("Front Microphone"),             MATE_MIXER_STREAM_CONTROL_ROLE_MICROPHONE,  TRUE, FALSE },
    { "Rear Mic",               N_("Rear Microphone"),              MATE_MIXER_STREAM_CONTROL_ROLE_MICROPHONE,  TRUE, FALSE },
    { "Dock Mic",               N_("Dock Microphone"),              MATE_MIXER_STREAM_CONTROL_ROLE_MICROPHONE,  TRUE, FALSE },
    { "Headphone Mic",          N_("Headphone Microphone"),         MATE_MIXER_STREAM_CONTROL_ROLE_MICROPHONE,  TRUE, FALSE },
    { "Headset Mic",            N_("Headset Microphone"),           MATE_MIXER_STREAM_CONTROL_ROLE_MICROPHONE,  TRUE, FALSE },
    { "Inverted Internal Mic",  N_("Inverted Internal Microphone"), MATE_MIXER_STREAM_CONTROL_ROLE_MICROPHONE,  TRUE, FALSE },
    { "Line",                   N_("Line In"),                      MATE_MIXER_STREAM_CONTROL_ROLE_PORT,        TRUE, FALSE },
    { "Line in",                N_("Line In"),                      MATE_MIXER_STREAM_CONTROL_ROLE_PORT,        TRUE, FALSE },
    { "Line In",                N_("Line In"),                      MATE_MIXER_STREAM_CONTROL_ROLE_PORT,        TRUE, FALSE },
    { "Aux",                    N_("Auxiliary"),                    MATE_MIXER_STREAM_CONTROL_ROLE_PORT,        TRUE, FALSE },
    { "Video",                  N_("Video"),                        MATE_MIXER_STREAM_CONTROL_ROLE_VIDEO,       TRUE, FALSE },
    { "TV Tuner",               N_("TV Tuner"),                     MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN,     TRUE, FALSE },
    { "FM",                     N_("FM"),                           MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN,     TRUE, FALSE },
    { "Phone",                  N_("Phone"),                        MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN,     TRUE, FALSE },
    { "Mic Boost",              N_("Microphone Boost"),             MATE_MIXER_STREAM_CONTROL_ROLE_BOOST,       FALSE, FALSE },
    { "Mic Boost (+20dB)",      N_("Microphone Boost (+20dB)"),     MATE_MIXER_STREAM_CONTROL_ROLE_BOOST,       FALSE, FALSE },
    { "Int Mic Boost",          N_("Internal Microphone Boost"),    MATE_MIXER_STREAM_CONTROL_ROLE_BOOST,       FALSE, FALSE },
    { "Internal Mic Boost",     N_("Internal Microphone Boost"),    MATE_MIXER_STREAM_CONTROL_ROLE_BOOST,       FALSE, FALSE },
    { "Front Mic Boost",        N_("Front Microphone Boost"),       MATE_MIXER_STREAM_CONTROL_ROLE_BOOST,       FALSE, FALSE },
    { "Rear Mic Boost",         N_("Rear Microphone Boost"),        MATE_MIXER_STREAM_CONTROL_ROLE_BOOST,       FALSE, FALSE },
    { "Dock Mic Boost",         N_("Dock Microphone Boost"),        MATE_MIXER_STREAM_CONTROL_ROLE_BOOST,       FALSE, FALSE },
    { "Headphone Mic Boost",    N_("Headphone Microphone Boost"),   MATE_MIXER_STREAM_CONTROL_ROLE_BOOST,       FALSE, FALSE },
    { "Headset Mic Boost",      N_("Headset Microphone Boost"),     MATE_MIXER_STREAM_CONTROL_ROLE_BOOST,       FALSE, FALSE },
    { "Line Boost",             N_("Line In Boost"),                MATE_MIXER_STREAM_CONTROL_ROLE_BOOST,       FALSE, FALSE },
    { NULL }
};

const AlsaSwitchInfo alsa_switches[] =
{
    /* Switches */
    { "Analog Output",          N_("Analog Output"),                MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Analog Source",          N_("Analog Source"),                MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Capture Source",         N_("Capture Source"),               MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Input Source",           N_("Input Source"),                 MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Input Source Select",    N_("Input Source"),                 MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Digital Input Source",   N_("Digital Input Source"),         MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "PCM Capture Source",     N_("PCM Capture Source"),           MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "IEC958 Playback Source", N_("Digital Playback Source"),      MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Mono Output Select",     N_("Mono Output"),                  MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Shared Mic/Line in",     N_("Shared Microphone/Line In"),    MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Mic Select",             N_("Microphone"),                   MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Mic Jack Mode",          N_("Microphone Jack Mode"),         MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Surround Jack Mode",     N_("Surround Jack Mode"),           MATE_MIXER_STREAM_SWITCH_ROLE_UNKNOWN },
    { "Auto-Mute Mode",         N_("Auto-Mute Mode"),               MATE_MIXER_STREAM_SWITCH_ROLE_UNKNOWN },

    /* Toggles */
    { "External Amplifier",     N_("External Amplifier"),       MATE_MIXER_STREAM_SWITCH_ROLE_UNKNOWN },
    { "Bass Boost",             N_("Bass Boost"),               MATE_MIXER_STREAM_SWITCH_ROLE_BOOST },
    { "Capture Boost",          N_("Capture Boost"),            MATE_MIXER_STREAM_SWITCH_ROLE_BOOST },
    { "IEC958",                 N_("Digital"),                  MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "IEC958 In",              N_("Digital In"),               MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "IEC958 Default PCM",     N_("Digital Default PCM"),      MATE_MIXER_STREAM_SWITCH_ROLE_UNKNOWN },
    { "IEC958 Optical",         N_("Optical"),                  MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Auto Gain Control",      N_("Auto Gain Control"),        MATE_MIXER_STREAM_SWITCH_ROLE_UNKNOWN },
    { "Mix",                    N_("Mix"),                      MATE_MIXER_STREAM_SWITCH_ROLE_UNKNOWN },
    { "Mix Mono",               N_("Mix Mono"),                 MATE_MIXER_STREAM_SWITCH_ROLE_UNKNOWN },
    { "Mic Capture",            N_("Microphone Capture"),       MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Input 1",                N_("Input 1"),                  MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { "Input 2",                N_("Input 2"),                  MATE_MIXER_STREAM_SWITCH_ROLE_PORT },
    { NULL }
};

const AlsaSwitchOptionInfo alsa_switch_options[] =
{
    /* Output options */
    { "Speakers",           N_("Speakers"),                     NULL },
    { "Headphones",         N_("Headphones"),                   NULL },
    { "FP Headphones",      N_("Front Panel Headphones"),       NULL },

    /* Microphone options */
    { "Mic",                N_("Microphone"),                   "audio-input-microphone" },
    { "Microphone",         N_("Microphone"),                   "audio-input-microphone" },
    { "Mic1",               N_("Microphone 1"),                 "audio-input-microphone" },
    { "Mic2",               N_("Microphone 2"),                 "audio-input-microphone" },
    { "Mic in",             N_("Microphone In"),                "audio-input-microphone" },
    { "Mic In",             N_("Microphone In"),                "audio-input-microphone" },
    { "Microphone In",      N_("Microphone In"),                "audio-input-microphone" },
    { "Front Mic",          N_("Front Microphone"),             "audio-input-microphone" },
    { "Front Microphone",   N_("Front Microphone"),             "audio-input-microphone" },
    { "Headphone Mic",      N_("Headphone Microphone"),         "audio-input-microphone" },
    { "Headset Mic",        N_("Headset Microphone"),           "audio-input-microphone" },
    { "Dock Mic",           N_("Dock Microphone"),              "audio-input-microphone" },
    { "Internal Mic",       N_("Internal Microphone"),          "audio-input-microphone" },
    { "Int Mic",            N_("Internal Microphone"),          "audio-input-microphone" },
    { "Internal Mic 1",     N_("Internal Microphone 1"),        "audio-input-microphone" },
    { "iMic",               N_("Internal Microphone"),          "audio-input-microphone" },
    { "i-Mic",              N_("Internal Microphone"),          "audio-input-microphone" },
    { "IntMic",             N_("Internal Microphone"),          "audio-input-microphone" },
    { "Int DMic",           N_("Internal Digital Microphone"),  "audio-input-microphone" },
    { "Digital Mic",        N_("Digital Microphone"),           "audio-input-microphone" },
    { "Digital Mic 1",      N_("Digital Microphone 1"),         "audio-input-microphone" },
    { "Digital Mic 2",      N_("Digital Microphone 2"),         "audio-input-microphone" },
    { "D-Mic",              N_("Digital Microphone"),           "audio-input-microphone" },
    { "ExtMic",             N_("External Microphone"),          "audio-input-microphone" },
    { "Ext Mic",            N_("External Microphone"),          "audio-input-microphone" },
    { "E-Mic",              N_("External Microphone"),          "audio-input-microphone" },
    { "e-Mic",              N_("External Microphone"),          "audio-input-microphone" },
    { "Rear Mic",           N_("Rear Microphone"),              "audio-input-microphone" },
    { "Rear Microphone",    N_("Rear Microphone"),              "audio-input-microphone" },
    { "Cam Mic",            N_("Camera Microphone"),            "audio-input-microphone" },

    /* Other options */
    { "Analog",             N_("Analog"),                       NULL },
    { "Analog In",          N_("Analog In"),                    NULL },
    { "Analog Inputs",      N_("Analog Inputs"),                NULL },
    { "Line in",            N_("Line In"),                      NULL },
    { "Line In",            N_("Line In"),                      NULL },
    { "Line-In",            N_("Line In"),                      NULL },
    { "Mic/Line",           N_("Microphone/Line In"),           NULL },
    { "Line/Mic",           N_("Line In/Microphone"),           NULL },
    { "LineIn",             N_("Line In"),                      NULL },
    { "Line",               N_("Line In"),                      NULL },
    { "Input1",             N_("Input 1"),                      NULL },
    { "Input2",             N_("Input 2"),                      NULL },
    { "IEC958 In",          N_("Digital In"),                   NULL },
    { "TV Tuner",           N_("TV Tuner"),                     NULL },
    { "FM",                 N_("FM"),                           NULL },
    { "AUX",                N_("Auxiliary"),                    NULL },
    { "AUX IN",             N_("Auxiliary In"),                 NULL },
    { "Aux In",             N_("Auxiliary In"),                 NULL },
    { "Aux",                N_("Auxiliary"),                    NULL },
    { "Aux0",               N_("Auxiliary 0"),                  NULL },
    { "Aux1",               N_("Auxiliary 1"),                  NULL },
    { "Aux2",               N_("Auxiliary 2"),                  NULL },
    { "Aux3",               N_("Auxiliary 3"),                  NULL },
    { "Docking-Station",    N_("Docking Station"),              NULL },
    { "Mixer",              N_("Mixer"),                        NULL },
    { "CD",                 N_("CD"),                           NULL },
    { "Video",              N_("Video"),                        NULL },
    { "Phone",              N_("Phone"),                        NULL },
    { "Unknown1",           N_("Unknown 1"),                    NULL },
    { "Unknown2",           N_("Unknown 2"),                    NULL },
    { "Enabled",            N_("Enabled"),                      NULL },
    { "Disabled",           N_("Disabled"),                     NULL },
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
#if SND_LIB_VERSION >= ALSA_PACK_VERSION (1, 0, 10)
    [SND_MIXER_SCHN_SIDE_LEFT]              = MATE_MIXER_CHANNEL_SIDE_LEFT,
    [SND_MIXER_SCHN_SIDE_RIGHT]             = MATE_MIXER_CHANNEL_SIDE_RIGHT,
    [SND_MIXER_SCHN_REAR_CENTER]            = MATE_MIXER_CHANNEL_BACK_CENTER
#endif
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
#if SND_LIB_VERSION >= ALSA_PACK_VERSION (1, 0, 10)
    [MATE_MIXER_CHANNEL_BACK_CENTER]        = SND_MIXER_SCHN_REAR_CENTER,
    [MATE_MIXER_CHANNEL_SIDE_LEFT]          = SND_MIXER_SCHN_SIDE_LEFT,
    [MATE_MIXER_CHANNEL_SIDE_RIGHT]         = SND_MIXER_SCHN_SIDE_RIGHT,
#else
    [MATE_MIXER_CHANNEL_BACK_CENTER]        = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_SIDE_LEFT]          = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_SIDE_RIGHT]         = SND_MIXER_SCHN_UNKNOWN,
#endif
    [MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER]  = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER] = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_FRONT_LEFT]     = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_FRONT_RIGHT]    = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_FRONT_CENTER]   = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_CENTER]         = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_BACK_LEFT]      = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_BACK_RIGHT]     = SND_MIXER_SCHN_UNKNOWN,
    [MATE_MIXER_CHANNEL_TOP_BACK_CENTER]    = SND_MIXER_SCHN_UNKNOWN
};
