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

#ifndef OSS_BACKEND_H
#define OSS_BACKEND_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include "oss-types.h"

#define OSS_TYPE_BACKEND                        \
        (oss_backend_get_type ())
#define OSS_BACKEND(o)                          \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), OSS_TYPE_BACKEND, OssBackend))
#define OSS_IS_BACKEND(o)                       \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OSS_TYPE_BACKEND))
#define OSS_BACKEND_CLASS(k)                    \
        (G_TYPE_CHECK_CLASS_CAST ((k), OSS_TYPE_BACKEND, OssBackendClass))
#define OSS_IS_BACKEND_CLASS(k)                 \
        (G_TYPE_CHECK_CLASS_TYPE ((k), OSS_TYPE_BACKEND))
#define OSS_BACKEND_GET_CLASS(o)                \
        (G_TYPE_INSTANCE_GET_CLASS ((o), OSS_TYPE_BACKEND, OssBackendClass))

typedef struct _OssBackendClass    OssBackendClass;
typedef struct _OssBackendPrivate  OssBackendPrivate;

struct _OssBackend
{
    MateMixerBackend parent;

    /*< private >*/
    OssBackendPrivate *priv;
};

struct _OssBackendClass
{
    MateMixerBackendClass parent_class;
};

GType                       oss_backend_get_type    (void) G_GNUC_CONST;

/* Support function for dynamic loading of the backend module */
void                        backend_module_init     (GTypeModule *module);
const MateMixerBackendInfo *backend_module_get_info (void);

#endif /* OSS_BACKEND_H */
