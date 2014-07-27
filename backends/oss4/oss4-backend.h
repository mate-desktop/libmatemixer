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

#ifndef OSS4_BACKEND_H
#define OSS4_BACKEND_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-backend-module.h>

#define OSS4_TYPE_BACKEND                       \
        (oss4_backend_get_type ())
#define OSS4_BACKEND(o)                         \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), OSS4_TYPE_BACKEND, Oss4Backend))
#define OSS4_IS_BACKEND(o)                      \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), OSS4_TYPE_BACKEND))
#define OSS4_BACKEND_CLASS(k)                   \
        (G_TYPE_CHECK_CLASS_CAST ((k), OSS4_TYPE_BACKEND, Oss4BackendClass))
#define OSS4_IS_BACKEND_CLASS(k)                \
        (G_TYPE_CHECK_CLASS_TYPE ((k), OSS4_TYPE_BACKEND))
#define OSS4_BACKEND_GET_CLASS(o)               \
        (G_TYPE_INSTANCE_GET_CLASS ((o), OSS4_TYPE_BACKEND, Oss4BackendClass))

typedef struct _Oss4Backend         Oss4Backend;
typedef struct _Oss4BackendClass    Oss4BackendClass;
typedef struct _Oss4BackendPrivate  Oss4BackendPrivate;

struct _Oss4Backend
{
    GObject parent;

    /*< private >*/
    Oss4BackendPrivate *priv;
};

struct _Oss4BackendClass
{
    GObjectClass parent_class;
};

GType                       oss4_backend_get_type   (void) G_GNUC_CONST;

/* Support function for dynamic loading of the backend module */
void                        backend_module_init     (GTypeModule *module);
const MateMixerBackendInfo *backend_module_get_info (void);

#endif /* OSS4_BACKEND_H */
