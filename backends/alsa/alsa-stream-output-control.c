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

#include "alsa-compat.h"
#include "alsa-constants.h"
#include "alsa-element.h"
#include "alsa-stream.h"
#include "alsa-stream-control.h"
#include "alsa-stream-output-control.h"

static void alsa_stream_output_control_class_init (AlsaStreamOutputControlClass *klass);
static void alsa_stream_output_control_init       (AlsaStreamOutputControl      *control);

G_DEFINE_TYPE (AlsaStreamOutputControl, alsa_stream_output_control, ALSA_TYPE_STREAM_CONTROL)

static gboolean alsa_stream_output_control_load                    (AlsaStreamControl           *control);

static gboolean alsa_stream_output_control_set_mute                (AlsaStreamControl           *control,
                                                                    gboolean                     mute);

static gboolean alsa_stream_output_control_set_volume              (AlsaStreamControl           *control,
                                                                    guint                        volume);

static gboolean alsa_stream_output_control_set_channel_volume      (AlsaStreamControl           *control,
                                                                    snd_mixer_selem_channel_id_t channel,
                                                                    guint                        volume);

static gboolean alsa_stream_output_control_get_volume_from_decibel (AlsaStreamControl           *control,
                                                                    gdouble                      decibel,
                                                                    guint                       *volume);

static gboolean alsa_stream_output_control_get_decibel_from_volume (AlsaStreamControl           *control,
                                                                    guint                        volume,
                                                                    gdouble                     *decibel);

static void     read_volume_data                                   (snd_mixer_elem_t            *el,
                                                                    AlsaControlData             *data);

static void
alsa_stream_output_control_class_init (AlsaStreamOutputControlClass *klass)
{
    AlsaStreamControlClass *control_class;

    control_class = ALSA_STREAM_CONTROL_CLASS (klass);

    control_class->load                    = alsa_stream_output_control_load;
    control_class->set_mute                = alsa_stream_output_control_set_mute;
    control_class->set_volume              = alsa_stream_output_control_set_volume;
    control_class->set_channel_volume      = alsa_stream_output_control_set_channel_volume;
    control_class->get_volume_from_decibel = alsa_stream_output_control_get_volume_from_decibel;
    control_class->get_decibel_from_volume = alsa_stream_output_control_get_decibel_from_volume;
}

static void
alsa_stream_output_control_init (AlsaStreamOutputControl *control)
{
}

