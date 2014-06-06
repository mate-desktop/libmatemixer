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

#ifndef MATEMIXER_PULSE_SINK_H
#define MATEMIXER_PULSE_SINK_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-stream.h>

#include <pulse/pulseaudio.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_PULSE_SINK            \
        (mate_mixer_pulse_sink_get_type ())
#define MATE_MIXER_PULSE_SINK(o)              \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_PULSE_SINK, MateMixerPulseSink))
#define MATE_MIXER_IS_PULSE_SINK(o)           \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_PULSE_SINK))
#define MATE_MIXER_PULSE_SINK_CLASS(k)        \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_PULSE_SINK, MateMixerPulseSinkClass))
#define MATE_MIXER_IS_PULSE_SINK_CLASS(k)     \
        (G_TYPE_CLASS_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_PULSE_SINK))
#define MATE_MIXER_PULSE_SINK_GET_CLASS(o)    \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_PULSE_SINK, MateMixerPulseSinkClass))

typedef struct _MateMixerPulseSink         MateMixerPulseSink;
typedef struct _MateMixerPulseSinkClass    MateMixerPulseSinkClass;
typedef struct _MateMixerPulseSinkPrivate  MateMixerPulseSinkPrivate;

struct _MateMixerPulseSink
{
    GObject parent;

    MateMixerPulseSinkPrivate *priv;
};

struct _MateMixerPulseSinkClass
{
    GObjectClass parent;
};

GType mate_mixer_pulse_sink_get_type (void) G_GNUC_CONST;

MateMixerPulseStream *mate_mixer_pulse_sink_new (MateMixerPulseConnection *connection,
                                                const pa_sink_info *info);

gboolean mate_mixer_pulse_sink_set_volume (MateMixerStream *stream, guint32 volume);
gboolean mate_mixer_pulse_sink_set_mute (MateMixerStream *stream, gboolean mute);

G_END_DECLS

#endif /* MATEMIXER_PULSE_SINK_H */
