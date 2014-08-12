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

#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include "oss-common.h"
#include "oss-device.h"
#include "oss-stream.h"
#include "oss-stream-control.h"

#define OSS_DEVICE_ICON "audio-card"

typedef enum
{
    OSS_DEV_ANY,
    OSS_DEV_INPUT,
    OSS_DEV_OUTPUT
} OssDevType;

typedef struct
{
    gchar                     *name;
    gchar                     *label;
    MateMixerStreamControlRole role;
    OssDevType                 type;
} OssDev;

static const OssDev oss_devices[] = {
    { "vol",     N_("Volume"),     MATE_MIXER_STREAM_CONTROL_ROLE_MASTER,  OSS_DEV_OUTPUT },
    { "bass",    N_("Bass"),       MATE_MIXER_STREAM_CONTROL_ROLE_BASS,    OSS_DEV_OUTPUT },
    { "treble",  N_("Treble"),     MATE_MIXER_STREAM_CONTROL_ROLE_TREBLE,  OSS_DEV_OUTPUT },
    { "synth",   N_("Synth"),      MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, OSS_DEV_INPUT },
    { "pcm",     N_("PCM"),        MATE_MIXER_STREAM_CONTROL_ROLE_PCM,     OSS_DEV_OUTPUT },
    /* OSS manual says this should be the beeper, but Linux OSS seems to assign it to
     * regular volume control */
    { "speaker", N_("Speaker"),    MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER, OSS_DEV_OUTPUT },
    { "line",    N_("Line-in"),    MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    OSS_DEV_INPUT },
    { "mic",     N_("Microphone"), MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    OSS_DEV_INPUT },
    { "cd",      N_("CD"),         MATE_MIXER_STREAM_CONTROL_ROLE_CD,      OSS_DEV_INPUT },
    /* Recording monitor */
    { "mix",     N_("Mixer"),      MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, OSS_DEV_OUTPUT },
    { "pcm2",    N_("PCM-2"),      MATE_MIXER_STREAM_CONTROL_ROLE_PCM,     OSS_DEV_OUTPUT },
    /* Recording level (master input) */
    { "rec",     N_("Record"),     MATE_MIXER_STREAM_CONTROL_ROLE_MASTER,  OSS_DEV_INPUT },
    { "igain",   N_("In-gain"),    MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, OSS_DEV_INPUT },
    { "ogain",   N_("Out-gain"),   MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, OSS_DEV_OUTPUT },
    { "line1",   N_("Line-1"),     MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    OSS_DEV_INPUT },
    { "line2",   N_("Line-2"),     MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    OSS_DEV_INPUT },
    { "line3",   N_("Line-3"),     MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    OSS_DEV_INPUT },
    { "dig1",    N_("Digital-1"),  MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    OSS_DEV_ANY },
    { "dig2",    N_("Digital-2"),  MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    OSS_DEV_ANY },
    { "dig3",    N_("Digital-3"),  MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    OSS_DEV_ANY },
    { "phin",    N_("Phone-in"),   MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    OSS_DEV_INPUT },
    { "phout",   N_("Phone-out"),  MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    OSS_DEV_OUTPUT },
    { "video",   N_("Video"),      MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    OSS_DEV_INPUT },
    { "radio",   N_("Radio"),      MATE_MIXER_STREAM_CONTROL_ROLE_PORT,    OSS_DEV_INPUT },
    { "monitor", N_("Monitor"),    MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, OSS_DEV_OUTPUT },
    { "depth",   N_("3D-depth"),   MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, OSS_DEV_OUTPUT },
    { "center",  N_("3D-center"),  MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, OSS_DEV_OUTPUT },
    { "midi",    N_("MIDI"),       MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN, OSS_DEV_INPUT }
};

#define OSS_N_DEVICES MIN (G_N_ELEMENTS (oss_devices), SOUND_MIXER_NRDEVICES)

struct _OssDevicePrivate
{
    gint       fd;
    gchar     *path;
    gint       devmask;
    gint       stereodevs;
    gint       recmask;
    gint       recsrc;
    OssStream *input;
    OssStream *output;
};

static void oss_device_class_init   (OssDeviceClass *klass);
static void oss_device_init         (OssDevice      *device);
static void oss_device_dispose      (GObject        *object);
static void oss_device_finalize     (GObject        *object);

G_DEFINE_TYPE (OssDevice, oss_device, MATE_MIXER_TYPE_DEVICE)

