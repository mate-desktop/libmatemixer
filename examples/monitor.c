/*
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <locale.h>
#include <glib.h>
#include <glib-object.h>

#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

#include <libmatemixer/matemixer.h>

static MateMixerContext *context;
static GMainLoop        *mainloop;

/* Convert MateMixerStreamControlRole constant to a string */
static const gchar *
get_role_string (MateMixerStreamControlRole role)
{
    switch (role) {
    case MATE_MIXER_STREAM_CONTROL_ROLE_MASTER:
        return "Master";
    case MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION:
        return "Application";
    case MATE_MIXER_STREAM_CONTROL_ROLE_PCM:
        return "PCM";
    case MATE_MIXER_STREAM_CONTROL_ROLE_SPEAKER:
        return "Speaker";
    case MATE_MIXER_STREAM_CONTROL_ROLE_MICROPHONE:
        return "Microphone";
    case MATE_MIXER_STREAM_CONTROL_ROLE_PORT:
        return "Port";
    case MATE_MIXER_STREAM_CONTROL_ROLE_BOOST:
        return "Boost";
    case MATE_MIXER_STREAM_CONTROL_ROLE_BASS:
        return "Bass";
    case MATE_MIXER_STREAM_CONTROL_ROLE_TREBLE:
        return "Treble";
    case MATE_MIXER_STREAM_CONTROL_ROLE_CD:
        return "CD";
    case MATE_MIXER_STREAM_CONTROL_ROLE_VIDEO:
        return "Video";
    case MATE_MIXER_STREAM_CONTROL_ROLE_MUSIC:
        return "Music";
    default:
        return "Unknown";
    }
}

/* Convert MateMixerStreamControlMediaRole constant to a string */
static const gchar *
get_media_role_string (MateMixerStreamControlMediaRole role)
{
    switch (role) {
    case MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_VIDEO:
        return "Video";
    case MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_MUSIC:
        return "Music";
    case MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_GAME:
        return "Game";
    case MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_EVENT:
        return "Event";
    case MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_PHONE:
        return "Phone";
    case MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_ANIMATION:
        return "Animation";
    case MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_PRODUCTION:
        return "Production";
    case MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_A11Y:
        return "A11y";
    case MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_TEST:
        return "Test";
    case MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_ABSTRACT:
        return "Abstract";
    case MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_FILTER:
        return "Filter";
    default:
        return "Unknown";
    }
}

/* Convert MateMixerDeviceSwitchRole constant to a string */
static const gchar *
get_device_switch_role_string (MateMixerDeviceSwitchRole role)
{
    switch (role) {
    case MATE_MIXER_DEVICE_SWITCH_ROLE_PROFILE:
        return "Device Profile";
    default:
        return "Unknown";
    }
}

/* Convert MateMixerStreamSwitchRole constant to a string */
static const gchar *
get_stream_switch_role_string (MateMixerStreamSwitchRole role)
{
    switch (role) {
    case MATE_MIXER_STREAM_SWITCH_ROLE_PORT:
        return "Port";
    case MATE_MIXER_STREAM_SWITCH_ROLE_BOOST:
        return "Boost";
    default:
        return "Unknown";
    }
}

/* Convert MateMixerDirection constant to a string */
static const gchar *
get_direction_string (MateMixerDirection direction)
{
    switch (direction) {
    case MATE_MIXER_DIRECTION_INPUT:
        return "Record";
    case MATE_MIXER_DIRECTION_OUTPUT:
        return "Playback";
    default:
        return "Unknown";
    }
}

static gdouble
get_volume_percentage (MateMixerStreamControl *control)
{
    guint volume;
    guint volume_min;
    guint volume_max;

    volume     = mate_mixer_stream_control_get_volume (control);
    volume_min = mate_mixer_stream_control_get_min_volume (control);
    volume_max = mate_mixer_stream_control_get_normal_volume (control);

    return (gdouble) (volume - volume_min) / (volume_max - volume_min) * 100;
}

/* Print a list of sound devices */
static void
print_devices (void)
{
    const GList *devices;

    /* Read a list of devices from the context */
    devices = mate_mixer_context_list_devices (context);
    while (devices != NULL) {
        const GList     *switches;
        MateMixerDevice *device = MATE_MIXER_DEVICE (devices->data);

        g_print ("Device %s:\n"
                 "\tLabel     : %s\n"
                 "\tIcon Name : %s\n\n",
                 mate_mixer_device_get_name (device),
                 mate_mixer_device_get_label (device),
                 mate_mixer_device_get_icon (device));

        /* Read a list of switches that belong to the device */
        switches = mate_mixer_device_list_switches (device);
        while (switches != NULL) {
            const GList           *options;
            MateMixerSwitch       *swtch  = MATE_MIXER_SWITCH (switches->data);
            MateMixerSwitchOption *active = mate_mixer_switch_get_active_option (swtch);

            g_print ("\tSwitch %s:\n"
                     "\t\tLabel : %s\n"
                     "\t\tRole  : %s\n",
                     mate_mixer_switch_get_name (swtch),
                     mate_mixer_switch_get_label (swtch),
                     get_device_switch_role_string (mate_mixer_device_switch_get_role (MATE_MIXER_DEVICE_SWITCH (swtch))));

            g_print ("\tOptions:\n");

            /* Read a list of switch options that belong to the switch */
            options = mate_mixer_switch_list_options (swtch);
            while (options != NULL) {
                MateMixerSwitchOption *option = MATE_MIXER_SWITCH_OPTION (options->data);

                g_print ("\t\t|%c| %s\n",
                         (active != NULL && option == active)
                            ? '*'
                            : '-',
                         mate_mixer_switch_option_get_label (option));

                options = options->next;
            }

            g_print ("\n");
            switches = switches->next;
        }

        devices = devices->next;
    }
}

