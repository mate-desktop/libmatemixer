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
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include "oss-backend.h"
#include "oss-common.h"
#include "oss-device.h"
#include "oss-stream.h"

#define BACKEND_NAME      "OSS"
#define BACKEND_PRIORITY  10
#define BACKEND_FLAGS     MATE_MIXER_BACKEND_NO_FLAGS

#if !defined(__linux__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
    /* At least on systems based on FreeBSD we will need to read device names
     * from the sndstat file, but avoid even trying that on systems where this
     * is not needed and the file is not present */
#define OSS_PATH_SNDSTAT  "/dev/sndstat"
#endif

#define OSS_MAX_DEVICES   32

struct _OssBackendPrivate
{
    gchar      *default_device;
    GSource    *timeout_source;
    GList      *streams;
    GList      *devices;
    GHashTable *devices_paths;
};

static void oss_backend_class_init     (OssBackendClass *klass);
static void oss_backend_class_finalize (OssBackendClass *klass);
static void oss_backend_init           (OssBackend      *oss);
static void oss_backend_dispose        (GObject         *object);
static void oss_backend_finalize       (GObject         *object);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
G_DEFINE_DYNAMIC_TYPE (OssBackend, oss_backend, MATE_MIXER_TYPE_BACKEND)
#pragma clang diagnostic pop

static gboolean     oss_backend_open             (MateMixerBackend *backend);
static void         oss_backend_close            (MateMixerBackend *backend);
static const GList *oss_backend_list_devices     (MateMixerBackend *backend);
static const GList *oss_backend_list_streams     (MateMixerBackend *backend);

static gboolean     read_devices                 (OssBackend       *oss);

static gboolean     read_device                  (OssBackend       *oss,
                                                  const gchar      *path,
                                                  gboolean         *added);

static gchar *      read_device_label            (OssBackend       *oss,
                                                  const gchar      *path,
                                                  gint              fd);

static gchar *      read_device_label_sndstat    (OssBackend       *oss,
                                                  const gchar      *sndstat,
                                                  const gchar      *path,
                                                  guint             index) G_GNUC_UNUSED;

static void         add_device                   (OssBackend       *oss,
                                                  OssDevice        *device);
static void         remove_device                (OssBackend       *oss,
                                                  OssDevice        *device);

static void         remove_device_by_path        (OssBackend       *oss,
                                                  const gchar      *path);

static void         remove_device_by_list_item   (OssBackend       *oss,
                                                  GList            *item);

static void         remove_stream                (OssBackend       *oss,
                                                  const gchar      *name);

static void         select_default_input_stream  (OssBackend       *oss);
static void         select_default_output_stream (OssBackend       *oss);

static void         free_stream_list             (OssBackend       *oss);

static gint         compare_devices              (gconstpointer     a,
                                                  gconstpointer     b);
static gint         compare_device_path          (gconstpointer     a,
                                                  gconstpointer     b);

static MateMixerBackendInfo info;

void
backend_module_init (GTypeModule *module)
{
    oss_backend_register_type (module);

    info.name          = BACKEND_NAME;
    info.priority      = BACKEND_PRIORITY;
    info.g_type        = OSS_TYPE_BACKEND;
    info.backend_flags = BACKEND_FLAGS;
    info.backend_type  = MATE_MIXER_BACKEND_OSS;
}

const MateMixerBackendInfo *backend_module_get_info (void)
{
    return &info;
}

static void
oss_backend_class_init (OssBackendClass *klass)
{
    GObjectClass          *object_class;
    MateMixerBackendClass *backend_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose  = oss_backend_dispose;
    object_class->finalize = oss_backend_finalize;

    backend_class = MATE_MIXER_BACKEND_CLASS (klass);
    backend_class->open         = oss_backend_open;
    backend_class->close        = oss_backend_close;
    backend_class->list_devices = oss_backend_list_devices;
    backend_class->list_streams = oss_backend_list_streams;

    g_type_class_add_private (object_class, sizeof (OssBackendPrivate));
}