static GList *  oss_device_list_streams    (MateMixerDevice  *device);

static gboolean read_mixer_devices         (OssDevice        *device);

static gboolean set_stream_default_control (OssStream        *stream,
                                            OssStreamControl *control,
                                            gboolean          force);

static void
oss_device_class_init (OssDeviceClass *klass)
{
    GObjectClass         *object_class;
    MateMixerDeviceClass *device_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose  = oss_device_dispose;
    object_class->finalize = oss_device_finalize;

    device_class = MATE_MIXER_DEVICE_CLASS (klass);
    device_class->list_streams = oss_device_list_streams;

    g_type_class_add_private (object_class, sizeof (OssDevicePrivate));
}

static void
oss_device_init (OssDevice *device)
{
    device->priv = G_TYPE_INSTANCE_GET_PRIVATE (device,
                                                OSS_TYPE_DEVICE,
                                                OssDevicePrivate);
}

static void
oss_device_dispose (GObject *object)
{
    OssDevice *device;

    device = OSS_DEVICE (object);

    g_clear_object (&device->priv->input);
    g_clear_object (&device->priv->output);

    G_OBJECT_CLASS (oss_device_parent_class)->dispose (object);
}

static void
oss_device_finalize (GObject *object)
{
    OssDevice *device = OSS_DEVICE (object);

    close (device->priv->fd);

    G_OBJECT_CLASS (oss_device_parent_class)->finalize (object);
}

