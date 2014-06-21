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

// XXX
// consider using guint for volumes, but see the other backends first

#include <glib.h>
#include <glib-object.h>

#include "matemixer-device.h"
#include "matemixer-enums.h"
#include "matemixer-enum-types.h"
#include "matemixer-port.h"
#include "matemixer-stream.h"

/**
 * SECTION:matemixer-stream
 * @include: libmatemixer/matemixer.h
 */

enum {
    MONITOR_VALUE,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

G_DEFINE_INTERFACE (MateMixerStream, mate_mixer_stream, G_TYPE_OBJECT)

static void
mate_mixer_stream_default_init (MateMixerStreamInterface *iface)
{
    g_object_interface_install_property (iface,
                                         g_param_spec_string ("name",
                                                              "Name",
                                                              "Name of the stream",
                                                              NULL,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_string ("description",
                                                              "Description",
                                                              "Description of the stream",
                                                              NULL,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_object ("device",
                                                              "Device",
                                                              "Device the stream belongs to",
                                                              MATE_MIXER_TYPE_DEVICE,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_flags ("flags",
                                                             "Flags",
                                                             "Capability flags of the stream",
                                                              MATE_MIXER_TYPE_STREAM_FLAGS,
                                                              MATE_MIXER_STREAM_NO_FLAGS,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_enum ("state",
                                                            "State",
                                                            "Current state of the stream",
                                                            MATE_MIXER_TYPE_STREAM_STATE,
                                                            MATE_MIXER_STREAM_UNKNOWN_STATE,
                                                            G_PARAM_READABLE |
                                                            G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_boolean ("mute",
                                                               "Mute",
                                                               "Mute state of the stream",
                                                               FALSE,
                                                               G_PARAM_READABLE |
                                                               G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_uint ("num-channels",
                                                            "Number of channels",
                                                            "Number of volume channels in the stream",
                                                            0,
                                                            G_MAXUINT,
                                                            0,
                                                            G_PARAM_READABLE |
                                                            G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_int64 ("volume",
                                                             "Volume",
                                                             "Volume of the stream",
                                                             G_MININT64,
                                                             G_MAXINT64,
                                                             0,
                                                             G_PARAM_READABLE |
                                                             G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_double ("balance",
                                                              "Balance",
                                                              "Balance value of the stream",
                                                              -1.0,
                                                              1.0,
                                                              0.0,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_double ("fade",
                                                              "Fade",
                                                              "Fade value of the stream",
                                                              -1.0,
                                                              1.0,
                                                              0.0,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_pointer ("ports",
                                                               "Ports",
                                                               "GList of the sound device ports",
                                                               G_PARAM_READABLE |
                                                               G_PARAM_STATIC_STRINGS));

    g_object_interface_install_property (iface,
                                         g_param_spec_object ("active-port",
                                                              "Active port",
                                                              "The currently active port of the stream",
                                                              MATE_MIXER_TYPE_PORT,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

    signals[MONITOR_VALUE] =
        g_signal_new ("monitor-value",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerStreamInterface, monitor_value),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__DOUBLE,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_DOUBLE);
}

const gchar *
mate_mixer_stream_get_name (MateMixerStream *stream)
{
    return MATE_MIXER_STREAM_GET_INTERFACE (stream)->get_name (stream);
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

MateMixerStreamFlags
mate_mixer_stream_get_flags (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), MATE_MIXER_STREAM_NO_FLAGS);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_flags)
        return iface->get_flags (stream);

    return MATE_MIXER_STREAM_NO_FLAGS;
}

MateMixerStreamState
mate_mixer_stream_get_state (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), MATE_MIXER_STREAM_UNKNOWN_STATE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_state)
        return iface->get_state (stream);

    return MATE_MIXER_STREAM_UNKNOWN_STATE;
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

gint64
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
mate_mixer_stream_set_volume (MateMixerStream *stream, gint64 volume)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_volume)
        return iface->set_volume (stream, volume);

    return FALSE;
}

gdouble
mate_mixer_stream_get_decibel (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), -MATE_MIXER_INFINITY);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_decibel)
        return iface->get_decibel (stream);

    return -MATE_MIXER_INFINITY;
}

gboolean
mate_mixer_stream_set_decibel (MateMixerStream *stream, gdouble decibel)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_decibel)
        return iface->set_decibel (stream, decibel);

    return FALSE;
}

guint
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
mate_mixer_stream_get_channel_position (MateMixerStream *stream, guint channel)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), MATE_MIXER_CHANNEL_UNKNOWN_POSITION);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_channel_position)
        return iface->get_channel_position (stream, channel);

    return MATE_MIXER_CHANNEL_UNKNOWN_POSITION;
}

