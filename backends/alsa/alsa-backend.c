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

#include "alsa-backend.h"
#include "alsa-device.h"
#include "alsa-stream.h"

#define BACKEND_NAME      "ALSA"
#define BACKEND_PRIORITY  20
#define BACKEND_FLAGS     MATE_MIXER_BACKEND_NO_FLAGS

#define ALSA_DEVICE_GET_ID(d)                                               \
        (g_object_get_data (G_OBJECT (d), "__matemixer_alsa_device_id"))

#define ALSA_DEVICE_SET_ID(d,id)                                            \
        (g_object_set_data_full (G_OBJECT (d),                              \
                                 "__matemixer_alsa_device_id",              \
                                 g_strdup (id),                             \
                                 g_free))

struct _AlsaBackendPrivate
{
    GSource    *timeout_source;
    GList      *streams;
    GList      *devices;
    GHashTable *devices_ids;
};

static void alsa_backend_class_init     (AlsaBackendClass *klass);
static void alsa_backend_class_finalize (AlsaBackendClass *klass);
static void alsa_backend_init           (AlsaBackend      *alsa);
static void alsa_backend_dispose        (GObject          *object);
static void alsa_backend_finalize       (GObject          *object);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
G_DEFINE_DYNAMIC_TYPE (AlsaBackend, alsa_backend, MATE_MIXER_TYPE_BACKEND)
#pragma clang diagnostic pop

static gboolean     alsa_backend_open            (MateMixerBackend *backend);
static void         alsa_backend_close           (MateMixerBackend *backend);
static const GList *alsa_backend_list_devices    (MateMixerBackend *backend);
static const GList *alsa_backend_list_streams    (MateMixerBackend *backend);

static gboolean     read_devices                 (AlsaBackend      *alsa);

static gboolean     read_device                  (AlsaBackend      *alsa,
                                                  const gchar      *card);

static void         add_device                   (AlsaBackend      *alsa,
                                                  AlsaDevice       *device);

static void         remove_device                (AlsaBackend      *alsa,
                                                  AlsaDevice       *device);
static void         remove_device_by_name        (AlsaBackend      *alsa,
                                                  const gchar      *name);
static void         remove_device_by_list_item   (AlsaBackend      *alsa,
                                                  GList            *item);

static void         remove_stream                (AlsaBackend      *alsa,
                                                  const gchar      *name);

static void         select_default_input_stream  (AlsaBackend      *alsa);
static void         select_default_output_stream (AlsaBackend      *alsa);

static void         free_stream_list             (AlsaBackend      *alsa);

static gint         compare_devices              (gconstpointer     a,
                                                  gconstpointer     b);
static gint         compare_device_name          (gconstpointer     a,
                                                  gconstpointer     b);

static MateMixerBackendInfo info;

void
backend_module_init (GTypeModule *module)
{
    alsa_backend_register_type (module);

    info.name          = BACKEND_NAME;
    info.priority      = BACKEND_PRIORITY;
    info.g_type        = ALSA_TYPE_BACKEND;
    info.backend_flags = BACKEND_FLAGS;
    info.backend_type  = MATE_MIXER_BACKEND_ALSA;
}

const MateMixerBackendInfo *backend_module_get_info (void)
{
    return &info;
}

static void
alsa_backend_class_init (AlsaBackendClass *klass)
{
    GObjectClass          *object_class;
    MateMixerBackendClass *backend_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose  = alsa_backend_dispose;
    object_class->finalize = alsa_backend_finalize;

    backend_class = MATE_MIXER_BACKEND_CLASS (klass);
    backend_class->open         = alsa_backend_open;
    backend_class->close        = alsa_backend_close;
    backend_class->list_devices = alsa_backend_list_devices;
    backend_class->list_streams = alsa_backend_list_streams;

    g_type_class_add_private (object_class, sizeof (AlsaBackendPrivate));
}

/* Called in the code generated by G_DEFINE_DYNAMIC_TYPE() */
static void
alsa_backend_class_finalize (AlsaBackendClass *klass)
{
}

