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

#ifndef MATEMIXER_PULSE_STREAM_H
#define MATEMIXER_PULSE_STREAM_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-stream.h>
#include <libmatemixer/matemixer-port.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_PULSE_STREAM            \
        (mate_mixer_pulse_stream_get_type ())
#define MATE_MIXER_PULSE_STREAM(o)              \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_PULSE_STREAM, MateMixerPulseStream))
#define MATE_MIXER_IS_PULSE_STREAM(o)           \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_PULSE_STREAM))
#define MATE_MIXER_PULSE_STREAM_CLASS(k)        \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_PULSE_STREAM, MateMixerPulseStreamClass))
#define MATE_MIXER_IS_PULSE_STREAM_CLASS(k)     \
        (G_TYPE_CLASS_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_PULSE_STREAM))
#define MATE_MIXER_PULSE_STREAM_GET_CLASS(o)    \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_PULSE_STREAM, MateMixerPulseStreamClass))

typedef struct _MateMixerPulseStream         MateMixerPulseStream;
typedef struct _MateMixerPulseStreamClass    MateMixerPulseStreamClass;
typedef struct _MateMixerPulseStreamPrivate  MateMixerPulseStreamPrivate;

struct _MateMixerPulseStream
{
    GObject parent;

    MateMixerPulseStreamPrivate *priv;
};

struct _MateMixerPulseStreamClass
{
    GObjectClass parent;

    gboolean (*set_volume) (MateMixerStream *stream, guint32 volume);
    gboolean (*set_mute)   (MateMixerStream *stream, gboolean mute);
};

GType mate_mixer_pulse_stream_get_type (void) G_GNUC_CONST;

MateMixerPulseConnection * mate_mixer_pulse_stream_get_connection  (MateMixerPulseStream *stream);
guint32                    mate_mixer_pulse_stream_get_index       (MateMixerPulseStream *stream);

/* Interface implementation */
const gchar *              mate_mixer_pulse_stream_get_name        (MateMixerStream      *stream);
const gchar *              mate_mixer_pulse_stream_get_description (MateMixerStream      *stream);
const gchar *              mate_mixer_pulse_stream_get_icon (MateMixerStream *stream);

MateMixerPort *            mate_mixer_pulse_stream_get_active_port (MateMixerStream *stream);
gboolean                   mate_mixer_pulse_stream_set_active_port (MateMixerStream *stream,
                                                                    MateMixerPort   *port);
const GList *              mate_mixer_pulse_stream_list_ports      (MateMixerStream      *stream);
gboolean                   mate_mixer_pulse_stream_set_mute        (MateMixerStream      *stream,
                                                                    gboolean              mute);
gboolean                   mate_mixer_pulse_stream_set_volume      (MateMixerStream      *stream,
                                                                    guint32               volume);

G_END_DECLS

#endif /* MATEMIXER_PULSE_STREAM_H */