/* Called in the code generated by G_DEFINE_DYNAMIC_TYPE() */
static void
oss_backend_class_finalize (OssBackendClass *klass)
{
}

static void
oss_backend_init (OssBackend *oss)
{
    oss->priv = G_TYPE_INSTANCE_GET_PRIVATE (oss,
                                             OSS_TYPE_BACKEND,
                                             OssBackendPrivate);

    oss->priv->devices_paths = g_hash_table_new_full (g_str_hash,
                                                      g_str_equal,
                                                      g_free,
                                                      NULL);
}

static void
oss_backend_dispose (GObject *object)
{
    MateMixerBackend *backend;
    MateMixerState    state;

    backend = MATE_MIXER_BACKEND (object);

    state = mate_mixer_backend_get_state (backend);
    if (state != MATE_MIXER_STATE_IDLE)
        oss_backend_close (backend);

    G_OBJECT_CLASS (oss_backend_parent_class)->dispose (object);
}

static void
oss_backend_finalize (GObject *object)
{
    OssBackend *oss;

    oss = OSS_BACKEND (object);

    g_hash_table_unref (oss->priv->devices_paths);

    G_OBJECT_CLASS (oss_backend_parent_class)->finalize (object);
}

static gboolean
oss_backend_open (MateMixerBackend *backend)
{
    OssBackend *oss;

    g_return_val_if_fail (OSS_IS_BACKEND (backend), FALSE);

    oss = OSS_BACKEND (backend);

    /* Discover added or removed OSS devices every second */
    oss->priv->timeout_source = g_timeout_source_new_seconds (1);
    g_source_set_callback (oss->priv->timeout_source,
                           (GSourceFunc) read_devices,
                           oss,
                           NULL);
    g_source_attach (oss->priv->timeout_source,
                     g_main_context_get_thread_default ());

    /* Read the initial list of devices so we have some starting point, there
     * isn't really a way to detect errors here, failing to add a device may
     * be a device-related problem so make the backend always open successfully */
    read_devices (oss);

    _mate_mixer_backend_set_state (backend, MATE_MIXER_STATE_READY);
    return TRUE;
}

void
oss_backend_close (MateMixerBackend *backend)
{
    OssBackend *oss;

    g_return_if_fail (OSS_IS_BACKEND (backend));

    oss = OSS_BACKEND (backend);

    g_source_destroy (oss->priv->timeout_source);

    if (oss->priv->devices != NULL) {
        g_list_free_full (oss->priv->devices, g_object_unref);
        oss->priv->devices = NULL;
    }
    if (oss->priv->default_device != NULL) {
        g_free (oss->priv->default_device);
        oss->priv->default_device = NULL;
    }

    free_stream_list (oss);

    g_hash_table_remove_all (oss->priv->devices_paths);

    _mate_mixer_backend_set_state (backend, MATE_MIXER_STATE_IDLE);
}

static const GList *
oss_backend_list_devices (MateMixerBackend *backend)
{
    g_return_val_if_fail (OSS_IS_BACKEND (backend), NULL);

    return OSS_BACKEND (backend)->priv->devices;
}

static const GList *
oss_backend_list_streams (MateMixerBackend *backend)
{
    OssBackend *oss;

    g_return_val_if_fail (OSS_IS_BACKEND (backend), NULL);

    oss = OSS_BACKEND (backend);

    if (oss->priv->streams == NULL) {
        GList *list;

        /* Walk through the list of devices and create the stream list, each
         * device has at most one input and one output stream */
        list = g_list_last (oss->priv->devices);

        while (list != NULL) {
            OssDevice *device = OSS_DEVICE (list->data);
            OssStream *stream;

            stream = oss_device_get_output_stream (device);
            if (stream != NULL)
                oss->priv->streams =
                    g_list_prepend (oss->priv->streams, g_object_ref (stream));

            stream = oss_device_get_input_stream (device);
            if (stream != NULL)
                oss->priv->streams =
                    g_list_prepend (oss->priv->streams, g_object_ref (stream));

            list = list->prev;
        }
    }
    return oss->priv->streams;
}

