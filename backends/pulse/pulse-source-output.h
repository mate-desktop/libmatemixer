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

#ifndef PULSE_SOURCE_OUTPUT_H
#define PULSE_SOURCE_OUTPUT_H

#include <glib.h>
#include <glib-object.h>

#include <pulse/pulseaudio.h>

#include "pulse-stream-control.h"
#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_SOURCE_OUTPUT                   \
        (pulse_source_output_get_type ())
#define PULSE_SOURCE_OUTPUT(o)                     \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_SOURCE_OUTPUT, PulseSourceOutput))
#define PULSE_IS_SOURCE_OUTPUT(o)                  \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_SOURCE_OUTPUT))
#define PULSE_SOURCE_OUTPUT_CLASS(k)               \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_SOURCE_OUTPUT, PulseSourceOutputClass))
#define PULSE_IS_SOURCE_OUTPUT_CLASS(k)            \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_SOURCE_OUTPUT))
#define PULSE_SOURCE_OUTPUT_GET_CLASS(o)           \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_SOURCE_OUTPUT, PulseSourceOutputClass))

typedef struct _PulseSourceOutputClass  PulseSourceOutputClass;

struct _PulseSourceOutput
{
    PulseStreamControl parent;
};

struct _PulseSourceOutputClass
{
    PulseStreamControlClass parent_class;
};

GType              pulse_source_output_get_type (void) G_GNUC_CONST;

PulseSourceOutput *pulse_source_output_new      (PulseConnection             *connection,
                                                 const pa_source_output_info *info,
                                                 PulseSource                 *source);

void               pulse_source_output_update   (PulseSourceOutput           *output,
                                                 const pa_source_output_info *info);

G_END_DECLS

#endif /* PULSE_SOURCE_OUTPUT_H */
