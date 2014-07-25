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
                                                            MATE_MIXER_STREAM_STATE_UNKNOWN,
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
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    /* Implementation required */
    return MATE_MIXER_STREAM_GET_INTERFACE (stream)->get_name (stream);
}

const gchar *
mate_mixer_stream_get_description (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    /* Implementation required */
    return MATE_MIXER_STREAM_GET_INTERFACE (stream)->get_description (stream);
}

MateMixerDevice *
mate_mixer_stream_get_device (MateMixerStream *stream)
{
    MateMixerDevice *device = NULL;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    g_object_get (G_OBJECT (stream),
                  "device", &device,
                  NULL);

    if (device != NULL)
        g_object_unref (device);

    return device;

}

MateMixerStreamFlags
mate_mixer_stream_get_flags (MateMixerStream *stream)
{
    MateMixerStreamFlags flags = MATE_MIXER_STREAM_NO_FLAGS;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), MATE_MIXER_STREAM_NO_FLAGS);

    g_object_get (G_OBJECT (stream),
                  "flags", &flags,
                  NULL);

    return flags;
}

MateMixerStreamState
mate_mixer_stream_get_state (MateMixerStream *stream)
{
    MateMixerStreamState state = MATE_MIXER_STREAM_STATE_UNKNOWN;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), MATE_MIXER_STREAM_STATE_UNKNOWN);

    g_object_get (G_OBJECT (stream),
                  "state", &state,
                  NULL);

    return state;
}

MateMixerStreamControl *
mate_mixer_stream_get_control (MateMixerStream *stream, const gchar *name)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    /* Implementation required */
    return MATE_MIXER_STREAM_GET_INTERFACE (stream)->get_control (stream, name);
}

MateMixerStreamControl *
mate_mixer_stream_get_default_control (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    /* Implementation required */
    return MATE_MIXER_STREAM_GET_INTERFACE (stream)->get_default_control (stream);
}

MateMixerPort *
mate_mixer_stream_get_port (MateMixerStream *stream, const gchar *name)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->get_port != NULL)
        return iface->get_port (stream, name);

    return FALSE;
}

MateMixerPort *
mate_mixer_stream_get_active_port (MateMixerStream *stream)
{
    MateMixerPort *port = NULL;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    g_object_get (G_OBJECT (stream),
                  "active-port", &port,
                  NULL);

    if (port != NULL)
        g_object_unref (port);

    return port;
}

gboolean
mate_mixer_stream_set_active_port (MateMixerStream *stream, MateMixerPort *port)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_PORT (port), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->set_active_port != NULL)
        return iface->set_active_port (stream, port);

    return FALSE;
}

const GList *
mate_mixer_stream_list_controls (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->list_controls != NULL)
        return iface->list_controls (stream);

    return NULL;
}

const GList *
mate_mixer_stream_list_ports (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->list_ports != NULL)
        return iface->list_ports (stream);

    return NULL;
}

gboolean
mate_mixer_stream_suspend (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->suspend != NULL)
        return iface->suspend (stream);

    return FALSE;
}

gboolean
mate_mixer_stream_resume (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->resume != NULL)
        return iface->resume (stream);

    return FALSE;
}

gboolean
mate_mixer_stream_monitor_get_enabled (MateMixerStream *stream)
{
    gboolean enabled = FALSE;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    g_object_get (G_OBJECT (stream),
                  "monitor-enabled", &enabled,
                  NULL);

    return enabled;
}

gboolean
mate_mixer_stream_monitor_set_enabled (MateMixerStream *stream, gboolean enabled)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->monitor_set_enabled != NULL)
        return iface->monitor_set_enabled (stream, enabled);

    return FALSE;
}

const gchar *
mate_mixer_stream_monitor_get_name (MateMixerStream *stream)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->monitor_get_name != NULL)
        return iface->monitor_get_name (stream);

    return FALSE;
}

gboolean
mate_mixer_stream_monitor_set_name (MateMixerStream *stream, const gchar *name)
{
    MateMixerStreamInterface *iface;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    iface = MATE_MIXER_STREAM_GET_INTERFACE (stream);

    if (iface->monitor_set_name != NULL)
        return iface->monitor_set_name (stream, name);

    return FALSE;
}
