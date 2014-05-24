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

#ifndef MATEMIXER_DEVICE_PORT_H
#define MATEMIXER_DEVICE_PORT_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-enums.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_DEVICE_PORT              \
        (mate_mixer_device_port_get_type ())
#define MATE_MIXER_DEVICE_PORT(o)                \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_DEVICE_PORT, MateMixerDevicePort))
#define MATE_MIXER_IS_DEVICE_PORT(o)             \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_DEVICE_PORT))
#define MATE_MIXER_DEVICE_PORT_CLASS(k)          \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_DEVICE_PORT, MateMixerDevicePortClass))
#define MATE_MIXER_IS_DEVICE_PORT_CLASS(k)       \
        (G_TYPE_CLASS_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_DEVICE_PORT))
#define MATE_MIXER_DEVICE_PORT_GET_CLASS(o)      \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_DEVICE_PORT, MateMixerDevicePortClass))

typedef struct _MateMixerDevicePort         MateMixerDevicePort;
typedef struct _MateMixerDevicePortClass    MateMixerDevicePortClass;
typedef struct _MateMixerDevicePortPrivate  MateMixerDevicePortPrivate;

struct _MateMixerDevicePort
{
    GObject parent;

    MateMixerDevicePortPrivate *priv;
};

struct _MateMixerDevicePortClass
{
    GObjectClass parent;    
};

GType mate_mixer_device_port_get_type (void) G_GNUC_CONST;

MateMixerDevicePort *mate_mixer_device_port_new (const gchar                  *identifier,
                                                 const gchar                  *name,
                                                 const gchar                  *icon,
                                                 guint32                       priority,
                                                 MateMixerDevicePortDirection  direction,
                                                 MateMixerDevicePortStatus     status,
                                                 gint64                        latency_offset);

const gchar *mate_mixer_device_port_get_identifier (MateMixerDevicePort *port);

const gchar *mate_mixer_device_port_get_name (MateMixerDevicePort *port);

const gchar *mate_mixer_device_port_get_icon (MateMixerDevicePort *port);

guint32      mate_mixer_device_port_get_priority (MateMixerDevicePort *port);

MateMixerDevicePortDirection mate_mixer_device_port_get_direction (MateMixerDevicePort *port);

MateMixerDevicePortStatus    mate_mixer_device_port_get_status (MateMixerDevicePort *port);

gint64                       mate_mixer_device_port_get_latency_offset (MateMixerDevicePort *port);

G_END_DECLS

#endif /* MATEMIXER_PORT_H */
