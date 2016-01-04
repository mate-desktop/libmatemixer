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

#include <glib.h>
#include <glib-object.h>
#include <alsa/asoundlib.h>

#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include "alsa-constants.h"
#include "alsa-element.h"
#include "alsa-stream-control.h"

struct _AlsaStreamControlPrivate
{
    AlsaControlData   data;
    guint32           channel_mask;
    snd_mixer_elem_t *element;
};

static void alsa_element_interface_init    (AlsaElementInterface   *iface);

static void alsa_stream_control_class_init (AlsaStreamControlClass *klass);
static void alsa_stream_control_init       (AlsaStreamControl      *control);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (AlsaStreamControl, alsa_stream_control,
                                  MATE_MIXER_TYPE_STREAM_CONTROL,
                                  G_IMPLEMENT_INTERFACE (ALSA_TYPE_ELEMENT,
                                                         alsa_element_interface_init))

static snd_mixer_elem_t *       alsa_stream_control_get_snd_element      (AlsaElement             *element);
static void                     alsa_stream_control_set_snd_element      (AlsaElement             *element,
                                                                          snd_mixer_elem_t        *el);

static gboolean                 alsa_stream_control_load                 (AlsaElement             *element);

static gboolean                 alsa_stream_control_set_mute             (MateMixerStreamControl  *mmsc,
                                                                          gboolean                 mute);

static guint                    alsa_stream_control_get_num_channels     (MateMixerStreamControl  *mmsc);

static guint                    alsa_stream_control_get_volume           (MateMixerStreamControl  *mmsc);

static gboolean                 alsa_stream_control_set_volume           (MateMixerStreamControl  *mmsc,
                                                                          guint                    volume);

static gdouble                  alsa_stream_control_get_decibel          (MateMixerStreamControl  *mmsc);

static gboolean                 alsa_stream_control_set_decibel          (MateMixerStreamControl  *mmsc,
                                                                          gdouble                  decibel);

static gboolean                 alsa_stream_control_has_channel_position (MateMixerStreamControl  *mmsc,
                                                                          MateMixerChannelPosition position);
static MateMixerChannelPosition alsa_stream_control_get_channel_position (MateMixerStreamControl  *mmsc,
                                                                          guint                    channel);

static guint                    alsa_stream_control_get_channel_volume   (MateMixerStreamControl  *mmsc,
                                                                          guint                    channel);
static gboolean                 alsa_stream_control_set_channel_volume   (MateMixerStreamControl  *mmsc,
                                                                          guint                    channel,
                                                                          guint                    volume);

static gdouble                  alsa_stream_control_get_channel_decibel  (MateMixerStreamControl  *mmsc,
                                                                          guint                    channel);
static gboolean                 alsa_stream_control_set_channel_decibel  (MateMixerStreamControl  *mmsc,
                                                                          guint                    channel,
                                                                          gdouble                  decibel);

static gboolean                 alsa_stream_control_set_balance          (MateMixerStreamControl  *mmsc,
                                                                          gfloat                   balance);

static gboolean                 alsa_stream_control_set_fade             (MateMixerStreamControl  *mmsc,
                                                                          gfloat                   fade);

static guint                    alsa_stream_control_get_min_volume       (MateMixerStreamControl  *mmsc);
static guint                    alsa_stream_control_get_max_volume       (MateMixerStreamControl  *mmsc);
static guint                    alsa_stream_control_get_normal_volume    (MateMixerStreamControl  *mmsc);
static guint                    alsa_stream_control_get_base_volume      (MateMixerStreamControl  *mmsc);

static void                     control_data_get_average_left_right      (AlsaControlData         *data,
                                                                          guint                   *left,
                                                                          guint                   *right);
static void                     control_data_get_average_front_back      (AlsaControlData         *data,
                                                                          guint                   *front,
                                                                          guint                   *back);

static gfloat                   control_data_get_balance                 (AlsaControlData         *data);
static gfloat                   control_data_get_fade                    (AlsaControlData         *data);