/* Print a list of streams */
static void
print_streams (void)
{
    const GList *streams;

    /* Read a list of streams from the context */
    streams = mate_mixer_context_list_streams (context);
    while (streams != NULL) {
        MateMixerStream *stream = MATE_MIXER_STREAM (streams->data);
        const GList     *controls;
        const GList     *switches;

        g_print ("Stream %s:\n"
                 "\tLabel     : %s\n"
                 "\tDirection : %s\n\n",
                 mate_mixer_stream_get_name (stream),
                 mate_mixer_stream_get_label (stream),
                 get_direction_string (mate_mixer_stream_get_direction (stream)));

        /* Read a list of controls in the stream */
        controls = mate_mixer_stream_list_controls (stream);
        while (controls != NULL) {
            MateMixerStreamControl *control = MATE_MIXER_STREAM_CONTROL (controls->data);

            g_print ("\tControl %s:\n"
                     "\t\tLabel      : %s\n"
                     "\t\tVolume     : %.0f %%\n"
                     "\t\tMuted      : %s\n"
                     "\t\tChannels   : %d\n"
                     "\t\tBalance    : %.1f\n"
                     "\t\tFade       : %.1f\n"
                     "\t\tRole       : %s\n"
                     "\t\tMedia role : %s\n",
                     mate_mixer_stream_control_get_name (control),
                     mate_mixer_stream_control_get_label (control),
                     get_volume_percentage (control),
                     mate_mixer_stream_control_get_mute (control) ? "Yes" : "No",
                     mate_mixer_stream_control_get_num_channels (control),
                     mate_mixer_stream_control_get_balance (control),
                     mate_mixer_stream_control_get_fade (control),
                     get_role_string (mate_mixer_stream_control_get_role (control)),
                     get_media_role_string (mate_mixer_stream_control_get_media_role (control)));

            g_print ("\n");
            controls = controls->next;
        }

        /* Read a list of switches in the stream */
        switches = mate_mixer_stream_list_switches (stream);
        while (switches != NULL) {
            const GList           *options;
            MateMixerSwitch       *swtch  = MATE_MIXER_SWITCH (switches->data);
            MateMixerSwitchOption *active = mate_mixer_switch_get_active_option (swtch);

            g_print ("\tSwitch %s:\n"
                     "\t\tLabel      : %s\n"
                     "\t\tRole       : %s\n",
                     mate_mixer_switch_get_name (swtch),
                     mate_mixer_switch_get_label (swtch),
                     get_stream_switch_role_string (mate_mixer_stream_switch_get_role (MATE_MIXER_STREAM_SWITCH (swtch))));

            g_print ("\tOptions:\n");

            /* Read a list of switch options that belong to the switch */
            options = mate_mixer_switch_list_options (swtch);
            while (options != NULL) {
                MateMixerSwitchOption *option = MATE_MIXER_SWITCH_OPTION (options->data);

                g_print ("\t\t|%c| %s\n",
                         (active != NULL && option == active)
                            ? '*'
                            : '-',
                         mate_mixer_switch_option_get_label (option));

                options = options->next;
            }

            g_print ("\n");
            switches = switches->next;
        }

        streams = streams->next;
    }
}

static void
connected (void)
{
    g_print ("Connected using the %s backend.\n\n",
             mate_mixer_context_get_backend_name (context));

    print_devices ();
    print_streams ();

    g_print ("Waiting for events. Hit CTRL+C to quit.\n");
}

static void
on_context_state_notify (void)
{
    MateMixerState state;

    state = mate_mixer_context_get_state (context);

    switch (state) {
    case MATE_MIXER_STATE_READY:
        /* This state can be reached repeatedly if the context is connected
         * to a sound server, the connection is dropped and then reestablished */
        connected ();
        break;
    case MATE_MIXER_STATE_FAILED:
        g_printerr ("Connection failed.\n");
        g_main_loop_quit (mainloop);
        break;
    default:
        break;
    }
}

static void
on_context_device_added (MateMixerContext *context, const gchar *name)
{
    g_print ("Device added: %s\n", name);
}

static void
on_context_device_removed (MateMixerContext *context, const gchar *name)
{
    g_print ("Device removed: %s\n", name);
}

static void
on_context_stream_added (MateMixerContext *context, const gchar *name)
{
    g_print ("Stream added: %s\n", name);
}