static void
alsa_backend_init (AlsaBackend *alsa)
{
    alsa->priv = G_TYPE_INSTANCE_GET_PRIVATE (alsa,
                                             ALSA_TYPE_BACKEND,
                                             AlsaBackendPrivate);

    alsa->priv->devices_ids = g_hash_table_new_full (g_str_hash,
                                                     g_str_equal,
                                                     g_free,
                                                     NULL);
}

static void
alsa_backend_dispose (GObject *object)
{
    MateMixerBackend *backend;
    MateMixerState    state;

    backend = MATE_MIXER_BACKEND (object);

    state = mate_mixer_backend_get_state (backend);
    if (state != MATE_MIXER_STATE_IDLE)
        alsa_backend_close (backend);

    G_OBJECT_CLASS (alsa_backend_parent_class)->dispose (object);
}

static void
alsa_backend_finalize (GObject *object)
{
    AlsaBackend *alsa;

    alsa = ALSA_BACKEND (object);

    g_hash_table_unref (alsa->priv->devices_ids);

    G_OBJECT_CLASS (alsa_backend_parent_class)->finalize (object);
}

static gboolean
alsa_backend_open (MateMixerBackend *backend)
{
    AlsaBackend *alsa;

    g_return_val_if_fail (ALSA_IS_BACKEND (backend), FALSE);

    alsa = ALSA_BACKEND (backend);

    /* Poll ALSA for changes every second, this only discovers added or removed
     * sound cards, sound card related events are handled by AlsaDevices */
    alsa->priv->timeout_source = g_timeout_source_new_seconds (1);
    g_source_set_callback (alsa->priv->timeout_source,
                           (GSourceFunc) read_devices,
                           alsa,
                           NULL);
    g_source_attach (alsa->priv->timeout_source,
                     g_main_context_get_thread_default ());

    /* Read the initial list of devices so we have some starting point, there
     * isn't really a way to detect errors here, failing to add a device may
     * be a device-related problem so make the backend always open successfully */
    read_devices (alsa);

    _mate_mixer_backend_set_state (backend, MATE_MIXER_STATE_READY);
    return TRUE;
}

void
alsa_backend_close (MateMixerBackend *backend)
{
    AlsaBackend *alsa;

    g_return_if_fail (ALSA_IS_BACKEND (backend));

    alsa = ALSA_BACKEND (backend);

    g_source_destroy (alsa->priv->timeout_source);

    if (alsa->priv->devices != NULL) {
        g_list_free_full (alsa->priv->devices, g_object_unref);
        alsa->priv->devices = NULL;
    }

    free_stream_list (alsa);

    g_hash_table_remove_all (alsa->priv->devices_ids);

    _mate_mixer_backend_set_state (backend, MATE_MIXER_STATE_IDLE);
}

static const GList *
alsa_backend_list_devices (MateMixerBackend *backend)
{
    g_return_val_if_fail (ALSA_IS_BACKEND (backend), NULL);

    return ALSA_BACKEND (backend)->priv->devices;
}

static const GList *
alsa_backend_list_streams (MateMixerBackend *backend)
{
    AlsaBackend *alsa;

    g_return_val_if_fail (ALSA_IS_BACKEND (backend), NULL);

    alsa = ALSA_BACKEND (backend);

    if (alsa->priv->streams == NULL) {
        GList *list;

        /* Walk through the list of devices and create the stream list, each
         * device has at most one input and one output stream */
        list = g_list_last (alsa->priv->devices);

        while (list != NULL) {
            AlsaDevice *device = ALSA_DEVICE (list->data);
            AlsaStream *stream;

            stream = alsa_device_get_output_stream (device);
            if (stream != NULL)
                alsa->priv->streams =
                    g_list_prepend (alsa->priv->streams, g_object_ref (stream));

            stream = alsa_device_get_input_stream (device);
            if (stream != NULL)
                alsa->priv->streams =
                    g_list_prepend (alsa->priv->streams, g_object_ref (stream));

            list = list->prev;
        }
    }
    return alsa->priv->streams;
}

