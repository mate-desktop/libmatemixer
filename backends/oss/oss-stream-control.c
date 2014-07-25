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

#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-port.h>
#include <libmatemixer/matemixer-stream-control.h>

#include "oss-common.h"
#include "oss-stream-control.h"

struct _OssStreamControlPrivate
{
    gint                        fd;
    gint                        dev_number;
    gchar                      *name;
    gchar                      *description;
    guint                       volume[2];
    gfloat                      balance;
    gboolean                    stereo;
    MateMixerPort              *port;
    MateMixerStreamControlRole  role;
    MateMixerStreamControlFlags flags;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_FLAGS,
    PROP_MUTE,
    PROP_VOLUME,
    PROP_BALANCE,
    PROP_FADE,
    PROP_FD,
    PROP_DEV_NUMBER,
    PROP_STEREO
};

static void mate_mixer_stream_control_interface_init (MateMixerStreamControlInterface *iface);

static void oss_stream_control_class_init   (OssStreamControlClass *klass);

static void oss_stream_control_get_property (GObject               *object,
                                             guint                  param_id,
                                             GValue                *value,
                                             GParamSpec            *pspec);
static void oss_stream_control_set_property (GObject               *object,
                                             guint                  param_id,
                                             const GValue          *value,
                                             GParamSpec            *pspec);

static void oss_stream_control_init         (OssStreamControl      *octl);
static void oss_stream_control_finalize     (GObject               *object);

G_DEFINE_TYPE_WITH_CODE (OssStreamControl, oss_stream_control, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_STREAM_CONTROL,
                                                mate_mixer_stream_control_interface_init))

static const gchar *            oss_stream_control_get_name             (MateMixerStreamControl  *ctl);
static const gchar *            oss_stream_control_get_description      (MateMixerStreamControl  *ctl);

static guint                    oss_stream_control_get_num_channels     (MateMixerStreamControl  *ctl);

static gboolean                 oss_stream_control_set_volume           (MateMixerStreamControl  *ctl,
                                                                         guint                    volume);

static guint                    oss_stream_control_get_channel_volume   (MateMixerStreamControl  *ctl,
                                                                         guint                    channel);
static gboolean                 oss_stream_control_set_channel_volume   (MateMixerStreamControl  *ctl,
                                                                         guint                    channel,
                                                                         guint                    volume);

static MateMixerChannelPosition oss_stream_control_get_channel_position (MateMixerStreamControl  *ctl,
                                                                         guint                    channel);
static gboolean                 oss_stream_control_has_channel_position (MateMixerStreamControl  *ctl,
                                                                         MateMixerChannelPosition position);

static gboolean                 oss_stream_control_set_balance          (MateMixerStreamControl  *ctl,
                                                                         gfloat                   balance);

static guint                    oss_stream_control_get_min_volume       (MateMixerStreamControl  *ctl);
static guint                    oss_stream_control_get_max_volume       (MateMixerStreamControl  *ctl);
static guint                    oss_stream_control_get_normal_volume    (MateMixerStreamControl  *ctl);
static guint                    oss_stream_control_get_base_volume      (MateMixerStreamControl  *ctl);

static void
mate_mixer_stream_control_interface_init (MateMixerStreamControlInterface *iface)
{
    iface->get_name             = oss_stream_control_get_name;
    iface->get_description      = oss_stream_control_get_description;
    iface->get_num_channels     = oss_stream_control_get_num_channels;
    iface->set_volume           = oss_stream_control_set_volume;
    iface->get_channel_volume   = oss_stream_control_get_channel_volume;
    iface->set_channel_volume   = oss_stream_control_set_channel_volume;
    iface->get_channel_position = oss_stream_control_get_channel_position;
    iface->has_channel_position = oss_stream_control_has_channel_position;
    iface->set_balance          = oss_stream_control_set_balance;
    iface->get_min_volume       = oss_stream_control_get_min_volume;
    iface->get_max_volume       = oss_stream_control_get_max_volume;
    iface->get_normal_volume    = oss_stream_control_get_normal_volume;
    iface->get_base_volume      = oss_stream_control_get_base_volume;
}