static void
alsa_element_interface_init (AlsaElementInterface *iface)
{
    iface->get_snd_element = alsa_stream_control_get_snd_element;
    iface->set_snd_element = alsa_stream_control_set_snd_element;
    iface->load            = alsa_stream_control_load;
}

static void
alsa_stream_control_class_init (AlsaStreamControlClass *klass)
{
    MateMixerStreamControlClass *control_class;

    control_class = MATE_MIXER_STREAM_CONTROL_CLASS (klass);

    control_class->set_mute             = alsa_stream_control_set_mute;
    control_class->get_num_channels     = alsa_stream_control_get_num_channels;
    control_class->get_volume           = alsa_stream_control_get_volume;
    control_class->set_volume           = alsa_stream_control_set_volume;
    control_class->get_decibel          = alsa_stream_control_get_decibel;
    control_class->set_decibel          = alsa_stream_control_set_decibel;
    control_class->has_channel_position = alsa_stream_control_has_channel_position;
    control_class->get_channel_position = alsa_stream_control_get_channel_position;
    control_class->get_channel_volume   = alsa_stream_control_get_channel_volume;
    control_class->set_channel_volume   = alsa_stream_control_set_channel_volume;
    control_class->get_channel_decibel  = alsa_stream_control_get_channel_decibel;
    control_class->set_channel_decibel  = alsa_stream_control_set_channel_decibel;
    control_class->set_balance          = alsa_stream_control_set_balance;
    control_class->set_fade             = alsa_stream_control_set_fade;
    control_class->get_min_volume       = alsa_stream_control_get_min_volume;
    control_class->get_max_volume       = alsa_stream_control_get_max_volume;
    control_class->get_normal_volume    = alsa_stream_control_get_normal_volume;
    control_class->get_base_volume      = alsa_stream_control_get_base_volume;

    g_type_class_add_private (G_OBJECT_CLASS (klass), sizeof (AlsaStreamControlPrivate));
}

static void
alsa_stream_control_init (AlsaStreamControl *control)
{
    control->priv = G_TYPE_INSTANCE_GET_PRIVATE (control,
                                                 ALSA_TYPE_STREAM_CONTROL,
                                                 AlsaStreamControlPrivate);
}

AlsaControlData *
alsa_stream_control_get_data (AlsaStreamControl *control)
{
    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (control), NULL);

    return &control->priv->data;
}

void
alsa_stream_control_set_data (AlsaStreamControl *control, AlsaControlData *data)
{
    MateMixerStreamControlFlags flags = MATE_MIXER_STREAM_CONTROL_NO_FLAGS;
    MateMixerStreamControl     *mmsc;
    gboolean                    mute = FALSE;

    g_return_if_fail (ALSA_IS_STREAM_CONTROL (control));
    g_return_if_fail (data != NULL);

    mmsc = MATE_MIXER_STREAM_CONTROL (control);

    control->priv->data = *data;

    g_object_freeze_notify (G_OBJECT (control));

    if (data->channels > 0) {
        if (data->switch_usable == TRUE) {
            /* If the mute switch is joined, all the channels get the same value,
             * otherwise the element has per-channel mute, which we don't support.
             * In that case, treat the control as unmuted if any channel is
             * unmuted. */
            if (data->channels == 1 || data->switch_joined == TRUE) {
                mute = data->m[0];
            } else {
                gint i;
                mute = TRUE;
                for (i = 0; i < data->channels; i++)
                    if (data->m[i] == FALSE) {
                        mute = FALSE;
                        break;
                    }
            }

            flags |= MATE_MIXER_STREAM_CONTROL_MUTE_READABLE;
            if (data->active == TRUE)
                flags |= MATE_MIXER_STREAM_CONTROL_MUTE_WRITABLE;
        }

        flags |= MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE;
        if (data->active == TRUE)
            flags |= MATE_MIXER_STREAM_CONTROL_VOLUME_WRITABLE;

        if (data->max_decibel > -MATE_MIXER_INFINITY)
            flags |= MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL;

        control->priv->channel_mask = _mate_mixer_create_channel_mask (data->c, data->channels);

        if (data->volume_joined == FALSE) {
            if (MATE_MIXER_CHANNEL_MASK_HAS_LEFT (control->priv->channel_mask) &&
                MATE_MIXER_CHANNEL_MASK_HAS_RIGHT (control->priv->channel_mask))
                flags |= MATE_MIXER_STREAM_CONTROL_CAN_BALANCE;

            if (MATE_MIXER_CHANNEL_MASK_HAS_FRONT (control->priv->channel_mask) &&
                MATE_MIXER_CHANNEL_MASK_HAS_BACK (control->priv->channel_mask))
                flags |= MATE_MIXER_STREAM_CONTROL_CAN_FADE;
        }

        g_object_notify (G_OBJECT (control), "volume");
    } else {
        control->priv->channel_mask = 0;
    }

    _mate_mixer_stream_control_set_mute (mmsc, mute);
    _mate_mixer_stream_control_set_flags (mmsc, flags);

    if (flags & MATE_MIXER_STREAM_CONTROL_CAN_BALANCE)
        _mate_mixer_stream_control_set_balance (mmsc, control_data_get_balance (data));
    if (flags & MATE_MIXER_STREAM_CONTROL_CAN_FADE)
        _mate_mixer_stream_control_set_fade (mmsc, control_data_get_fade (data));

    g_object_thaw_notify (G_OBJECT (control));
}

