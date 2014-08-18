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

#ifndef PULSE_BACKEND_H
#define PULSE_BACKEND_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include "pulse-types.h"

#define PULSE_TYPE_BACKEND                      \
        (pulse_backend_get_type ())
#define PULSE_BACKEND(o)                        \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_BACKEND, PulseBackend))
#define PULSE_IS_BACKEND(o)                     \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_BACKEND))
#define PULSE_BACKEND_CLASS(k)                  \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_BACKEND, PulseBackendClass))
#define PULSE_IS_BACKEND_CLASS(k)               \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_BACKEND))
#define PULSE_BACKEND_GET_CLASS(o)              \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_BACKEND, PulseBackendClass))

typedef struct _PulseBackendClass    PulseBackendClass;
typedef struct _PulseBackendPrivate  PulseBackendPrivate;

struct _PulseBackend
{
    MateMixerBackend parent;

    /*< private >*/
    PulseBackendPrivate *priv;
};

struct _PulseBackendClass
{
    MateMixerBackendClass parent_class;
};

GType                       pulse_backend_get_type  (void) G_GNUC_CONST;

/* Support function for dynamic loading of the backend module */
void                        backend_module_init     (GTypeModule *module);
const MateMixerBackendInfo *backend_module_get_info (void);

#endif /* PULSE_BACKEND_H */
