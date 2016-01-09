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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#ifdef HAVE_SYSCTLBYNAME
#include <sys/sysctl.h>
#endif

#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include "oss-backend.h"
#include "oss-common.h"
#include "oss-device.h"
#include "oss-stream.h"

#define BACKEND_NAME      "OSS"
#define BACKEND_PRIORITY  10
#define BACKEND_FLAGS     MATE_MIXER_BACKEND_NO_FLAGS

#define OSS_MAX_DEVICES   32

struct _OssBackendPrivate
{
    OssDevice  *default_device;
    GSource    *timeout_source;
    GSource    *timeout_source_default;
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
                                                  const gchar      *path);

static gchar *      read_device_label            (OssBackend       *oss,
                                                  const gchar      *path,
                                                  gint              fd);

#ifdef HAVE_SYSCTLBYNAME
static gboolean     read_device_default_unit     (OssBackend       *oss);
#endif

static void         add_device                   (OssBackend       *oss,
                                                  OssDevice        *device);
static void         remove_device                (OssBackend       *oss,
                                                  OssDevice        *device);

static void         remove_device_by_path        (OssBackend       *oss,
                                                  const gchar      *path);

static void         remove_device_by_list_item   (OssBackend       *oss,
                                                  GList            *item);

static void         remove_stream                (OssBackend       *oss,
                                                  MateMixerStream  *stream);

static void         select_default_input_stream  (OssBackend       *oss);
static void         select_default_output_stream (OssBackend       *oss);

static void         set_default_device           (OssBackend       *oss,
                                                  OssDevice        *device);
static void         set_default_device_index     (OssBackend       *oss,
                                                  guint             index,
                                                  gboolean          fallback) G_GNUC_UNUSED;

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

    /*
     * Read the initial list of devices so we have some starting point, there
     * isn't really a way to detect errors here, failing to add a device may
     * be a device-related problem so make the backend always open successfully.
     */
    read_devices (oss);

    /*
     * Try to read the current default device and if the sysctl call succeeds,
     * poll OSS for changes every second.
     *
     * This feature is only available on FreeBSD and probably some derived
     * systems.
     */
#ifdef HAVE_SYSCTLBYNAME
    do {
        int    unit;
        int    ret;
        size_t len = sizeof (unit);

        ret = sysctlbyname ("hw.snd.default_unit", &unit, &len, NULL, 0);
        if (ret == 0) {
            set_default_device_index (oss, unit, TRUE);

            oss->priv->timeout_source_default = g_timeout_source_new_seconds (1);
            g_source_set_callback (oss->priv->timeout_source_default,
                                   (GSourceFunc) read_device_default_unit,
                                   oss,
                                   NULL);
            g_source_attach (oss->priv->timeout_source_default,
                             g_main_context_get_thread_default ());
        }
    } while (0);
#endif /* HAVE_SYSCTLBYNAME */

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

    if (oss->priv->timeout_source_default != NULL)
        g_source_destroy (oss->priv->timeout_source_default);

    _mate_mixer_clear_object_list (&oss->priv->devices);
    _mate_mixer_clear_object_list (&oss->priv->streams);

    g_clear_object (&oss->priv->default_device);
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
    guint i;

    for (i = 0; i < OSS_MAX_DEVICES; i++) {
        gchar *path;

        path = g_strdup_printf ("/dev/mixer%u", i);

        /*
         * On FreeBSD both /dev/mixer and /dev/mixer0 point to the same mixer
         * device.
         * On NetBSD and OpenBSD /dev/mixer is a symlink to one of the real mixer
         * device nodes.
         * On Linux /dev/mixer is the first device and /dev/mixer1 is the second
         * device.
         *
         * Handle all of these cases by trying /dev/mixer if /dev/mixer0 fails.
         */
        if (read_device (oss, path) == FALSE && i == 0)
            read_device (oss, "/dev/mixer");

        g_free (path);
    }

    /* Select the first device as default if one is not selected already */
    if (oss->priv->default_device == NULL) {
        OssDevice *first = NULL;

        if (oss->priv->devices != NULL)
            first = OSS_DEVICE (oss->priv->devices->data);

        set_default_device (oss, first);
    }

    return G_SOURCE_CONTINUE;
}

