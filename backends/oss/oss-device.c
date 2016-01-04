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
#include "oss-switch-option.h"

/*
 * NOTES:
 *
 * OSS has a predefined list of channels (or "devices"), which historically used
 * to be mapped to individual sound card pins. At this time, the channels are
 * chosen somehow arbitrarily by drivers.
 *
 * Each of the channels may have a record switch, which toggles between playback
 * and capture direction. OSS doesn't have mute switches and we can't really use
 * the record switch as one. For this reason all channels are modelled as
 * muteless stream controls and the record switch is modelled as a port switch.
 *
 * Also, we avoid modelling capturable channels as both input and output channels,
 * because the ones which allow capture are normally capture-only channels
 * (OSS just doesn't have the ability to make the distinction), so each channel in
 * the list contains a flag of whether it can be used as a capture source, given
 * that it's reported as capturable. Capturable channels are therefore modelled
 * as input controls and this approach avoids for example putting PCM in an input
 * stream (which makes no sense).
 *
 * OSS also has an indicator of whether the record switch is exclusive (only
 * allows one capture source at a time), to simplify the lives of applications
 * we always create a port switch and therefore assume the exclusivity is always
 * true. Ideally, we should probably model a bunch of toggles, one for each channel
 * with capture capability if they are known not to be exclusive.
 */

#define OSS_DEVICE_ICON "audio-card"

#define OSS_POLL_TIMEOUT_NORMAL   500
#define OSS_POLL_TIMEOUT_RAPID     50
#define OSS_POLL_TIMEOUT_RESTORE 3000

typedef enum {
    OSS_POLL_NORMAL,
    OSS_POLL_RAPID
} OssPollMode;

typedef enum {
    OSS_DEV_ANY,
    OSS_DEV_INPUT,
    OSS_DEV_OUTPUT
} OssDevChannelType;

typedef struct {
    gchar                     *name;
    gchar                     *label;
    MateMixerStreamControlRole role;
    OssDevChannelType          type;
    gchar                     *icon;
} OssDevChannel;

/* Index of a channel in the array corresponds to the channel number passed to ioctl()s,
 * device names are takes from soundcard.h */
