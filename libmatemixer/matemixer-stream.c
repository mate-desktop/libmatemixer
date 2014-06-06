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

#include "matemixer-device.h"
#include "matemixer-enums.h"
#include "matemixer-enum-types.h"
#include "matemixer-port.h"
#include "matemixer-stream.h"

G_DEFINE_INTERFACE (MateMixerStream, mate_mixer_stream, G_TYPE_OBJECT)

static void
mate_mixer_stream_default_init (MateMixerStreamInterface *iface)
{
    g_object_interface_install_property (iface,
                                         g_param_spec_string ("name",
                                                              "Name",
                                                              "Name of the stream",
                                                              NULL,
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_string ("description",
                                                              "Description",
                                                              "Description of the stream",
                                                              NULL,
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_string ("icon",
                                                              "Icon",
                                                              "Name of the stream icon",
                                                              NULL,
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_object ("device",
                                                              "Device",
                                                              "Device the stream belongs to",
                                                              MATE_MIXER_TYPE_DEVICE,
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_enum ("status",
                                                            "Status",
                                                            "Status of the stream",
                                                            MATE_MIXER_TYPE_STREAM_STATUS,
                                                            MATE_MIXER_STREAM_UNKNOWN_STATUS,
                                                            G_PARAM_CONSTRUCT_ONLY |
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_double ("volume",
                                                              "Volume",
                                                              "Volume of the stream",
                                                              -1, 0, 1,
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_string ("mute",
                                                              "Mute",
                                                              "Mute state of the stream",
                                                              FALSE,
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
}

const gchar *
mate_mixer_stream_get_name (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_name)
        return iface->get_name (stream);

    return NULL;
}

const gchar *
mate_mixer_stream_get_description (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_description)
        return iface->get_description (stream);

    return NULL;
}

const gchar *
mate_mixer_stream_get_icon (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_icon)
        return iface->get_icon (stream);

    return NULL;
}

MateMixerDevice *
mate_mixer_stream_get_device (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_device)
        return iface->get_device (stream);

    return NULL;
}

MateMixerStreamStatus
mate_mixer_stream_get_status (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), MATE_MIXER_STREAM_UNKNOWN_STATUS);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_status)
        return iface->get_status (stream);

    return MATE_MIXER_STREAM_UNKNOWN_STATUS;
}

gboolean
mate_mixer_stream_get_mute (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_mute)
        return iface->get_mute (stream);

    return FALSE;
}

gboolean
mate_mixer_stream_set_mute (MateMixerStream *stream, gboolean mute)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_mute)
        return iface->set_mute (stream, mute);

    return FALSE;
}

guint32
mate_mixer_stream_get_volume (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_volume)
        return iface->get_volume (stream);

    return 0;
}

gboolean
mate_mixer_stream_set_volume (MateMixerStream *stream, guint32 volume)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_volume)
        return iface->set_volume (stream, volume);

    return FALSE;
}

gdouble
mate_mixer_stream_get_volume_db (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_volume_db)
        return iface->get_volume_db (stream);

    return 0;
}

gboolean
mate_mixer_stream_set_volume_db (MateMixerStream *stream, gdouble volume_db)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_volume_db)
        return iface->set_volume_db (stream, volume_db);

    return FALSE;
}

guint8
mate_mixer_stream_get_num_channels (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_num_channels)
        return iface->get_num_channels (stream);

    return 0;
}

MateMixerChannelPosition
mate_mixer_stream_get_channel_position (MateMixerStream *stream, guint8 channel)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), MATE_MIXER_CHANNEL_UNKNOWN_POSITION);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_channel_position)
        return iface->get_channel_position (stream, channel);

    return MATE_MIXER_CHANNEL_UNKNOWN_POSITION;
}

guint32
mate_mixer_stream_get_channel_volume (MateMixerStream *stream, guint8 channel)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_channel_volume)
        return iface->get_channel_volume (stream, channel);

    return 0;
}

gboolean
mate_mixer_stream_set_channel_volume (MateMixerStream *stream, guint8 channel, guint32 volume)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_channel_volume)
        return iface->set_channel_volume (stream, channel, volume);

    return FALSE;
}

gdouble
mate_mixer_stream_get_channel_volume_db (MateMixerStream *stream, guint8 channel)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_channel_volume_db)
        return iface->get_channel_volume_db (stream, channel);

    return 0;
}

gboolean
mate_mixer_stream_set_channel_volume_db (MateMixerStream *stream, guint8 channel, gdouble volume_db)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_channel_volume_db)
        return iface->set_channel_volume_db (stream, channel, volume_db);

    return FALSE;
}

gboolean
mate_mixer_stream_has_position (MateMixerStream *stream, MateMixerChannelPosition position)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->has_position)
        return iface->has_position (stream, position);

    return FALSE;
}

guint32
mate_mixer_stream_get_position_volume (MateMixerStream *stream, MateMixerChannelPosition position)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_position_volume)
        return iface->get_position_volume (stream, position);

    return 0;
}

gboolean
mate_mixer_stream_set_position_volume (MateMixerStream *stream, MateMixerChannelPosition position, guint32 volume)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_position_volume)
        return iface->set_position_volume (stream, position, volume);

    return FALSE;
}

gdouble
mate_mixer_stream_get_position_volume_db (MateMixerStream *stream, MateMixerChannelPosition position)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_position_volume_db)
        return iface->get_position_volume_db (stream, position);

    return 0;
}

gboolean
mate_mixer_stream_set_position_volume_db (MateMixerStream *stream, MateMixerChannelPosition position, gdouble volume_db)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_position_volume_db)
        return iface->set_position_volume_db (stream, position, volume_db);

    return FALSE;
}

gdouble
mate_mixer_stream_get_balance (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_balance)
        return iface->get_balance (stream);

    return 0;
}

gboolean
mate_mixer_stream_set_balance (MateMixerStream *stream, gdouble balance)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_balance)
        return iface->set_balance (stream, balance);

    return FALSE;
}

gdouble
mate_mixer_stream_get_fade (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_fade)
        return iface->get_fade (stream);

    return 0;
}

gboolean
mate_mixer_stream_set_fade (MateMixerStream *stream, gdouble fade)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_fade)
        return iface->set_fade (stream, fade);

    return FALSE;
}

const GList *
mate_mixer_stream_list_ports (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->list_ports)
        return iface->list_ports (stream);

    return NULL;
}

MateMixerPort *
mate_mixer_stream_get_active_port (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_active_port)
        return iface->get_active_port (stream);

    return NULL;
}

gboolean
mate_mixer_stream_set_active_port (MateMixerStream *stream, MateMixerPort *port)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_PORT (port), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_active_port)
        return iface->set_active_port (stream, port);

    return FALSE;
}
