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

#ifndef PULSE_SOURCE_CONTROL_H
#define PULSE_SOURCE_CONTROL_H

#include <glib.h>
#include <glib-object.h>

#include <pulse/pulseaudio.h>

#include "pulse-stream-control.h"
#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_SOURCE_CONTROL                 \
        (pulse_source_control_get_type ())
#define PULSE_SOURCE_CONTROL(o)                   \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_SOURCE_CONTROL, PulseSourceControl))
#define PULSE_IS_SOURCE_CONTROL(o)                \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_SOURCE_CONTROL))
#define PULSE_SOURCE_CONTROL_CLASS(k)             \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_SOURCE_CONTROL, PulseSourceControlClass))
#define PULSE_IS_SOURCE_CONTROL_CLASS(k)          \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_SOURCE_CONTROL))
#define PULSE_SOURCE_CONTROL_GET_CLASS(o)         \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_SOURCE_CONTROL, PulseSourceControlClass))

typedef struct _PulseSourceControlClass    PulseSourceControlClass;
typedef struct _PulseSourceControlPrivate  PulseSourceControlPrivate;

struct _PulseSourceControl
{
    PulseStreamControl parent;
};

struct _PulseSourceControlClass
{
    PulseStreamControlClass parent_class;
};

GType               pulse_source_control_get_type (void) G_GNUC_CONST;

PulseSourceControl *pulse_source_control_new      (PulseConnection      *connection,
                                                   const pa_source_info *info,
                                                   PulseSource          *source);

void                pulse_source_control_update   (PulseSourceControl   *control,
                                                   const pa_source_info *info);

G_END_DECLS

#endif /* PULSE_SOURCE_CONTROL_H */