gint64
mate_mixer_stream_get_channel_volume (MateMixerStream *stream, guint channel)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_channel_volume)
        return iface->get_channel_volume (stream, channel);

    return 0;
}

gboolean
mate_mixer_stream_set_channel_volume (MateMixerStream *stream,
                                      guint            channel,
                                      gint64           volume)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_channel_volume)
        return iface->set_channel_volume (stream, channel, volume);

    return FALSE;
}

gdouble
mate_mixer_stream_get_channel_decibel (MateMixerStream *stream, guint channel)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), -MATE_MIXER_INFINITY);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_channel_decibel)
        return iface->get_channel_decibel (stream, channel);

    return -MATE_MIXER_INFINITY;
}

gboolean
mate_mixer_stream_set_channel_decibel (MateMixerStream *stream,
                                       guint            channel,
                                       gdouble          decibel)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_channel_decibel)
        return iface->set_channel_decibel (stream, channel, decibel);

    return FALSE;
}

gboolean
mate_mixer_stream_has_position (MateMixerStream          *stream,
                                MateMixerChannelPosition  position)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->has_position)
        return iface->has_position (stream, position);

    return FALSE;
}

gint64
mate_mixer_stream_get_position_volume (MateMixerStream          *stream,
                                       MateMixerChannelPosition  position)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_position_volume)
        return iface->get_position_volume (stream, position);

    return 0;
}

gboolean
mate_mixer_stream_set_position_volume (MateMixerStream          *stream,
                                       MateMixerChannelPosition  position,
                                       gint64                    volume)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_position_volume)
        return iface->set_position_volume (stream, position, volume);

    return FALSE;
}

gdouble
mate_mixer_stream_get_position_decibel (MateMixerStream          *stream,
                                        MateMixerChannelPosition  position)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), -MATE_MIXER_INFINITY);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_position_decibel)
        return iface->get_position_decibel (stream, position);

    return -MATE_MIXER_INFINITY;
}

gboolean
mate_mixer_stream_set_position_decibel (MateMixerStream          *stream,
                                        MateMixerChannelPosition  position,
                                        gdouble                   decibel)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_position_decibel)
        return iface->set_position_decibel (stream, position, decibel);

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

gboolean
mate_mixer_stream_suspend (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->suspend)
        return iface->suspend (stream);

    return FALSE;
}

gboolean
mate_mixer_stream_resume (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->resume)
        return iface->resume (stream);

    return FALSE;
}

gboolean
mate_mixer_stream_monitor_start (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->monitor_start)
        return iface->monitor_start (stream);

    return FALSE;
}

void
mate_mixer_stream_monitor_stop (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_if_fail (MATE_MIXER_IS_STREAM (stream));

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->monitor_stop)
        iface->monitor_stop (stream);
}

gboolean
mate_mixer_stream_monitor_is_running (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->monitor_is_running)
        return iface->monitor_is_running (stream);

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
mate_mixer_stream_set_active_port (MateMixerStream *stream, const gchar *port)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);
    g_return_val_if_fail (port != NULL, FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_active_port)
        return iface->set_active_port (stream, port);

    return FALSE;
}

gint64
mate_mixer_stream_get_min_volume (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_min_volume)
        return iface->get_min_volume (stream);

    return 0;
}

gint64
mate_mixer_stream_get_max_volume (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_max_volume)
        return iface->get_max_volume (stream);

    return 0;
}

gint64
mate_mixer_stream_get_normal_volume (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_normal_volume)
        return iface->get_normal_volume (stream);

    return 0;
}

gint64
mate_mixer_stream_get_base_volume (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), 0);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_base_volume)
        return iface->get_base_volume (stream);

    return 0;
}