static gboolean
read_devices (AlsaBackend *alsa)
{
    gint     num;
    gint     ret;
    gchar    card[16];
    gboolean added = FALSE;

    /* Read the default device first, it will be either one of the hardware cards
     * that will be queried later, or a software mixer */
    if (read_device (alsa, "default") == TRUE)
        added = TRUE;

    for (num = -1;;) {
        /* Read number of the next sound card */
        ret = snd_card_next (&num);
        if (ret < 0 ||
            num < 0)
            break;

        g_snprintf (card, sizeof (card), "hw:%d", num);

        if (read_device (alsa, card) == TRUE)
            added = TRUE;
    }

    /* If any card has been added, make sure we have the most suitable default
     * input and output streams */
    if (added == TRUE) {
        select_default_input_stream (alsa);
        select_default_output_stream (alsa);
    }
    return G_SOURCE_CONTINUE;
}

static gboolean
read_device (AlsaBackend *alsa, const gchar *card)
{
    AlsaDevice          *device;
    snd_ctl_t           *ctl;
    snd_ctl_card_info_t *info;
    const gchar         *id;
    gint                 ret;

    /*
     * The device may be already known.
     *
     * Make sure it is removed from the list of devices if it fails to be
     * read. This commonly happens with the "default" device, which is not
     * reassigned by ALSA when the sound card is removed or the sound mixer
     * quits.
    */
    ret = snd_ctl_open (&ctl, card, 0);
    if (ret < 0) {
        remove_device_by_name (alsa, card);
        return FALSE;
    }

    snd_ctl_card_info_alloca (&info);

    ret = snd_ctl_card_info (ctl, info);
    if (ret < 0) {
        g_warning ("Failed to read card info: %s", snd_strerror (ret));

        remove_device_by_name (alsa, card);
        snd_ctl_close (ctl);
        return FALSE;
    }

    id = snd_ctl_card_info_get_id (info);

    /* We also keep a list of device identifiers to be sure no card is
     * added twice, this could commonly happen because some card may
     * also be assigned to the "default" ALSA device */
    if (g_hash_table_contains (alsa->priv->devices_ids, id) == TRUE) {
        snd_ctl_close (ctl);
        return FALSE;
    }

    device = alsa_device_new (card, snd_ctl_card_info_get_name (info));

    if (alsa_device_open (device) == FALSE) {
        g_object_unref (device);
        snd_ctl_close (ctl);
        return FALSE;
    }

    ALSA_DEVICE_SET_ID (device, id);
    add_device (alsa, device);

    snd_ctl_close (ctl);
    return TRUE;
}

static void
add_device (AlsaBackend *alsa, AlsaDevice *device)
{
    /* Takes reference of device */
    alsa->priv->devices =
        g_list_insert_sorted_with_data (alsa->priv->devices,
                                        device,
                                        (GCompareDataFunc) compare_devices,
                                        NULL);

    /* Keep track of device identifiers */
    g_hash_table_add (alsa->priv->devices_ids, g_strdup (ALSA_DEVICE_GET_ID (device)));

    g_signal_connect_swapped (G_OBJECT (device),
                              "closed",
                              G_CALLBACK (remove_device),
                              alsa);
    g_signal_connect_swapped (G_OBJECT (device),
                              "stream-removed",
                              G_CALLBACK (remove_stream),
                              alsa);

    g_signal_connect_swapped (G_OBJECT (device),
                              "closed",
                              G_CALLBACK (free_stream_list),
                              alsa);
    g_signal_connect_swapped (G_OBJECT (device),
                              "stream-added",
                              G_CALLBACK (free_stream_list),
                              alsa);
    g_signal_connect_swapped (G_OBJECT (device),
                              "stream-removed",
                              G_CALLBACK (free_stream_list),
                              alsa);

    g_signal_emit_by_name (G_OBJECT (alsa),
                           "device-added",
                           mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));

    /* Load the device elements after emitting device-added, because the load
     * function will most likely emit stream-added on the device and backend */
    alsa_device_load (device);
}

static void
remove_device (AlsaBackend *alsa, AlsaDevice *device)
{
    GList *item;

    item = g_list_find (alsa->priv->devices, device);
    if (item != NULL)
        remove_device_by_list_item (alsa, item);
}

