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

#include <strings.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <alsa/asoundlib.h>
#include <libmatemixer/matemixer.h>

#include "alsa-constants.h"
#include "alsa-device.h"
#include "alsa-element.h"
#include "alsa-stream.h"
#include "alsa-stream-control.h"
#include "alsa-stream-input-control.h"
#include "alsa-stream-output-control.h"
#include "alsa-switch.h"
#include "alsa-switch-option.h"
#include "alsa-toggle.h"

#define ALSA_DEVICE_ICON "audio-card"

struct _AlsaDevicePrivate
{
    snd_mixer_t  *handle;
    GMainContext *context;
    GMutex        mutex;
    GCond         cond;
    AlsaStream   *input;
    AlsaStream   *output;
    GHashTable   *switches;
    gboolean      events_pending;
};

enum {
    CLOSED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void alsa_device_class_init (AlsaDeviceClass *klass);
static void alsa_device_init       (AlsaDevice      *device);
static void alsa_device_dispose    (GObject         *object);
static void alsa_device_finalize   (GObject         *object);

G_DEFINE_TYPE (AlsaDevice, alsa_device, MATE_MIXER_TYPE_DEVICE)

static MateMixerSwitch *alsa_device_get_switch    (MateMixerDevice            *mmd,
                                                   const gchar                *name);

static GList *          alsa_device_list_streams  (MateMixerDevice            *mmd);
static GList *          alsa_device_list_switches (MateMixerDevice            *mmd);

static gboolean         add_stream_input_control  (AlsaDevice                 *device,
                                                   snd_mixer_elem_t           *el);
static gboolean         add_stream_output_control (AlsaDevice                 *device,
                                                   snd_mixer_elem_t           *el);

static gboolean         add_switch                (AlsaDevice                 *device,
                                                   AlsaStream                 *stream,
                                                   snd_mixer_elem_t           *el);

static gboolean         add_device_switch         (AlsaDevice                 *device,
                                                   snd_mixer_elem_t           *el);

static gboolean         add_stream_input_switch   (AlsaDevice                 *device,
                                                   snd_mixer_elem_t           *el);
static gboolean         add_stream_output_switch  (AlsaDevice                 *device,
                                                   snd_mixer_elem_t           *el);

static gboolean         add_stream_input_toggle   (AlsaDevice                 *device,
                                                   snd_mixer_elem_t           *el);
static gboolean         add_stream_output_toggle  (AlsaDevice                 *device,
                                                   snd_mixer_elem_t           *el);

static void             load_element              (AlsaDevice                 *device,
                                                   snd_mixer_elem_t           *el);

static void             load_elements_by_name     (AlsaDevice                 *device,
                                                   const gchar                *name);

static void             remove_elements_by_name   (AlsaDevice                 *device,
                                                   const gchar                *name);

static void             handle_poll               (AlsaDevice                 *device);

static gboolean         handle_process_events     (AlsaDevice                 *device);

static int              handle_callback           (snd_mixer_t                *handle,
                                                   guint                       mask,
                                                   snd_mixer_elem_t           *el);
static int              handle_element_callback   (snd_mixer_elem_t           *el,
                                                   guint                       mask);

static void             close_device              (AlsaDevice                 *device);

static gchar *          get_element_name          (snd_mixer_elem_t           *el);
static void             get_control_info          (snd_mixer_elem_t           *el,
                                                   gchar                     **name,
                                                   gchar                     **label,
                                                   MateMixerStreamControlRole *role);

static void             get_switch_info           (snd_mixer_elem_t           *el,
                                                   gchar                     **name,
                                                   gchar                     **label);

static void
alsa_device_class_init (AlsaDeviceClass *klass)
{
    GObjectClass         *object_class;
    MateMixerDeviceClass *device_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose  = alsa_device_dispose;
    object_class->finalize = alsa_device_finalize;

    device_class = MATE_MIXER_DEVICE_CLASS (klass);
    device_class->get_switch    = alsa_device_get_switch;
    device_class->list_streams  = alsa_device_list_streams;
    device_class->list_switches = alsa_device_list_switches;

    signals[CLOSED] =
        g_signal_new ("closed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AlsaDeviceClass, closed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0,
                      G_TYPE_NONE);

    g_type_class_add_private (object_class, sizeof (AlsaDevicePrivate));
}

static void
alsa_device_init (AlsaDevice *device)
{
    device->priv = G_TYPE_INSTANCE_GET_PRIVATE (device,
                                                ALSA_TYPE_DEVICE,
                                                AlsaDevicePrivate);

    device->priv->switches = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    g_object_unref);