static void
oss_stream_control_class_init (OssStreamControlClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = oss_stream_control_finalize;
    object_class->get_property = oss_stream_control_get_property;
    object_class->set_property = oss_stream_control_set_property;

    g_object_class_install_property (object_class,
                                     PROP_FD,
                                     g_param_spec_int ("fd",
                                                       "File descriptor",
                                                       "File descriptor of the device",
                                                       G_MININT,
                                                       G_MAXINT,
                                                       -1,
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (object_class,
                                     PROP_DEV_NUMBER,
                                     g_param_spec_int ("dev-number",
                                                       "Dev number",
                                                       "OSS device number",
                                                       G_MININT,
                                                       G_MAXINT,
                                                       0,
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (object_class,
                                     PROP_STEREO,
                                     g_param_spec_boolean ("stereo",
                                                           "Stereo",
                                                           "Stereo",
                                                           FALSE,
                                                           G_PARAM_CONSTRUCT_ONLY |
                                                           G_PARAM_READWRITE |
                                                           G_PARAM_STATIC_STRINGS));

    g_object_class_override_property (object_class, PROP_NAME, "name");
    g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (object_class, PROP_FLAGS, "flags");
    g_object_class_override_property (object_class, PROP_MUTE, "mute");
    g_object_class_override_property (object_class, PROP_VOLUME, "volume");
    g_object_class_override_property (object_class, PROP_BALANCE, "balance");
    g_object_class_override_property (object_class, PROP_FADE, "fade");

    g_type_class_add_private (object_class, sizeof (OssStreamControlPrivate));
}

static void
oss_stream_control_get_property (GObject    *object,
                                 guint       param_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    OssStreamControl *octl;

    octl = OSS_STREAM_CONTROL (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, octl->priv->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, octl->priv->description);
        break;
    case PROP_FLAGS:
        g_value_set_flags (value, octl->priv->flags);
        break;
    case PROP_MUTE:
        /* Not supported */
        g_value_set_boolean (value, FALSE);
        break;
    case PROP_VOLUME:
        g_value_set_uint (value, MAX (octl->priv->volume[0], octl->priv->volume[1]));
        break;
    case PROP_BALANCE:
        g_value_set_float (value, octl->priv->balance);
        break;
    case PROP_FADE:
        /* Not supported */
        g_value_set_float (value, 0.0f);
        break;
    case PROP_FD:
        g_value_set_int (value, octl->priv->fd);
        break;
    case PROP_DEV_NUMBER:
        g_value_set_int (value, octl->priv->dev_number);
        break;
    case PROP_STEREO:
        g_value_set_boolean (value, octl->priv->stereo);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
oss_stream_control_set_property (GObject      *object,
                                 guint         param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    OssStreamControl *octl;

    octl = OSS_STREAM_CONTROL (object);

    switch (param_id) {
    case PROP_FD:
        octl->priv->fd = dup (g_value_get_int (value));
        break;
    case PROP_DEV_NUMBER:
        octl->priv->dev_number = g_value_get_int (value);
        break;
    case PROP_STEREO:
        octl->priv->stereo = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
oss_stream_control_init (OssStreamControl *octl)
{
    octl->priv = G_TYPE_INSTANCE_GET_PRIVATE (octl,
                                              OSS_TYPE_STREAM_CONTROL,
                                              OssStreamControlPrivate);
}

static void
oss_stream_control_finalize (GObject *object)
{
    OssStreamControl *octl;

    octl = OSS_STREAM_CONTROL (object);

    g_free (octl->priv->name);
    g_free (octl->priv->description);

    if (octl->priv->fd != -1)
        g_close (octl->priv->fd, NULL);

    G_OBJECT_CLASS (oss_stream_control_parent_class)->finalize (object);
}

OssStreamControl *
oss_stream_control_new (gint         fd,
                        gint         dev_number,
                        const gchar *name,
                        const gchar *description,
                        gboolean     stereo)
{
    OssStreamControl *ctl;

    ctl = g_object_new (OSS_TYPE_STREAM_CONTROL,
                        "fd", fd,
                        "dev-number", dev_number,
                        "stereo", stereo,
                        NULL);

    ctl->priv->name = g_strdup (name);
    ctl->priv->description = g_strdup (description);

    return ctl;
}

gboolean
oss_stream_control_update (OssStreamControl *octl)
{
    gint v;
    gint ret;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (octl), FALSE);

    ret = ioctl (octl->priv->fd, MIXER_READ (octl->priv->dev_number), &v);
    if (ret < 0) {
        g_warning ("Failed to read volume: %s", g_strerror (errno));
        return FALSE;
    }

    octl->priv->volume[0] = v & 0xFF;

    if (octl->priv->stereo == TRUE)
        octl->priv->volume[1] = (v >> 8) & 0xFF;

    return TRUE;
}

gboolean
oss_stream_control_set_port (OssStreamControl *octl, MateMixerPort *port)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (octl), FALSE);

    // XXX provide property

    if (octl->priv->port != NULL)
        g_object_unref (octl->priv->port);

    octl->priv->port = g_object_ref (port);
    return TRUE;
}

gboolean
oss_stream_control_set_role (OssStreamControl *octl, MateMixerStreamControlRole role)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (octl), FALSE);

    octl->priv->role = role;
    return TRUE;
}

static const gchar *
oss_stream_control_get_name (MateMixerStreamControl *ctl)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (ctl), NULL);

    return OSS_STREAM_CONTROL (ctl)->priv->name;
}