static void
remove_device_by_name (AlsaBackend *alsa, const gchar *name)
{
    GList *item;

    item = g_list_find_custom (alsa->priv->devices, name, compare_device_name);
    if (item != NULL)
        remove_device_by_list_item (alsa, item);
}

static void
remove_device_by_list_item (AlsaBackend *alsa, GList *item)
{
    AlsaDevice *device;

    device = ALSA_DEVICE (item->data);

    g_signal_handlers_disconnect_by_func (G_OBJECT (device),
                                          G_CALLBACK (remove_device),
                                          alsa);

    /* May emit removed signals */
    if (alsa_device_is_open (device) == TRUE)
        alsa_device_close (device);

    g_signal_handlers_disconnect_by_data (G_OBJECT (device),
                                          alsa);

    alsa->priv->devices = g_list_delete_link (alsa->priv->devices, item);

    g_hash_table_remove (alsa->priv->devices_ids,
                         ALSA_DEVICE_GET_ID (device));

    /* The list may have been invalidated by device signals */
    free_stream_list (alsa);

    g_signal_emit_by_name (G_OBJECT (alsa),
                           "device-removed",
                           mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));

    g_object_unref (device);
}

static void
remove_stream (AlsaBackend *alsa, const gchar *name)
{
    MateMixerStream *stream;

    stream = mate_mixer_backend_get_default_input_stream (MATE_MIXER_BACKEND (alsa));

    if (stream != NULL && strcmp (mate_mixer_stream_get_name (stream), name) == 0)
        select_default_input_stream (alsa);

    stream = mate_mixer_backend_get_default_output_stream (MATE_MIXER_BACKEND (alsa));

    if (stream != NULL && strcmp (mate_mixer_stream_get_name (stream), name) == 0)
        select_default_output_stream (alsa);
}

static void
select_default_input_stream (AlsaBackend *alsa)
{
    GList *list;

    list = alsa->priv->devices;
    while (list != NULL) {
        AlsaDevice *device = ALSA_DEVICE (list->data);
        AlsaStream *stream = alsa_device_get_input_stream (device);

        if (stream != NULL) {
            _mate_mixer_backend_set_default_input_stream (MATE_MIXER_BACKEND (alsa),
                                                          MATE_MIXER_STREAM (stream));
            return;
        }
        list = list->next;
    }

    /* In the worst case unset the default stream */
    _mate_mixer_backend_set_default_input_stream (MATE_MIXER_BACKEND (alsa), NULL);
}

static void
select_default_output_stream (AlsaBackend *alsa)
{
    GList *list;

    list = alsa->priv->devices;
    while (list != NULL) {
        AlsaDevice *device = ALSA_DEVICE (list->data);
        AlsaStream *stream = alsa_device_get_output_stream (device);

        if (stream != NULL) {
            _mate_mixer_backend_set_default_output_stream (MATE_MIXER_BACKEND (alsa),
                                                           MATE_MIXER_STREAM (stream));
            return;
        }
        list = list->next;
    }

    /* In the worst case unset the default stream */
    _mate_mixer_backend_set_default_output_stream (MATE_MIXER_BACKEND (alsa), NULL);
}

static void
free_stream_list (AlsaBackend *alsa)
{
    if (alsa->priv->streams == NULL)
        return;

    g_list_free_full (alsa->priv->streams, g_object_unref);

    alsa->priv->streams = NULL;
}

static gint
compare_devices (gconstpointer a, gconstpointer b)
{
    MateMixerDevice *d1 = MATE_MIXER_DEVICE (a);
    MateMixerDevice *d2 = MATE_MIXER_DEVICE (b);

    return strcmp (mate_mixer_device_get_name (d1), mate_mixer_device_get_name (d2));
}

static gint
compare_device_name (gconstpointer a, gconstpointer b)
{
    MateMixerDevice *device = MATE_MIXER_DEVICE (a);
    const gchar     *name   = (const gchar *) b;

    return strcmp (mate_mixer_device_get_name (device), name);
}