static gboolean
read_device (OssBackend *oss, const gchar *path)
{
    OssDevice *device;
    gint       fd;
    gchar     *bname;
    gchar     *label;

    fd = g_open (path, O_RDWR, 0);
    if (fd == -1) {
        if (errno != ENOENT && errno != ENXIO)
            g_debug ("%s: %s", path, g_strerror (errno));

        remove_device_by_path (oss, path);
        return FALSE;
    }

    /*
     * Don't proceed if the device is already known.
     *
     * Opening the device was still tested to be absolutely sure that the
     * device is removed it case it has disappeared, but normally the
     * device's polling facility should handle this by itself.
     */
    if (g_hash_table_contains (oss->priv->devices_paths, path) == TRUE) {
        g_close (fd, NULL);
        return TRUE;
    }

    bname = g_path_get_basename (path);
    label = read_device_label (oss, path, fd);

    device = oss_device_new (bname, label, path, fd);
    g_free (bname);
    g_free (label);
    g_close (fd, NULL);

    if G_LIKELY (device != NULL) {
        if (oss_device_open (device) == TRUE) {
            add_device (oss, device);
            return TRUE;
        }

        g_object_unref (device);
    }
    return FALSE;
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
#endif /* SOUND_MIXER_INFO */

    index = (guint) g_ascii_strtoull (path + sizeof ("/dev/mixer") - 1,
                                      NULL,
                                      10);
#ifdef HAVE_SYSCTLBYNAME
    /*
     * Older FreeBSDs don't support the ioctl.
     *
     * Assume that the mixer device number matches the PCM device number and
     * use sysctl to figure out the name of the device.
     */
    do {
        gchar *name;
        char   label[255];
        int    ret;
        size_t len = sizeof (label);

        name = g_strdup_printf ("dev.pcm.%d.%%desc", index);

        ret = sysctlbyname (name, &label, &len, NULL, 0);
        g_free (name);

        if (ret == 0)
            return g_strndup (label, len);
    } while (0);
#endif /* HAVE_SYSCTLBYNAME */

    return g_strdup_printf (_("OSS Mixer %d"), index);
}

#ifdef HAVE_SYSCTLBYNAME
static gboolean
read_device_default_unit (OssBackend *oss)
{
    int    unit;
    int    ret;
    size_t len = sizeof (unit);

    ret = sysctlbyname ("hw.snd.default_unit", &unit, &len, NULL, 0);
    if (ret == 0)
        set_default_device_index (oss, unit, TRUE);

    return G_SOURCE_CONTINUE;
}
#endif /* HAVE_SYSCTLBYNAME */

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
                           MATE_MIXER_DEVICE (device));

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
    OssDevice *device;

    device = OSS_DEVICE (item->data);

    g_signal_handlers_disconnect_by_func (G_OBJECT (device),
                                          G_CALLBACK (remove_device),
                                          oss);

    oss->priv->devices = g_list_delete_link (oss->priv->devices, item);

    g_hash_table_remove (oss->priv->devices_paths,
                         oss_device_get_path (device));

    /* If the removed device is the current default device, change the default
     * to the first device in the internal list */
    if (device == oss->priv->default_device) {
        OssDevice *first = NULL;

        if (oss->priv->devices != NULL)
            first = OSS_DEVICE (oss->priv->devices->data);

        set_default_device (oss, first);
    }

    /* Closing a device emits stream-removed signals, close after fixing the
     * default device to avoid re-validating default streams as they get
     * removed */
    oss_device_close (device);

    g_signal_handlers_disconnect_by_data (G_OBJECT (device), oss);

    g_signal_emit_by_name (G_OBJECT (oss),
                           "device-removed",
                           MATE_MIXER_DEVICE (device));

    g_object_unref (device);
}

