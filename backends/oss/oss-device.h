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

#ifndef OSS_DEVICE_H
#define OSS_DEVICE_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "oss-types.h"

G_BEGIN_DECLS

#define OSS_TYPE_DEVICE                         \
        (oss_device_get_type ())
#define OSS_DEVICE(o)                           \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), OSS_TYPE_DEVICE, OssDevice))
#define OSS_IS_DEVICE(o)                        \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OSS_TYPE_DEVICE))
#define OSS_DEVICE_CLASS(k)                     \
        (G_TYPE_CHECK_CLASS_CAST ((k), OSS_TYPE_DEVICE, OssDeviceClass))
#define OSS_IS_DEVICE_CLASS(k)                  \
        (G_TYPE_CHECK_CLASS_TYPE ((k), OSS_TYPE_DEVICE))
#define OSS_DEVICE_GET_CLASS(o)                 \
        (G_TYPE_INSTANCE_GET_CLASS ((o), OSS_TYPE_DEVICE, OssDeviceClass))

typedef struct _OssDeviceClass    OssDeviceClass;
typedef struct _OssDevicePrivate  OssDevicePrivate;

struct _OssDevice
{
    MateMixerDevice parent;

    /*< private >*/
    OssDevicePrivate *priv;
};

struct _OssDeviceClass
{
    MateMixerDeviceClass parent;

    /*< private >*/
    void (*closed) (OssDevice *device);
};

GType        oss_device_get_type          (void) G_GNUC_CONST;

OssDevice *  oss_device_new               (const gchar *name,
                                           const gchar *label,
                                           const gchar *path,
                                           gint         fd);

gboolean     oss_device_open              (OssDevice   *device);
gboolean     oss_device_is_open           (OssDevice   *device);
void         oss_device_close             (OssDevice   *device);

void         oss_device_load              (OssDevice   *device);

const gchar *oss_device_get_path          (OssDevice   *device);

OssStream *  oss_device_get_input_stream  (OssDevice   *device);
OssStream *  oss_device_get_output_stream (OssDevice   *device);

G_END_DECLS

#endif /* OSS_DEVICE_H */
