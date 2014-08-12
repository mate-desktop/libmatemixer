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

#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>

#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include "oss-common.h"
#include "oss-stream-control.h"

struct _OssStreamControlPrivate
{
    gint                        fd;
    gint                        devnum;
    guint                       volume[2];
    gfloat                      balance;
    gboolean                    stereo;
    MateMixerStreamControlRole  role;
    MateMixerStreamControlFlags flags;
};

static void oss_stream_control_class_init (OssStreamControlClass *klass);

static void oss_stream_control_init       (OssStreamControl      *control);
static void oss_stream_control_finalize   (GObject               *object);

G_DEFINE_TYPE (OssStreamControl, oss_stream_control, MATE_MIXER_TYPE_STREAM_CONTROL)

static gboolean                 oss_stream_control_set_mute             (MateMixerStreamControl  *mmsc,
                                                                         gboolean                 mute);

static guint                    oss_stream_control_get_num_channels     (MateMixerStreamControl  *mmsc);

static guint                    oss_stream_control_get_volume           (MateMixerStreamControl  *mmsc);

static gboolean                 oss_stream_control_set_volume           (MateMixerStreamControl  *mmsc,
                                                                         guint                    volume);

static gboolean                 oss_stream_control_has_channel_position (MateMixerStreamControl  *mmsc,
                                                                         MateMixerChannelPosition position);
static MateMixerChannelPosition oss_stream_control_get_channel_position (MateMixerStreamControl  *mmsc,
                                                                         guint                    channel);

static guint                    oss_stream_control_get_channel_volume   (MateMixerStreamControl  *mmsc,
                                                                         guint                    channel);
static gboolean                 oss_stream_control_set_channel_volume   (MateMixerStreamControl  *mmsc,
                                                                         guint                    channel,
                                                                         guint                    volume);

static gboolean                 oss_stream_control_set_balance          (MateMixerStreamControl  *mmsc,
                                                                         gfloat                   balance);

static guint                    oss_stream_control_get_min_volume       (MateMixerStreamControl  *mmsc);
static guint                    oss_stream_control_get_max_volume       (MateMixerStreamControl  *mmsc);
static guint                    oss_stream_control_get_normal_volume    (MateMixerStreamControl  *mmsc);
static guint                    oss_stream_control_get_base_volume      (MateMixerStreamControl  *mmsc);

static gboolean                 write_volume                            (OssStreamControl        *control,
                                                                         gint                     volume);

static void
oss_stream_control_class_init (OssStreamControlClass *klass)
{
    GObjectClass                *object_class;
    MateMixerStreamControlClass *control_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = oss_stream_control_finalize;

    control_class = MATE_MIXER_STREAM_CONTROL_CLASS (klass);
    control_class->set_mute             = oss_stream_control_set_mute;
    control_class->get_num_channels     = oss_stream_control_get_num_channels;
    control_class->get_volume           = oss_stream_control_get_volume;
    control_class->set_volume           = oss_stream_control_set_volume;
    control_class->get_channel_volume   = oss_stream_control_get_channel_volume;
    control_class->set_channel_volume   = oss_stream_control_set_channel_volume;
    control_class->get_channel_position = oss_stream_control_get_channel_position;
    control_class->has_channel_position = oss_stream_control_has_channel_position;
    control_class->set_balance          = oss_stream_control_set_balance;
    control_class->get_min_volume       = oss_stream_control_get_min_volume;
    control_class->get_max_volume       = oss_stream_control_get_max_volume;
    control_class->get_normal_volume    = oss_stream_control_get_normal_volume;
    control_class->get_base_volume      = oss_stream_control_get_base_volume;

    g_type_class_add_private (object_class, sizeof (OssStreamControlPrivate));
}

static void
oss_stream_control_init (OssStreamControl *control)
{
    control->priv = G_TYPE_INSTANCE_GET_PRIVATE (control,
                                                 OSS_TYPE_STREAM_CONTROL,
                                                 OssStreamControlPrivate);
}

static void
oss_stream_control_finalize (GObject *object)
{
    OssStreamControl *control;

    control = OSS_STREAM_CONTROL (object);

    close (control->priv->fd);

    G_OBJECT_CLASS (oss_stream_control_parent_class)->finalize (object);
}

OssStreamControl *
oss_stream_control_new (const gchar               *name,
                        const gchar               *label,
                        MateMixerStreamControlRole role,
                        gint                       fd,
                        gint                       devnum,
                        gboolean                   stereo)
{
    OssStreamControl           *control;
    MateMixerStreamControlFlags flags;

    flags = MATE_MIXER_STREAM_CONTROL_HAS_VOLUME |
            MATE_MIXER_STREAM_CONTROL_CAN_SET_VOLUME;
    if (stereo == TRUE)
        flags |= MATE_MIXER_STREAM_CONTROL_CAN_BALANCE;

    control = g_object_new (OSS_TYPE_STREAM_CONTROL,
                            "name", name,
                            "label", label,
                            "flags", flags,
                            NULL);

    control->priv->fd = fd;
    control->priv->devnum = devnum;
    control->priv->stereo = stereo;
    return control;
}

gboolean
oss_stream_control_update (OssStreamControl *control)
{
    gint v;
    gint ret;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (control), FALSE);

    ret = ioctl (control->priv->fd, MIXER_READ (control->priv->devnum), &v);
    if (ret < 0) {
        g_warning ("Failed to read volume: %s", g_strerror (errno));
        return FALSE;
    }

    control->priv->volume[0] = v & 0xFF;

    if (control->priv->stereo == TRUE) {
        gfloat balance;
        guint  left;
        guint  right;

        control->priv->volume[1] = (v >> 8) & 0xFF;

        /* Calculate balance */
        left  = control->priv->volume[0];
        right = control->priv->volume[1];
        if (left == right)
            balance = 0.0f;
        else if (left > right)
            balance = -1.0f + ((gfloat) right / (gfloat) left);
        else
            balance = +1.0f - ((gfloat) left / (gfloat) right);

        _mate_mixer_stream_control_set_balance (MATE_MIXER_STREAM_CONTROL (control),
                                                balance);
    }
    return TRUE;
}