static gboolean
read_devices (OssBackend *oss)
{
    gint     i;
    gboolean added = FALSE;

    for (i = 0; i < OSS_MAX_DEVICES; i++) {
        gchar   *path;
        gboolean added_current;

        path = g_strdup_printf ("/dev/mixer%i", i);

        /* On recent FreeBSD both /dev/mixer and /dev/mixer0 point to the same
         * mixer device, on NetBSD and OpenBSD /dev/mixer is a symlink to one
         * of the real mixer device nodes, on Linux /dev/mixer is the first
         * device and /dev/mixer1 is the second device.
         * Handle all of these cases by trying /dev/mixer if /dev/mixer0 fails */
        if (read_device (oss, path, &added_current) == FALSE && i == 0)
            read_device (oss, "/dev/mixer", &added_current);

        if (added_current == TRUE)
            added = TRUE;

        g_free (path);
    }

    /* If any card has been added, make sure we have the most suitable default
     * input and output streams */
    if (added == TRUE) {
        select_default_input_stream (oss);
        select_default_output_stream (oss);
    }
    return G_SOURCE_CONTINUE;
}

static gboolean
read_device (OssBackend *oss, const gchar *path, gboolean *added)
{
    OssDevice *device;
    gint       fd;
    gchar     *bname;
    gchar     *label;

    *added = FALSE;

    fd = g_open (path, O_RDWR, 0);
    if (fd == -1) {
        if (errno != ENOENT && errno != ENXIO)
            g_debug ("%s: %s", path, g_strerror (errno));

        remove_device_by_path (oss, path);
        return FALSE;
    }

    /* Don't proceed if the device is already known, opening the device was
     * still tested to be absolutely sure that the device is removed it case
     * it has disappeared, but normally the device's polling facility should
     * handle this by itself */
    if (g_hash_table_contains (oss->priv->devices_paths, path) == TRUE) {
        close (fd);
        return TRUE;
    }

    bname = g_path_get_basename (path);
    label = read_device_label (oss, path, fd);

    device = oss_device_new (bname, label, path, fd);
    g_free (bname);
    g_free (label);

    close (fd);

    if G_LIKELY (device != NULL) {
        *added = oss_device_open (device);
        if (*added == TRUE)
            add_device (oss, device);
        else
            g_object_unref (device);
    }
    return *added;
}

static gchar *
read_device_label (OssBackend *oss, const gchar *path, gint fd)
{
    guint index;

#ifdef SOUND_MIXER_INFO
    do {
        struct mixer_info info;

        /* Prefer device name supplied by the system, but this calls fails
         * with EINVAL on FreeBSD */
        if (ioctl (fd, SOUND_MIXER_INFO, &info) == 0)
            return g_strdup (info.name);
    } while (0);
#endif

    index = (guint) g_ascii_strtoull (path + sizeof ("/dev/mixer") - 1,
                                      NULL,
                                      10);
#ifdef OSS_PATH_SNDSTAT
    /* If the ioctl doesn't succeed, assume that the mixer device number
     * matches the pcm number in the sndstat file, this is a bit desperate, but
     * it should be correct on FreeBSD */
    do {
        gchar *label;

        label = read_device_label_sndstat (oss, OSS_PATH_SNDSTAT, path, index);
        if (label != NULL)
            return label;
    } while (0);
#endif

    return g_strdup_printf (_("OSS Mixer %d"), index);
}

