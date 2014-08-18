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

#ifndef OSS_STREAM_H
#define OSS_STREAM_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "oss-types.h"

G_BEGIN_DECLS

#define OSS_TYPE_STREAM                         \
        (oss_stream_get_type ())
#define OSS_STREAM(o)                           \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), OSS_TYPE_STREAM, OssStream))
#define OSS_IS_STREAM(o)                        \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OSS_TYPE_STREAM))
#define OSS_STREAM_CLASS(k)                     \
        (G_TYPE_CHECK_CLASS_CAST ((k), OSS_TYPE_STREAM, OssStreamClass))
#define OSS_IS_STREAM_CLASS(k)                  \
        (G_TYPE_CHECK_CLASS_TYPE ((k), OSS_TYPE_STREAM))
#define OSS_STREAM_GET_CLASS(o)                 \
        (G_TYPE_INSTANCE_GET_CLASS ((o), OSS_TYPE_STREAM, OssStreamClass))

typedef struct _OssStreamClass    OssStreamClass;
typedef struct _OssStreamPrivate  OssStreamPrivate;

struct _OssStream
{
    MateMixerStream parent;

    /*< private >*/
    OssStreamPrivate *priv;
};

struct _OssStreamClass
{
    MateMixerStreamClass parent;
};

GType             oss_stream_get_type            (void) G_GNUC_CONST;

OssStream *       oss_stream_new                 (const gchar       *name,
                                                  MateMixerDevice   *device,
                                                  MateMixerDirection direction);

void              oss_stream_add_control         (OssStream         *stream,
                                                  OssStreamControl  *control);

void              oss_stream_load                (OssStream         *stream);

gboolean          oss_stream_has_controls        (OssStream         *stream);
gboolean          oss_stream_has_default_control (OssStream         *stream);

OssStreamControl *oss_stream_get_default_control (OssStream         *stream);
void              oss_stream_set_default_control (OssStream         *stream,
                                                  OssStreamControl  *control);

void              oss_stream_set_switch_data     (OssStream         *stream,
                                                  gint               fd,
                                                  GList             *options);

void              oss_stream_remove_all          (OssStream         *stream);

G_END_DECLS

#endif /* OSS_STREAM_H */