    device->priv->context = g_main_context_ref_thread_default ();

    g_mutex_init (&device->priv->mutex);
    g_cond_init (&device->priv->cond);
}

static void
alsa_device_dispose (GObject *object)
{
    AlsaDevice *device;

    device = ALSA_DEVICE (object);

    g_clear_object (&device->priv->input);
    g_clear_object (&device->priv->output);

    g_hash_table_remove_all (device->priv->switches);

    G_OBJECT_CLASS (alsa_device_parent_class)->dispose (object);
}

static void
alsa_device_finalize (GObject *object)
{
    AlsaDevice *device;

    device = ALSA_DEVICE (object);

    g_mutex_clear (&device->priv->mutex);
    g_cond_clear (&device->priv->cond);

    g_hash_table_unref (device->priv->switches);
    g_main_context_unref (device->priv->context);

    if (device->priv->handle != NULL)
        snd_mixer_close (device->priv->handle);

    G_OBJECT_CLASS (alsa_device_parent_class)->dispose (object);
}

AlsaDevice *
alsa_device_new (const gchar *name, const gchar *label)
{
    AlsaDevice  *device;
    gchar       *stream_name;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);

    device = g_object_new (ALSA_TYPE_DEVICE,
                           "name", name,
                           "label", label,
                           "icon", ALSA_DEVICE_ICON,
                           NULL);

    /* Create input and output streams, they will exist the whole time, but
     * the added and removed signals will be emitted when the first control or
     * switch is added or the last one removed */
    stream_name = g_strdup_printf ("alsa-input-%s", name);
    device->priv->input = alsa_stream_new (stream_name,
                                           MATE_MIXER_DEVICE (device),
                                           MATE_MIXER_STREAM_INPUT);
    g_free (stream_name);

    stream_name = g_strdup_printf ("alsa-output-%s", name);
    device->priv->output = alsa_stream_new (stream_name,
                                            MATE_MIXER_DEVICE (device),
                                            MATE_MIXER_STREAM_OUTPUT);
    g_free (stream_name);

    return device;
}

gboolean
alsa_device_open (AlsaDevice *device)
{
    snd_mixer_t *handle;
    const gchar *name;
    gint         ret;

    g_return_val_if_fail (ALSA_IS_DEVICE (device), FALSE);
    g_return_val_if_fail (device->priv->handle == NULL, FALSE);

    name = mate_mixer_device_get_name (MATE_MIXER_DEVICE (device));

    g_debug ("Opening device %s (%s)",
             name,
             mate_mixer_device_get_label (MATE_MIXER_DEVICE (device)));

    /* Open the mixer for the current device */
    ret = snd_mixer_open (&handle, 0);
    if (ret < 0) {
        g_warning ("Failed to open mixer: %s", snd_strerror (ret));
        return FALSE;
    }
    ret = snd_mixer_attach (handle, name);
    if (ret < 0) {
        g_warning ("Failed to attach mixer to %s: %s",
                   name,
                   snd_strerror (ret));

        snd_mixer_close (handle);
        return FALSE;
    }
    ret = snd_mixer_selem_register (handle, NULL, NULL);
    if (ret < 0) {
        g_warning ("Failed to register simple element for %s: %s",
                   name,
                   snd_strerror (ret));

        snd_mixer_close (handle);
        return FALSE;
    }
    ret = snd_mixer_load (handle);
    if (ret < 0) {
        g_warning ("Failed to load mixer elements for %s: %s",
                   name,
                   snd_strerror (ret));

        snd_mixer_close (handle);
        return FALSE;
    }

    device->priv->handle = handle;
    return TRUE;
}

