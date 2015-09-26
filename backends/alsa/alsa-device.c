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

#include <string.h>
#include <libintl.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <alsa/asoundlib.h>
#include <libmatemixer/matemixer.h>

#include "alsa-compat.h"
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

#define ALSA_STREAM_CONTROL_GET_SCORE(c)                        \
        (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (c),      \
                                             "__matemixer_alsa_control_score")))

#define ALSA_STREAM_CONTROL_SET_SCORE(c,score)                  \
        (g_object_set_data (G_OBJECT (c),                       \
                            "__matemixer_alsa_control_score",   \
                            GINT_TO_POINTER (score)))

#define ALSA_STREAM_DEFAULT_CONTROL_GET_SCORE(s)                \
        (ALSA_STREAM_CONTROL_GET_SCORE (alsa_stream_get_default_control (ALSA_STREAM (s))))

struct _AlsaDevicePrivate
{
    snd_mixer_t  *handle;
    GMainContext *context;
    GMutex        mutex;
    GCond         cond;
    AlsaStream   *input;
    AlsaStream   *output;
    GList        *streams;
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

static const GList *      alsa_device_list_streams  (MateMixerDevice            *mmd);

static void               add_element               (AlsaDevice                 *device,
                                                     AlsaStream                 *stream,
                                                     AlsaElement                *element);

static void               add_stream_input_control  (AlsaDevice                 *device,
                                                     snd_mixer_elem_t           *el);
static void               add_stream_output_control (AlsaDevice                 *device,
                                                     snd_mixer_elem_t           *el);

static void               add_switch                (AlsaDevice                 *device,
                                                     AlsaStream                 *stream,
                                                     snd_mixer_elem_t           *el);

static void               add_toggle                (AlsaDevice                 *device,
                                                     AlsaStream                 *stream,
                                                     AlsaToggleType              type,
                                                     snd_mixer_elem_t           *el);

static void               add_stream_input_switch   (AlsaDevice                 *device,
                                                     snd_mixer_elem_t           *el);
static void               add_stream_output_switch  (AlsaDevice                 *device,
                                                     snd_mixer_elem_t           *el);

static void               add_stream_input_toggle   (AlsaDevice                 *device,
                                                     snd_mixer_elem_t           *el);
static void               add_stream_output_toggle  (AlsaDevice                 *device,
                                                     snd_mixer_elem_t           *el);

static void               load_element              (AlsaDevice                 *device,
                                                     snd_mixer_elem_t           *el);

static void               load_elements_by_name     (AlsaDevice                 *device,
                                                     const gchar                *name);

static void               remove_elements_by_name   (AlsaDevice                 *device,
                                                     const gchar                *name);

static void               handle_poll               (AlsaDevice                 *device);

static gboolean           handle_process_events     (AlsaDevice                 *device);

static int                handle_callback           (snd_mixer_t                *handle,
                                                     guint                       mask,
                                                     snd_mixer_elem_t           *el);
static int                handle_element_callback   (snd_mixer_elem_t           *el,
                                                     guint                       mask);

static void               validate_default_controls (AlsaDevice                 *device);

static AlsaStreamControl *get_best_stream_control   (AlsaStream                 *stream);

static gchar *            get_element_name          (snd_mixer_elem_t           *el);

static void               get_control_info          (snd_mixer_elem_t           *el,
                                                     gchar                     **name,
                                                     gchar                     **label,
                                                     MateMixerStreamControlRole *role,
                                                     gint                       *score);
static void               get_input_control_info    (snd_mixer_elem_t           *el,
                                                     gchar                     **name,
                                                     gchar                     **label,
                                                     MateMixerStreamControlRole *role,
                                                     gint                       *score);
static void               get_output_control_info   (snd_mixer_elem_t           *el,
                                                     gchar                     **name,
                                                     gchar                     **label,
                                                     MateMixerStreamControlRole *role,
                                                     gint                       *score);

static MateMixerDirection get_switch_direction      (snd_mixer_elem_t           *el);
static void               get_switch_info           (snd_mixer_elem_t           *el,
                                                     gchar                     **name,
                                                     gchar                     **label,
                                                     MateMixerStreamSwitchRole  *role);

static void               close_mixer               (AlsaDevice                 *device);

static void               free_stream_list          (AlsaDevice                 *device);

static void
alsa_device_class_init (AlsaDeviceClass *klass)
{
    GObjectClass         *object_class;
    MateMixerDeviceClass *device_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose  = alsa_device_dispose;
    object_class->finalize = alsa_device_finalize;

    device_class = MATE_MIXER_DEVICE_CLASS (klass);
    device_class->list_streams = alsa_device_list_streams;

    signals[CLOSED] =
        g_signal_new ("closed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_FIRST,
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

    free_stream_list (device);

    G_OBJECT_CLASS (alsa_device_parent_class)->dispose (object);
}

static void
alsa_device_finalize (GObject *object)
{
    AlsaDevice *device;

    device = ALSA_DEVICE (object);

    g_mutex_clear (&device->priv->mutex);
    g_cond_clear (&device->priv->cond);

    g_main_context_unref (device->priv->context);

    close_mixer (device);

    G_OBJECT_CLASS (alsa_device_parent_class)->finalize (object);
}

AlsaDevice *
alsa_device_new (const gchar *name, const gchar *label)
{
    AlsaDevice  *device;
    gchar       *stream_name;

    g_return_val_if_fail (name  != NULL, NULL);
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
                                           MATE_MIXER_DIRECTION_INPUT);
    g_free (stream_name);

    stream_name = g_strdup_printf ("alsa-output-%s", name);
    device->priv->output = alsa_stream_new (stream_name,
                                            MATE_MIXER_DEVICE (device),
                                            MATE_MIXER_DIRECTION_OUTPUT);
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

gboolean
alsa_device_is_open (AlsaDevice *device)
{
    g_return_val_if_fail (ALSA_IS_DEVICE (device), FALSE);

    if (device->priv->handle != NULL)
        return TRUE;

    return FALSE;
}

void
alsa_device_close (AlsaDevice *device)
{
    g_return_if_fail (ALSA_IS_DEVICE (device));

    if (device->priv->handle == NULL)
        return;

    /* Make each stream remove its controls and switches */
    if (alsa_stream_has_controls_or_switches (device->priv->input) == TRUE) {
        const gchar *name =
            mate_mixer_stream_get_name (MATE_MIXER_STREAM (device->priv->input));

        alsa_stream_remove_all (device->priv->input);
        free_stream_list (device);

        g_signal_emit_by_name (G_OBJECT (device),
                               "stream-removed",
                               name);
    }

    if (alsa_stream_has_controls_or_switches (device->priv->output) == TRUE) {
        const gchar *name =
            mate_mixer_stream_get_name (MATE_MIXER_STREAM (device->priv->output));

        alsa_stream_remove_all (device->priv->output);
        free_stream_list (device);

        g_signal_emit_by_name (G_OBJECT (device),
                               "stream-removed",
                               name);
    }

    close_mixer (device);

    g_signal_emit (G_OBJECT (device), signals[CLOSED], 0);
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

    /* Assign proper default controls */
    validate_default_controls (device);

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
    if (alsa_stream_has_controls_or_switches (device->priv->input) == TRUE)
        return device->priv->input;

    return NULL;
}

AlsaStream *
alsa_device_get_output_stream (AlsaDevice *device)
{
    g_return_val_if_fail (ALSA_IS_DEVICE (device), NULL);

    /* Normally controlless streams should not exist, here we simulate the
     * behaviour for the owning instance */
    if (alsa_stream_has_controls_or_switches (device->priv->output) == TRUE)
        return device->priv->output;

    return NULL;
}

static const GList *
alsa_device_list_streams (MateMixerDevice *mmd)
{
    AlsaDevice *device;

    g_return_val_if_fail (ALSA_IS_DEVICE (mmd), NULL);

    device = ALSA_DEVICE (mmd);

    if (device->priv->streams == NULL) {
        AlsaStream *stream;

        stream = alsa_device_get_output_stream (device);
        if (stream != NULL)
            device->priv->streams =
                g_list_prepend (device->priv->streams, g_object_ref (stream));

        stream = alsa_device_get_input_stream (device);
        if (stream != NULL)
            device->priv->streams =
                g_list_prepend (device->priv->streams, g_object_ref (stream));
    }
    return device->priv->streams;
}

static void
add_element (AlsaDevice *device, AlsaStream *stream, AlsaElement *element)
{
    snd_mixer_elem_t *el;
    gboolean          add_stream = FALSE;

    if (alsa_element_load (element) == FALSE)
        return;

    if (alsa_stream_has_controls_or_switches (stream) == FALSE)
        add_stream = TRUE;

    /* Add element to the stream depending on its type */
    if (ALSA_IS_STREAM_CONTROL (element))
        alsa_stream_add_control (stream, ALSA_STREAM_CONTROL (element));
    else if (ALSA_IS_SWITCH (element))
        alsa_stream_add_switch (stream, ALSA_SWITCH (element));
    else if (ALSA_IS_TOGGLE (element))
        alsa_stream_add_toggle (stream, ALSA_TOGGLE (element));
    else {
        g_warn_if_reached ();
        return;
    }

    if (add_stream == TRUE) {
        const gchar *name =
            mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream));

        free_stream_list (device);

        /* Pretend the stream has just been created now that we have added
         * the first control */
        g_signal_emit_by_name (G_OBJECT (device),
                               "stream-added",
                               name);
    }