static void
remove_stream (OssBackend *oss, MateMixerStream *stream)
{
    MateMixerStream *def;

    def = mate_mixer_backend_get_default_input_stream (MATE_MIXER_BACKEND (oss));
    if (def == stream)
        select_default_input_stream (oss);

    def = mate_mixer_backend_get_default_output_stream (MATE_MIXER_BACKEND (oss));
    if (def == stream)
        select_default_output_stream (oss);
}

static void
select_default_input_stream (OssBackend *oss)
{
    OssStream *stream;
    GList     *list;

    /* First try the default device */
    if (oss->priv->default_device != NULL) {
        stream = oss_device_get_input_stream (oss->priv->default_device);
        if (stream != NULL) {
            _mate_mixer_backend_set_default_input_stream (MATE_MIXER_BACKEND (oss),
                                                          MATE_MIXER_STREAM (stream));
            return;
        }
    }

    list = oss->priv->devices;
    while (list != NULL) {
        OssDevice *device = OSS_DEVICE (list->data);

        if (device != oss->priv->default_device) {
            stream = oss_device_get_input_stream (device);

            if (stream != NULL) {
                _mate_mixer_backend_set_default_input_stream (MATE_MIXER_BACKEND (oss),
                                                              MATE_MIXER_STREAM (stream));
                return;
            }
        }
        list = list->next;
    }

    /* In the worst case unset the default stream */
    _mate_mixer_backend_set_default_input_stream (MATE_MIXER_BACKEND (oss), NULL);
}

static void
select_default_output_stream (OssBackend *oss)
{
    OssStream *stream;
    GList     *list;

    /* First try the default device */
    if (oss->priv->default_device != NULL) {
        stream = oss_device_get_output_stream (oss->priv->default_device);
        if (stream != NULL) {
            _mate_mixer_backend_set_default_output_stream (MATE_MIXER_BACKEND (oss),
                                                           MATE_MIXER_STREAM (stream));
            return;
        }
    }

    list = oss->priv->devices;
    while (list != NULL) {
        OssDevice *device = OSS_DEVICE (list->data);

        if (device != oss->priv->default_device) {
            stream = oss_device_get_output_stream (device);

            if (stream != NULL) {
                _mate_mixer_backend_set_default_output_stream (MATE_MIXER_BACKEND (oss),
                                                               MATE_MIXER_STREAM (stream));
                return;
            }
        }
        list = list->next;
    }

    /* In the worst case unset the default stream */
    _mate_mixer_backend_set_default_output_stream (MATE_MIXER_BACKEND (oss), NULL);
}

static void
set_default_device (OssBackend *oss, OssDevice *device)
{
    if (oss->priv->default_device == device)
        return;

    g_clear_object (&oss->priv->default_device);

    if (device != NULL) {
        oss->priv->default_device = g_object_ref (device);

        g_debug ("Default device changed to %s",
                 mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));
    } else
        g_debug ("Default device unset");

    select_default_input_stream (oss);
    select_default_output_stream (oss);
}

static void
set_default_device_index (OssBackend *oss, guint index, gboolean fallback)
{
    gchar           *name;
    MateMixerDevice *device;

    name = g_strdup_printf ("mixer%u", index);

    /* Find the device with matching index */
    device = mate_mixer_backend_get_device (MATE_MIXER_BACKEND (oss), name);
    if (device == NULL && index == 0)
        device = mate_mixer_backend_get_device (MATE_MIXER_BACKEND (oss), "mixer");

    g_free (name);

    if (device != NULL) {
        set_default_device (oss, OSS_DEVICE (device));
        return;
    }
    /* Fallback causes the first available device to be set in case device
     * with the requested index is not available */
    if (fallback == TRUE && oss->priv->devices != NULL) {
        set_default_device (oss, OSS_DEVICE (oss->priv->devices->data));
        return;
    }

    set_default_device (oss, NULL);
}

static void
free_stream_list (OssBackend *oss)
{
    _mate_mixer_clear_object_list (&oss->priv->streams);
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
