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
#include <glib-unix.h>
#include <locale.h>

#include <libmatemixer/matemixer.h>

static MateMixerControl *control;

static gchar *
create_volume_bar (MateMixerStream *stream, double *percent)
{
    GString *string;
    gint64   volume;
    gint64   volume_min;
    gint64   volume_max;
    double   p;
    int      i;
    int      length = 30;
    int      stars;

    volume     = mate_mixer_stream_get_volume (stream);
    volume_min = mate_mixer_stream_get_min_volume (stream);
    volume_max = mate_mixer_stream_get_normal_volume (stream);

    string = g_string_new ("[");

    p = (double) (volume - volume_min) / (volume_max - volume_min) * 100;
    if (percent != NULL)
        *percent = p;

    stars = (int) ((p / 100) * length);

    for (i = 0; i < length; i++)
        g_string_append_c (string, (i < stars) ? '*' : '.');

    return g_string_free (g_string_append_c (string, ']'), FALSE);
}

static void
print_devices (void)
{
    const GList      *devices;
    const GList      *ports;
    const GList      *profiles;
    MateMixerProfile *active_profile;

    devices = mate_mixer_control_list_devices (control);

    while (devices) {
        MateMixerDevice *device = MATE_MIXER_DEVICE (devices->data);

        g_print ("Device %s\n"
                 "       |-| Description : %s\n"
                 "       |-| Icon Name   : %s\n\n",
                 mate_mixer_device_get_name (device),
                 mate_mixer_device_get_description (device),
                 mate_mixer_device_get_icon (device));

        ports = mate_mixer_device_list_ports (device);
        while (ports) {
            MateMixerPort *port = MATE_MIXER_PORT (ports->data);

            g_print ("       |-| Port %s\n"
                     "                |-| Description : %s\n"
                     "                |-| Icon Name   : %s\n"
                     "                |-| Priority    : %lu\n"
                     "                |-| Status      : \n\n",
                     mate_mixer_port_get_name (port),
                     mate_mixer_port_get_description (port),
                     mate_mixer_port_get_icon (port),
                     mate_mixer_port_get_priority (port));

            ports = ports->next;
        }

        profiles = mate_mixer_device_list_profiles (device);

        active_profile = mate_mixer_device_get_active_profile (device);
        while (profiles) {
            MateMixerProfile *profile = MATE_MIXER_PROFILE (profiles->data);

            g_print ("       |%c| Profile %s\n"
                     "                |-| Description : %s\n"
                     "                |-| Priority    : %lu\n\n",
                     (profile == active_profile)
                        ? 'A'
                        : '-',
                     mate_mixer_profile_get_name (profile),
                     mate_mixer_profile_get_description (profile),
                     mate_mixer_profile_get_priority (profile));

            profiles = profiles->next;
        }
        g_print ("\n");

        devices = devices->next;
    }
}

static void
print_streams (void)
{
    const GList      *streams;
    const GList      *ports;
    const GList      *profiles;
    MateMixerProfile *active_profile;

    streams = mate_mixer_control_list_streams (control);

    while (streams) {
        MateMixerStream *stream = MATE_MIXER_STREAM (streams->data);
        gchar   *volume_bar;
        gdouble  volume;

        volume_bar = create_volume_bar (stream, &volume);

        g_print ("Stream %s\n"
                 "       |-| Description : %s\n"
                 "       |-| Icon Name   : %s\n"
                 "       |-| Volume      : %s %.1f %%\n"
                 "       |-| Muted       : %s\n"
                 "       |-| Channels    : %d\n"
                 "       |-| Balance     : %.1f\n"
                 "       |-| Fade        : %.1f\n\n",
                 mate_mixer_stream_get_name (stream),
                 mate_mixer_stream_get_description (stream),
                 mate_mixer_stream_get_icon (stream),
                 volume_bar,
                 volume,
                 mate_mixer_stream_get_mute (stream) ? "Yes" : "No",
                 mate_mixer_stream_get_num_channels (stream),
                 mate_mixer_stream_get_balance (stream),
                 mate_mixer_stream_get_fade (stream));

        g_free (volume_bar);

        ports = mate_mixer_stream_list_ports (stream);
        while (ports) {
            MateMixerPort *port = MATE_MIXER_PORT (ports->data);

            g_print ("       |-| Port %s\n"
                     "                |-| Description : %s\n"
                     "                |-| Icon Name   : %s\n"
                     "                |-| Priority    : %lu\n"
                     "                |-| Status      : \n\n",
                     mate_mixer_port_get_name (port),
                     mate_mixer_port_get_description (port),
                     mate_mixer_port_get_icon (port),
                     mate_mixer_port_get_priority (port));

            ports = ports->next;
        }

        streams = streams->next;
    }
}

static void
connected (void)
{
    g_print ("Connected using the %s backend.\n\n",
             mate_mixer_control_get_backend_name (control));

    print_devices ();
    print_streams ();

    g_print ("Waiting for events. Hit CTRL+C to quit.\n");
}

static void
state_cb (void)
{
    MateMixerState state;

    state = mate_mixer_control_get_state (control);

    switch (state) {
    case MATE_MIXER_STATE_READY:
        connected ();
        break;
    case MATE_MIXER_STATE_FAILED:
        g_printerr ("aaa");
        break;
    default:
        g_assert_not_reached ();
        break;
    }
}

static gboolean
signal_cb (gpointer mainloop)
{
    g_idle_add ((GSourceFunc) g_main_loop_quit, mainloop);
    return FALSE;
}

int main ()
{
    MateMixerState  state;
    GMainLoop      *mainloop;

    setlocale (LC_ALL, "");

    /* The library */

    if (!mate_mixer_init ())
        return 1;

    control = mate_mixer_control_new ();

    mate_mixer_control_set_app_name (control, "MateMixer Monitor");
    mate_mixer_control_set_app_icon (control, "multimedia-volume-control");

    if (!mate_mixer_control_open (control)) {
        g_object_unref (control);
        mate_mixer_deinit ();
        return 1;
    }

    state = mate_mixer_control_get_state (control);

    switch (state) {
    case MATE_MIXER_STATE_READY:
        connected ();
        break;
    case MATE_MIXER_STATE_CONNECTING:
        g_print ("Waiting for connection...\n");

        /* This state means that the result will be determined asynchronously, so
         * let's wait until the state transitions to a different value */
        g_signal_connect (control, "notify::state", G_CALLBACK (state_cb), NULL);
        break;
    default:
        /* If mate_mixer_control_open() returns TRUE, these two are the only
         * possible states.
         * In case mate_mixer_control_open() returned FALSE, the current state
         * would be MATE_MIXER_STATE_FAILED */
        g_assert_not_reached ();
        break;
    }

    mainloop = g_main_loop_new (NULL, FALSE);

    g_unix_signal_add (SIGTERM, signal_cb, mainloop);
    g_unix_signal_add (SIGINT,  signal_cb, mainloop);

    g_main_loop_run (mainloop);

    g_object_unref (control);
    mate_mixer_deinit ();
    return 0;
}