    el = alsa_element_get_snd_element (element);

    /* Register to receive callbacks for element changes */
    snd_mixer_elem_set_callback (el, handle_element_callback);
    snd_mixer_elem_set_callback_private (el, device);
}

static void
add_stream_input_control (AlsaDevice *device, snd_mixer_elem_t *el)
{
    AlsaStreamControl         *control;
    gchar                     *name;
    gchar                     *label;
    gint                       score;
    MateMixerStreamControlRole role;

    get_input_control_info (el, &name, &label, &role, &score);

    g_debug ("Reading device %s input control %s",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             name);

    control = alsa_stream_input_control_new (name, label, role, device->priv->input);
    g_free (name);
    g_free (label);

    ALSA_STREAM_CONTROL_SET_SCORE (control, score);

    alsa_element_set_snd_element (ALSA_ELEMENT (control), el);

    add_element (device, device->priv->input, ALSA_ELEMENT (control));

    g_object_unref (control);
}

static void
add_stream_output_control (AlsaDevice *device, snd_mixer_elem_t *el)
{
    AlsaStreamControl         *control;
    gchar                     *label;
    gchar                     *name;
    gint                       score;
    MateMixerStreamControlRole role;

    get_output_control_info (el, &name, &label, &role, &score);

    g_debug ("Reading device %s output control %s",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             name);

    control = alsa_stream_output_control_new (name, label, role, device->priv->output);
    g_free (name);
    g_free (label);

    ALSA_STREAM_CONTROL_SET_SCORE (control, score);

    alsa_element_set_snd_element (ALSA_ELEMENT (control), el);

    add_element (device, device->priv->output, ALSA_ELEMENT (control));

    g_object_unref (control);
}

