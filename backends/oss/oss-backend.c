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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libmatemixer/matemixer-backend.h>
#include <libmatemixer/matemixer-backend-module.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-stream.h>

#include "oss-backend.h"
#include "oss-common.h"
#include "oss-device.h"

#define BACKEND_NAME      "OSS"
#define BACKEND_PRIORITY  9

#define PATH_SNDSTAT      "/dev/sndstat"

struct _OssBackendPrivate
{
    GFile           *sndstat;
    GFile           *dev;
    GFileMonitor    *dev_monitor;
    GHashTable      *devices;
    GHashTable      *streams;
    MateMixerStream *default_input;
    MateMixerStream *default_output;
    MateMixerState   state;
};

enum {
    PROP_0,
    PROP_STATE,
    PROP_DEFAULT_INPUT,
    PROP_DEFAULT_OUTPUT
};

static void mate_mixer_backend_interface_init (MateMixerBackendInterface *iface);

static void oss_backend_class_init     (OssBackendClass *klass);
static void oss_backend_class_finalize (OssBackendClass *klass);

static void oss_backend_get_property   (GObject         *object,
                                        guint            param_id,
                                        GValue          *value,
                                        GParamSpec      *pspec);

static void oss_backend_init           (OssBackend      *oss);
static void oss_backend_dispose        (GObject         *object);
static void oss_backend_finalize       (GObject         *object);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (OssBackend, oss_backend,
                                G_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (MATE_MIXER_TYPE_BACKEND,
                                                               mate_mixer_backend_interface_init))

static gboolean oss_backend_open         (MateMixerBackend  *backend);
static void     oss_backend_close        (MateMixerBackend  *backend);
static GList *  oss_backend_list_devices (MateMixerBackend  *backend);
static GList *  oss_backend_list_streams (MateMixerBackend  *backend);

static void     change_state                    (OssBackend        *oss,
                                                 MateMixerState     state);

static void     on_dev_monitor_changed          (GFileMonitor      *monitor,
                                                 GFile             *file,
                                                 GFile             *other_file,
                                                 GFileMonitorEvent  event_type,
                                                 OssBackend        *oss);

static gboolean read_device                     (OssBackend        *oss,
                                                 const gchar        *path);

static gchar *  read_device_description         (OssBackend        *oss,
                                                 const gchar       *path,
                                                 gint               fd);
static gchar *  read_device_sndstat_description (const gchar       *path,
                                                 GFileInputStream  *input);

static void     add_device                      (OssBackend        *oss,
                                                 OssDevice         *device);
static void     remove_device                   (OssBackend        *oss,
                                                 OssDevice         *device);

static MateMixerBackendInfo info;

void
backend_module_init (GTypeModule *module)
{
    oss_backend_register_type (module);

    info.name         = BACKEND_NAME;
    info.priority     = BACKEND_PRIORITY;
    info.g_type       = OSS_TYPE_BACKEND;
    info.backend_type = MATE_MIXER_BACKEND_OSS;
}

const MateMixerBackendInfo *backend_module_get_info (void)
{
    return &info;
}

static void
mate_mixer_backend_interface_init (MateMixerBackendInterface *iface)
{
    iface->open         = oss_backend_open;
    iface->close        = oss_backend_close;
    iface->list_devices = oss_backend_list_devices;
    iface->list_streams = oss_backend_list_streams;
}

static void
oss_backend_class_init (OssBackendClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = oss_backend_dispose;
    object_class->finalize     = oss_backend_finalize;
    object_class->get_property = oss_backend_get_property;

    g_object_class_override_property (object_class, PROP_STATE, "state");
    g_object_class_override_property (object_class, PROP_DEFAULT_INPUT, "default-input");
    g_object_class_override_property (object_class, PROP_DEFAULT_OUTPUT, "default-output");

    g_type_class_add_private (object_class, sizeof (OssBackendPrivate));
}

/* Called in the code generated by G_DEFINE_DYNAMIC_TYPE_EXTENDED() */
static void
oss_backend_class_finalize (OssBackendClass *klass)
{
}