void
alsa_device_load (AlsaDevice *device)
{
    GThread          *thread;
    GError           *error = NULL;
    snd_mixer_elem_t *el;

    g_return_if_fail (ALSA_IS_DEVICE (device));
    g_return_if_fail (device->priv->handle != NULL);

    /* Process the mixer elements */
    el = snd_mixer_first_elem (device->priv->handle);
    while (el != NULL) {
        load_element (device, el);

        el = snd_mixer_elem_next (el);
    }

    /* Set callback for ALSA events */
    snd_mixer_set_callback (device->priv->handle, handle_callback);
    snd_mixer_set_callback_private (device->priv->handle, device);

    /* Start the polling thread */
    thread = g_thread_try_new ("matemixer-alsa-poll",
                               (GThreadFunc) handle_poll,
                               device,
                               &error);
    if (thread == NULL) {
        /* The error is not treated as fatal, because without the polling
         * thread we still have most of the functionality */
        g_warning ("Failed to create poll thread: %s", error->message);
        g_error_free (error);
    } else
        g_thread_unref (thread);
}

AlsaStream *
alsa_device_get_input_stream (AlsaDevice *device)
{
    g_return_val_if_fail (ALSA_IS_DEVICE (device), NULL);

    /* Normally controlless streams should not exist, here we simulate the
     * behaviour for the owning instance */
    if (alsa_stream_is_empty (device->priv->input) == FALSE)
        return device->priv->input;

    return NULL;
}

AlsaStream *
alsa_device_get_output_stream (AlsaDevice *device)
{
    g_return_val_if_fail (ALSA_IS_DEVICE (device), NULL);

    /* Normally controlless streams should not exist, here we simulate the
     * behaviour for the owning instance */
    if (alsa_stream_is_empty (device->priv->output) == FALSE)
        return device->priv->output;

    return NULL;
}

static gboolean
add_element (AlsaDevice *device, AlsaStream *stream, AlsaElement *element)
{
    gboolean added = FALSE;

    if (alsa_element_load (element) == FALSE)
        return FALSE;

    if (stream != NULL) {
        gboolean empty = FALSE;

        if (alsa_stream_is_empty (stream) == TRUE) {
            const gchar *name =
                mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream));

            /* Pretend the stream has just been created now that we are adding
             * the first control */
            g_signal_emit_by_name (G_OBJECT (device),
                                   "stream-added",
                                   name);
            empty = TRUE;
        }

        if (ALSA_IS_STREAM_CONTROL (element)) {
            alsa_stream_add_control (stream, ALSA_STREAM_CONTROL (element));

            /* If this is the first control, set it as the default one.
             * The controls often seem to come in the order of importance, but this is
             * driver specific, so we may later see if there is another control which
             * better matches the default. */
            if (empty == TRUE)
                alsa_stream_set_default_control (stream, ALSA_STREAM_CONTROL (element));

            added = TRUE;
        } else if (ALSA_IS_SWITCH (element)) {
            /* Switch belonging to a stream */
            alsa_stream_add_switch (stream, ALSA_SWITCH (element));
            added = TRUE;
        }
    } else if (ALSA_IS_SWITCH (element)) {
        /* Switch belonging to the device */
        const gchar *name =
            mate_mixer_switch_get_name (MATE_MIXER_SWITCH (element));

        g_hash_table_insert (device->priv->switches,
                             g_strdup (name),
                             g_object_ref (element));

        g_signal_emit_by_name (G_OBJECT (device),
                               "switch-added",
                               name);
        added = TRUE;
    }

    if G_LIKELY (added == TRUE) {
        snd_mixer_elem_t *el = alsa_element_get_snd_element (element);

        snd_mixer_elem_set_callback (el, handle_element_callback);
        snd_mixer_elem_set_callback_private (el, device);
    }
    return added;
}