static void
add_switch (AlsaDevice *device, AlsaStream *stream, snd_mixer_elem_t *el)
{
    AlsaElement              *element = NULL;
    GList                    *options = NULL;
    gchar                    *name;
    gchar                    *label;
    gchar                     item[128];
    guint                     i;
    gint                      count;
    MateMixerStreamSwitchRole role;

    count = snd_mixer_selem_get_enum_items (el);
    if G_UNLIKELY (count <= 0) {
        g_debug ("Skipping mixer switch %s with %d items",
                 snd_mixer_selem_get_name (el),
                 count);
        return;
    }

    for (i = 0; i < count; i++) {
        gint ret = snd_mixer_selem_get_enum_item_name (el, i, sizeof (item), item);

        if G_LIKELY (ret == 0) {
            gint j;
            AlsaSwitchOption *option = NULL;

            for (j = 0; alsa_switch_options[j].name != NULL; j++)
                if (strcmp (item, alsa_switch_options[j].name) == 0) {
                    option = alsa_switch_option_new (item,
                                                     gettext (alsa_switch_options[j].label),
                                                     alsa_switch_options[j].icon,
                                                     i);
                    break;
                }

            if (option == NULL)
                option = alsa_switch_option_new (item, item, NULL, i);

            options = g_list_prepend (options, option);
        } else
            g_warning ("Failed to read switch item name: %s", snd_strerror (ret));
    }

    if G_UNLIKELY (options == NULL)
        return;

    get_switch_info (el, &name, &label, &role);

    /* Takes ownership of options */
    element = ALSA_ELEMENT (alsa_switch_new (stream,
                                             name, label,
                                             role,
                                             g_list_reverse (options)));
    g_free (name);
    g_free (label);

    alsa_element_set_snd_element (element, el);

    add_element (device, stream, element);

    g_object_unref (element);
}