static void
oss_backend_get_property (GObject    *object,
                          guint       param_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    OssBackend *oss;

    oss = OSS_BACKEND (object);

    switch (param_id) {
    case PROP_STATE:
        g_value_set_enum (value, oss->priv->state);
        break;
    case PROP_DEFAULT_INPUT:
        g_value_set_object (value, oss->priv->default_input);
        break;
    case PROP_DEFAULT_OUTPUT:
        g_value_set_object (value, oss->priv->default_output);
        break;
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
oss_backend_init (OssBackend *oss)
{
    oss->priv = G_TYPE_INSTANCE_GET_PRIVATE (oss,
                                             OSS_TYPE_BACKEND,
                                             OssBackendPrivate);

    oss->priv->devices = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                g_free,
                                                g_object_unref);

    oss->priv->streams = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                g_free,
                                                g_object_unref);
}

static void
oss_backend_dispose (GObject *object)
{
    oss_backend_close (MATE_MIXER_BACKEND (object));
}

static void
oss_backend_finalize (GObject *object)
{
    OssBackend *oss;

    oss = OSS_BACKEND (object);

    g_hash_table_destroy (oss->priv->devices);
    g_hash_table_destroy (oss->priv->streams);
}

static gboolean
oss_backend_open (MateMixerBackend *backend)
{
    OssBackend *oss;
    GError     *error = NULL;
    gint        i;

    g_return_val_if_fail (OSS_IS_BACKEND (backend), FALSE);

    oss = OSS_BACKEND (backend);

    /* Monitor changes on /dev to catch hot-(un)plugged devices */
    // XXX works on linux, doesn't on freebsd, what to do on netbsd/openbsd?
    oss->priv->dev = g_file_new_for_path ("/dev");
    oss->priv->dev_monitor = g_file_monitor_directory (oss->priv->dev,
                                                       G_FILE_MONITOR_NONE,
                                                       NULL,
                                                       &error);
    if (oss->priv->dev_monitor != NULL) {
        g_signal_connect (G_OBJECT (oss->priv->dev_monitor),
                          "changed",
                          G_CALLBACK (on_dev_monitor_changed),
                          oss);
    } else {
        g_debug ("Failed to monitor /dev: %s", error->message);
        g_error_free (error);
    }

#if !defined(__linux__) && !defined(__NetBSD__) &&  !defined(__OpenBSD__)
    /* At least on systems based on FreeBSD we will need to read devices names
     * from the sndstat file, but avoid even trying that on systems where this
     * is not needed and the file is not present */
    oss->priv->sndstat = g_file_new_for_path (PATH_SNDSTAT);
#endif

    for (i = 0; i < 5; i++) {
        /* According to the OSS documentation the mixer devices are
         * /dev/mixer0 - /dev/mixer4, of course some systems create them
         * dynamically but this approach should be good enough */
        gchar *path = g_strdup_printf ("/dev/mixer%i", i);

        if (read_device (oss, path) == FALSE && i == 0) {
            /* For the first mixer device check also /dev/mixer, but it
             * might be a symlink to a real mixer device */
            if (g_file_test ("/dev/mixer", G_FILE_TEST_IS_SYMLINK) == FALSE)
                read_device (oss, "/dev/mixer");
        }

        g_free (path);
    }

    change_state (oss, MATE_MIXER_STATE_READY);
    return TRUE;
}

void
oss_backend_close (MateMixerBackend *backend)
{
    OssBackend *oss;

    g_return_if_fail (OSS_IS_BACKEND (backend));

    oss = OSS_BACKEND (backend);

    g_clear_object (&oss->priv->default_input);
    g_clear_object (&oss->priv->default_output);

    g_hash_table_remove_all (oss->priv->streams);
    g_hash_table_remove_all (oss->priv->devices);

    g_clear_object (&oss->priv->dev_monitor);
    g_clear_object (&oss->priv->dev);
    g_clear_object (&oss->priv->sndstat);
}

static GList *
oss_backend_list_devices (MateMixerBackend *backend)
{
    GList *list;

    g_return_val_if_fail (OSS_IS_BACKEND (backend), NULL);

    /* Convert the hash table to a sorted linked list, this list is expected
     * to be cached in the main library */
    list = g_hash_table_get_values (OSS_BACKEND (backend)->priv->devices);
    if (list != NULL) {
        g_list_foreach (list, (GFunc) g_object_ref, NULL);

        return list;
    }
    return NULL;
}

