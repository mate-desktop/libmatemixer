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

/**
 * SECTION:matemixer-stream-control
 * @include: libmatemixer/matemixer.h
 */

G_DEFINE_INTERFACE (MateMixerStreamControl, mate_mixer_stream_control, G_TYPE_OBJECT)

static void
mate_mixer_stream_control_default_init (MateMixerStreamControlInterface *iface)
{
    g_object_interface_install_property (iface,
                                         g_param_spec_string ("name",
                                                              "Name",
                                                              "Name of the stream control",
                                                              NULL,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_string ("description",
                                                              "Description",
                                                              "Description of the stream control",
                                                              NULL,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_flags ("flags",
                                                             "Flags",
                                                             "Capability flags of the stream control",
                                                              MATE_MIXER_TYPE_STREAM_CONTROL_FLAGS,
                                                              MATE_MIXER_STREAM_CONTROL_NO_FLAGS,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_boolean ("mute",
                                                               "Mute",
                                                               "Mute state of the stream control",
                                                               FALSE,
                                                               G_PARAM_READABLE |
                                                               G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_uint ("volume",
                                                            "Volume",
                                                            "Volume of the stream control",
                                                            0,
                                                            G_MAXUINT,
                                                            0,
                                                            G_PARAM_READABLE |
                                                            G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_float ("balance",
                                                             "Balance",
                                                             "Balance value of the stream control",
                                                             -1.0f,
                                                             1.0f,
                                                             0.0f,
                                                             G_PARAM_READABLE |
                                                             G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_float ("fade",
                                                             "Fade",
                                                             "Fade value of the stream control",
                                                             -1.0f,
                                                             1.0f,
                                                             0.0f,
                                                             G_PARAM_READABLE |
                                                             G_PARAM_STATIC_STRINGS));
}

const gchar *
mate_mixer_stream_control_get_name (MateMixerStreamControl *ctl)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), NULL);

    /* Implementation required */
    return MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl)->get_name (ctl);
}

const gchar *
mate_mixer_stream_control_get_description (MateMixerStreamControl *ctl)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), NULL);

    /* Implementation required */
    return MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl)->get_description (ctl);
}

MateMixerStreamControlFlags
mate_mixer_stream_control_get_flags (MateMixerStreamControl *ctl)
{
    MateMixerStreamControlFlags flags = MATE_MIXER_STREAM_CONTROL_NO_FLAGS;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), MATE_MIXER_STREAM_CONTROL_NO_FLAGS);

    g_object_get (G_OBJECT (ctl),
                  "flags", &flags,
                  NULL);

    return flags;
}

gboolean
mate_mixer_stream_control_get_mute (MateMixerStreamControl *ctl)
{
    gboolean mute = FALSE;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), FALSE);

    g_object_get (G_OBJECT (ctl),
                  "mute", &mute,
                  NULL);

    return mute;
}

gboolean
mate_mixer_stream_control_set_mute (MateMixerStreamControl *ctl, gboolean mute)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), FALSE);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->set_mute != NULL)
        return iface->set_mute (ctl, mute);

    return FALSE;
}

guint
mate_mixer_stream_control_get_volume (MateMixerStreamControl *ctl)
{
    guint volume = 0;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), 0);

    g_object_get (G_OBJECT (ctl),
                  "volume", &volume,
                  NULL);

    return volume;
}

gboolean
mate_mixer_stream_control_set_volume (MateMixerStreamControl *ctl, guint volume)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), FALSE);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->set_volume != NULL)
        return iface->set_volume (ctl, volume);

    return FALSE;
}

gdouble
mate_mixer_stream_control_get_decibel (MateMixerStreamControl *ctl)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), -MATE_MIXER_INFINITY);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->get_decibel != NULL)
        return iface->get_decibel (ctl);

    return -MATE_MIXER_INFINITY;
}

gboolean
mate_mixer_stream_control_set_decibel (MateMixerStreamControl *ctl, gdouble decibel)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), FALSE);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->set_decibel != NULL)
        return iface->set_decibel (ctl, decibel);

    return FALSE;
}