static void
add_toggle (AlsaDevice       *device,
            AlsaStream       *stream,
            AlsaToggleType    type,
            snd_mixer_elem_t *el)
{
    AlsaElement              *element;
    AlsaSwitchOption         *on;
    AlsaSwitchOption         *off;
    gchar                    *name;
    gchar                    *label;
    MateMixerStreamSwitchRole role;

    on  = alsa_switch_option_new ("On", _("On"), NULL, 1);
    off = alsa_switch_option_new ("Off", _("Off"), NULL, 0);

    get_switch_info (el, &name, &label, &role);

    element = ALSA_ELEMENT (alsa_toggle_new (stream,
                                             name, label,
                                             role,
                                             type,
                                             on, off));
    g_free (name);
    g_free (label);
    g_object_unref (on);
    g_object_unref (off);

    alsa_element_set_snd_element (element, el);

    add_element (device, stream, element);

    g_object_unref (element);
}

static void
add_stream_input_switch (AlsaDevice *device, snd_mixer_elem_t *el)
{
    g_debug ("Reading device %s input switch %s (%d items)",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             snd_mixer_selem_get_name (el),
             snd_mixer_selem_get_enum_items (el));

    add_switch (device, device->priv->input, el);
}

static void
add_stream_output_switch (AlsaDevice *device, snd_mixer_elem_t *el)
{
    g_debug ("Reading device %s output switch %s (%d items)",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             snd_mixer_selem_get_name (el),
             snd_mixer_selem_get_enum_items (el));

    add_switch (device, device->priv->output, el);
}

static void
add_stream_input_toggle (AlsaDevice *device, snd_mixer_elem_t *el)
{
    g_debug ("Reading device %s input toggle %s",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             snd_mixer_selem_get_name (el));

    add_toggle (device, device->priv->input, ALSA_TOGGLE_CAPTURE, el);
}

static void
add_stream_output_toggle (AlsaDevice *device, snd_mixer_elem_t *el)
{
    g_debug ("Reading device %s output toggle %s",
             mate_mixer_device_get_name (MATE_MIXER_DEVICE (device)),
             snd_mixer_selem_get_name (el));

    add_toggle (device, device->priv->output, ALSA_TOGGLE_PLAYBACK, el);
}

static void
load_element (AlsaDevice *device, snd_mixer_elem_t *el)
{
    gboolean cvolume = FALSE;
    gboolean pvolume = FALSE;

    if (snd_mixer_selem_is_enumerated (el) == 1) {
        MateMixerDirection direction;
        gboolean           cenum = FALSE;
        gboolean           penum = FALSE;

#if SND_LIB_VERSION >= ALSA_PACK_VERSION (1, 0, 10)
        /* The enumeration may have a capture or a playback capability.
         * If it has either both or none, try to guess the more appropriate
         * direction. */
        cenum = snd_mixer_selem_is_enum_capture (el);
        penum = snd_mixer_selem_is_enum_playback (el);
#endif
        if (cenum ^ penum) {
            if (cenum == TRUE)
                direction = MATE_MIXER_DIRECTION_INPUT;
            else
                direction = MATE_MIXER_DIRECTION_OUTPUT;
        } else
            direction = get_switch_direction (el);

        if (direction == MATE_MIXER_DIRECTION_INPUT)
            add_stream_input_switch (device, el);
        else
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
    alsa_stream_load_elements (device->priv->input, name);
    alsa_stream_load_elements (device->priv->output, name);
}

static void
remove_elements_by_name (AlsaDevice *device, const gchar *name)
{
    if (alsa_stream_remove_elements (device->priv->input, name) == TRUE) {
        /* Removing last stream element "removes" the stream */
        if (alsa_stream_has_controls_or_switches (device->priv->input) == FALSE) {
            const gchar *stream_name =
                mate_mixer_stream_get_name (MATE_MIXER_STREAM (device->priv->input));

            free_stream_list (device);
            g_signal_emit_by_name (G_OBJECT (device),
                                   "stream-removed",
                                   stream_name);
        }
    }

    if (alsa_stream_remove_elements (device->priv->output, name) == TRUE) {
        /* Removing last stream element "removes" the stream */
        if (alsa_stream_has_controls_or_switches (device->priv->output) == FALSE) {
            const gchar *stream_name =
                mate_mixer_stream_get_name (MATE_MIXER_STREAM (device->priv->output));

            free_stream_list (device);
            g_signal_emit_by_name (G_OBJECT (device),
                                   "stream-removed",
                                   stream_name);
        }
    }
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
            alsa_device_close (device);
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

        if (device->priv->handle == NULL) {
            /* The mixer is already closed */
            return 0;
        }

        load_element (device, el);

        /* Revalidate default controls assignment */
        validate_default_controls (device);
    }
    return 0;
}

