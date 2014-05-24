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

#include "matemixer-device-port.h"
#include "matemixer-enums.h"

struct _MateMixerDevicePortPrivate
{
    gchar    *identifier;
    gchar    *name;
    gchar    *icon;
    guint32   priority;
    gint64    latency_offset;

    MateMixerDevicePortDirection  direction;
    MateMixerDevicePortStatus     status;
};

enum
{
    PROP_0,
    PROP_IDENTIFIER,
    PROP_NAME,
    PROP_ICON,
    PROP_PRIORITY,
    // PROP_DIRECTION,
    // PROP_STATUS,
    PROP_LATENCY_OFFSET,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (MateMixerDevicePort, mate_mixer_device_port, G_TYPE_OBJECT);

static void
mate_mixer_device_port_init (MateMixerDevicePort *port)
{
    port->priv = G_TYPE_INSTANCE_GET_PRIVATE (
        port,
        MATE_MIXER_TYPE_DEVICE_PORT,
        MateMixerDevicePortPrivate);
}

static void
mate_mixer_device_port_get_property (GObject     *object,
                                     guint        param_id,
                                     GValue      *value,
                                     GParamSpec  *pspec)
{
    MateMixerDevicePort *port;

    port = MATE_MIXER_DEVICE_PORT (object);

    switch (param_id) {
    case PROP_IDENTIFIER:
        g_value_set_string (value, port->priv->identifier);
        break;
    case PROP_NAME:
        g_value_set_string (value, port->priv->name);
        break;
    case PROP_ICON:
        g_value_set_string (value, port->priv->icon);
        break;
    case PROP_PRIORITY:
        g_value_set_uint (value, port->priv->priority);
        break;
    // case PROP_DIRECTION:
        // break;
    // case PROP_STATUS:
        // break;
    case PROP_LATENCY_OFFSET:
        g_value_set_int64 (value, port->priv->latency_offset);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_device_port_set_property (GObject       *object,
                                     guint          param_id,
                                     const GValue  *value,
                                     GParamSpec    *pspec)
{
    MateMixerDevicePort *port;

    port = MATE_MIXER_DEVICE_PORT (object);

    switch (param_id) {
    case PROP_IDENTIFIER:
        port->priv->identifier = g_strdup (g_value_get_string (value));
        break;
    case PROP_NAME:
        port->priv->name = g_strdup (g_value_get_string (value));
        break;
    case PROP_ICON:
        port->priv->icon = g_strdup (g_value_get_string (value));
        break;
    case PROP_PRIORITY:
        port->priv->priority = g_value_get_uint (value);
        break;
    // case PROP_DIRECTION:
        // break;
    // case PROP_STATUS:
        // break;
    case PROP_LATENCY_OFFSET:
        port->priv->latency_offset = g_value_get_int64 (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_device_port_finalize (GObject *object)
{
    MateMixerDevicePort *port;

    port = MATE_MIXER_DEVICE_PORT (object);

    g_free (port->priv->identifier);
    g_free (port->priv->name);
    g_free (port->priv->icon);

    G_OBJECT_CLASS (mate_mixer_device_port_parent_class)->finalize (object);
}

static void
mate_mixer_device_port_class_init (MateMixerDevicePortClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = mate_mixer_device_port_finalize;
    object_class->get_property = mate_mixer_device_port_get_property;
    object_class->set_property = mate_mixer_device_port_set_property;

    properties[PROP_IDENTIFIER] = g_param_spec_string (
        "identifier",
        "Identifier",
        "Identifier of the device port",
        NULL,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    properties[PROP_NAME] = g_param_spec_string (
        "name",
        "Name",
        "Name of the device port",
        NULL,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    properties[PROP_ICON] = g_param_spec_string (
        "icon",
        "Icon",
        "Icon name for the device port",
        NULL,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    properties[PROP_PRIORITY] = g_param_spec_uint (
        "priority",
        "Priority",
        "Priority of the device port",
        0,
        G_MAXUINT32,
        0,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    properties[PROP_LATENCY_OFFSET] = g_param_spec_int64 (
        "latency-offset",
        "Latency offset",
        "Latency offset of the device port",
        G_MININT64,
        G_MAXINT64,
        0,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    g_type_class_add_private (object_class, sizeof (MateMixerDevicePortPrivate));
}

MateMixerDevicePort *
mate_mixer_device_port_new (const gchar                 *identifier,
                            const gchar                  *name,
                            const gchar                  *icon,
                            guint32                       priority,
                            MateMixerDevicePortDirection  direction,
                            MateMixerDevicePortStatus     status,
                            gint64                        latency_offset)
{
    return g_object_new (MATE_MIXER_TYPE_DEVICE_PORT,
        "identifier",       identifier,
        "name",             name,
        "icon",             icon,
        "priority",         priority,
        //"direction",        direction,
        //"status",           status,
        "latency-offset",   latency_offset,
        NULL);
}

const gchar *
mate_mixer_device_port_get_identifier (MateMixerDevicePort *port)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PORT (port), NULL);

    return port->priv->identifier;
}

const gchar *
mate_mixer_device_port_get_name (MateMixerDevicePort *port)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PORT (port), NULL);

    return port->priv->name;
}

const gchar *
mate_mixer_device_port_get_icon (MateMixerDevicePort *port)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PORT (port), NULL);

    return port->priv->icon;
}

guint32
mate_mixer_device_port_get_priority (MateMixerDevicePort *port)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PORT (port), 0);

    return port->priv->priority;
}

MateMixerDevicePortDirection
mate_mixer_device_port_get_direction (MateMixerDevicePort *port)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PORT (port), 0);

    return port->priv->direction;
}

MateMixerDevicePortStatus
mate_mixer_device_port_get_status (MateMixerDevicePort *port)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PORT (port), 0);

    return port->priv->status;
}

gint64
mate_mixer_device_port_get_latency_offset (MateMixerDevicePort *port)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE_PORT (port), 0);

    return port->priv->latency_offset;
}