static const OssDevChannel oss_devices[] = {
    { "vol",     N_("Volume"),      MATE_MIXER_STREAM_CONTROL_ROLE_MASTER,     OSS_DEV_OUTPUT,  NULL },
    { "bass",    N_("Bass"),        MATE_MIXER_STREAM_CONTROL_ROLE_BASS,       OSS_DEV_OUTPUT,  NULL },
    { "treble",  N_("Treble"),      MATE_MIXER_STREAM_CONTROL_ROLE_TREBLE,     OSS_DEV_OUTPUT,  NULL },
    { "synth",   N_("Synth"),       MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN,    OSS_DEV_INPUT,   NULL },
    { "pcm",     N_("PCM"),         MATE_MIXER_STREAM_CONTROL_ROLE_PCM,        OSS_DEV_OUTPUT,  NULL },
    /* OSS manual says this should be the beeper, but Linux OSS seems to assign it to
     * regular volume control */
    { "speaker", N_("Speaker"),     MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER,    OSS_DEV_OUTPUT,  NULL },
    { "line",    N_("Line In"),     MATE_MIXER_STREAM_CONTROL_ROLE_PORT,       OSS_DEV_INPUT,   NULL },
    { "mic",     N_("Microphone"),  MATE_MIXER_STREAM_CONTROL_ROLE_MICROPHONE, OSS_DEV_INPUT,   "audio-input-microphone" },
    { "cd",      N_("CD"),          MATE_MIXER_STREAM_CONTROL_ROLE_CD,         OSS_DEV_INPUT,   NULL },
    /* Recording monitor */
    { "mix",     N_("Mixer"),       MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN,    OSS_DEV_OUTPUT,  NULL },
    { "pcm2",    N_("PCM 2"),       MATE_MIXER_STREAM_CONTROL_ROLE_PCM,        OSS_DEV_OUTPUT,  NULL },
    /* Recording level (master input) */
    { "rec",     N_("Record"),      MATE_MIXER_STREAM_CONTROL_ROLE_MASTER,     OSS_DEV_INPUT,   NULL },
    { "igain",   N_("Input Gain"),  MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN,    OSS_DEV_INPUT,   NULL },
    { "ogain",   N_("Output Gain"), MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN,    OSS_DEV_OUTPUT,  NULL },
    { "line1",   N_("Line In 1"),   MATE_MIXER_STREAM_CONTROL_ROLE_PORT,       OSS_DEV_INPUT,   NULL },
    { "line2",   N_("Line In 2"),   MATE_MIXER_STREAM_CONTROL_ROLE_PORT,       OSS_DEV_INPUT,   NULL },
    { "line3",   N_("Line In 3"),   MATE_MIXER_STREAM_CONTROL_ROLE_PORT,       OSS_DEV_INPUT,   NULL },
    /* These 3 can be attached to either digital input or output */
    { "dig1",    N_("Digital 1"),   MATE_MIXER_STREAM_CONTROL_ROLE_PORT,       OSS_DEV_ANY,     NULL },
    { "dig2",    N_("Digital 2"),   MATE_MIXER_STREAM_CONTROL_ROLE_PORT,       OSS_DEV_ANY,     NULL },
    { "dig3",    N_("Digital 3"),   MATE_MIXER_STREAM_CONTROL_ROLE_PORT,       OSS_DEV_ANY,     NULL },
    { "phin",    N_("Phone In"),    MATE_MIXER_STREAM_CONTROL_ROLE_PORT,       OSS_DEV_INPUT,   NULL },
    { "phout",   N_("Phone Out"),   MATE_MIXER_STREAM_CONTROL_ROLE_PORT,       OSS_DEV_OUTPUT,  NULL },
    { "video",   N_("Video"),       MATE_MIXER_STREAM_CONTROL_ROLE_VIDEO,      OSS_DEV_INPUT,   NULL },
    { "radio",   N_("Radio"),       MATE_MIXER_STREAM_CONTROL_ROLE_PORT,       OSS_DEV_INPUT,   NULL },

    /* soundcard.h on some systems include more channels, but different files provide
     * different meanings for the remaining ones and the value is doubtful */
};

#define OSS_N_DEVICES MIN (G_N_ELEMENTS (oss_devices), SOUND_MIXER_NRDEVICES)

/* Priorities for selecting default controls */
static const gint oss_input_priority[] = {
    11, /* rec */
    12, /* igain */
    7,  /* mic */
    6,  /* line */
    14, /* line1 */
    15, /* line2 */
    16, /* line3 */
    17, /* dig1 */
    18, /* dig2 */
    19, /* dig3 */
    20, /* phin */
    8,  /* cd */
    22, /* video */
    23, /* radio */
    3   /* synth */
};

static const gint oss_output_priority[] = {
    0,  /* vol */
    4,  /* pcm */
    10, /* pcm2 */
    5,  /* speaker */
    17, /* dig1 */
    18, /* dig2 */
    19, /* dig3 */
    21, /* phone out */
    13, /* ogain */
    9,  /* mix */
    1,  /* bass */
    2   /* treble */
};

struct _OssDevicePrivate
{
    gint        fd;
    gchar      *path;
    gint        devmask;
    gint        stereodevs;
    gint        recmask;
    guint       poll_tag;
    guint       poll_tag_restore;
    guint       poll_counter;
    gboolean    poll_use_counter;
    OssPollMode poll_mode;
    GList      *streams;
    OssStream  *input;
    OssStream  *output;
};

