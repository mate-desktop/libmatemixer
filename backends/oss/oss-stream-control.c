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
#include "oss-stream.h"
#include "oss-stream-control.h"

#define LEFT_CHANNEL  0
#define RIGHT_CHANNEL 1

#define OSS_VOLUME_JOIN(left,right)   (((left) & 0xFF) | (((right) & 0xFF) << 8))

#define OSS_VOLUME_JOIN_SAME(volume)  (OSS_VOLUME_JOIN ((volume), (volume)))
#define OSS_VOLUME_JOIN_ARRAY(volume) (OSS_VOLUME_JOIN ((volume[LEFT_CHANNEL]), \
                                                        (volume[RIGHT_CHANNEL])))

#define OSS_VOLUME_TAKE_LEFT(volume)  ((volume) & 0xFF)
#define OSS_VOLUME_TAKE_RIGHT(volume) (((volume) >> 8) & 0xFF)

struct _OssStreamControlPrivate
{
    gint     fd;
    gint     devnum;
    guint8   volume[2];
    gboolean stereo;
};

static void oss_stream_control_class_init (OssStreamControlClass *klass);
static void oss_stream_control_init       (OssStreamControl      *control);
static void oss_stream_control_finalize   (GObject               *object);

G_DEFINE_TYPE (OssStreamControl, oss_stream_control, MATE_MIXER_TYPE_STREAM_CONTROL)

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

static void                     store_volume                            (OssStreamControl        *control,
                                                                         gint                     volume);

static void                     update_balance                          (OssStreamControl        *control);

static gboolean                 write_and_store_volume                  (OssStreamControl        *control,
                                                                         gint                     volume);

static void
oss_stream_control_class_init (OssStreamControlClass *klass)
{
    GObjectClass                *object_class;
    MateMixerStreamControlClass *control_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = oss_stream_control_finalize;

    control_class = MATE_MIXER_STREAM_CONTROL_CLASS (klass);
    control_class->get_num_channels     = oss_stream_control_get_num_channels;
    control_class->get_volume           = oss_stream_control_get_volume;
    control_class->set_volume           = oss_stream_control_set_volume;
    control_class->get_channel_volume   = oss_stream_control_get_channel_volume;
    control_class->set_channel_volume   = oss_stream_control_set_channel_volume;
    control_class->has_channel_position = oss_stream_control_has_channel_position;
    control_class->get_channel_position = oss_stream_control_get_channel_position;
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

    if (control->priv->fd != -1)
        close (control->priv->fd);

    G_OBJECT_CLASS (oss_stream_control_parent_class)->finalize (object);
}

OssStreamControl *
oss_stream_control_new (const gchar               *name,
                        const gchar               *label,
                        MateMixerStreamControlRole role,
                        OssStream                 *stream,
                        gint                       fd,
                        gint                       devnum,
                        gboolean                   stereo)
{
    OssStreamControl           *control;
    gint                        newfd;
    MateMixerStreamControlFlags flags;

    g_return_val_if_fail (name  != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (OSS_IS_STREAM (stream), NULL);

    newfd = dup (fd);
    if (newfd == -1) {
        g_warning ("Failed to duplicate file descriptor: %s",
                   g_strerror (errno));
        return NULL;
    }

    flags = MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE |
            MATE_MIXER_STREAM_CONTROL_VOLUME_WRITABLE;
    if (stereo == TRUE)
        flags |= MATE_MIXER_STREAM_CONTROL_CAN_BALANCE;

    control = g_object_new (OSS_TYPE_STREAM_CONTROL,
                            "name", name,
                            "label", label,
                            "flags", flags,
                            "role", role,
                            "stream", stream,
                            NULL);

    control->priv->fd = newfd;
    control->priv->devnum = devnum;
    control->priv->stereo = stereo;
    return control;
}

gint
oss_stream_control_get_devnum (OssStreamControl *control)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (control), 0);

    return control->priv->devnum;
}

void
oss_stream_control_load (OssStreamControl *control)
{
    gint v, ret;

    g_return_if_fail (OSS_IS_STREAM_CONTROL (control));

    if G_UNLIKELY (control->priv->fd == -1)
        return;

    ret = ioctl (control->priv->fd, MIXER_READ (control->priv->devnum), &v);
    if (ret == -1)
        return;

    store_volume (control, v);
}

void
oss_stream_control_close (OssStreamControl *control)
{
    g_return_if_fail (OSS_IS_STREAM_CONTROL (control));

    if (control->priv->fd == -1)
        return;

    close (control->priv->fd);
    control->priv->fd = -1;
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
        return MAX (control->priv->volume[LEFT_CHANNEL],
                    control->priv->volume[RIGHT_CHANNEL]);
    else
        return control->priv->volume[LEFT_CHANNEL];
}

static gboolean
oss_stream_control_set_volume (MateMixerStreamControl *mmsc, guint volume)
{
    OssStreamControl *control;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), FALSE);

    control = OSS_STREAM_CONTROL (mmsc);

    if G_UNLIKELY (control->priv->fd == -1)
        return FALSE;

    return write_and_store_volume (control, OSS_VOLUME_JOIN_SAME (CLAMP (volume, 0, 100)));
}