static int
handle_element_callback (snd_mixer_elem_t *el, guint mask)
{
    AlsaDevice *device;
    gchar      *name;

    device = snd_mixer_elem_get_callback_private (el);
    if (device->priv->handle == NULL) {
        /* The mixer is already closed */
        return 0;
    }

    name = get_element_name (el);

    if (mask == SND_CTL_EVENT_MASK_REMOVE) {
        /* Make sure this function is not called again with the element */
        snd_mixer_elem_set_callback_private (el, NULL);
        snd_mixer_elem_set_callback (el, NULL);

        remove_elements_by_name (device, name);

        /* Revalidate default controls assignment */
        validate_default_controls (device);
    } else {
        if (mask & SND_CTL_EVENT_MASK_INFO) {
            remove_elements_by_name (device, name);
            load_element (device, el);

            /* Revalidate default controls assignment */
            validate_default_controls (device);
        }
        if (mask & SND_CTL_EVENT_MASK_VALUE)
            load_elements_by_name (device, name);
    }
    g_free (name);

    return 0;
}

static void
validate_default_controls (AlsaDevice *device)
{
    AlsaStreamControl *best;
    gint               best_score;
    gint               current_score;

    /*
     * Select the most suitable default control. Don't try too hard here because
     * our list of known elements is incomplete and most drivers seem to provide
     * the list in a reasonable order with the best element at the start. Each
     * element in our list has a value (or score) which is simply its position
     * in the list. Better elements are on the top, so smaller value represents
     * a better element.
     *
     * Two cases are handled here:
     * 1) The current default control is in our list, but the list also includes
     *    a better element.
     * 2) The current default control is not in our list, but the list includes
     *    an element which is reasonably good.
     *
     * In other cases just keep the first control as the default.
     */
    if (alsa_stream_has_controls (device->priv->input) == TRUE) {
        best = get_best_stream_control (device->priv->input);

        best_score    = ALSA_STREAM_CONTROL_GET_SCORE (best);
        current_score = ALSA_STREAM_DEFAULT_CONTROL_GET_SCORE (device->priv->input);

        /* See if the best element would make a good default one */
        if (best_score > -1) {
            g_debug ("Found usable default input element %s (score %d)",
                     mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (best)),
                     best_score);

            if (current_score == -1 || best_score < current_score)
                alsa_stream_set_default_control (device->priv->input, best);
        }
    }

    if (alsa_stream_has_controls (device->priv->output) == TRUE) {
        best = get_best_stream_control (device->priv->output);

        best_score    = ALSA_STREAM_CONTROL_GET_SCORE (best);
        current_score = ALSA_STREAM_DEFAULT_CONTROL_GET_SCORE (device->priv->output);

        /* See if the best element would make a good default one */
        if (best_score > -1) {
            g_debug ("Found usable default output element %s (score %d)",
                     mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (best)),
                     best_score);

            if (current_score == -1 || best_score < current_score)
                alsa_stream_set_default_control (device->priv->output, best);
        }
    }
}