static gchar *
read_device_label_sndstat (OssBackend  *oss,
                           const gchar *sndstat,
                           const gchar *path,
                           guint        index)
{
    FILE  *fp;
    gchar *label = NULL;
    gchar *prefix;
    gchar  line[512];

    fp = g_fopen (sndstat, "r");
    if (fp == NULL) {
        g_debug ("Failed to open %s: %s", sndstat, g_strerror (errno));
        return NULL;
    }

    /* Example line:
     * pcm0: <ATI R6xx (HDMI)> (play) default */
    prefix = g_strdup_printf ("pcm%u: ", index);

    while (fgets (line, sizeof (line), fp) != NULL) {
        gchar *p;

        if (g_str_has_prefix (line, prefix) == FALSE)
            continue;

        p = strchr (line, '<');
        if (p != NULL && *p && *(++p)) {
            gchar *end = strchr (p, '>');

            if (end != NULL) {
                label = g_strndup (p, end - p);

                /* Normally the default OSS device is /dev/dsp, but on FreeBSD
                 * /dev/dsp doesn't physically exist on the filesystem, but is
                 * managed by the kernel according to the user-settable default
                 * device, in sndstat the default card definition ends with the
                 * word "default" */
                if (g_str_has_suffix (line, "default")) {
                    g_free (oss->priv->default_device);

                    oss->priv->default_device = g_strdup (path);
                }
            } else {
                g_debug ("Failed to read sndstat line: %s", line);
            }
            break;
        }
    }

    g_free (prefix);
    fclose (fp);

    return label;
}

static void
add_device (OssBackend *oss, OssDevice *device)
{
    /* Takes reference of device */
    oss->priv->devices =
        g_list_insert_sorted_with_data (oss->priv->devices,
                                        device,
                                        (GCompareDataFunc) compare_devices,
                                        NULL);

    /* Keep track of added device paths */
    g_hash_table_add (oss->priv->devices_paths,
                      g_strdup (oss_device_get_path (device)));

    g_signal_connect_swapped (G_OBJECT (device),
                              "closed",
                              G_CALLBACK (remove_device),
                              oss);
    g_signal_connect_swapped (G_OBJECT (device),
                              "stream-removed",
                              G_CALLBACK (remove_stream),
                              oss);

    g_signal_connect_swapped (G_OBJECT (device),
                              "closed",
                              G_CALLBACK (free_stream_list),
                              oss);
    g_signal_connect_swapped (G_OBJECT (device),
                              "stream-added",
                              G_CALLBACK (free_stream_list),
                              oss);
    g_signal_connect_swapped (G_OBJECT (device),
                              "stream-removed",
                              G_CALLBACK (free_stream_list),
                              oss);

    g_signal_emit_by_name (G_OBJECT (oss),
                           "device-added",
                           mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));

    /* Load the device elements after emitting device-added, because the load
     * function will most likely emit stream-added on the device and backend */
    oss_device_load (device);
}

static void
remove_device (OssBackend *oss, OssDevice *device)
{
    GList *item;

    item = g_list_find (oss->priv->devices, device);
    if (item != NULL)
        remove_device_by_list_item (oss, item);
}

static void
remove_device_by_path (OssBackend *oss, const gchar *path)
{
    GList *item;

    item = g_list_find_custom (oss->priv->devices, path, compare_device_path);
    if (item != NULL)
        remove_device_by_list_item (oss, item);
}

static void
remove_device_by_list_item (OssBackend *oss, GList *item)
{
    OssDevice   *device;
    const gchar *path;

    device = OSS_DEVICE (item->data);

    g_signal_handlers_disconnect_by_func (G_OBJECT (device),
                                          G_CALLBACK (remove_device),
                                          oss);

    /* May emit removed signals */
    if (oss_device_is_open (device) == TRUE)
        oss_device_close (device);

    g_signal_handlers_disconnect_by_data (G_OBJECT (device),
                                          oss);

    oss->priv->devices = g_list_delete_link (oss->priv->devices, item);

    path = oss_device_get_path (device);
    g_hash_table_remove (oss->priv->devices_paths, path);

    if (g_strcmp0 (oss->priv->default_device, path) == 0) {
        g_free (oss->priv->default_device);
        oss->priv->default_device = NULL;
    }

    /* The list may have been invalidated by device signals */
    free_stream_list (oss);

    g_signal_emit_by_name (G_OBJECT (oss),
                           "device-removed",
                           mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));

    g_object_unref (device);
}