static snd_mixer_elem_t *
alsa_stream_control_get_snd_element (AlsaElement *element)
{
    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (element), NULL);

    return ALSA_STREAM_CONTROL (element)->priv->element;
}

static void
alsa_stream_control_set_snd_element (AlsaElement *element, snd_mixer_elem_t *el)
{
    g_return_if_fail (ALSA_IS_STREAM_CONTROL (element));

    ALSA_STREAM_CONTROL (element)->priv->element = el;
}

static gboolean
alsa_stream_control_load (AlsaElement *element)
{
    AlsaStreamControl *control;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (element), FALSE);

    control = ALSA_STREAM_CONTROL (element);

    return ALSA_STREAM_CONTROL_GET_CLASS (control)->load (control);
}

static gboolean
alsa_stream_control_set_mute (MateMixerStreamControl *mmsc, gboolean mute)
{
    AlsaStreamControl *control;
    gboolean           change = FALSE;
    gint               i;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), FALSE);

    control = ALSA_STREAM_CONTROL (mmsc);

    /* If the switch is joined, only verify the first channel */
    if (control->priv->data.switch_joined == TRUE) {
        if (control->priv->data.m[0] != mute)
            change = TRUE;
    } else {
        /* Avoid trying to set the mute if all channels are already at the
         * selected mute value */
        for (i = 0; i < control->priv->data.channels; i++)
            if (control->priv->data.m[i] != mute) {
                change = TRUE;
                break;
            }
    }

    if (change == TRUE) {
        AlsaStreamControlClass *klass;

        klass = ALSA_STREAM_CONTROL_GET_CLASS (control);
        if (klass->set_mute (control, mute) == FALSE)
            return FALSE;

        for (i = 0; i < control->priv->data.channels; i++)
            control->priv->data.m[i] = mute;
    }
    return TRUE;
}

static guint
alsa_stream_control_get_num_channels (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), 0);

    return ALSA_STREAM_CONTROL (mmsc)->priv->data.channels;
}

static guint
alsa_stream_control_get_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), 0);

    return ALSA_STREAM_CONTROL (mmsc)->priv->data.volume;
}

