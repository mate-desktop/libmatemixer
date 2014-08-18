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

#ifndef PULSE_DEVICE_H
#define PULSE_DEVICE_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include <pulse/pulseaudio.h>

#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_DEVICE                       \
        (pulse_device_get_type ())
#define PULSE_DEVICE(o)                         \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_DEVICE, PulseDevice))
#define PULSE_IS_DEVICE(o)                      \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_DEVICE))
#define PULSE_DEVICE_CLASS(k)                   \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_DEVICE, PulseDeviceClass))
#define PULSE_IS_DEVICE_CLASS(k)                \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_DEVICE))
#define PULSE_DEVICE_GET_CLASS(o)               \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_IS_DEVICE, PulseDeviceClass))

typedef struct _PulseDeviceClass    PulseDeviceClass;
typedef struct _PulseDevicePrivate  PulseDevicePrivate;

struct _PulseDevice
{
    MateMixerDevice parent;

    /*< private >*/
    PulseDevicePrivate *priv;
};

struct _PulseDeviceClass
{
    MateMixerDeviceClass parent;
};

GType            pulse_device_get_type       (void) G_GNUC_CONST;

PulseDevice *    pulse_device_new            (PulseConnection    *connection,
                                              const pa_card_info *info);

void             pulse_device_update         (PulseDevice        *device,
                                              const pa_card_info *info);

void             pulse_device_add_stream     (PulseDevice        *device,
                                              PulseStream        *stream);

void             pulse_device_remove_stream  (PulseDevice        *device,
                                              PulseStream        *stream);

guint32          pulse_device_get_index      (PulseDevice        *device);
PulseConnection *pulse_device_get_connection (PulseDevice        *device);

PulsePort *      pulse_device_get_port       (PulseDevice        *device,
                                              const gchar        *name);

G_END_DECLS

#endif /* PULSE_DEVICE_H */
