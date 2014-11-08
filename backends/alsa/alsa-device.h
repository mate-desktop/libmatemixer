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

#ifndef ALSA_DEVICE_H
#define ALSA_DEVICE_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "alsa-types.h"

G_BEGIN_DECLS

#define ALSA_TYPE_DEVICE                        \
        (alsa_device_get_type ())
#define ALSA_DEVICE(o)                          \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), ALSA_TYPE_DEVICE, AlsaDevice))
#define ALSA_IS_DEVICE(o)                       \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ALSA_TYPE_DEVICE))
#define ALSA_DEVICE_CLASS(k)                    \
        (G_TYPE_CHECK_CLASS_CAST ((k), ALSA_TYPE_DEVICE, AlsaDeviceClass))
#define ALSA_IS_DEVICE_CLASS(k)                 \
        (G_TYPE_CHECK_CLASS_TYPE ((k), ALSA_TYPE_DEVICE))
#define ALSA_DEVICE_GET_CLASS(o)                \
        (G_TYPE_INSTANCE_GET_CLASS ((o), ALSA_TYPE_DEVICE, AlsaDeviceClass))

typedef struct _AlsaDeviceClass    AlsaDeviceClass;
typedef struct _AlsaDevicePrivate  AlsaDevicePrivate;

struct _AlsaDevice
{
    MateMixerDevice parent;

    /*< private >*/
    AlsaDevicePrivate *priv;
};

struct _AlsaDeviceClass
{
    MateMixerDeviceClass parent_class;

    /*< private >*/
    void (*closed) (AlsaDevice *device);
};

GType       alsa_device_get_type          (void) G_GNUC_CONST;

AlsaDevice *alsa_device_new               (const gchar *name,
                                           const gchar *label);

gboolean    alsa_device_open              (AlsaDevice  *device);
gboolean    alsa_device_is_open           (AlsaDevice  *device);
void        alsa_device_close             (AlsaDevice  *device);

void        alsa_device_load              (AlsaDevice  *device);

AlsaStream *alsa_device_get_input_stream  (AlsaDevice  *device);
AlsaStream *alsa_device_get_output_stream (AlsaDevice  *device);

G_END_DECLS

#endif /* ALSA_DEVICE_H */
