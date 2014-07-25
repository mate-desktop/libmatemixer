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

#include "oss-common.h"
#include "oss-device.h"
#include "oss-stream.h"
#include "oss-stream-control.h"

#define OSS_DEVICE_ICON "audio-card"

typedef struct
{
    gchar                     *name;
    gchar                     *description;
    MateMixerStreamControlRole role;
    MateMixerStreamFlags       flags;
} OssDeviceName;

static const OssDeviceName const oss_devices[] = {
    { "vol",     N_("Volume"),     MATE_MIXER_STREAM_CONTROL_ROLE_MASTER,  MATE_MIXER_STREAM_OUTPUT },
    { "bass",    N_("Bass"),       MATE_MIXER_STREAM_CONTROL_ROLE_BASS,    MATE_MIXER_STREAM_OUTPUT },
    { "treble",  N_("Treble"),     MATE_MIXER_STREAM_CONTROL_ROLE_TREBLE,  MATE_MIXER_STREAM_OUTPUT },
    { "synth",   N_("Synth"),      MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, MATE_MIXER_STREAM_INPUT },
    { "pcm",     N_("PCM"),        MATE_MIXER_STREAM_CONTROL_ROLE_PCM,     MATE_MIXER_STREAM_OUTPUT },
    { "speaker", N_("Speaker"),    MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, MATE_MIXER_STREAM_OUTPUT },
    { "line",    N_("Line-in"),    MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    MATE_MIXER_STREAM_INPUT },
    { "mic",     N_("Microphone"), MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    MATE_MIXER_STREAM_INPUT },
    { "cd",      N_("CD"),         MATE_MIXER_STREAM_CONTROL_ROLE_CD,      MATE_MIXER_STREAM_INPUT },
    /* Recording monitor */
    { "mix",     N_("Mixer"),      MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, MATE_MIXER_STREAM_OUTPUT },
    { "pcm2",    N_("PCM-2"),      MATE_MIXER_STREAM_CONTROL_ROLE_PCM,     MATE_MIXER_STREAM_OUTPUT },
    /* Recording level (master input) */
    { "rec",     N_("Record"),     MATE_MIXER_STREAM_CONTROL_ROLE_MASTER,  MATE_MIXER_STREAM_INPUT },
    { "igain",   N_("In-gain"),    MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, MATE_MIXER_STREAM_INPUT },
    { "ogain",   N_("Out-gain"),   MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, MATE_MIXER_STREAM_OUTPUT },
    { "line1",   N_("Line-1"),     MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    MATE_MIXER_STREAM_INPUT },
    { "line2",   N_("Line-2"),     MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    MATE_MIXER_STREAM_INPUT },
    { "line3",   N_("Line-3"),     MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    MATE_MIXER_STREAM_INPUT },
    { "dig1",    N_("Digital-1"),  MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    MATE_MIXER_STREAM_NO_FLAGS },
    { "dig2",    N_("Digital-2"),  MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    MATE_MIXER_STREAM_NO_FLAGS },
    { "dig3",    N_("Digital-3"),  MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    MATE_MIXER_STREAM_NO_FLAGS },
    { "phin",    N_("Phone-in"),   MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    MATE_MIXER_STREAM_INPUT },
    { "phout",   N_("Phone-out"),  MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    MATE_MIXER_STREAM_OUTPUT },
    { "video",   N_("Video"),      MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    MATE_MIXER_STREAM_INPUT },
    { "radio",   N_("Radio"),      MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    MATE_MIXER_STREAM_INPUT },
    { "monitor", N_("Monitor"),    MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, MATE_MIXER_STREAM_OUTPUT },
    { "depth",   N_("3D-depth"),   MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, MATE_MIXER_STREAM_OUTPUT },
    { "center",  N_("3D-center"),  MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, MATE_MIXER_STREAM_OUTPUT },
    { "midi",    N_("MIDI"),       MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, MATE_MIXER_STREAM_INPUT }
};

