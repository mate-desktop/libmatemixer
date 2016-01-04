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

#include "alsa-element.h"
#include "alsa-stream.h"
#include "alsa-switch-option.h"
#include "alsa-toggle.h"

struct _AlsaTogglePrivate
{
    AlsaToggleType    type;
    guint32           channel_mask;
    snd_mixer_elem_t *element;
};

static void alsa_element_interface_init (AlsaElementInterface *iface);

static void alsa_toggle_class_init      (AlsaToggleClass      *klass);
static void alsa_toggle_init            (AlsaToggle           *toggle);

G_DEFINE_TYPE_WITH_CODE (AlsaToggle, alsa_toggle, MATE_MIXER_TYPE_STREAM_TOGGLE,
                         G_IMPLEMENT_INTERFACE (ALSA_TYPE_ELEMENT,
                                                alsa_element_interface_init))

static gboolean          alsa_toggle_set_active_option (MateMixerSwitch       *mms,
                                                        MateMixerSwitchOption *mmso);

static snd_mixer_elem_t *alsa_toggle_get_snd_element   (AlsaElement           *element);
static void              alsa_toggle_set_snd_element   (AlsaElement           *element,
                                                        snd_mixer_elem_t      *el);
static gboolean          alsa_toggle_load              (AlsaElement           *element);

static void
alsa_element_interface_init (AlsaElementInterface *iface)
{
    iface->get_snd_element = alsa_toggle_get_snd_element;
    iface->set_snd_element = alsa_toggle_set_snd_element;
    iface->load            = alsa_toggle_load;
}

static void
alsa_toggle_class_init (AlsaToggleClass *klass)
{
    MateMixerSwitchClass *switch_class;

    switch_class = MATE_MIXER_SWITCH_CLASS (klass);
    switch_class->set_active_option = alsa_toggle_set_active_option;

    g_type_class_add_private (G_OBJECT_CLASS (klass), sizeof (AlsaTogglePrivate));
}

static void
alsa_toggle_init (AlsaToggle *toggle)
{
    toggle->priv = G_TYPE_INSTANCE_GET_PRIVATE (toggle,
                                                ALSA_TYPE_TOGGLE,
                                                AlsaTogglePrivate);
}