static AlsaStreamControl *
get_best_stream_control (AlsaStream *stream)
{
    const GList       *list;
    AlsaStreamControl *best = NULL;
    guint              best_score = -1;

    list = mate_mixer_stream_list_controls (MATE_MIXER_STREAM (stream));
    while (list != NULL) {
        AlsaStreamControl *current;
        guint              current_score;

        current = ALSA_STREAM_CONTROL (list->data);
        current_score = ALSA_STREAM_CONTROL_GET_SCORE (current);

        if (best == NULL || (current_score != -1 &&
                               (best_score == -1 || current_score < best_score))) {
            best = current;
            best_score = current_score;
        }
        list = list->next;
    }
    return best;
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
                  MateMixerStreamControlRole *role,
                  gint                       *score)
{
    MateMixerStreamControlRole r = MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN;
    const gchar               *n;
    const gchar               *l = NULL;
    gint                       i;

    n = snd_mixer_selem_get_name (el);

    for (i = 0; alsa_controls[i].name != NULL; i++) {
        if (strcmp (n, alsa_controls[i].name) != 0)
            continue;

        l = gettext (alsa_controls[i].label);
        r = alsa_controls[i].role;
        break;
    }

    *name = get_element_name (el);
    if (l != NULL) {
        *label = g_strdup (l);
        *score = i;
    } else {
        *label = g_strdup (n);
        *score = -1;
    }

    *role = r;
}

static void
get_input_control_info (snd_mixer_elem_t           *el,
                        gchar                     **name,
                        gchar                     **label,
                        MateMixerStreamControlRole *role,
                        gint                       *score)
{
    get_control_info (el, name, label, role, score);

    if (*score > -1 && alsa_controls[*score].use_default_input == FALSE)
        *score = -1;
}

static void
get_output_control_info (snd_mixer_elem_t           *el,
                         gchar                     **name,
                         gchar                     **label,
                         MateMixerStreamControlRole *role,
                         gint                       *score)
{
    get_control_info (el, name, label, role, score);

    if (*score > -1 && alsa_controls[*score].use_default_output == FALSE)
        *score = -1;
}

static MateMixerDirection
get_switch_direction (snd_mixer_elem_t *el)
{
    MateMixerDirection direction;
    gchar             *name;

    name = g_ascii_strdown (snd_mixer_selem_get_name (el), -1);

    /* Guess element direction by name, inspired by qasmixer */
    if (strstr (name, "mic") != NULL ||
        strstr (name, "adc") != NULL ||
        strstr (name, "capture") != NULL ||
        strstr (name, "input source") != NULL)
        direction = MATE_MIXER_DIRECTION_INPUT;
    else
        direction = MATE_MIXER_DIRECTION_OUTPUT;

    g_free (name);
    return direction;
}

static void
get_switch_info (snd_mixer_elem_t          *el,
                 gchar                    **name,
                 gchar                    **label,
                 MateMixerStreamSwitchRole *role)
{
    MateMixerStreamSwitchRole r = MATE_MIXER_STREAM_SWITCH_ROLE_UNKNOWN;
    const gchar              *n;
    const gchar              *l = NULL;
    gint                      i;

    n = snd_mixer_selem_get_name (el);

    for (i = 0; alsa_switches[i].name != NULL; i++) {
        if (strcmp (n, alsa_switches[i].name) != 0)
            continue;

        l = gettext (alsa_switches[i].label);
        r = alsa_switches[i].role;
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
close_mixer (AlsaDevice *device)
{
    snd_mixer_t *handle;

    if (device->priv->handle == NULL)
        return;

    /* Closing the mixer may fire up remove callbacks, prevent this by unsetting
     * the handle before closing it and checking it in the callback.
     * Ideally, we should unset callbacks from all the elements, but this seems
     * to do the job. */
     handle = device->priv->handle;

     device->priv->handle = NULL;
     snd_mixer_close (handle);
}

static void
free_stream_list (AlsaDevice *device)
{
    /* This function is called each time the stream list changes */
    if (device->priv->streams == NULL)
        return;

    g_list_free_full (device->priv->streams, g_object_unref);

    device->priv->streams = NULL;
}