static gboolean
alsa_stream_control_set_volume (MateMixerStreamControl *mmsc, guint volume)
{
    AlsaStreamControl *control;
    gboolean           change = FALSE;
    gint               i;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), FALSE);

    control = ALSA_STREAM_CONTROL (mmsc);
    volume  = CLAMP (volume, control->priv->data.min, control->priv->data.max);

    /* If the volume is joined, only verify the first channel */
    if (control->priv->data.volume_joined == TRUE) {
        if (control->priv->data.v[0] != volume)
            change = TRUE;
    } else {
        /* Avoid trying to set the volume if all channels are already at the
         * selected volume */
        for (i = 0; i < control->priv->data.channels; i++)
            if (control->priv->data.v[i] != volume) {
                change = TRUE;
                break;
            }
    }

    if (change == TRUE) {
        AlsaStreamControlClass *klass;

        klass = ALSA_STREAM_CONTROL_GET_CLASS (control);
        if (klass->set_volume (control, volume) == FALSE)
            return FALSE;

        for (i = 0; i < control->priv->data.channels; i++)
            control->priv->data.v[i] = volume;

        control->priv->data.volume = volume;

        g_object_notify (G_OBJECT (control), "volume");
    }
    return TRUE;
}

static gdouble
alsa_stream_control_get_decibel (MateMixerStreamControl *mmsc)
{
    AlsaStreamControl      *control;
    AlsaStreamControlClass *klass;
    guint                   volume;
    gdouble                 decibel;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), -MATE_MIXER_INFINITY);

    control = ALSA_STREAM_CONTROL (mmsc);
    klass   = ALSA_STREAM_CONTROL_GET_CLASS (control);
    volume  = alsa_stream_control_get_volume (mmsc);

    if (klass->get_decibel_from_volume (control, volume, &decibel) == FALSE)
        return -MATE_MIXER_INFINITY;

    return decibel;
}

static gboolean
alsa_stream_control_set_decibel (MateMixerStreamControl *mmsc, gdouble decibel)
{
    AlsaStreamControl      *control;
    AlsaStreamControlClass *klass;
    guint                   volume;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), FALSE);

    control = ALSA_STREAM_CONTROL (mmsc);
    klass   = ALSA_STREAM_CONTROL_GET_CLASS (control);

    if (klass->get_volume_from_decibel (control, decibel, &volume) == FALSE)
        return FALSE;

    return alsa_stream_control_set_volume (mmsc, volume);
}

static gboolean
alsa_stream_control_has_channel_position (MateMixerStreamControl  *mmsc,
                                          MateMixerChannelPosition position)
{
    AlsaStreamControl *control;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), FALSE);

    control = ALSA_STREAM_CONTROL (mmsc);

    if MATE_MIXER_CHANNEL_MASK_HAS_CHANNEL (control->priv->channel_mask, position)
        return TRUE;
    else
        return FALSE;
}

static MateMixerChannelPosition
alsa_stream_control_get_channel_position (MateMixerStreamControl *mmsc, guint channel)
{
    AlsaStreamControl *control;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), MATE_MIXER_CHANNEL_UNKNOWN);

    control = ALSA_STREAM_CONTROL (mmsc);

    if (channel >= control->priv->data.channels)
        return MATE_MIXER_CHANNEL_UNKNOWN;

    return control->priv->data.c[channel];
}

static guint
alsa_stream_control_get_channel_volume (MateMixerStreamControl *mmsc, guint channel)
{
    AlsaStreamControl *control;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), 0);

    control = ALSA_STREAM_CONTROL (mmsc);

    if (channel >= control->priv->data.channels)
        return 0;

    return control->priv->data.v[channel];
}

static gboolean
alsa_stream_control_set_channel_volume (MateMixerStreamControl *mmsc, guint channel, guint volume)
{
    AlsaStreamControl *control;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), FALSE);

    control = ALSA_STREAM_CONTROL (mmsc);

    if (channel >= control->priv->data.channels)
        return FALSE;

    /* Set volume for all channels at once when channels are joined */
    if (control->priv->data.volume_joined == TRUE)
        return alsa_stream_control_set_volume (mmsc, volume);

    volume = CLAMP (volume, control->priv->data.min, control->priv->data.max);

    if (volume != control->priv->data.v[channel]) {
        AlsaStreamControlClass *klass;

        /* Convert channel index to ALSA channel position and make sure it is valid */
        snd_mixer_selem_channel_id_t c = alsa_channel_map_to[control->priv->data.c[channel]];
        if G_UNLIKELY (c == SND_MIXER_SCHN_UNKNOWN) {
            g_warn_if_reached ();
            return FALSE;
        }

        klass = ALSA_STREAM_CONTROL_GET_CLASS (control);
        if (klass->set_channel_volume (control, c, volume) == FALSE)
            return FALSE;

        control->priv->data.v[channel] = volume;

        /* The global volume is always set to the highest channel volume */
        control->priv->data.volume = MAX (control->priv->data.volume, volume);

        g_object_notify (G_OBJECT (control), "volume");
    }
    return TRUE;
}