static guint
oss_stream_control_get_channel_volume (MateMixerStreamControl *mmsc, guint channel)
{
    OssStreamControl *control;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), 0);

    control = OSS_STREAM_CONTROL (mmsc);

    if (control->priv->stereo == TRUE) {
        if (channel == LEFT_CHANNEL || channel == RIGHT_CHANNEL)
            return control->priv->volume[channel];
    } else {
        if (channel == LEFT_CHANNEL)
            return control->priv->volume[LEFT_CHANNEL];
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

    if G_UNLIKELY (control->priv->fd == -1)
        return FALSE;

    if (channel != LEFT_CHANNEL &&
        (control->priv->stereo == FALSE || channel != RIGHT_CHANNEL))
        return FALSE;

    volume = CLAMP (volume, 0, 100);
    if (channel == LEFT_CHANNEL)
        v = OSS_VOLUME_JOIN (volume, control->priv->volume[RIGHT_CHANNEL]);
    else
        v = OSS_VOLUME_JOIN (control->priv->volume[LEFT_CHANNEL], volume);

    return write_and_store_volume (control, v);
}

static MateMixerChannelPosition
oss_stream_control_get_channel_position (MateMixerStreamControl *mmsc, guint channel)
{
    OssStreamControl *control;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), MATE_MIXER_CHANNEL_UNKNOWN);

    control = OSS_STREAM_CONTROL (mmsc);

    if (control->priv->stereo == TRUE) {
        if (channel == LEFT_CHANNEL)
            return MATE_MIXER_CHANNEL_FRONT_LEFT;
        else if (channel == RIGHT_CHANNEL)
            return MATE_MIXER_CHANNEL_FRONT_RIGHT;
    } else {
        if (channel == LEFT_CHANNEL)
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
    gint              volume[2];

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), FALSE);

    control = OSS_STREAM_CONTROL (mmsc);

    if G_UNLIKELY (control->priv->fd == -1)
        return FALSE;

    max = MAX (control->priv->volume[LEFT_CHANNEL],
               control->priv->volume[RIGHT_CHANNEL]);
    if (balance <= 0) {
        volume[RIGHT_CHANNEL] = (balance + 1.0f) * max;
        volume[LEFT_CHANNEL]  = max;
    } else {
        volume[LEFT_CHANNEL]  = (1.0f - balance) * max;
        volume[RIGHT_CHANNEL] = max;
    }
    return write_and_store_volume (control, OSS_VOLUME_JOIN_ARRAY (volume));
}

static guint
oss_stream_control_get_min_volume (MateMixerStreamControl *mmsc)
{
    return 0;
}

static guint
oss_stream_control_get_max_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), 0);

    return 100;
}

static guint
oss_stream_control_get_normal_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), 0);

    return 100;
}

static guint
oss_stream_control_get_base_volume (MateMixerStreamControl *mmsc)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (mmsc), 0);

    return 100;
}

static void
store_volume (OssStreamControl *control, gint volume)
{
    if (control->priv->stereo == TRUE) {
        if (volume == OSS_VOLUME_JOIN_ARRAY (control->priv->volume))
            return;

        control->priv->volume[LEFT_CHANNEL]  = OSS_VOLUME_TAKE_LEFT (volume);
        control->priv->volume[RIGHT_CHANNEL] = OSS_VOLUME_TAKE_RIGHT (volume);

        g_object_freeze_notify (G_OBJECT (control));

        g_object_notify (G_OBJECT (control), "volume");

        /* Emits signal if balance has changed */
        update_balance (control);

        g_object_thaw_notify (G_OBJECT (control));
    } else {
        volume = OSS_VOLUME_TAKE_LEFT (volume);
        if (volume == control->priv->volume[LEFT_CHANNEL])
            return;

        control->priv->volume[LEFT_CHANNEL] = volume;

        g_object_notify (G_OBJECT (control), "volume");
    }
}

static void
update_balance (OssStreamControl *control)
{
    gfloat balance;
    guint  left;
    guint  right;

    left  = control->priv->volume[LEFT_CHANNEL];
    right = control->priv->volume[RIGHT_CHANNEL];

    if (left == right)
        balance = 0.0f;
    else if (left > right)
        balance = -1.0f + ((gfloat) right / (gfloat) left);
    else
        balance = +1.0f - ((gfloat) left / (gfloat) right);

    _mate_mixer_stream_control_set_balance (MATE_MIXER_STREAM_CONTROL (control),
                                            balance);
}

static gboolean
write_and_store_volume (OssStreamControl *control, gint volume)
{
    gint ret;

    /* Nothing to do? */
    if (volume == OSS_VOLUME_JOIN_ARRAY (control->priv->volume))
        return TRUE;

    /* The ioctl might also change the passed volume */
    ret = ioctl (control->priv->fd, MIXER_WRITE (control->priv->devnum), &volume);
    if (ret == -1)
        return FALSE;

    store_volume (control, volume & 0xFFFF);
    return TRUE;
}
