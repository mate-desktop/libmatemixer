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

#ifndef PULSE_SOURCE_H
#define PULSE_SOURCE_H

#include <glib.h>
#include <glib-object.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-device.h"
#include "pulse-stream.h"

G_BEGIN_DECLS

#define PULSE_TYPE_SOURCE                       \
        (pulse_source_get_type ())
#define PULSE_SOURCE(o)                         \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_SOURCE, PulseSource))
#define PULSE_IS_SOURCE(o)                      \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_SOURCE))
#define PULSE_SOURCE_CLASS(k)                   \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_SOURCE, PulseSourceClass))
#define PULSE_IS_SOURCE_CLASS(k)                \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_SOURCE))
#define PULSE_SOURCE_GET_CLASS(o)               \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_SOURCE, PulseSourceClass))

typedef struct _PulseSource         PulseSource;
typedef struct _PulseSourceClass    PulseSourceClass;

struct _PulseSource
{
    PulseStream parent;
};

struct _PulseSourceClass
{
    PulseStreamClass parent_class;
};

GType        pulse_source_get_type   (void) G_GNUC_CONST;

PulseStream *pulse_source_new        (PulseConnection      *connection,
                                      const pa_source_info *info,
                                      PulseDevice          *device);

gboolean     pulse_source_update     (PulseStream          *pstream,
                                      const pa_source_info *info,
                                      PulseDevice          *device);

G_END_DECLS

#endif /* PULSE_SOURCE_H */