static gboolean
oss_stream_control_set_mute (MateMixerStreamControl *mmsc, gboolean mute)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), FALSE);

    // TODO
    return TRUE;
}

static guint
oss_stream_control_get_num_channels (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), 0);

    return (OSS_STREAM_CONTROL (mmsc)->priv->stereo == TRUE) ? 2 : 1;
}

static guint
oss_stream_control_get_volume (MateMixerStreamControl *mmsc)
{
    OssStreamControl *control;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), 0);

    control = OSS_STREAM_CONTROL (mmsc);

    if (control->priv->stereo == TRUE)
        return MAX (control->priv->volume[0], control->priv->volume[1]);
    else
        return control->priv->volume[0];
}

static gboolean
oss_stream_control_set_volume (MateMixerStreamControl *mmsc, guint volume)
{
    OssStreamControl *control;
    gint              v;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), FALSE);

    control = OSS_STREAM_CONTROL (mmsc);

    v = CLAMP (volume, 0, 100);
    if (control->priv->stereo == TRUE)
        v |= (volume & 0xFF) << 8;

    return write_volume (control, v);
}

static guint
oss_stream_control_get_channel_volume (MateMixerStreamControl *mmsc, guint channel)
{
    OssStreamControl *control;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), 0);

    control = OSS_STREAM_CONTROL (mmsc);

    if (control->priv->stereo == TRUE) {
        if (channel == 0 || channel == 1)
            return control->priv->volume[channel];
    } else {
        if (channel == 0)
            return control->priv->volume[0];
    }
    return 0;
}

static gboolean
oss_stream_control_set_channel_volume (MateMixerStreamControl *mmsc,
                                       guint                   channel,
                                       guint                   volume)
{
    OssStreamControl *control;
    gint              v;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), FALSE);

    control = OSS_STREAM_CONTROL (mmsc);
    volume  = CLAMP (volume, 0, 100);

    if (control->priv->stereo == TRUE) {
        if (channel > 1)
            return FALSE;

        /* Stereo channel volume - left channel is in the lowest 8 bits and
         * right channel is in the higher 8 bits */
        if (channel == 0)
            v = volume | (control->priv->volume[1] << 8);
        else
            v = control->priv->volume[0] | (volume << 8);
    } else {
        if (channel > 0)
            return FALSE;

        /* Single channel volume - only lowest 8 bits are used */
        v = volume;
    }

    return write_volume (control, v);
}

static MateMixerChannelPosition
oss_stream_control_get_channel_position (MateMixerStreamControl *mmsc, guint channel)
{
    OssStreamControl *control;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), MATE_MIXER_CHANNEL_UNKNOWN);

    control = OSS_STREAM_CONTROL (mmsc);

    if (control->priv->stereo == TRUE) {
        if (channel == 0)
            return MATE_MIXER_CHANNEL_FRONT_LEFT;
        else if (channel == 1)
            return MATE_MIXER_CHANNEL_FRONT_RIGHT;
    } else {
        if (channel == 0)
            return MATE_MIXER_CHANNEL_MONO;
    }
    return MATE_MIXER_CHANNEL_UNKNOWN;
}

static gboolean
oss_stream_control_has_channel_position (MateMixerStreamControl  *mmsc,
                                         MateMixerChannelPosition position)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), FALSE);

    if (position == MATE_MIXER_CHANNEL_MONO)
        return OSS_STREAM_CONTROL (mmsc)->priv->stereo == FALSE;

    if (position == MATE_MIXER_CHANNEL_FRONT_LEFT ||
        position == MATE_MIXER_CHANNEL_FRONT_RIGHT)
        return OSS_STREAM_CONTROL (mmsc)->priv->stereo == TRUE;

    return FALSE;
}

static gboolean
oss_stream_control_set_balance (MateMixerStreamControl *mmsc, gfloat balance)
{
    OssStreamControl *control;
    guint             max;
    gint              v;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), FALSE);

    control = OSS_STREAM_CONTROL (mmsc);

    max = MAX (control->priv->volume[0], control->priv->volume[1]);
    if (balance <= 0) {
        control->priv->volume[1] = (balance + 1.0f) * max;
        control->priv->volume[0] = max;
    } else {
        control->priv->volume[0] = (1.0f - balance) * max;
        control->priv->volume[1] = max;
    }

    v = control->priv->volume[0] | (control->priv->volume[1] << 8);

    return write_volume (control, v);
}

static guint
oss_stream_control_get_min_volume (MateMixerStreamControl *mmsc)
{
    return 0;
}

static guint
oss_stream_control_get_max_volume (MateMixerStreamControl *mmsc)
{
    return 100;
}

static guint
oss_stream_control_get_normal_volume (MateMixerStreamControl *mmsc)
{
    return 100;
}

static guint
oss_stream_control_get_base_volume (MateMixerStreamControl *mmsc)
{
    return 100;
}

static gboolean
write_volume (OssStreamControl *control, gint volume)
{
    gint ret;

    ret = ioctl (control->priv->fd, MIXER_WRITE (control->priv->devnum), &volume);
    if (ret < 0) {
        g_warning ("Failed to set volume: %s", g_strerror (errno));
        return FALSE;
    }
    return TRUE;
}