enum {
    CLOSED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void oss_device_class_init (OssDeviceClass *klass);
static void oss_device_init       (OssDevice      *device);
static void oss_device_dispose    (GObject        *object);
static void oss_device_finalize   (GObject        *object);

G_DEFINE_TYPE (OssDevice, oss_device, MATE_MIXER_TYPE_DEVICE)

static const GList *oss_device_list_streams       (MateMixerDevice *mmd);

static gboolean     poll_mixer                    (OssDevice       *device);
static gboolean     poll_mixer_restore            (OssDevice       *device);

static void         read_mixer_devices            (OssDevice       *device);
static void         read_mixer_switch             (OssDevice       *device);

static guint        create_poll_source            (OssDevice       *device,
                                                   OssPollMode      mode);
static guint        create_poll_restore_source    (OssDevice       *device);

static void         free_stream_list              (OssDevice       *device);

static gint         compare_stream_control_devnum (gconstpointer    a,
                                                   gconstpointer    b);

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

    signals[CLOSED] =
        g_signal_new ("closed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (OssDeviceClass, closed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0,
                      G_TYPE_NONE);

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

    free_stream_list (device);

    G_OBJECT_CLASS (oss_device_parent_class)->dispose (object);
}

static void
oss_device_finalize (GObject *object)
{
    OssDevice *device = OSS_DEVICE (object);

    if (device->priv->fd != -1)
        close (device->priv->fd);

    g_free (device->priv->path);

    G_OBJECT_CLASS (oss_device_parent_class)->finalize (object);
}