static void
remove_stream (OssBackend *oss, const gchar *name)
{
    MateMixerStream *stream;

    stream = mate_mixer_backend_get_default_input_stream (MATE_MIXER_BACKEND (oss));

    if (stream != NULL && strcmp (mate_mixer_stream_get_name (stream), name) == 0)
        select_default_input_stream (oss);

    stream = mate_mixer_backend_get_default_output_stream (MATE_MIXER_BACKEND (oss));

    if (stream != NULL && strcmp (mate_mixer_stream_get_name (stream), name) == 0)
        select_default_output_stream (oss);
}

static OssDevice *
get_default_device (OssBackend *oss)
{
    GList *item;

    if (oss->priv->default_device == NULL)
        return NULL;

    item = g_list_find_custom (oss->priv->devices,
                               oss->priv->default_device,
                               compare_device_path);
    if G_LIKELY (item != NULL)
        return OSS_DEVICE (item->data);

    return NULL;
}

static void
select_default_input_stream (OssBackend *oss)
{
    OssDevice *device;
    OssStream *stream;
    GList     *list;

    device = get_default_device (oss);
    if (device != NULL) {
        stream = oss_device_get_input_stream (device);
        if (stream != NULL) {
            _mate_mixer_backend_set_default_input_stream (MATE_MIXER_BACKEND (oss),
                                                          MATE_MIXER_STREAM (stream));
            return;
        }
    }

    list = oss->priv->devices;
    while (list != NULL) {
        device = OSS_DEVICE (list->data);
        stream = oss_device_get_input_stream (device);

        if (stream != NULL) {
            _mate_mixer_backend_set_default_input_stream (MATE_MIXER_BACKEND (oss),
                                                          MATE_MIXER_STREAM (stream));
            return;
        }
        list = list->next;
    }

    /* In the worst case unset the default stream */
    _mate_mixer_backend_set_default_input_stream (MATE_MIXER_BACKEND (oss), NULL);
}

static void
select_default_output_stream (OssBackend *oss)
{
    OssDevice *device;
    OssStream *stream;
    GList     *list;

    device = get_default_device (oss);
    if (device != NULL) {
        stream = oss_device_get_output_stream (device);
        if (stream != NULL) {
            _mate_mixer_backend_set_default_output_stream (MATE_MIXER_BACKEND (oss),
                                                           MATE_MIXER_STREAM (stream));
            return;
        }
    }

    list = oss->priv->devices;
    while (list != NULL) {
        device = OSS_DEVICE (list->data);
        stream = oss_device_get_output_stream (device);

        if (stream != NULL) {
            _mate_mixer_backend_set_default_output_stream (MATE_MIXER_BACKEND (oss),
                                                           MATE_MIXER_STREAM (stream));
            return;
        }
        list = list->next;
    }

    /* In the worst case unset the default stream */
    _mate_mixer_backend_set_default_output_stream (MATE_MIXER_BACKEND (oss), NULL);
}

static void
free_stream_list (OssBackend *oss)
{
    if (oss->priv->streams == NULL)
        return;

    g_list_free_full (oss->priv->streams, g_object_unref);

    oss->priv->streams = NULL;
}

static gint
compare_devices (gconstpointer a, gconstpointer b)
{
    MateMixerDevice *d1 = MATE_MIXER_DEVICE (a);
    MateMixerDevice *d2 = MATE_MIXER_DEVICE (b);

    return strcmp (mate_mixer_device_get_name (d1), mate_mixer_device_get_name (d2));
}

static gint
compare_device_path (gconstpointer a, gconstpointer b)
{
    OssDevice   *device = OSS_DEVICE (a);
    const gchar *path   = (const gchar *) b;

    return strcmp (oss_device_get_path (device), path);
}