static GList *
oss_backend_list_streams (MateMixerBackend *backend)
{
    GList *list;

    g_return_val_if_fail (OSS_IS_BACKEND (backend), NULL);

    /* Convert the hash table to a sorted linked list, this list is expected
     * to be cached in the main library */
    list = g_hash_table_get_values (OSS_BACKEND (backend)->priv->streams);
    if (list != NULL) {
        g_list_foreach (list, (GFunc) g_object_ref, NULL);

        return list;
    }
    return NULL;
}

static void
change_state (OssBackend *oss, MateMixerState state)
{
    if (oss->priv->state == state)
        return;

    oss->priv->state = state;

    g_object_notify (G_OBJECT (oss), "state");
}

static void
on_dev_monitor_changed (GFileMonitor     *monitor,
                        GFile            *file,
                        GFile            *other_file,
                        GFileMonitorEvent event_type,
                        OssBackend       *oss)
{
    gchar *path;

    if (event_type != G_FILE_MONITOR_EVENT_CREATED &&
        event_type != G_FILE_MONITOR_EVENT_DELETED)
        return;

    path = g_file_get_path (file);

    /* Only handle creation and deletion of mixer devices */
    if (g_str_has_prefix (path, "/dev/mixer") == FALSE) {
        g_free (path);
        return;
    }
    if (strcmp (path, "/dev/mixer") == 0 &&
        g_file_test (path, G_FILE_TEST_IS_SYMLINK) == TRUE) {
        g_free (path);
        return;
    }

    if (event_type == G_FILE_MONITOR_EVENT_DELETED) {
        OssDevice *device;

        device = g_hash_table_lookup (oss->priv->devices, path);
        if (device != NULL)
            remove_device (oss, device);
    } else
        read_device (oss, path);

    g_free (path);
}

static gboolean
read_device (OssBackend *oss, const gchar *path)
{
    OssDevice   *device;
    gint         fd;
    gboolean     ret;
    const gchar *description;

    /* We assume that the name and capabilities of a device do not change */
    device = g_hash_table_lookup (oss->priv->devices, path);
    if (G_UNLIKELY (device != NULL)) {
        g_debug ("Attempt to re-read already know device %s", path);
        return TRUE;
    }

    fd = g_open (path, O_RDWR, 0);
    if (fd == -1) {
        if (errno != ENOENT && errno != ENXIO)
            g_debug ("%s: %s", path, g_strerror (errno));
        return FALSE;
    }

    device = oss_device_new (path, fd);

    description = read_device_description (oss, path, fd);
    if (description != NULL)
        oss_device_set_description (device, description);
    else
        oss_device_set_description (device, _("Unknown device"));

    /* Close the descriptor as the device should dup it if it intends
     * to keep it */
    g_close (fd, NULL);

    ret = oss_device_read (device);
    if (ret == TRUE)
        add_device (oss, device);

    g_object_unref (device);
    return ret;
}

static gchar *
read_device_description (OssBackend *oss, const gchar *path, gint fd)
{
    struct mixer_info info;

    if (ioctl (fd, SOUND_MIXER_INFO, &info) == 0) {
        /* Prefer device name supplied by the system, but this fails on FreeBSD */
        return g_strdup (info.name);
    }

    /* Reading the sndstat file is a bit desperate, but seem to be the only
     * way to determine the name of the sound card on FreeBSD, it also has an
     * advantage in being able to find out the default one */
    if (oss->priv->sndstat != NULL) {
        GError           *error = NULL;
        GFileInputStream *input = g_file_read (oss->priv->sndstat, NULL, &error);

        if (input == NULL) {
            /* The file can only be open by one application, otherwise the call
             * fails with EBUSY */
            if (error->code == G_IO_ERROR_BUSY) {
                g_debug ("Delayed reading %s as it is busy", PATH_SNDSTAT);
                // XXX use timeout and try again
            } else {
                if (error->code == G_IO_ERROR_NOT_FOUND)
                    g_debug ("Device description is unknown as %s does not exist",
                             PATH_SNDSTAT);
                else
                    g_debug ("%s: %s", PATH_SNDSTAT, error->message);

                g_clear_object (&oss->priv->sndstat);
            }
            g_error_free (error);
        } else
            return read_device_sndstat_description (path, input);
    }

    return NULL;
}