static gdouble
alsa_stream_control_get_channel_decibel (MateMixerStreamControl *mmsc, guint channel)
{
    AlsaStreamControl      *control;
    AlsaStreamControlClass *klass;
    guint                   volume;
    gdouble                 decibel;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), -MATE_MIXER_INFINITY);

    control = ALSA_STREAM_CONTROL (mmsc);

    if (channel >= control->priv->data.channels)
        return -MATE_MIXER_INFINITY;

    klass  = ALSA_STREAM_CONTROL_GET_CLASS (control);
    volume = control->priv->data.v[channel];

    if (klass->get_decibel_from_volume (control, volume, &decibel) == FALSE)
        return -MATE_MIXER_INFINITY;

    return decibel;
}

static gboolean
alsa_stream_control_set_channel_decibel (MateMixerStreamControl *mmsc,
                                         guint                   channel,
                                         gdouble                 decibel)
{
    AlsaStreamControl      *control;
    AlsaStreamControlClass *klass;
    guint                   volume;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), FALSE);

    control = ALSA_STREAM_CONTROL (mmsc);
    klass   = ALSA_STREAM_CONTROL_GET_CLASS (control);

    if (klass->get_volume_from_decibel (control, decibel, &volume) == FALSE)
        return FALSE;

    return alsa_stream_control_set_channel_volume (mmsc, channel, volume);
}

static gboolean
alsa_stream_control_set_balance (MateMixerStreamControl *mmsc, gfloat balance)
{
    AlsaStreamControlClass *klass;
    AlsaStreamControl      *control;
    AlsaControlData        *data;
    guint                   left,
                            right;
    guint                   nleft,
                            nright;
    guint                   max;
    guint                   channel;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), FALSE);

    control = ALSA_STREAM_CONTROL (mmsc);
    klass   = ALSA_STREAM_CONTROL_GET_CLASS (control);

    data = &control->priv->data;
    control_data_get_average_left_right (data, &left, &right);

    max = MAX (left, right);
    if (balance <= 0) {
        nright = (balance + 1.0f) * max;
        nleft  = max;
    } else {
        nleft  = (1.0f - balance) * max;
        nright = max;
    }

    for (channel = 0; channel < data->channels; channel++) {
        gboolean lc = MATE_MIXER_IS_LEFT_CHANNEL (data->c[channel]);
        gboolean rc = MATE_MIXER_IS_RIGHT_CHANNEL (data->c[channel]);

        if (lc == TRUE || rc == TRUE) {
            guint volume;
            if (lc == TRUE) {
                if (left == 0)
                    volume = nleft;
                else
                    volume = CLAMP (((guint64) data->v[channel] * (guint64) nleft) / (guint64) left,
                                    data->min,
                                    data->max);
            } else {
                if (right == 0)
                    volume = nright;
                else
                    volume = CLAMP (((guint64) data->v[channel] * (guint64) nright) / (guint64) right,
                                    data->min,
                                    data->max);
            }

            if (klass->set_channel_volume (control,
                                           alsa_channel_map_to[data->c[channel]],
                                           volume) == TRUE)
                data->v[channel] = volume;
        }
    }
    return TRUE;
}