guint
mate_mixer_stream_control_get_num_channels (MateMixerStreamControl *ctl)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), 0);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->get_num_channels != NULL)
        return iface->get_num_channels (ctl);

    return 0;
}

MateMixerChannelPosition
mate_mixer_stream_control_get_channel_position (MateMixerStreamControl *ctl, guint channel)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), MATE_MIXER_CHANNEL_UNKNOWN);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->get_channel_position != NULL)
        return iface->get_channel_position (ctl, channel);

    return MATE_MIXER_CHANNEL_UNKNOWN;
}

guint
mate_mixer_stream_control_get_channel_volume (MateMixerStreamControl *ctl, guint channel)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), 0);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->get_channel_volume != NULL)
        return iface->get_channel_volume (ctl, channel);

    return 0;
}

gboolean
mate_mixer_stream_control_set_channel_volume (MateMixerStreamControl *ctl,
                                              guint                   channel,
                                              guint                   volume)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), FALSE);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->set_channel_volume != NULL)
        return iface->set_channel_volume (ctl, channel, volume);

    return FALSE;
}

gdouble
mate_mixer_stream_control_get_channel_decibel (MateMixerStreamControl *ctl, guint channel)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), -MATE_MIXER_INFINITY);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->get_channel_decibel != NULL)
        return iface->get_channel_decibel (ctl, channel);

    return -MATE_MIXER_INFINITY;
}

gboolean
mate_mixer_stream_control_set_channel_decibel (MateMixerStreamControl *ctl,
                                               guint                   channel,
                                               gdouble                 decibel)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), FALSE);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->set_channel_decibel != NULL)
        return iface->set_channel_decibel (ctl, channel, decibel);

    return FALSE;
}

gboolean
mate_mixer_stream_control_has_channel_position (MateMixerStreamControl   *ctl,
                                                MateMixerChannelPosition  position)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), FALSE);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->has_channel_position != NULL)
        return iface->has_channel_position (ctl, position);

    return FALSE;
}

gfloat
mate_mixer_stream_control_get_balance (MateMixerStreamControl *ctl)
{
    gfloat balance = 0.0f;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), 0.0f);

    g_object_get (G_OBJECT (ctl),
                  "balance", &balance,
                  NULL);

    return 0.0f;
}

gboolean
mate_mixer_stream_control_set_balance (MateMixerStreamControl *ctl, gfloat balance)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), FALSE);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->set_balance != NULL)
        return iface->set_balance (ctl, balance);

    return FALSE;
}

gfloat
mate_mixer_stream_control_get_fade (MateMixerStreamControl *ctl)
{
    gfloat fade = 0.0f;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), 0.0f);

    g_object_get (G_OBJECT (ctl),
                  "fade", &fade,
                  NULL);

    return fade;
}

gboolean
mate_mixer_stream_control_set_fade (MateMixerStreamControl *ctl, gfloat fade)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), FALSE);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->set_fade != NULL)
        return iface->set_fade (ctl, fade);

    return FALSE;
}

guint
mate_mixer_stream_control_get_min_volume (MateMixerStreamControl *ctl)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), 0);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->get_min_volume != NULL)
        return iface->get_min_volume (ctl);

    return 0;
}

guint
mate_mixer_stream_control_get_max_volume (MateMixerStreamControl *ctl)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), 0);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->get_max_volume != NULL)
        return iface->get_max_volume (ctl);

    return 0;
}

guint
mate_mixer_stream_control_get_normal_volume (MateMixerStreamControl *ctl)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), 0);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->get_normal_volume != NULL)
        return iface->get_normal_volume (ctl);

    return 0;
}

guint
mate_mixer_stream_control_get_base_volume (MateMixerStreamControl *ctl)
{
    MateMixerStreamControlInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (ctl), 0);

    iface = MATE_MIXER_STREAM_CONTROL_GET_INTERFACE (ctl);

    if (iface->get_base_volume != NULL)
        return iface->get_base_volume (ctl);

    return 0;
}