OssDevice *
oss_device_new (const gchar *name, const gchar *label, const gchar *path, gint fd)
{
    OssDevice *device;
    gchar     *stream_name;

    g_return_val_if_fail (name  != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (path  != NULL, NULL);

    device = g_object_new (OSS_TYPE_DEVICE,
                           "name", name,
                           "label", label,
                           "icon", OSS_DEVICE_ICON,
                           NULL);

    device->priv->fd   = dup (fd);
    device->priv->path = g_strdup (path);

    stream_name = g_strdup_printf ("oss-input-%s", name);
    device->priv->input = oss_stream_new (stream_name,
                                          MATE_MIXER_DEVICE (device),
                                          MATE_MIXER_STREAM_INPUT);
    g_free (stream_name);

    stream_name = g_strdup_printf ("oss-output-%s", name);
    device->priv->output = oss_stream_new (stream_name,
                                           MATE_MIXER_DEVICE (device),
                                           MATE_MIXER_STREAM_OUTPUT);
    g_free (stream_name);

    return device;
}

gboolean
oss_device_open (OssDevice *device)
{
    gint ret;

    g_return_val_if_fail (OSS_IS_DEVICE (device), FALSE);

    g_debug ("Opening device %s (%s)",
             device->priv->path,
             mate_mixer_device_get_label (MATE_MIXER_DEVICE (device)));

    /* Read the essential information about the device, these values are not
     * expected to change and will not be queried */
    ret = ioctl (device->priv->fd,
                 MIXER_READ (SOUND_MIXER_DEVMASK),
                 &device->priv->devmask);
    if (ret != 0)
        goto fail;

    ret = ioctl (device->priv->fd,
                 MIXER_READ (SOUND_MIXER_STEREODEVS),
                 &device->priv->stereodevs);
    if (ret < 0)
        goto fail;

    ret = ioctl (device->priv->fd,
                 MIXER_READ (SOUND_MIXER_RECMASK),
                 &device->priv->recmask);
    if (ret < 0)
        goto fail;

    /* The recording source mask may change at any time, here we just read
     * the initial value */
    ret = ioctl (device->priv->fd,
                 MIXER_READ (SOUND_MIXER_RECSRC),
                 &device->priv->recsrc);
    if (ret < 0)
        goto fail;

    /* NOTE: Linux also supports SOUND_MIXER_OUTSRC and SOUND_MIXER_OUTMASK which
     * inform about/enable input->output, we could potentially create toggles
     * for these, but these constants are not defined on any BSD. */

    return TRUE;

fail:
    g_warning ("Failed to read device %s: %s",
               device->priv->path,
               g_strerror (errno));

    return FALSE;
}

gboolean
oss_device_load (OssDevice *device)
{
    MateMixerStreamControl *control;

    g_return_val_if_fail (OSS_IS_DEVICE (device), FALSE);

    read_mixer_devices (device);

    control = mate_mixer_stream_get_default_control (MATE_MIXER_STREAM (device->priv->input));
    if (control == NULL) {
        // XXX pick something
    }

    if (control != NULL)
        g_debug ("Default input stream control is %s",
                 mate_mixer_stream_control_get_label (control));

    control = mate_mixer_stream_get_default_control (MATE_MIXER_STREAM (device->priv->output));
    if (control == NULL) {
        // XXX pick something
    }

    if (control != NULL)
        g_debug ("Default output stream control is %s",
                 mate_mixer_stream_control_get_label (control));

    return TRUE;
}

gint
oss_device_get_fd (OssDevice *device)
{
    g_return_val_if_fail (OSS_IS_DEVICE (device), -1);

    return device->priv->fd;
}

const gchar *
oss_device_get_path (OssDevice *device)
{
    g_return_val_if_fail (OSS_IS_DEVICE (device), NULL);

    return device->priv->path;
}

OssStream *
oss_device_get_input_stream (OssDevice *device)
{
    g_return_val_if_fail (OSS_IS_DEVICE (device), NULL);

    return device->priv->input;
}

OssStream *
oss_device_get_output_stream (OssDevice *device)
{
    g_return_val_if_fail (OSS_IS_DEVICE (device), NULL);

    return device->priv->output;
}

static GList *
oss_device_list_streams (MateMixerDevice *mmd)
{
    OssDevice *device;
    GList     *list = NULL;

    g_return_val_if_fail (OSS_IS_DEVICE (mmd), NULL);

    device = OSS_DEVICE (mmd);

    if (device->priv->output != NULL)
        list = g_list_prepend (list, g_object_ref (device->priv->output));
    if (device->priv->input != NULL)
        list = g_list_prepend (list, g_object_ref (device->priv->input));

    return list;
}

#define OSS_MASK_HAS_DEVICE(mask,i) ((gboolean) (((mask) & (1 << (i))) > 0))

static gboolean
read_mixer_devices (OssDevice *device)
{
    gint i;

    for (i = 0; i < OSS_N_DEVICES; i++) {
        OssStreamControl *control;
        gboolean          input = FALSE;

        /* Skip unavailable controls */
        if (OSS_MASK_HAS_DEVICE (device->priv->devmask, i) == FALSE)
            continue;

        if (oss_devices[i].type == OSS_DEV_ANY) {
            input = OSS_MASK_HAS_DEVICE (device->priv->recmask, i);
        }
        else if (oss_devices[i].type == OSS_DEV_INPUT) {
            input = TRUE;
        }

        control = oss_stream_control_new (oss_devices[i].name,
                                          oss_devices[i].label,
                                          oss_devices[i].role,
                                          device->priv->fd,
                                          i,
                                          OSS_MASK_HAS_DEVICE (device->priv->stereodevs, i));

        if (input == TRUE) {
            oss_stream_add_control (OSS_STREAM (device->priv->input), control);

            if (i == SOUND_MIXER_RECLEV || i == SOUND_MIXER_IGAIN) {
                if (i == SOUND_MIXER_RECLEV)
                    set_stream_default_control (OSS_STREAM (device->priv->input),
                                                control,
                                                TRUE);
                else
                    set_stream_default_control (OSS_STREAM (device->priv->input),
                                                control,
                                                FALSE);
            }
        } else {
            oss_stream_add_control (OSS_STREAM (device->priv->output), control);

            if (i == SOUND_MIXER_VOLUME || i == SOUND_MIXER_PCM) {
                if (i == SOUND_MIXER_VOLUME)
                    set_stream_default_control (OSS_STREAM (device->priv->output),
                                                control,
                                                TRUE);
                else
                    set_stream_default_control (OSS_STREAM (device->priv->output),
                                                control,
                                                FALSE);
            }
        }

        g_debug ("Added control %s",
                 mate_mixer_stream_control_get_label (MATE_MIXER_STREAM_CONTROL (control)));

        oss_stream_control_update (control);
    }
    return TRUE;
}

static gboolean
set_stream_default_control (OssStream *stream, OssStreamControl *control, gboolean force)
{
    MateMixerStreamControl *current;

    current = mate_mixer_stream_get_default_control (MATE_MIXER_STREAM (stream));
    if (current == NULL || force == TRUE) {
        oss_stream_set_default_control (stream, OSS_STREAM_CONTROL (control));
        return TRUE;
    }
    return FALSE;
}
