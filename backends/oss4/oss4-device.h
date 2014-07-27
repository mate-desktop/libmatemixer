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

#ifndef OSS4_DEVICE_H
#define OSS4_DEVICE_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define OSS4_TYPE_DEVICE                        \
        (oss4_device_get_type ())
#define OSS4_DEVICE(o)                          \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), OSS4_TYPE_DEVICE, Oss4Device))
#define OSS4_IS_DEVICE(o)                       \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OSS4_TYPE_DEVICE))
#define OSS4_DEVICE_CLASS(k)                    \
        (G_TYPE_CHECK_CLASS_CAST ((k), OSS4_TYPE_DEVICE, Oss4DeviceClass))
#define OSS4_IS_DEVICE_CLASS(k)                 \
        (G_TYPE_CHECK_CLASS_TYPE ((k), OSS4_TYPE_DEVICE))
#define OSS4_DEVICE_GET_CLASS(o)                \
        (G_TYPE_INSTANCE_GET_CLASS ((o), OSS4_TYPE_DEVICE, Oss4DeviceClass))

typedef struct _Oss4Device         Oss4Device;
typedef struct _Oss4DeviceClass    Oss4DeviceClass;
typedef struct _Oss4DevicePrivate  Oss4DevicePrivate;

struct _Oss4Device
{
    GObject parent;

    /*< private >*/
    Oss4DevicePrivate *priv;
};

struct _Oss4DeviceClass
{
    GObjectClass parent;
};

GType            oss4_device_get_type          (void) G_GNUC_CONST;

Oss4Device *     oss4_device_new (const gchar *name,
                                  const gchar *description,
                                  gint         fd,
                                  gint         index);

gboolean         oss4_device_read              (Oss4Device   *device);

gint             oss4_device_get_index         (Oss4Device *odevice);

MateMixerStream *oss4_device_get_input_stream  (Oss4Device   *odevice);
MateMixerStream *oss4_device_get_output_stream (Oss4Device   *odevice);

G_END_DECLS

#endif /* OSS_DEVICE_H */