OssDevice *
oss_device_new (const gchar *name,
                const gchar *label,
                const gchar *path,
                gint         fd)
{
    OssDevice *device;
    gint       newfd;

    g_return_val_if_fail (name  != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (path  != NULL, NULL);

    newfd = dup (fd);
    if (newfd == -1) {
        g_warning ("Failed to duplicate file descriptor: %s",
                   g_strerror (errno));
        return NULL;
    }

    device = g_object_new (OSS_TYPE_DEVICE,
                           "name", name,
                           "label", label,
                           "icon", OSS_DEVICE_ICON,
                           NULL);

    device->priv->fd   = newfd;
    device->priv->path = g_strdup (path);

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
    ret = ioctl (device->priv->fd, MIXER_READ (SOUND_MIXER_DEVMASK),
                 &device->priv->devmask);
    if (ret == -1)
        goto fail;

    ret = ioctl (device->priv->fd, MIXER_READ (SOUND_MIXER_STEREODEVS),
                 &device->priv->stereodevs);
    if (ret == -1)
        goto fail;

    ret = ioctl (device->priv->fd, MIXER_READ (SOUND_MIXER_RECMASK),
                 &device->priv->recmask);
    if (ret == -1)
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
oss_device_is_open (OssDevice *device)
{
    g_return_val_if_fail (OSS_IS_DEVICE (device), FALSE);

    if (device->priv->fd != -1)
        return TRUE;

    return FALSE;
}

void
oss_device_close (OssDevice *device)
{
    g_return_if_fail (OSS_IS_DEVICE (device));

    if (device->priv->fd == -1)
        return;

    /* Make each stream remove its controls and switch */
    if (device->priv->input != NULL) {
        const gchar *name =
            mate_mixer_stream_get_name (MATE_MIXER_STREAM (device->priv->input));

        oss_stream_remove_all (device->priv->input);
        free_stream_list (device);

        g_signal_emit_by_name (G_OBJECT (device),
                               "stream-removed",
                               name);

        g_clear_object (&device->priv->input);
    }

    if (device->priv->output != NULL) {
        const gchar *name =
            mate_mixer_stream_get_name (MATE_MIXER_STREAM (device->priv->output));

        oss_stream_remove_all (device->priv->output);
        free_stream_list (device);

        g_signal_emit_by_name (G_OBJECT (device),
                               "stream-removed",
                               name);

        g_clear_object (&device->priv->output);
    }

    if (device->priv->poll_tag != 0)
        g_source_remove (device->priv->poll_tag);

    if (device->priv->poll_tag_restore != 0)
        g_source_remove (device->priv->poll_tag_restore);

    close (device->priv->fd);
    device->priv->fd = -1;

    g_signal_emit (G_OBJECT (device), signals[CLOSED], 0);
}

void
oss_device_load (OssDevice *device)
{
    const GList *controls;
    const gchar *name;
    gchar       *stream_name;
    guint        i;

    g_return_if_fail (OSS_IS_DEVICE (device));

    name = mate_mixer_device_get_name (MATE_MIXER_DEVICE (device));

    stream_name = g_strdup_printf ("oss-input-%s", name);
    device->priv->input = oss_stream_new (stream_name,
                                          MATE_MIXER_DEVICE (device),
                                          MATE_MIXER_DIRECTION_INPUT);
    g_free (stream_name);

    stream_name = g_strdup_printf ("oss-output-%s", name);
    device->priv->output = oss_stream_new (stream_name,
                                           MATE_MIXER_DEVICE (device),
                                           MATE_MIXER_DIRECTION_OUTPUT);
    g_free (stream_name);

    read_mixer_devices (device);

    /* Set default input control */
    if (oss_stream_has_controls (device->priv->input) == TRUE) {
        controls = mate_mixer_stream_list_controls (MATE_MIXER_STREAM (device->priv->input));

        for (i = 0; i < G_N_ELEMENTS (oss_input_priority); i++) {
            GList *item = g_list_find_custom ((GList *) controls,
                                              GINT_TO_POINTER (oss_input_priority[i]),
                                              compare_stream_control_devnum);
            if (item == NULL)
                continue;

            oss_stream_set_default_control (device->priv->input,
                                            OSS_STREAM_CONTROL (item->data));
            break;
        }

        /* Create an input switch for recording sources */
        if (device->priv->recmask != 0)
            read_mixer_switch (device);
    } else
        g_clear_object (&device->priv->input);

    /* Set default output control */
    if (oss_stream_has_controls (device->priv->output) == TRUE) {
        controls = mate_mixer_stream_list_controls (MATE_MIXER_STREAM (device->priv->output));

        for (i = 0; i < G_N_ELEMENTS (oss_output_priority); i++) {
            GList *item = g_list_find_custom ((GList *) controls,
                                              GINT_TO_POINTER (oss_output_priority[i]),
                                              compare_stream_control_devnum);
            if (item == NULL)
                continue;

            oss_stream_set_default_control (device->priv->output,
                                            OSS_STREAM_CONTROL (item->data));
            break;
        }
    } else
        g_clear_object (&device->priv->output);

    /*
     * See if we can use the modify_counter field to optimize polling.
     *
     * Only do this on Linux for now as the counter doesn't update on BSDs.
     */
#if defined(SOUND_MIXER_INFO) && defined(__linux__)
    do {
        struct mixer_info info;
        gint   ret;

        ret = ioctl (device->priv->fd, SOUND_MIXER_INFO, &info);
        if (ret == 0) {
            device->priv->poll_counter = info.modify_counter;
            device->priv->poll_use_counter = TRUE;
        }
    } while (0);
#endif

    /*
     * Use a polling strategy inspired by KMix:
     *
     * Poll for changes with the OSS_POLL_TIMEOUT_NORMAL interval. When we
     * encounter a change in modify_counter, decrease the interval to
     * OSS_POLL_TIMEOUT_RAPID for a few seconds to allow for smoother
     * adjustments, for example when user drags a slider.
     *
     * This is not used on systems which don't support the modify_counter
     * field, because there is no way to find out whether anything has
     * changed and therefore when to start the rapid polling.
     */
    device->priv->poll_tag = create_poll_source (device, OSS_POLL_NORMAL);
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

static const GList *
oss_device_list_streams (MateMixerDevice *mmd)
{
    OssDevice *device;

    g_return_val_if_fail (OSS_IS_DEVICE (mmd), NULL);

    device = OSS_DEVICE (mmd);

    if (device->priv->streams == NULL) {
        if (device->priv->output != NULL)
            device->priv->streams = g_list_prepend (device->priv->streams,
                                                    g_object_ref (device->priv->output));

        if (device->priv->input != NULL)
            device->priv->streams = g_list_prepend (device->priv->streams,
                                                    g_object_ref (device->priv->input));
    }
    return device->priv->streams;
}

#define OSS_MASK_HAS_DEVICE(mask,i) ((gboolean) (((mask) & (1 << (i))) > 0))

static void
read_mixer_devices (OssDevice *device)
{
    OssStreamControl *control;
    const gchar      *name;
    guint             i;

    name = mate_mixer_device_get_name (MATE_MIXER_DEVICE (device));

    for (i = 0; i < OSS_N_DEVICES; i++) {
        OssStream *stream;
        gboolean   stereo;

        /* Skip unavailable controls */
        if (OSS_MASK_HAS_DEVICE (device->priv->devmask, i) == FALSE)
            continue;

        /* The control is assigned to a stream according to the predefined type.
         *
         * OSS may allow some controls to be both input and output, but the API
         * is too simple to tell what exactly is a control capable of. Here we
         * simplify things a bit and assign each control to exactly one stream. */
        switch (oss_devices[i].type) {
        case OSS_DEV_INPUT:
            stream = device->priv->input;
            break;
        case OSS_DEV_OUTPUT:
            stream = device->priv->output;
            break;
        default:
            if (OSS_MASK_HAS_DEVICE (device->priv->recmask, i) == TRUE)
                stream = device->priv->input;
            else
                stream = device->priv->output;
            break;
        }

        stereo  = OSS_MASK_HAS_DEVICE (device->priv->stereodevs, i);
        control = oss_stream_control_new (oss_devices[i].name,
                                          gettext (oss_devices[i].label),
                                          oss_devices[i].role,
                                          stream,
                                          device->priv->fd,
                                          i,
                                          stereo);
        if G_UNLIKELY (control == NULL)
            continue;

        if (oss_stream_has_controls (stream) == FALSE) {
            const gchar *name =
                mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream));

            free_stream_list (device);

            /* Pretend the stream has just been created now that we are adding
             * the first control */
            g_signal_emit_by_name (G_OBJECT (device),
                                   "stream-added",
                                   name);
        }

        g_debug ("Adding device %s control %s",
                 name,
                 mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (control)));

        oss_stream_add_control (stream, control);
        oss_stream_control_load (control);

        g_object_unref (control);
    }
}

