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

#ifndef ALSA_STREAM_CONTROL_H
#define ALSA_STREAM_CONTROL_H

#include <glib.h>
#include <glib-object.h>
#include <alsa/asoundlib.h>
#include <libmatemixer/matemixer.h>

#include "alsa-types.h"

G_BEGIN_DECLS

typedef struct {
    gboolean                 active;
    MateMixerChannelPosition c[MATE_MIXER_CHANNEL_MAX];
    guint                    v[MATE_MIXER_CHANNEL_MAX];
    gboolean                 m[MATE_MIXER_CHANNEL_MAX];
    guint                    volume;
    gboolean                 volume_joined;
    gboolean                 switch_usable;
    gboolean                 switch_joined;
    guint                    min;
    guint                    max;
    gdouble                  min_decibel;
    gdouble                  max_decibel;
    guint                    channels;
} AlsaControlData;

#define ALSA_TYPE_STREAM_CONTROL                \
        (alsa_stream_control_get_type ())
#define ALSA_STREAM_CONTROL(o)                  \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), ALSA_TYPE_STREAM_CONTROL, AlsaStreamControl))
#define ALSA_IS_STREAM_CONTROL(o)               \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ALSA_TYPE_STREAM_CONTROL))
#define ALSA_STREAM_CONTROL_CLASS(k)            \
        (G_TYPE_CHECK_CLASS_CAST ((k), ALSA_TYPE_STREAM_CONTROL, AlsaStreamControlClass))
#define ALSA_IS_STREAM_CONTROL_CLASS(k)         \
        (G_TYPE_CHECK_CLASS_TYPE ((k), ALSA_TYPE_STREAM_CONTROL))
#define ALSA_STREAM_CONTROL_GET_CLASS(o)        \
        (G_TYPE_INSTANCE_GET_CLASS ((o), ALSA_TYPE_STREAM_CONTROL, AlsaStreamControlClass))

typedef struct _AlsaStreamControlClass    AlsaStreamControlClass;
typedef struct _AlsaStreamControlPrivate  AlsaStreamControlPrivate;

struct _AlsaStreamControl
{
    MateMixerStreamControl parent;

    /*< private >*/
    AlsaStreamControlPrivate *priv;

};

struct _AlsaStreamControlClass
{
    MateMixerStreamControlClass parent_class;

    /*< private >*/
    gboolean (*load)                    (AlsaStreamControl           *control);

    gboolean (*set_mute)                (AlsaStreamControl           *control,
                                         gboolean                     mute);

    gboolean (*set_volume)              (AlsaStreamControl           *control,
                                         guint                        volume);

    gboolean (*set_channel_volume)      (AlsaStreamControl           *control,
                                         snd_mixer_selem_channel_id_t channel,
                                         guint                        volume);

    gboolean (*get_volume_from_decibel) (AlsaStreamControl           *control,
                                         gdouble                      decibel,
                                         guint                       *volume);

    gboolean (*get_decibel_from_volume) (AlsaStreamControl           *control,
                                         guint                        volume,
                                         gdouble                     *decibel);
};

GType              alsa_stream_control_get_type        (void) G_GNUC_CONST;

AlsaControlData *  alsa_stream_control_get_data        (AlsaStreamControl *control);

void               alsa_stream_control_set_data        (AlsaStreamControl *control,
                                                        AlsaControlData   *data);

G_END_DECLS

#endif /* ALSA_STREAM_CONTROL_H */
