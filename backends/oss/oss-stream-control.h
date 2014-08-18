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

#ifndef OSS_STREAM_CONTROL_H
#define OSS_STREAM_CONTROL_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "oss-types.h"

G_BEGIN_DECLS

#define OSS_TYPE_STREAM_CONTROL                 \
        (oss_stream_control_get_type ())
#define OSS_STREAM_CONTROL(o)                   \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), OSS_TYPE_STREAM_CONTROL, OssStreamControl))
#define OSS_IS_STREAM_CONTROL(o)                \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OSS_TYPE_STREAM_CONTROL))
#define OSS_STREAM_CONTROL_CLASS(k)             \
        (G_TYPE_CHECK_CLASS_CAST ((k), OSS_TYPE_STREAM_CONTROL, OssStreamControlClass))
#define OSS_IS_STREAM_CONTROL_CLASS(k)          \
        (G_TYPE_CHECK_CLASS_TYPE ((k), OSS_TYPE_STREAM_CONTROL))
#define OSS_STREAM_CONTROL_GET_CLASS(o)         \
        (G_TYPE_INSTANCE_GET_CLASS ((o), OSS_TYPE_STREAM_CONTROL, OssStreamControlClass))

typedef struct _OssStreamControlClass    OssStreamControlClass;
typedef struct _OssStreamControlPrivate  OssStreamControlPrivate;

struct _OssStreamControl
{
    MateMixerStreamControl parent;

    /*< private >*/
    OssStreamControlPrivate *priv;
};

struct _OssStreamControlClass
{
    MateMixerStreamControlClass parent;
};

GType             oss_stream_control_get_type   (void) G_GNUC_CONST;

OssStreamControl *oss_stream_control_new        (const gchar               *name,
                                                 const gchar               *label,
                                                 MateMixerStreamControlRole role,
                                                 OssStream                 *stream,
                                                 gint                       fd,
                                                 gint                       devnum,
                                                 gboolean                   stereo);

gint              oss_stream_control_get_devnum (OssStreamControl          *control);

void              oss_stream_control_load       (OssStreamControl          *control);
void              oss_stream_control_close      (OssStreamControl          *control);

G_END_DECLS

#endif /* OSS_STREAM_CONTROL_H */