static void
read_mixer_switch (OssDevice *device)
{
    GList *options = NULL;
    guint  i;

    /* This is always an input recording source switch */
    for (i = 0; i < G_N_ELEMENTS (oss_input_priority); i++) {
        gint             devnum = oss_input_priority[i];
        OssSwitchOption *option;

        /* Avoid devices that are not present or not recordable */
        if (OSS_MASK_HAS_DEVICE (device->priv->devmask, devnum) == FALSE ||
            OSS_MASK_HAS_DEVICE (device->priv->recmask, devnum) == FALSE)
            continue;

        option = oss_switch_option_new (oss_devices[devnum].name,
                                        gettext (oss_devices[devnum].label),
                                        oss_devices[devnum].icon,
                                        devnum);
        options = g_list_prepend (options, option);
    }

    if G_LIKELY (options != NULL)
        oss_stream_set_switch_data (device->priv->input,
                                    device->priv->fd,
                                    g_list_reverse (options));
}

static gboolean
poll_mixer (OssDevice *device)
{
    gboolean load = TRUE;

    if G_UNLIKELY (device->priv->fd == -1)
        return G_SOURCE_REMOVE;

#ifdef SOUND_MIXER_INFO
    if (device->priv->poll_use_counter == TRUE) {
        gint   ret;
        struct mixer_info info;

        /*
         * The modify_counter field increases each time a change occurs on
         * the device.
         *
         * If this ioctl() works, we use the field to only poll the controls
         * if a change actually occured and we can also adjust the poll interval.
         *
         * The call is also used to detect unplugged devices early.
         */
        ret = ioctl (device->priv->fd, SOUND_MIXER_INFO, &info);
        if (ret == -1) {
            if (errno == EINTR)
                return G_SOURCE_CONTINUE;

            oss_device_close (device);
            return G_SOURCE_REMOVE;
        }

        if (device->priv->poll_counter < info.modify_counter)
            device->priv->poll_counter = info.modify_counter;
        else
            load = FALSE;
    }
#endif

    if (load == TRUE) {
        if (device->priv->input != NULL)
            oss_stream_load (device->priv->input);
        if (device->priv->output != NULL)
            oss_stream_load (device->priv->output);

        if (device->priv->poll_use_counter == TRUE &&
            device->priv->poll_mode == OSS_POLL_NORMAL) {
            /* Create a new rapid source */
            device->priv->poll_tag = create_poll_source (device, OSS_POLL_RAPID);

            /* Also create another source to restore the poll interval to the
             * original state */
            device->priv->poll_tag_restore = create_poll_restore_source (device);

            device->priv->poll_mode = OSS_POLL_RAPID;
            return G_SOURCE_REMOVE;
        }
    }
    return G_SOURCE_CONTINUE;
}