static const gchar *
oss_stream_control_get_description (MateMixerStreamControl *ctl)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (ctl), NULL);

    return OSS_STREAM_CONTROL (ctl)->priv->description;
}

static guint
oss_stream_control_get_num_channels (MateMixerStreamControl *ctl)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (ctl), 0);

    if (OSS_STREAM_CONTROL (ctl)->priv->stereo == TRUE)
        return 2;
    else
        return 1;
}

static gboolean
oss_stream_control_set_volume (MateMixerStreamControl *ctl, guint volume)
{
    OssStreamControl *octl;
    int v;
    int ret;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (ctl), FALSE);

    octl = OSS_STREAM_CONTROL (ctl);

    /* Some backends may allow setting higher value than maximum, but not here */
    if (volume > 100)
        volume = 100;

    v = volume;
    if (octl->priv->stereo == TRUE)
        v |= (volume & 0xFF) << 8;

    ret = ioctl (octl->priv->fd, MIXER_WRITE (octl->priv->dev_number), &v);
    if (ret < 0) {
        g_warning ("Failed to set volume: %s", g_strerror (errno));
        return FALSE;
    }
    return TRUE;
}

static guint
oss_stream_control_get_channel_volume (MateMixerStreamControl *ctl, guint channel)
{
    OssStreamControl *octl;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (ctl), 0);

    octl = OSS_STREAM_CONTROL (ctl);

    if (channel > 1)
        return 0;

    /* Right channel on mono stream will always have zero volume */
    return octl->priv->volume[channel];
}

static gboolean
oss_stream_control_set_channel_volume (MateMixerStreamControl *ctl,
                                       guint                   channel,
                                       guint                   volume)
{
    OssStreamControl *octl;
    int ret;
    int v;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (ctl), FALSE);

    if (channel > 1)
        return FALSE;

    octl = OSS_STREAM_CONTROL (ctl);

    /* Some backends may allow setting higher value than maximum, but not here */
    if (volume > 100)
        volume = 100;

    if (channel == 0)
        v = volume;
    else
        v = octl->priv->volume[0];

    if (channel == 1) {
        if (octl->priv->stereo == FALSE)
            return FALSE;

        v |= volume << 8;
    } else
        v |= octl->priv->volume[1] << 8;

    ret = ioctl (octl->priv->fd, MIXER_WRITE (octl->priv->dev_number), &v);
    if (ret < 0) {
        g_warning ("Failed to set channel volume: %s", g_strerror (errno));
        return FALSE;
    }
    return TRUE;
}

static MateMixerChannelPosition
oss_stream_control_get_channel_position (MateMixerStreamControl *ctl, guint channel)
{
    OssStreamControl *octl;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (ctl), MATE_MIXER_CHANNEL_UNKNOWN);

    octl = OSS_STREAM_CONTROL (ctl);

    if (octl->priv->stereo == TRUE) {
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
oss_stream_control_has_channel_position (MateMixerStreamControl  *ctl,
                                         MateMixerChannelPosition position)
{
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (ctl), FALSE);

    if (position == MATE_MIXER_CHANNEL_MONO)
        return OSS_STREAM_CONTROL (ctl)->priv->stereo == FALSE;

    if (position == MATE_MIXER_CHANNEL_FRONT_LEFT ||
        position == MATE_MIXER_CHANNEL_FRONT_RIGHT)
        return OSS_STREAM_CONTROL (ctl)->priv->stereo == TRUE;

    return FALSE;
}

static gboolean
oss_stream_control_set_balance (MateMixerStreamControl *ctl, gfloat balance)
{
    OssStreamControl *octl;

    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (ctl), FALSE);

    octl = OSS_STREAM_CONTROL (ctl);

    if (octl->priv->stereo == FALSE)
        return FALSE;

    // XXX
    return TRUE;
}

static guint
oss_stream_control_get_min_volume (MateMixerStreamControl *ctl)
{
    return 0;
}

static guint
oss_stream_control_get_max_volume (MateMixerStreamControl *ctl)
{
    return 100;
}

static guint
oss_stream_control_get_normal_volume (MateMixerStreamControl *ctl)
{
    return 100;
}

static guint
oss_stream_control_get_base_volume (MateMixerStreamControl *ctl)
{
    return 100;
}
