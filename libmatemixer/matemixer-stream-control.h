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

#ifndef MATEMIXER_STREAM_CONTROL_H
#define MATEMIXER_STREAM_CONTROL_H

#include <math.h>
#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-enums.h>

G_BEGIN_DECLS

#ifdef INFINITY
#  define MATE_MIXER_INFINITY INFINITY
#else
#  define MATE_MIXER_INFINITY G_MAXDOUBLE
#endif

#define MATE_MIXER_TYPE_STREAM_CONTROL                  \
        (mate_mixer_stream_control_get_type ())
#define MATE_MIXER_STREAM_CONTROL(o)                    \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_STREAM_CONTROL, MateMixerStreamControl))
#define MATE_MIXER_IS_STREAM_CONTROL(o)                 \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_STREAM_CONTROL))
#define MATE_MIXER_STREAM_CONTROL_GET_INTERFACE(o)      \
        (G_TYPE_INSTANCE_GET_INTERFACE ((o), MATE_MIXER_TYPE_STREAM_CONTROL, MateMixerStreamControlInterface))

typedef struct _MateMixerStreamControl           MateMixerStreamControl; /* dummy object */
typedef struct _MateMixerStreamControlInterface  MateMixerStreamControlInterface;

struct _MateMixerStreamControlInterface
{
    GTypeInterface parent_iface;

    /*< private >*/
    const gchar *               (*get_name)               (MateMixerStreamControl   *ctl);
    const gchar *               (*get_description)        (MateMixerStreamControl   *ctl);

    gboolean                    (*set_mute)               (MateMixerStreamControl   *ctl,
                                                           gboolean                  mute);

    guint                       (*get_num_channels)       (MateMixerStreamControl   *ctl);

    gboolean                    (*set_volume)             (MateMixerStreamControl   *ctl,
                                                           guint                     volume);

    gdouble                     (*get_decibel)            (MateMixerStreamControl   *ctl);
    gboolean                    (*set_decibel)            (MateMixerStreamControl   *ctl,
                                                           gdouble                   decibel);

    gboolean                    (*has_channel_position)   (MateMixerStreamControl   *ctl,
                                                           MateMixerChannelPosition  position);
    MateMixerChannelPosition    (*get_channel_position)   (MateMixerStreamControl   *ctl,
                                                           guint                     channel);

    guint                       (*get_channel_volume)     (MateMixerStreamControl   *ctl,
                                                           guint                     channel);
    gboolean                    (*set_channel_volume)     (MateMixerStreamControl   *ctl,
                                                           guint                     channel,
                                                           guint                     volume);

    gdouble                     (*get_channel_decibel)    (MateMixerStreamControl   *ctl,
                                                           guint                     channel);
    gboolean                    (*set_channel_decibel)    (MateMixerStreamControl   *ctl,
                                                           guint                     channel,
                                                           gdouble                   decibel);

    gboolean                    (*set_balance)            (MateMixerStreamControl   *ctl,
                                                           gfloat                    balance);

    gboolean                    (*set_fade)               (MateMixerStreamControl   *ctl,
                                                           gfloat                    fade);

    guint                       (*get_min_volume)         (MateMixerStreamControl   *ctl);
    guint                       (*get_max_volume)         (MateMixerStreamControl   *ctl);
    guint                       (*get_normal_volume)      (MateMixerStreamControl   *ctl);
    guint                       (*get_base_volume)        (MateMixerStreamControl   *ctl);
};

GType                       mate_mixer_stream_control_get_type             (void) G_GNUC_CONST;

const gchar *               mate_mixer_stream_control_get_name             (MateMixerStreamControl   *ctl);
const gchar *               mate_mixer_stream_control_get_description      (MateMixerStreamControl   *ctl);
MateMixerStreamControlFlags mate_mixer_stream_control_get_flags            (MateMixerStreamControl   *ctl);

gboolean                    mate_mixer_stream_control_get_mute             (MateMixerStreamControl   *ctl);
gboolean                    mate_mixer_stream_control_set_mute             (MateMixerStreamControl   *ctl,
                                                                            gboolean                  mute);

guint                       mate_mixer_stream_control_get_num_channels     (MateMixerStreamControl   *ctl);

guint                       mate_mixer_stream_control_get_volume           (MateMixerStreamControl   *ctl);
gboolean                    mate_mixer_stream_control_set_volume           (MateMixerStreamControl   *ctl,
                                                                            guint                     volume);

gdouble                     mate_mixer_stream_control_get_decibel          (MateMixerStreamControl   *ctl);
gboolean                    mate_mixer_stream_control_set_decibel          (MateMixerStreamControl   *ctl,
                                                                            gdouble                   decibel);

gboolean                    mate_mixer_stream_control_has_channel_position (MateMixerStreamControl   *ctl,
                                                                            MateMixerChannelPosition  position);
MateMixerChannelPosition    mate_mixer_stream_control_get_channel_position (MateMixerStreamControl   *ctl,
                                                                            guint                     channel);

guint                       mate_mixer_stream_control_get_channel_volume   (MateMixerStreamControl   *ctl,
                                                                            guint                     channel);
gboolean                    mate_mixer_stream_control_set_channel_volume   (MateMixerStreamControl   *ctl,
                                                                            guint                     channel,
                                                                            guint                     volume);

gdouble                     mate_mixer_stream_control_get_channel_decibel  (MateMixerStreamControl   *ctl,
                                                                            guint                     channel);
gboolean                    mate_mixer_stream_control_set_channel_decibel  (MateMixerStreamControl   *ctl,
                                                                            guint                     channel,
                                                                            gdouble                   decibel);

gfloat                      mate_mixer_stream_control_get_balance          (MateMixerStreamControl   *ctl);
gboolean                    mate_mixer_stream_control_set_balance          (MateMixerStreamControl   *ctl,
                                                                            gfloat                    balance);

gfloat                      mate_mixer_stream_control_get_fade             (MateMixerStreamControl   *ctl);
gboolean                    mate_mixer_stream_control_set_fade             (MateMixerStreamControl   *ctl,
                                                                            gfloat                    fade);

guint                       mate_mixer_stream_control_get_min_volume       (MateMixerStreamControl   *ctl);
guint                       mate_mixer_stream_control_get_max_volume       (MateMixerStreamControl   *ctl);
guint                       mate_mixer_stream_control_get_normal_volume    (MateMixerStreamControl   *ctl);
guint                       mate_mixer_stream_control_get_base_volume      (MateMixerStreamControl   *ctl);

G_END_DECLS

#endif /* MATEMIXER_STREAM_CONTROL_H */