AlsaStreamControl *
alsa_stream_output_control_new (const gchar               *name,
                                const gchar               *label,
                                MateMixerStreamControlRole role,
                                AlsaStream                *stream)
{
    g_return_val_if_fail (name  != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (ALSA_IS_STREAM (stream), NULL);

    return g_object_new (ALSA_TYPE_STREAM_OUTPUT_CONTROL,
                         "name", name,
                         "label", label,
                         "role", role,
                         "stream", stream,
                         NULL);
}

static gboolean
alsa_stream_output_control_load (AlsaStreamControl *control)
{
    AlsaControlData   data;
    snd_mixer_elem_t *el;

    g_return_val_if_fail (ALSA_IS_STREAM_OUTPUT_CONTROL (control), FALSE);

    el = alsa_element_get_snd_element (ALSA_ELEMENT (control));
    if G_UNLIKELY (el == NULL)
        return FALSE;

    if G_UNLIKELY (snd_mixer_selem_has_playback_volume (el) == 0 &&
                   snd_mixer_selem_has_common_volume (el) == 0) {
        g_warn_if_reached ();
        return FALSE;
    }

    memset (&data, 0, sizeof (AlsaControlData));

    /* We model any control switch as mute */
    if (snd_mixer_selem_has_playback_switch (el) == 1 ||
        snd_mixer_selem_has_common_switch (el) == 1)
        data.switch_usable = TRUE;

    data.active = snd_mixer_selem_is_active (el);

    /* Read the volume data but do not error out if it fails, since ALSA reports
     * the control to have a volume, expect the control to match what we need - slider
     * with an optional mute toggle.
     * If it fails to read the volume data, just treat it as a volumeless control */
    read_volume_data (el, &data);

    alsa_stream_control_set_data (control, &data);
    return TRUE;
}

static gboolean
alsa_stream_output_control_set_mute (AlsaStreamControl *control, gboolean mute)
{
    snd_mixer_elem_t *el;
    gint              ret;

    g_return_val_if_fail (ALSA_IS_STREAM_OUTPUT_CONTROL (control), FALSE);

    el = alsa_element_get_snd_element (ALSA_ELEMENT (control));
    if G_UNLIKELY (el == NULL)
        return FALSE;

    /* Set the switch for all channels */
    ret = snd_mixer_selem_set_playback_switch_all (el, !mute);
    if (ret < 0) {
        g_warning ("Failed to set playback switch: %s", snd_strerror (ret));
        return FALSE;
    }
    return TRUE;
}

static gboolean
alsa_stream_output_control_set_volume (AlsaStreamControl *control, guint volume)
{
    snd_mixer_elem_t *el;
    gint              ret;

    g_return_val_if_fail (ALSA_IS_STREAM_OUTPUT_CONTROL (control), FALSE);

    el = alsa_element_get_snd_element (ALSA_ELEMENT (control));
    if G_UNLIKELY (el == NULL)
        return FALSE;

    /* Set the volume for all channels */
    ret = snd_mixer_selem_set_playback_volume_all (el, volume);
    if (ret < 0) {
        g_warning ("Failed to set volume: %s", snd_strerror (ret));
        return FALSE;
    }
    return TRUE;
}

static gboolean
alsa_stream_output_control_set_channel_volume (AlsaStreamControl           *control,
                                               snd_mixer_selem_channel_id_t channel,
                                               guint                        volume)
{
    snd_mixer_elem_t *el;
    gint              ret;

    g_return_val_if_fail (ALSA_IS_STREAM_OUTPUT_CONTROL (control), FALSE);

    el = alsa_element_get_snd_element (ALSA_ELEMENT (control));
    if G_UNLIKELY (el == NULL)
        return FALSE;

    /* Set the volume for a single channel */
    ret = snd_mixer_selem_set_playback_volume (el, channel, volume);
    if (ret < 0) {
        g_warning ("Failed to set channel volume: %s", snd_strerror (ret));
        return FALSE;
    }
    return TRUE;
}

static gboolean
alsa_stream_output_control_get_volume_from_decibel (AlsaStreamControl *control,
                                                    gdouble            decibel,
                                                    guint             *volume)
{
#if SND_LIB_VERSION >= ALSA_PACK_VERSION (1, 0, 17)
    snd_mixer_elem_t *el;
    glong             value;
    gint              ret;

    g_return_val_if_fail (ALSA_IS_STREAM_OUTPUT_CONTROL (control), FALSE);

    el = alsa_element_get_snd_element (ALSA_ELEMENT (control));
    if G_UNLIKELY (el == NULL)
        return FALSE;

    ret = snd_mixer_selem_ask_playback_dB_vol (el, (glong) (decibel * 100), 0, &value);
    if (ret < 0) {
        g_warning ("Failed to convert volume: %s", snd_strerror (ret));
        return FALSE;
    }

    *volume = value;
    return TRUE;
#else
    return FALSE;
#endif
}

static gboolean
alsa_stream_output_control_get_decibel_from_volume (AlsaStreamControl *control,
                                                    guint              volume,
                                                    gdouble           *decibel)
{
#if SND_LIB_VERSION >= ALSA_PACK_VERSION (1, 0, 17)
    snd_mixer_elem_t *el;
    glong             value;
    gint              ret;

    g_return_val_if_fail (ALSA_IS_STREAM_OUTPUT_CONTROL (control), FALSE);

    el = alsa_element_get_snd_element (ALSA_ELEMENT (control));
    if G_UNLIKELY (el == NULL)
        return FALSE;

    ret = snd_mixer_selem_ask_playback_vol_dB (el, (glong) volume, &value);
    if (ret < 0) {
        g_warning ("Failed to convert volume: %s", snd_strerror (ret));
        return FALSE;
    }

    *decibel = value / 100.0;
    return TRUE;
#else
    return FALSE;
#endif
}

static void
read_volume_data (snd_mixer_elem_t *el, AlsaControlData *data)
{
    glong volume;
    glong min, max;
    gint  ret;
    gint  i;

    /* Read volume ranges, this call should never fail on valid input */
#if SND_LIB_VERSION >= ALSA_PACK_VERSION (1, 0, 10)
    ret = snd_mixer_selem_get_playback_volume_range (el, &min, &max);
    if G_UNLIKELY (ret < 0) {
        g_warning ("Failed to read playback volume range: %s", snd_strerror (ret));
        return;
    }
#else
    snd_mixer_selem_get_playback_volume_range (el, &min, &max);
#endif
    data->min = (guint) min;
    data->max = (guint) max;

#if SND_LIB_VERSION >= ALSA_PACK_VERSION (1, 0, 10)
    /* This fails when decibels are not supported */
    ret = snd_mixer_selem_get_playback_dB_range (el, &min, &max);
    if (ret == 0) {
        data->min_decibel = min / 100.0;
        data->max_decibel = max / 100.0;
    } else
        data->min_decibel = data->max_decibel = -MATE_MIXER_INFINITY;
#else
    data->min_decibel = data->max_decibel = -MATE_MIXER_INFINITY;
#endif

    for (i = 0; i < MATE_MIXER_CHANNEL_MAX; i++)
        data->v[i] = data->min;

    data->volume = data->min;
    data->volume_joined = snd_mixer_selem_has_playback_volume_joined (el);

    if (data->switch_usable == TRUE)
        data->switch_joined = snd_mixer_selem_has_playback_switch_joined (el);

    if (snd_mixer_selem_is_playback_mono (el) == 1) {
        /* Special handling for single channel controls */
        ret = snd_mixer_selem_get_playback_volume (el, SND_MIXER_SCHN_MONO, &volume);
        if (ret == 0) {
            data->channels = 1;

            data->c[0] = MATE_MIXER_CHANNEL_MONO;
            data->v[0] = data->volume = (guint) volume;
        } else {
            g_warning ("Failed to read playback volume: %s", snd_strerror (ret));
        }

        if (data->switch_usable == TRUE) {
            gint value;

            ret = snd_mixer_selem_get_playback_switch (el, SND_MIXER_SCHN_MONO, &value);
            if G_LIKELY (ret == 0)
                data->m[0] = !value;
        }
    } else {
        snd_mixer_selem_channel_id_t channel;

        /* We use numeric channel indices, but ALSA only works with channel
         * positions, go over all the positions supported by ALSA and create
         * a list of channels */
        for (channel = 0; channel < SND_MIXER_SCHN_LAST; channel++) {
            if (snd_mixer_selem_has_playback_channel (el, channel) == 0)
                continue;

            if (data->switch_usable == TRUE) {
                gint value;

                ret = snd_mixer_selem_get_playback_switch (el, channel, &value);
                if (ret == 0)
                    data->m[channel] = !value;
            }

            ret = snd_mixer_selem_get_playback_volume (el, channel, &volume);
            if (ret < 0) {
                g_warning ("Failed to read playback volume: %s", snd_strerror (ret));
                continue;
            }
            data->channels++;

            /* The single value volume is the highest channel volume */
            if (data->volume < volume)
                data->volume = volume;

            data->c[channel] = alsa_channel_map_from[channel];
            data->v[channel] = (guint) volume;
        }
    }
}