static MateMixerSwitch *
alsa_device_get_switch (MateMixerDevice *mmd, const gchar *name)
{
    g_return_val_if_fail (ALSA_IS_DEVICE (mmd), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_hash_table_lookup (ALSA_DEVICE (mmd)->priv->switches, name);
}

static GList *
alsa_device_list_streams (MateMixerDevice *mmd)
{
    AlsaDevice *device;
    GList      *list = NULL;

    g_return_val_if_fail (ALSA_IS_DEVICE (mmd), NULL);

    device = ALSA_DEVICE (mmd);

    if (device->priv->output != NULL)
        list = g_list_prepend (list, g_object_ref (device->priv->output));
    if (device->priv->input != NULL)
        list = g_list_prepend (list, g_object_ref (device->priv->input));

    return list;
}

static GList *
alsa_device_list_switches (MateMixerDevice *mmd)
{
    GList *list;

    g_return_val_if_fail (ALSA_IS_DEVICE (mmd), NULL);

    /* Convert the hash table to a linked list, this list is expected to be
     * cached in the main library */
    list = g_hash_table_get_values (ALSA_DEVICE (mmd)->priv->switches);
    if (list != NULL)
        g_list_foreach (list, (GFunc) g_object_ref, NULL);

    return list;
}

static gboolean
add_stream_input_control (AlsaDevice *device, snd_mixer_elem_t *el)
{
    AlsaStreamControl         *control;
    gchar                     *name;
    gchar                     *label;
    MateMixerStreamControlRole role;

    get_control_info (el, &name, &label, &role);

    g_debug ("Found device %s input control %s",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             label);

    control = alsa_stream_input_control_new (name, label, role);
    g_free (name);
    g_free (label);

    alsa_element_set_snd_element (ALSA_ELEMENT (control), el);

    if (add_element (device, device->priv->input, ALSA_ELEMENT (control)) == FALSE) {
        g_object_unref (control);
        return FALSE;
    }
    return TRUE;
}

static gboolean
add_stream_output_control (AlsaDevice *device, snd_mixer_elem_t *el)
{
    AlsaStreamControl         *control;
    gchar                     *label;
    gchar                     *name;
    MateMixerStreamControlRole role;

    get_control_info (el, &name, &label, &role);

    g_debug ("Found device %s output control %s",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             label);

    control = alsa_stream_output_control_new (name, label, role);
    g_free (name);
    g_free (label);

    alsa_element_set_snd_element (ALSA_ELEMENT (control), el);

    if (add_element (device, device->priv->output, ALSA_ELEMENT (control)) == FALSE) {
        g_object_unref (control);
        return FALSE;
    }
    return TRUE;
}

static AlsaToggle *
create_toggle (AlsaDevice *device, snd_mixer_elem_t *el, AlsaToggleType type)
{
    AlsaToggle       *toggle;
    AlsaSwitchOption *on;
    AlsaSwitchOption *off;
    gchar            *name;
    gchar            *label;

    on  = alsa_switch_option_new ("On", _("On"), NULL, 1);
    off = alsa_switch_option_new ("Off", _("Off"), NULL, 0);

    get_switch_info (el, &name, &label);
    toggle = alsa_toggle_new (name,
                              label,
                              type,
                              on, off);
    g_free (name);
    g_free (label);

    alsa_element_set_snd_element (ALSA_ELEMENT (toggle), el);

    g_object_unref (on);
    g_object_unref (off);

    return toggle;
}

static gboolean
add_switch (AlsaDevice *device, AlsaStream *stream, snd_mixer_elem_t *el)
{
    AlsaElement *element = NULL;
    GList       *options = NULL;
    gchar       *name;
    gchar       *label;
    gchar        item[128];
    guint        i;
    gint         count;
    gint         ret;

    count = snd_mixer_selem_get_enum_items (el);
    if G_UNLIKELY (count <= 0) {
        g_debug ("Skipping mixer switch %s with %d items",
                 snd_mixer_selem_get_name (el),
                 count);
        return FALSE;
    }

    for (i = 0; i < count; i++) {
        ret = snd_mixer_selem_get_enum_item_name (el, i,
                                                  sizeof (item),
                                                  item);
        if G_LIKELY (ret == 0)
            options = g_list_prepend (options,
                                      alsa_switch_option_new (item, item, NULL, i));
        else
            g_warning ("Failed to read switch item name: %s", snd_strerror (ret));
    }

    get_switch_info (el, &name, &label);

    /* Takes ownership of options */
    element = ALSA_ELEMENT (alsa_switch_new (name, label, g_list_reverse (options)));
    g_free (name);
    g_free (label);

    alsa_element_set_snd_element (element, el);

    if (add_element (device, stream, element) == FALSE) {
        g_object_unref (element);
        return FALSE;
    }
    return TRUE;
}

static gboolean
add_device_switch (AlsaDevice *device, snd_mixer_elem_t *el)
{
    g_debug ("Reading device %s switch %s (%d items)",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             snd_mixer_selem_get_name (el),
             snd_mixer_selem_get_enum_items (el));

    return add_switch (device, NULL, el);
}

static gboolean
add_stream_input_switch (AlsaDevice *device, snd_mixer_elem_t *el)
{
    g_debug ("Reading device %s input switch %s (%d items)",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             snd_mixer_selem_get_name (el),
             snd_mixer_selem_get_enum_items (el));

    return add_switch (device, device->priv->input, el);
}

static gboolean
add_stream_output_switch (AlsaDevice *device, snd_mixer_elem_t *el)
{
    g_debug ("Reading device %s output switch %s (%d items)",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             snd_mixer_selem_get_name (el),
             snd_mixer_selem_get_enum_items (el));

    return add_switch (device, device->priv->output, el);
}

static gboolean
add_stream_input_toggle (AlsaDevice *device, snd_mixer_elem_t *el)
{
    AlsaToggle *toggle;

    g_debug ("Reading device %s input toggle %s",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             snd_mixer_selem_get_name (el));

    toggle = create_toggle (device, el, ALSA_TOGGLE_CAPTURE);

    if (add_element (device, device->priv->input, ALSA_ELEMENT (toggle)) == FALSE) {
        g_object_unref (toggle);
        return FALSE;
    }
    return TRUE;
}

static gboolean
add_stream_output_toggle (AlsaDevice *device, snd_mixer_elem_t *el)
{
    AlsaToggle *toggle;

    g_debug ("Reading device %s output toggle %s",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             snd_mixer_selem_get_name (el));

    toggle = create_toggle (device, el, ALSA_TOGGLE_PLAYBACK);

    if (add_element (device, device->priv->output, ALSA_ELEMENT (toggle)) == FALSE) {
        g_object_unref (toggle);
        return FALSE;
    }
    return TRUE;
}

static void
load_element (AlsaDevice *device, snd_mixer_elem_t *el)
{
    gboolean cvolume = FALSE;
    gboolean pvolume = FALSE;

    if (snd_mixer_selem_is_enumerated (el) == 1) {
        gboolean cenum = FALSE;
        gboolean penum = FALSE;

        if (snd_mixer_selem_is_enum_capture (el) == 1)
            cenum = TRUE;
        if (snd_mixer_selem_is_enum_playback (el) == 1)
            penum = TRUE;

        /* Enumerated controls which are not marked as capture or playback
         * are considered to be a part of the whole device, although sometimes
         * this is incorrectly reported by the driver */
        if (cenum == FALSE && penum == FALSE) {
            add_device_switch (device, el);
        }
        else if (cenum == TRUE)
            add_stream_input_switch (device, el);
        else if (penum == TRUE)
            add_stream_output_switch (device, el);
    }

    if (snd_mixer_selem_has_capture_volume (el) == 1 ||
        snd_mixer_selem_has_common_volume (el) == 1)
        cvolume = TRUE;
    if (snd_mixer_selem_has_playback_volume (el) == 1 ||
        snd_mixer_selem_has_common_volume (el) == 1)
        pvolume = TRUE;

    if (cvolume == FALSE && pvolume == FALSE) {
        /* Control without volume and with a switch are modelled as toggles */
        if (snd_mixer_selem_has_capture_switch (el) == 1)
            add_stream_input_toggle (device, el);

        if (snd_mixer_selem_has_playback_switch (el) == 1)
            add_stream_output_toggle (device, el);
    } else {
        if (cvolume == TRUE)
            add_stream_input_control (device, el);
        if (pvolume == TRUE)
            add_stream_output_control (device, el);
    }
}

static void
load_elements_by_name (AlsaDevice *device, const gchar *name)
{
    AlsaElement *element;

    alsa_stream_load_elements (device->priv->input, name);
    alsa_stream_load_elements (device->priv->output, name);

    element = g_hash_table_lookup (device->priv->switches, name);
    if (element != NULL)
        alsa_element_load (element);
}

static void
remove_elements_by_name (AlsaDevice *device, const gchar *name)
{
    if (alsa_stream_remove_elements (device->priv->input, name) == TRUE) {
        /* Removing last stream element "removes" the stream */
        if (alsa_stream_is_empty (device->priv->input) == TRUE) {
            const gchar *stream_name =
                mate_mixer_stream_get_name (MATE_MIXER_STREAM (device->priv->input));

            g_signal_emit_by_name (G_OBJECT (device),
                                   "stream-removed",
                                   stream_name);
        }
    }

    if (alsa_stream_remove_elements (device->priv->output, name) == TRUE) {
        /* Removing last stream element "removes" the stream */
        if (alsa_stream_is_empty (device->priv->output) == TRUE) {
            const gchar *stream_name =
                mate_mixer_stream_get_name (MATE_MIXER_STREAM (device->priv->output));

            g_signal_emit_by_name (G_OBJECT (device),
                                   "stream-removed",
                                   stream_name);
        }
    }

    if (g_hash_table_remove (device->priv->switches, name) == TRUE)
        g_signal_emit_by_name (G_OBJECT (device),
                               "switch-removed",
                               name);
}

static void
handle_poll (AlsaDevice *device)
{
    /* This function is called in a worker thread. It is supposed to wait for
     * ALSA events and call handle_process_events(). Processing the events might
     * result in emitting the CLOSED signal and unreffing the instance in the
     * owner, so keep an extra reference during the lifetime of the thread. */
    g_object_ref (device);

    while (TRUE) {
        gint ret = snd_mixer_wait (device->priv->handle, -1);
        if (ret < 0) {
            if (ret == EINTR)
                continue;
            break;
        }

        device->priv->events_pending = TRUE;

        /* Process the events in the main thread because most events end up
         * emitting signals */
        g_main_context_invoke (device->priv->context,
                               (GSourceFunc) handle_process_events,
                               device);

        g_mutex_lock (&device->priv->mutex);

        /* Use a GCond to wait until the events are processed. The processing
         * function may be called any time later in the main loop and snd_mixer_wait()
         * returns instantly while there are pending events. Without the wait,
         * g_main_context_invoke() could be called repeatedly to create idle sources
         * until the first idle source function is called. */
        while (device->priv->events_pending == TRUE)
            g_cond_wait (&device->priv->cond, &device->priv->mutex);

        g_mutex_unlock (&device->priv->mutex);

        /* Exit the thread if the processing function closed the device */
        if (device->priv->handle == NULL)
            break;
    }

    g_debug ("Terminating poll thread for device %s",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)));

    g_object_unref (device);
}

