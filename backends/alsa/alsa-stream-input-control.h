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

#ifndef ALSA_STREAM_INPUT_CONTROL_H
#define ALSA_STREAM_INPUT_CONTROL_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "alsa-stream-control.h"
#include "alsa-types.h"

G_BEGIN_DECLS

#define ALSA_TYPE_STREAM_INPUT_CONTROL          \
        (alsa_stream_input_control_get_type ())
#define ALSA_STREAM_INPUT_CONTROL(o)            \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), ALSA_TYPE_STREAM_INPUT_CONTROL, AlsaStreamInputControl))
#define ALSA_IS_STREAM_INPUT_CONTROL(o)         \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ALSA_TYPE_STREAM_INPUT_CONTROL))
#define ALSA_STREAM_INPUT_CONTROL_CLASS(k)      \
        (G_TYPE_CHECK_CLASS_CAST ((k), ALSA_TYPE_STREAM_INPUT_CONTROL, AlsaStreamInputControlClass))
#define ALSA_IS_STREAM_INPUT_CONTROL_CLASS(k)   \
        (G_TYPE_CHECK_CLASS_TYPE ((k), ALSA_TYPE_STREAM_INPUT_CONTROL))
#define ALSA_STREAM_INPUT_CONTROL_GET_CLASS(o)  \
        (G_TYPE_INSTANCE_GET_CLASS ((o), ALSA_TYPE_STREAM_INPUT_CONTROL, AlsaStreamInputControlClass))

typedef struct _AlsaStreamInputControlClass    AlsaStreamInputControlClass;
typedef struct _AlsaStreamInputControlPrivate  AlsaStreamInputControlPrivate;

struct _AlsaStreamInputControl
{
    AlsaStreamControl parent;
};

struct _AlsaStreamInputControlClass
{
    AlsaStreamControlClass parent_class;
};

GType              alsa_stream_input_control_get_type (void) G_GNUC_CONST;

AlsaStreamControl *alsa_stream_input_control_new      (const gchar               *name,
                                                       const gchar               *label,
                                                       MateMixerStreamControlRole role,
                                                       AlsaStream                *stream);

G_END_DECLS

#endif /* ALSA_STREAM_INPUT_CONTROL_H */