struct _OssDevicePrivate
{
    gint             fd;
    gchar           *path;
    gchar           *name;
    gchar           *description;
    MateMixerStream *input;
    MateMixerStream *output;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_ICON,
    PROP_ACTIVE_PROFILE,
    PROP_PATH,
    PROP_FD
};

static void mate_mixer_device_interface_init (MateMixerDeviceInterface *iface);

static void oss_device_class_init   (OssDeviceClass *klass);

static void oss_device_get_property (GObject        *object,
                                     guint           param_id,
                                     GValue         *value,
                                     GParamSpec     *pspec);
static void oss_device_set_property (GObject        *object,
                                     guint           param_id,
                                     const GValue   *value,
                                     GParamSpec     *pspec);

static void oss_device_init         (OssDevice      *device);
static void oss_device_finalize     (GObject        *object);

G_DEFINE_TYPE_WITH_CODE (OssDevice, oss_device, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_DEVICE,
                                                mate_mixer_device_interface_init))

static const gchar *oss_device_get_name        (MateMixerDevice *device);
static const gchar *oss_device_get_description (MateMixerDevice *device);
static const gchar *oss_device_get_icon        (MateMixerDevice *device);

static gboolean     read_mixer_devices         (OssDevice       *device);

static void
mate_mixer_device_interface_init (MateMixerDeviceInterface *iface)
{
    iface->get_name        = oss_device_get_name;
    iface->get_description = oss_device_get_description;
    iface->get_icon        = oss_device_get_icon;
}

static void
oss_device_class_init (OssDeviceClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = oss_device_finalize;
    object_class->get_property = oss_device_get_property;
    object_class->set_property = oss_device_set_property;

    g_object_class_install_property (object_class,
                                     PROP_PATH,
                                     g_param_spec_string ("path",
                                                          "Path",
                                                          "Path to the device",
                                                          NULL,
                                                          G_PARAM_CONSTRUCT_ONLY |
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));

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

    g_object_class_override_property (object_class, PROP_NAME, "name");
    g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (object_class, PROP_ICON, "icon");
    g_object_class_override_property (object_class, PROP_ACTIVE_PROFILE, "active-profile");

    g_type_class_add_private (object_class, sizeof (OssDevicePrivate));
}

