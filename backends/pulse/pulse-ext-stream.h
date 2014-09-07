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

#ifndef PULSE_EXT_STREAM_H
#define PULSE_EXT_STREAM_H

#include <glib.h>
#include <glib-object.h>

#include <pulse/pulseaudio.h>
#include <pulse/ext-stream-restore.h>

#include "pulse-stream-control.h"
#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_EXT_STREAM                   \
        (pulse_ext_stream_get_type ())
#define PULSE_EXT_STREAM(o)                     \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_EXT_STREAM, PulseExtStream))
#define PULSE_IS_EXT_STREAM(o)                  \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_EXT_STREAM))
#define PULSE_EXT_STREAM_CLASS(k)               \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_EXT_STREAM, PulseExtStreamClass))
#define PULSE_IS_EXT_STREAM_CLASS(k)            \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_EXT_STREAM))
#define PULSE_EXT_STREAM_GET_CLASS(o)           \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_EXT_STREAM, PulseExtStreamClass))

typedef struct _PulseExtStreamClass    PulseExtStreamClass;
typedef struct _PulseExtStreamPrivate  PulseExtStreamPrivate;

struct _PulseExtStream
{
    MateMixerStoredControl parent;

    /*< private >*/
    PulseExtStreamPrivate *priv;
};

struct _PulseExtStreamClass
{
    MateMixerStoredControlClass parent_class;
};

GType           pulse_ext_stream_get_type (void) G_GNUC_CONST;

PulseExtStream *pulse_ext_stream_new      (PulseConnection                  *connection,
                                           const pa_ext_stream_restore_info *info,
                                           PulseStream                      *parent);

void            pulse_ext_stream_update   (PulseExtStream                   *ext,
                                           const pa_ext_stream_restore_info *info,
                                           PulseStream                      *parent);

G_END_DECLS

#endif /* PULSE_EXT_STREAM_H */
