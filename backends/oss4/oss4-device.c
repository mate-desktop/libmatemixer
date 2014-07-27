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
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-device.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-port.h>
#include <libmatemixer/matemixer-port-private.h>
#include <libmatemixer/matemixer-stream.h>
#include <libmatemixer/matemixer-stream-control.h>

#include "oss4-common.h"
#include "oss4-device.h"

#define OSS4_DEVICE_ICON "audio-card"

struct _Oss4DevicePrivate
{
    gint             fd;
    gint             index;
    gchar           *name;
    gchar           *description;
    gchar           *icon;
    MateMixerStream *input;
    MateMixerStream *output;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_ICON,
    PROP_ACTIVE_PROFILE,
    PROP_FD,
    PROP_INDEX
};

static void mate_mixer_device_interface_init (MateMixerDeviceInterface *iface);

static void oss4_device_class_init   (Oss4DeviceClass *klass);

static void oss4_device_get_property (GObject        *object,
                                      guint           param_id,
                                      GValue         *value,
                                      GParamSpec     *pspec);
static void oss4_device_set_property (GObject        *object,
                                      guint           param_id,
                                      const GValue   *value,
                                      GParamSpec     *pspec);

static void oss4_device_init         (Oss4Device     *device);
static void oss4_device_finalize     (GObject        *object);

G_DEFINE_TYPE_WITH_CODE (Oss4Device, oss4_device, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_DEVICE,
                                                mate_mixer_device_interface_init))

static const gchar *oss4_device_get_name        (MateMixerDevice *device);
static const gchar *oss4_device_get_description (MateMixerDevice *device);
static const gchar *oss4_device_get_icon        (MateMixerDevice *device);

static gboolean     read_mixer_devices          (Oss4Device      *device);

static void
mate_mixer_device_interface_init (MateMixerDeviceInterface *iface)
{
    iface->get_name        = oss4_device_get_name;
    iface->get_description = oss4_device_get_description;
    iface->get_icon        = oss4_device_get_icon;
}

static void
oss4_device_class_init (Oss4DeviceClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = oss4_device_finalize;
    object_class->get_property = oss4_device_get_property;
    object_class->set_property = oss4_device_set_property;

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
                                     PROP_INDEX,
                                     g_param_spec_int ("index",
                                                       "Index",
                                                       "Index of the device",
                                                       G_MININT,
                                                       G_MAXINT,
                                                       0,
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS));

    g_object_class_override_property (object_class, PROP_NAME, "name");
    g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (object_class, PROP_ICON, "icon");
    g_object_class_override_property (object_class, PROP_ACTIVE_PROFILE, "active-profile");

    g_type_class_add_private (object_class, sizeof (Oss4DevicePrivate));
}

static void
oss4_device_get_property (GObject    *object,
                         guint       param_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    Oss4Device *device;

    device = OSS4_DEVICE (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, device->priv->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, device->priv->description);
        break;
    case PROP_ICON:
        g_value_set_string (value, OSS4_DEVICE_ICON);
        break;
    case PROP_ACTIVE_PROFILE:
        /* Not supported */
        g_value_set_object (value, NULL);
        break;
    case PROP_FD:
        g_value_set_int (value, device->priv->fd);
        break;
    case PROP_INDEX:
        g_value_set_int (value, device->priv->index);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
oss4_device_set_property (GObject      *object,
                         guint         param_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    Oss4Device *device;

    device = OSS4_DEVICE (object);

    switch (param_id) {
    case PROP_NAME:
        /* Construct-only string */
        device->priv->name = g_strdup (g_value_get_string (value));
        break;
    case PROP_DESCRIPTION:
        /* Construct-only string */
        device->priv->description = g_strdup (g_value_get_string (value));
        break;
    case PROP_ICON:
        /* Construct-only string */
        device->priv->icon = g_strdup (g_value_get_string (value));
        break;
    case PROP_FD:
        device->priv->fd = dup (g_value_get_int (value));
        break;
    case PROP_INDEX:
        device->priv->index = g_value_get_int (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
oss4_device_init (Oss4Device *device)
{
    device->priv = G_TYPE_INSTANCE_GET_PRIVATE (device,
                                                OSS4_TYPE_DEVICE,
                                                Oss4DevicePrivate);
}

static void
oss4_device_finalize (GObject *object)
{
    Oss4Device *device;

    device = OSS4_DEVICE (object);

    g_free (device->priv->name);
    g_free (device->priv->description);

    if (device->priv->fd != -1)
        g_close (device->priv->fd, NULL);

    G_OBJECT_CLASS (oss4_device_parent_class)->finalize (object);
}

Oss4Device *
oss4_device_new (const gchar *name,
                 const gchar *description,
                 gint         fd,
                 gint         index)
{
    Oss4Device *device;

    device = g_object_new (OSS4_TYPE_DEVICE,
                           "name", name,
                           "description", description,
                           "fd", fd,
                           "index", index,
                           NULL);

    return device;
}

gboolean
oss4_device_read (Oss4Device *odevice)
{
    gint exts;
    gint ret;
    gint i;

    ret = ioctl (odevice->priv->fd, SNDCTL_MIX_NREXT, &exts);
    if (ret == -1)
        return FALSE;

    for (i = 0; i < exts; i++) {
        oss_mixext ext;

        ext.dev  = odevice->priv->index;
        ext.ctrl = i;
        ret = ioctl (odevice->priv->fd, SNDCTL_MIX_EXTINFO, &ext);
        if (ret == -1)
            continue;

        g_debug ("Mixer control %d type %d\n"
                 " min %d max %d\n"
                 " id %s\n"
                 " extname %s",
                 i,ext.type, ext.minvalue, ext.maxvalue, ext.id, ext.extname);
    }

    return TRUE;
}

gint
oss4_device_get_index (Oss4Device *odevice)
{
    g_return_val_if_fail (OSS4_IS_DEVICE (odevice), FALSE);

    return odevice->priv->index;
}

MateMixerStream *
oss4_device_get_input_stream (Oss4Device *odevice)
{
    g_return_val_if_fail (OSS4_IS_DEVICE (odevice), FALSE);

    return odevice->priv->input;
}

MateMixerStream *
oss4_device_get_output_stream (Oss4Device *odevice)
{
    g_return_val_if_fail (OSS4_IS_DEVICE (odevice), FALSE);

    return odevice->priv->output;
}

static gboolean
read_mixer_devices (Oss4Device *device)
{
}

static const gchar *
oss4_device_get_name (MateMixerDevice *device)
{
    g_return_val_if_fail (OSS4_IS_DEVICE (device), NULL);

    return OSS4_DEVICE (device)->priv->name;
}

static const gchar *
oss4_device_get_description (MateMixerDevice *device)
{
    g_return_val_if_fail (OSS4_IS_DEVICE (device), NULL);

    return OSS4_DEVICE (device)->priv->description;
}

static const gchar *
oss4_device_get_icon (MateMixerDevice *device)
{
    g_return_val_if_fail (OSS4_IS_DEVICE (device), NULL);

    return OSS4_DEVICE_ICON;
}