static void
oss_device_get_property (GObject    *object,
                         guint       param_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    OssDevice *device;

    device = OSS_DEVICE (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, device->priv->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, device->priv->description);
        break;
    case PROP_ICON:
        g_value_set_string (value, OSS_DEVICE_ICON);
        break;
    case PROP_ACTIVE_PROFILE:
        /* Not supported */
        g_value_set_object (value, NULL);
        break;
    case PROP_PATH:
        g_value_set_string (value, device->priv->path);
        break;
    case PROP_FD:
        g_value_set_int (value, device->priv->fd);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
oss_device_set_property (GObject      *object,
                         guint         param_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    OssDevice *device;

    device = OSS_DEVICE (object);

    switch (param_id) {
    case PROP_PATH:
        /* Construct-only string */
        device->priv->path = g_strdup (g_value_get_string (value));
        break;
    case PROP_FD:
        device->priv->fd = dup (g_value_get_int (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
oss_device_init (OssDevice *device)
{
    device->priv = G_TYPE_INSTANCE_GET_PRIVATE (device,
                                                OSS_TYPE_DEVICE,
                                                OssDevicePrivate);
}

static void
oss_device_finalize (GObject *object)
{
    OssDevice *device;

    device = OSS_DEVICE (object);

    g_free (device->priv->name);
    g_free (device->priv->description);

    if (device->priv->fd != -1)
        g_close (device->priv->fd, NULL);

    G_OBJECT_CLASS (oss_device_parent_class)->finalize (object);
}

OssDevice *
oss_device_new (const gchar *path, gint fd)
{
    OssDevice *device;
    gchar     *basename;

    g_return_val_if_fail (path != NULL, NULL);

    device = g_object_new (OSS_TYPE_DEVICE,
                           "path", path,
                           "fd", fd,
                           NULL);

    basename = g_path_get_basename (path);

    device->priv->name = g_strdup_printf ("oss-%s", basename);
    g_free (basename);

    return device;
}

gboolean
oss_device_read (OssDevice *device)
{
    gchar                  *name;
    gchar                  *basename;
    MateMixerStreamControl *ctl;

    g_return_val_if_fail (OSS_IS_DEVICE (device), FALSE);

    // XXX avoid calling this function repeatedly

    g_debug ("Querying device %s (%s)",
             device->priv->path,
             device->priv->description);

    basename = g_path_get_basename (device->priv->path);

    name = g_strdup_printf ("oss-input-%s", basename);
    device->priv->input = MATE_MIXER_STREAM (oss_stream_new (name,
                                                             device->priv->description,
                                                             MATE_MIXER_STREAM_INPUT));
    g_free (name);

    name = g_strdup_printf ("oss-output-%s", basename);
    device->priv->output = MATE_MIXER_STREAM (oss_stream_new (name,
                                                              device->priv->description,
                                                              MATE_MIXER_STREAM_OUTPUT |
                                                              MATE_MIXER_STREAM_PORTS_FIXED));
    g_free (name);

    if (read_mixer_devices (device) == FALSE)
        return FALSE;

    // XXX prefer active ports as default if there is no default

    ctl = mate_mixer_stream_get_default_control (device->priv->input);
    if (ctl == NULL) {
        const GList *list = mate_mixer_stream_list_controls (device->priv->input);

        if (list != NULL) {
            ctl = MATE_MIXER_STREAM_CONTROL (list->data);
            oss_stream_set_default_control (OSS_STREAM (device->priv->input),
                                            OSS_STREAM_CONTROL (ctl));
        } else
            g_clear_object (&device->priv->input);
    }

    if (ctl != NULL)
        g_debug ("Default input stream control is %s",
                 mate_mixer_stream_control_get_description (ctl));

    ctl = mate_mixer_stream_get_default_control (device->priv->output);
    if (ctl == NULL) {
        const GList *list = mate_mixer_stream_list_controls (device->priv->output);

        if (list != NULL) {
            ctl = MATE_MIXER_STREAM_CONTROL (list->data);
            oss_stream_set_default_control (OSS_STREAM (device->priv->output),
                                            OSS_STREAM_CONTROL (ctl));
        } else
            g_clear_object (&device->priv->output);
    }

    if (ctl != NULL)
        g_debug ("Default output stream control is %s",
                 mate_mixer_stream_control_get_description (ctl));

    return TRUE;
}

const gchar *
oss_device_get_path (OssDevice *odevice)
{
    g_return_val_if_fail (OSS_IS_DEVICE (odevice), FALSE);

    return odevice->priv->path;
}

MateMixerStream *
oss_device_get_input_stream (OssDevice *odevice)
{
    g_return_val_if_fail (OSS_IS_DEVICE (odevice), FALSE);

    return odevice->priv->input;
}

MateMixerStream *
oss_device_get_output_stream (OssDevice *odevice)
{
    g_return_val_if_fail (OSS_IS_DEVICE (odevice), FALSE);

    return odevice->priv->output;
}

gboolean
oss_device_set_description (OssDevice *odevice, const gchar *description)
{
    g_return_val_if_fail (OSS_IS_DEVICE (odevice), FALSE);

    if (g_strcmp0 (odevice->priv->description, description) != 0) {
        g_free (odevice->priv->description);

        odevice->priv->description = g_strdup (description);

        g_object_notify (G_OBJECT (odevice), "description");
        return TRUE;
    }
    return FALSE;
}

static gboolean
read_mixer_devices (OssDevice *device)
{
    gint              devmask,
                      stereomask,
                      recmask;
    gint              ret;
    gint              i;
    OssStreamControl *ctl;

    ret = ioctl (device->priv->fd, MIXER_READ (SOUND_MIXER_DEVMASK), &devmask);
    if (ret < 0)
        goto fail;
    ret = ioctl (device->priv->fd, MIXER_READ (SOUND_MIXER_STEREODEVS), &stereomask);
    if (ret < 0)
        goto fail;
    ret = ioctl (device->priv->fd, MIXER_READ (SOUND_MIXER_RECMASK), &recmask);
    if (ret < 0)
        goto fail;

    for (i = 0; i < MIN (G_N_ELEMENTS (oss_devices), SOUND_MIXER_NRDEVICES); i++) {
        gboolean       stereo;
        gboolean       input = FALSE;
        MateMixerPort *port = NULL;

        /* Skip unavailable controls */
        if ((devmask & (1 << i)) == 0)
            continue;

        if ((stereomask & (1 << i)) > 0)
            stereo = TRUE;
        else
            stereo = FALSE;

        if (oss_devices[i].flags == MATE_MIXER_STREAM_NO_FLAGS) {
            if ((recmask & (1 << i)) > 0)
                input = TRUE;
        }
        if (oss_devices[i].flags == MATE_MIXER_STREAM_INPUT) {
            if ((recmask & (1 << i)) == 0) {
                g_debug ("Skipping non-input device %s", oss_devices[i].name);
                continue;
            }
            input = TRUE;
        }

        ctl = oss_stream_control_new (device->priv->fd,
                                      i,
                                      oss_devices[i].name,
                                      oss_devices[i].description,
                                      stereo);

        if (oss_devices[i].role == MATE_MIXER_STREAM_CONTROL_ROLE_PORT)
            port = _mate_mixer_port_new (oss_devices[i].name,
                                         oss_devices[i].description,
                                         NULL,
                                         0,
                                         0);

        if (input == TRUE) {
            oss_stream_add_control (OSS_STREAM (device->priv->input), ctl);

            if (i == SOUND_MIXER_RECLEV || i == SOUND_MIXER_IGAIN) {
                if (i == SOUND_MIXER_RECLEV) {
                    oss_stream_set_default_control (OSS_STREAM (device->priv->input), ctl);
                } else {
                    MateMixerStreamControl *defctl;

                    defctl = mate_mixer_stream_get_default_control (device->priv->input);
                    if (defctl == NULL)
                        oss_stream_set_default_control (OSS_STREAM (device->priv->input), ctl);
                }
            }

            if (port != NULL)
                oss_stream_add_port (OSS_STREAM (device->priv->input), port);
        } else {
            oss_stream_add_control (OSS_STREAM (device->priv->output), ctl);

            if (i == SOUND_MIXER_VOLUME) {
                oss_stream_set_default_control (OSS_STREAM (device->priv->output), ctl);
            }
            else if (i == SOUND_MIXER_PCM) {
                MateMixerStreamControl *defctl;

                defctl = mate_mixer_stream_get_default_control (device->priv->output);
                if (defctl == NULL)
                    oss_stream_set_default_control (OSS_STREAM (device->priv->output), ctl);
            }

            if (port != NULL)
                oss_stream_add_port (OSS_STREAM (device->priv->output), port);
        }

        if (port != NULL)
            oss_stream_control_set_port (ctl, port);

        oss_stream_control_set_role (ctl, oss_devices[i].role);

        g_debug ("Added control %s",
                 mate_mixer_stream_control_get_description (MATE_MIXER_STREAM_CONTROL (ctl)));

        oss_stream_control_update (ctl);
    }
    return TRUE;

fail:
    g_warning ("Failed to read device %s: %s",
               device->priv->path,
               g_strerror (errno));

    return FALSE;
}

static const gchar *
oss_device_get_name (MateMixerDevice *device)
{
    g_return_val_if_fail (OSS_IS_DEVICE (device), NULL);

    return OSS_DEVICE (device)->priv->name;
}

static const gchar *
oss_device_get_description (MateMixerDevice *device)
{
    g_return_val_if_fail (OSS_IS_DEVICE (device), NULL);

    return OSS_DEVICE (device)->priv->description;
}

static const gchar *
oss_device_get_icon (MateMixerDevice *device)
{
    g_return_val_if_fail (OSS_IS_DEVICE (device), NULL);

    return OSS_DEVICE_ICON;
}