static gchar *
read_device_sndstat_description (const gchar *path, GFileInputStream *input)
{
    gchar            *line;
    gchar            *prefix;
    gchar            *description = NULL;
    GDataInputStream *stream;

    if (G_UNLIKELY (g_str_has_prefix (path, "/dev/mixer")) == FALSE) {
        g_warn_if_reached ();
        return NULL;
    }

    /* We assume that the mixer device number matches the pcm number in the
     * sndstat file, this is a bit desperate, but it seems to do the
     * right thing in practice */
    prefix = g_strdup_printf ("pcm%u: ",
                              (guint) g_ascii_strtoull (path + sizeof ("/dev/mixer") - 1,
                                                        NULL,
                                                        10));

    stream = g_data_input_stream_new (G_INPUT_STREAM (input));

    while (TRUE) {
        GError *error = NULL;
        gchar  *p;

        line = g_data_input_stream_read_line (stream, NULL, NULL, &error);
        if (line == NULL) {
            if (error != NULL) {
                g_warning ("%s: %s", path, error->message);
                g_error_free (error);
            }
            break;
        }

        if (g_str_has_prefix (line, prefix) == FALSE)
            continue;

        /* Example line:
         * pcm0: <ATI R6xx (HDMI)> (play) default */
        p = strchr (line, '<');
        if (p != NULL && *p && *(++p)) {
            gchar *end = strchr (p, '>');

            if (end != NULL)
                description = g_strndup (p, end - p);
        }

        // XXX we can also read "default" at the end of the line
        // XXX http://ashish.is.lostca.se/2011/05/23/default-sound-device-in-freebsd/
        if (g_str_has_suffix (line, "default") ||
            g_str_has_suffix (line, "default)"))
            ;

        if (description != NULL)
            break;
    }

    g_object_unref (stream);
    g_free (prefix);

    return description;
}

static void
add_device (OssBackend *oss, OssDevice *device)
{
    MateMixerStream *stream;

    /* Add device, file path is used as the key rather than the name, because
     * the name is not known until an OssDevice instance is created */
    g_hash_table_insert (oss->priv->devices,
                         g_strdup (oss_device_get_path (device)),
                         g_object_ref (device));

    g_signal_emit_by_name (G_OBJECT (oss),
                           "device-added",
                           mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));

    /* Add streams if they exist */
    stream = oss_device_get_input_stream (device);
    if (stream != NULL) {
        g_hash_table_insert (oss->priv->streams,
                             g_strdup (mate_mixer_stream_get_name (stream)),
                             g_object_ref (stream));

        g_signal_emit_by_name (G_OBJECT (oss),
                               "stream-added",
                               mate_mixer_stream_get_name (stream));
    }

    stream = oss_device_get_output_stream (device);
    if (stream != NULL) {
        g_hash_table_insert (oss->priv->streams,
                             g_strdup (mate_mixer_stream_get_name (stream)),
                             g_object_ref (stream));

        g_signal_emit_by_name (G_OBJECT (oss),
                               "stream-added",
                               mate_mixer_stream_get_name (stream));
    }
}

static void
remove_device (OssBackend *oss, OssDevice *device)
{
    MateMixerStream *stream;
    const gchar     *path;

    /* Remove the device streams first as they are a part of the device */
    stream = oss_device_get_input_stream (device);
    if (stream != NULL) {
        const gchar *name = mate_mixer_stream_get_name (stream);

        g_hash_table_remove (oss->priv->streams, name);
        g_signal_emit_by_name (G_OBJECT (oss),
                               "stream-removed",
                               name);
    }

    stream = oss_device_get_output_stream (device);
    if (stream != NULL) {
        const gchar *name = mate_mixer_stream_get_name (stream);

        g_hash_table_remove (oss->priv->streams, stream);
        g_signal_emit_by_name (G_OBJECT (oss),
                               "stream-removed",
                               name);
    }

    path = oss_device_get_path (device);

    /* Remove the device */
    g_object_ref (device);

    g_hash_table_remove (oss->priv->devices, path);
    g_signal_emit_by_name (G_OBJECT (oss),
                           "device-removed",
                           mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));

    g_object_unref (device);
}