static gboolean
poll_mixer_restore (OssDevice *device)
{
    if G_LIKELY (device->priv->poll_mode == OSS_POLL_RAPID) {
        /* Remove the current rapid source */
        g_source_remove (device->priv->poll_tag);

        device->priv->poll_tag  = create_poll_source (device, OSS_POLL_NORMAL);
        device->priv->poll_mode = OSS_POLL_NORMAL;
    }

    /* Remove the tag for this function as it is only called once, the tag
     * is only kept so we can remove it in the case the device is closed */
    device->priv->poll_tag_restore = 0;

    return G_SOURCE_REMOVE;
}

static guint
create_poll_source (OssDevice *device, OssPollMode mode)
{
    GSource *source;
    guint    timeout;
    guint    tag;

    switch (mode) {
    case OSS_POLL_NORMAL:
        timeout = OSS_POLL_TIMEOUT_NORMAL;
        break;
    case OSS_POLL_RAPID:
        timeout = OSS_POLL_TIMEOUT_RAPID;
        break;
    default:
        g_warn_if_reached ();
        return 0;
    }

    source = g_timeout_source_new (timeout);
    g_source_set_callback (source,
                           (GSourceFunc) poll_mixer,
                           device,
                           NULL);

    tag = g_source_attach (source, g_main_context_get_thread_default ());
    g_source_unref (source);

    return tag;
}

static guint
create_poll_restore_source (OssDevice *device)
{
    GSource *source;
    guint    tag;

    source = g_timeout_source_new (OSS_POLL_TIMEOUT_RESTORE);
    g_source_set_callback (source,
                           (GSourceFunc) poll_mixer_restore,
                           device,
                           NULL);

    tag = g_source_attach (source, g_main_context_get_thread_default ());
    g_source_unref (source);

    return tag;
}

static void
free_stream_list (OssDevice *device)
{
    /* This function is called each time the stream list changes */
    if (device->priv->streams == NULL)
        return;

    g_list_free_full (device->priv->streams, g_object_unref);

    device->priv->streams = NULL;
}

static gint
compare_stream_control_devnum (gconstpointer a, gconstpointer b)
{
    OssStreamControl *control = OSS_STREAM_CONTROL (a);
    guint             devnum  = GPOINTER_TO_INT (b);

    return !(oss_stream_control_get_devnum (control) == devnum);
}