static void
on_context_stream_removed (MateMixerContext *context, const gchar *name)
{
    g_print ("Stream removed: %s\n", name);
}

#ifdef G_OS_UNIX
static gboolean
on_signal (gpointer mainloop)
{
    g_idle_add ((GSourceFunc) g_main_loop_quit, mainloop);

    return G_SOURCE_REMOVE;
}
#endif

int main (int argc, char *argv[])
{
    MateMixerState  state;
    GOptionContext *ctx;
    gboolean        debug   = FALSE;
    gchar          *backend = NULL;
    gchar          *server  = NULL;
    GError         *error   = NULL;
    GOptionEntry    entries[] = {
        { "backend", 'b', 0, G_OPTION_ARG_STRING, &backend, "Sound system to use (pulseaudio, alsa, oss, null)", NULL },
        { "debug",   'd', 0, G_OPTION_ARG_NONE,   &debug,   "Enable debug", NULL },
        { "server",  's', 0, G_OPTION_ARG_STRING, &server,  "Sound server address", NULL },
        { NULL }
    };

    ctx = g_option_context_new ("- libmatemixer monitor");

    g_option_context_add_main_entries (ctx, entries, NULL);

    if (g_option_context_parse (ctx, &argc, &argv, &error) == FALSE) {
        g_printerr ("%s\n", error->message);
        g_error_free (error);
        g_option_context_free (ctx);
        return 1;
    }

    g_option_context_free (ctx);

    if (debug == TRUE)
        g_setenv ("G_MESSAGES_DEBUG", "all", FALSE);

    /* Initialize the library.
     * If the function returns FALSE, the library is not usable. */
    if (mate_mixer_init () == FALSE)
        return 1;

    setlocale (LC_ALL, "");

    /* Create a libmatemixer context to access the library */
    context = mate_mixer_context_new ();

    /* Fill in some details about our application, only used with the PulseAudio backend */
    mate_mixer_context_set_app_name (context, "MateMixer Monitor");
    mate_mixer_context_set_app_id (context, "org.mate-desktop.libmatemixer-monitor");
    mate_mixer_context_set_app_version (context, "1.0");
    mate_mixer_context_set_app_icon (context, "multimedia-volume-control");

    if (backend != NULL) {
        if (strcmp (backend, "pulseaudio") == 0)
            mate_mixer_context_set_backend_type (context, MATE_MIXER_BACKEND_PULSEAUDIO);
        else if (strcmp (backend, "alsa") == 0)
            mate_mixer_context_set_backend_type (context, MATE_MIXER_BACKEND_ALSA);
        else if (strcmp (backend, "oss") == 0)
            mate_mixer_context_set_backend_type (context, MATE_MIXER_BACKEND_OSS);
        else if (strcmp (backend, "null") == 0)
            mate_mixer_context_set_backend_type (context, MATE_MIXER_BACKEND_NULL);
        else
            g_printerr ("Sound system backend '%s' is unknown, the backend will be auto-detected.\n",
                        backend);

        g_free (backend);
    }

    /* Set PulseAudio server address if requested */
    if (server != NULL) {
        mate_mixer_context_set_server_address (context, server);
        g_free (server);
    }

    /* Initiate connection to a sound system */
    if (mate_mixer_context_open (context) == FALSE) {
        g_printerr ("Could not connect to a sound system, quitting.\n");
        g_object_unref (context);
        return 1;
    }

    /* Connect to some basic signals of the context */
    g_signal_connect (G_OBJECT (context),
                      "device-added",
                      G_CALLBACK (on_context_device_added),
                      NULL);
    g_signal_connect (G_OBJECT (context),
                      "device-removed",
                      G_CALLBACK (on_context_device_removed),
                      NULL);
    g_signal_connect (G_OBJECT (context),
                      "stream-added",
                      G_CALLBACK (on_context_stream_added),
                      NULL);
    g_signal_connect (G_OBJECT (context),
                      "stream-removed",
                      G_CALLBACK (on_context_stream_removed),
                      NULL);

    /* When mate_mixer_context_open() returns TRUE, the state must be either
     * MATE_MIXER_STATE_READY or MATE_MIXER_STATE_CONNECTING. */
    state = mate_mixer_context_get_state (context);

    switch (state) {
    case MATE_MIXER_STATE_READY:
        connected ();
        break;
    case MATE_MIXER_STATE_CONNECTING:
        g_print ("Waiting for connection...\n");

        /* The state will change asynchronously to either MATE_MIXER_STATE_READY
         * or MATE_MIXER_STATE_FAILED, wait for the change in a main loop */
        g_signal_connect (G_OBJECT (context),
                          "notify::state",
                          G_CALLBACK (on_context_state_notify),
                          NULL);
        break;
    default:
        g_assert_not_reached ();
        break;
    }

    mainloop = g_main_loop_new (NULL, FALSE);

#ifdef G_OS_UNIX
    g_unix_signal_add (SIGTERM, on_signal, mainloop);
    g_unix_signal_add (SIGINT,  on_signal, mainloop);
#endif

    g_main_loop_run (mainloop);

    g_object_unref (context);
    return 0;
}