static gboolean
handle_process_events (AlsaDevice *device)
{
    g_mutex_lock (&device->priv->mutex);

    if (device->priv->handle != NULL) {
        gint ret = snd_mixer_handle_events (device->priv->handle);
        if (ret < 0)
            close_device (device);
    }

    device->priv->events_pending = FALSE;

    g_cond_signal (&device->priv->cond);
    g_mutex_unlock (&device->priv->mutex);

    return G_SOURCE_REMOVE;
}

/* ALSA has a per-mixer callback and per-element callback, per-mixer callback
 * is only used for added elements and per-element callback for all the
 * other messages (no, the documentation doesn't say anything about that). */
static int
handle_callback (snd_mixer_t *handle, guint mask, snd_mixer_elem_t *el)
{
    if (mask & SND_CTL_EVENT_MASK_ADD) {
        AlsaDevice *device = snd_mixer_get_callback_private (handle);

        load_element (device, el);
    }
    return 0;
}

static int
handle_element_callback (snd_mixer_elem_t *el, guint mask)
{
    AlsaDevice *device;
    gchar      *name;

    device = snd_mixer_elem_get_callback_private (el);
    name = get_element_name (el);

    if (mask == SND_CTL_EVENT_MASK_REMOVE) {
        /* Make sure this function is not called again with the element */
        snd_mixer_elem_set_callback_private (el, NULL);
        snd_mixer_elem_set_callback (el, NULL);

        remove_elements_by_name (device, name);
    } else {
        if (mask & SND_CTL_EVENT_MASK_INFO) {
            remove_elements_by_name (device, name);
            load_element (device, el);
        }
        if (mask & SND_CTL_EVENT_MASK_VALUE)
            load_elements_by_name (device, name);
    }
    g_free (name);

    return 0;
}