static gboolean
alsa_stream_control_set_fade (MateMixerStreamControl *mmsc, gfloat fade)
{
    AlsaStreamControlClass *klass;
    AlsaStreamControl      *control;
    AlsaControlData        *data;
    guint                   front,
                            back;
    guint                   nfront,
                            nback;
    guint                   max;
    guint                   channel;

    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), FALSE);

    control = ALSA_STREAM_CONTROL (mmsc);
    klass   = ALSA_STREAM_CONTROL_GET_CLASS (control);

    data = &control->priv->data;
    control_data_get_average_front_back (data, &front, &back);

    max = MAX (front, back);
    if (fade <= 0) {
        nback  = (fade + 1.0f) * max;
        nfront = max;
    } else {
        nfront = (1.0f - fade) * max;
        nback  = max;
    }

    for (channel = 0; channel < data->channels; channel++) {
        gboolean fc = MATE_MIXER_IS_FRONT_CHANNEL (data->c[channel]);
        gboolean bc = MATE_MIXER_IS_BACK_CHANNEL (data->c[channel]);

        if (fc == TRUE || bc == TRUE) {
            guint volume;
            if (fc == TRUE) {
                if (front == 0)
                    volume = nfront;
                else
                    volume = CLAMP (((guint64) data->v[channel] * (guint64) nfront) / (guint64) front,
                                    data->min,
                                    data->max);
            } else {
                if (back == 0)
                    volume = nback;
                else
                    volume = CLAMP (((guint64) data->v[channel] * (guint64) nback) / (guint64) back,
                                    data->min,
                                    data->max);
            }

            if (klass->set_channel_volume (control,
                                           alsa_channel_map_to[data->c[channel]],
                                           volume) == TRUE)
                data->v[channel] = volume;
        }
    }
    return TRUE;
}

static guint
alsa_stream_control_get_min_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), 0);

    return ALSA_STREAM_CONTROL (mmsc)->priv->data.min;
}

static guint
alsa_stream_control_get_max_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), 0);

    return ALSA_STREAM_CONTROL (mmsc)->priv->data.max;
}

static guint
alsa_stream_control_get_normal_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), 0);

    return ALSA_STREAM_CONTROL (mmsc)->priv->data.max;
}

static guint
alsa_stream_control_get_base_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (ALSA_IS_STREAM_CONTROL (mmsc), 0);

    return ALSA_STREAM_CONTROL (mmsc)->priv->data.max;
}

static void
control_data_get_average_left_right (AlsaControlData *data, guint *left, guint *right)
{
    guint l = 0,
          r = 0;
    guint nl = 0,
          nr = 0;
    guint channel;

    for (channel = 0; channel < data->channels; channel++)
        if MATE_MIXER_IS_LEFT_CHANNEL (data->c[channel]) {
            l += data->v[channel];
            nl++;
        }
        else if MATE_MIXER_IS_RIGHT_CHANNEL (data->c[channel]) {
            r += data->v[channel];
            nr++;
        }

    *left  = (nl > 0) ? l / nl : data->max;
    *right = (nr > 0) ? r / nr : data->max;
}

static void
control_data_get_average_front_back (AlsaControlData *data, guint *front, guint *back)
{
    guint f = 0,
          b = 0;
    guint nf = 0,
          nb = 0;
    guint channel;

    for (channel = 0; channel < data->channels; channel++)
        if MATE_MIXER_IS_FRONT_CHANNEL (data->c[channel]) {
            f += data->v[channel];
            nf++;
        }
        else if MATE_MIXER_IS_BACK_CHANNEL (data->c[channel]) {
            b += data->v[channel];
            nb++;
        }

    *front = (nf > 0) ? f / nf : data->max;
    *back  = (nb > 0) ? b / nb : data->max;
}

static gfloat
control_data_get_balance (AlsaControlData *data)
{
    guint left;
    guint right;

    control_data_get_average_left_right (data, &left, &right);
    if (left == right)
        return 0.0f;

    if (left > right)
        return -1.0f + ((gfloat) right / (gfloat) left);
    else
        return +1.0f - ((gfloat) left / (gfloat) right);
}

static gfloat
control_data_get_fade (AlsaControlData *data)
{
    guint front;
    guint back;

    control_data_get_average_front_back (data, &front, &back);
    if (front == back)
        return 0.0f;

    if (front > back)
        return -1.0f + ((gfloat) back / (gfloat) front);
    else
        return +1.0f - ((gfloat) front / (gfloat) back);
}