AlsaToggle *
alsa_toggle_new (AlsaStream               *stream,
                 const gchar              *name,
                 const gchar              *label,
                 MateMixerStreamSwitchRole role,
                 AlsaToggleType            type,
                 AlsaSwitchOption         *on,
                 AlsaSwitchOption         *off)
{
    AlsaToggle *toggle;

    g_return_val_if_fail (ALSA_IS_STREAM (stream), NULL);
    g_return_val_if_fail (name  != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (ALSA_IS_SWITCH_OPTION (on), NULL);
    g_return_val_if_fail (ALSA_IS_SWITCH_OPTION (off), NULL);

    toggle = g_object_new (ALSA_TYPE_TOGGLE,
                           "name", name,
                           "label", label,
                           "flags", MATE_MIXER_STREAM_SWITCH_TOGGLE,
                           "role", role,
                           "stream", stream,
                           "on-state-option", on,
                           "off-state-option", off,
                           NULL);

    toggle->priv->type = type;
    return toggle;
}

static gboolean
alsa_toggle_set_active_option (MateMixerSwitch *mms, MateMixerSwitchOption *mmso)
{
    AlsaToggle *toggle;
    gint        value;
    gint        ret;

    g_return_val_if_fail (ALSA_IS_TOGGLE (mms), FALSE);
    g_return_val_if_fail (ALSA_IS_SWITCH_OPTION (mmso), FALSE);

    toggle = ALSA_TOGGLE (mms);

    if G_UNLIKELY (toggle->priv->element == NULL)
        return FALSE;

    /* For toggles the 0/1 value is stored as the switch option id, there is not really
     * a need to validate that the option belong to the switch, just make sure it
     * contains the value 0 or 1 */
    value = alsa_switch_option_get_id (ALSA_SWITCH_OPTION (mmso));
    if G_UNLIKELY (value != 0 && value != 1) {
        g_warn_if_reached ();
        return FALSE;
    }

    if (toggle->priv->type == ALSA_TOGGLE_CAPTURE)
        ret = snd_mixer_selem_set_capture_switch_all (toggle->priv->element, value);
    else
        ret = snd_mixer_selem_set_playback_switch_all (toggle->priv->element, value);

    if (ret < 0) {
        g_warning ("Failed to set value of toggle %s: %s",
                   snd_mixer_selem_get_name (toggle->priv->element),
                   snd_strerror (ret));
        return FALSE;
    }

    return TRUE;
}

static snd_mixer_elem_t *
alsa_toggle_get_snd_element (AlsaElement *element)
{
    g_return_val_if_fail (ALSA_IS_TOGGLE (element), NULL);

    return ALSA_TOGGLE (element)->priv->element;
}

static void
alsa_toggle_set_snd_element (AlsaElement *element, snd_mixer_elem_t *el)
{
    g_return_if_fail (ALSA_IS_TOGGLE (element));

    ALSA_TOGGLE (element)->priv->element = el;
}

static gboolean
alsa_toggle_load (AlsaElement *element)
{
    AlsaToggle                  *toggle;
    gint                         value;
    gint                         ret;
    snd_mixer_selem_channel_id_t c;

    g_return_val_if_fail (ALSA_IS_TOGGLE (element), FALSE);

    toggle = ALSA_TOGGLE (element);

    if G_UNLIKELY (toggle->priv->element == NULL)
        return FALSE;

    /* When reading the first time we try all the channels, otherwise only the
     * ones which returned success before */
    if (toggle->priv->channel_mask == 0) {
        for (c = 0; c < SND_MIXER_SCHN_LAST; c++) {
            if (toggle->priv->type == ALSA_TOGGLE_CAPTURE)
                ret = snd_mixer_selem_get_capture_switch (toggle->priv->element,
                                                          c,
                                                          &value);
            else
                ret = snd_mixer_selem_get_playback_switch (toggle->priv->element,
                                                           c,
                                                           &value);

            /* The active enum option is set per-channel, so when reading it the
             * first time, create a mask of all channels for which we read the
             * value successfully */
            if (ret == 0)
                toggle->priv->channel_mask |= 1 << c;
        }

        /* The last ALSA call might have failed, but it doesn't matter if we have
         * a channel mask */
        if (toggle->priv->channel_mask > 0)
            ret = 0;
    } else {
        for (c = 0; !(toggle->priv->channel_mask & (1 << c)); c++)
            ;

        /* When not reading the mask, the first usable channel is enough, we don't
         * support per-channel selections anyway */
        if (toggle->priv->type == ALSA_TOGGLE_CAPTURE)
            ret = snd_mixer_selem_get_capture_switch (toggle->priv->element,
                                                      c,
                                                      &value);
        else
            ret = snd_mixer_selem_get_playback_switch (toggle->priv->element,
                                                       c,
                                                       &value);
    }

    if (ret == 0) {
        MateMixerSwitchOption *active;

        if (value > 0)
            active = mate_mixer_stream_toggle_get_state_option (MATE_MIXER_STREAM_TOGGLE (toggle),
                                                                TRUE);
        else
            active = mate_mixer_stream_toggle_get_state_option (MATE_MIXER_STREAM_TOGGLE (toggle),
                                                                FALSE);

        _mate_mixer_switch_set_active_option (MATE_MIXER_SWITCH (toggle), active);
        return TRUE;
    }

    g_warning ("Failed to read state of toggle %s: %s",
               snd_mixer_selem_get_name (toggle->priv->element),
               snd_strerror (ret));

    return FALSE;
}