static void
close_device (AlsaDevice *device)
{
    if (device->priv->handle != NULL) {
        snd_mixer_close (device->priv->handle);
        device->priv->handle = NULL;
    }

    /* This signal tells the owner that the device has been closed voluntarily
     * from within the instance */
    g_signal_emit (G_OBJECT (device), signals[CLOSED], 0);
}

static gchar *
get_element_name (snd_mixer_elem_t *el)
{
    return g_strdup_printf ("%s-%d",
                            snd_mixer_selem_get_name (el),
                            snd_mixer_selem_get_index (el));
}

static void
get_control_info (snd_mixer_elem_t           *el,
                  gchar                     **name,
                  gchar                     **label,
                  MateMixerStreamControlRole *role)
{
    MateMixerStreamControlRole r = MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN;
    const gchar               *n;
    const gchar               *l = NULL;
    gint                       i;

    n = snd_mixer_selem_get_name (el);

    for (i = 0; alsa_controls[i].name != NULL; i++)
        if (strcmp (n, alsa_controls[i].name) == 0) {
            l = alsa_controls[i].label;
            r = alsa_controls[i].role;
            break;
        }

    *name = get_element_name (el);
    if (l != NULL)
        *label = g_strdup (l);
    else
        *label = g_strdup (n);

    *role = r;
}

static void
get_switch_info (snd_mixer_elem_t           *el,
                 gchar                     **name,
                 gchar                     **label)
{
    // MateMixerStreamControlRole r = MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN;
    const gchar               *n;
    const gchar               *l = NULL;
    // gint                       i;

    n = snd_mixer_selem_get_name (el);

    // TODO provide translated label and flags

/*
    for (i = 0; alsa_controls[i].name != NULL; i++)
        if (strcmp (n, alsa_controls[i].name) == 0) {
            l = alsa_controls[i].label;
            r = alsa_controls[i].role;
            break;
        }
*/
    *name = get_element_name (el);
    if (l != NULL)
        *label = g_strdup (l);
    else
        *label = g_strdup (n);

    // *role = r;
}
