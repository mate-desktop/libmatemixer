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
#include "alsa-switch.h"
#include "alsa-switch-option.h"

struct _AlsaSwitchPrivate
{
    GList            *options;
    guint32           channel_mask;
    snd_mixer_elem_t *element;
};

static void alsa_element_interface_init (AlsaElementInterface *iface);

static void alsa_switch_class_init      (AlsaSwitchClass      *klass);
static void alsa_switch_init            (AlsaSwitch           *swtch);
static void alsa_switch_dispose         (GObject              *object);

G_DEFINE_TYPE_WITH_CODE (AlsaSwitch, alsa_switch,
                         MATE_MIXER_TYPE_STREAM_SWITCH,
                         G_IMPLEMENT_INTERFACE (ALSA_TYPE_ELEMENT,
                                                alsa_element_interface_init))

static gboolean               alsa_switch_set_active_option (MateMixerSwitch       *mms,
                                                             MateMixerSwitchOption *mmso);

static const GList *          alsa_switch_list_options      (MateMixerSwitch       *mms);

static snd_mixer_elem_t *     alsa_switch_get_snd_element   (AlsaElement           *element);
static void                   alsa_switch_set_snd_element   (AlsaElement           *element,
                                                             snd_mixer_elem_t      *el);
static gboolean               alsa_switch_load              (AlsaElement           *element);

static void
alsa_element_interface_init (AlsaElementInterface *iface)
{
    iface->get_snd_element = alsa_switch_get_snd_element;
    iface->set_snd_element = alsa_switch_set_snd_element;
    iface->load            = alsa_switch_load;
}

static void
alsa_switch_class_init (AlsaSwitchClass *klass)
{
    GObjectClass         *object_class;
    MateMixerSwitchClass *switch_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = alsa_switch_dispose;

    switch_class = MATE_MIXER_SWITCH_CLASS (klass);
    switch_class->set_active_option = alsa_switch_set_active_option;
    switch_class->list_options      = alsa_switch_list_options;

    g_type_class_add_private (G_OBJECT_CLASS (klass), sizeof (AlsaSwitchPrivate));
}

static void
alsa_switch_dispose (GObject *object)
{
    AlsaSwitch *swtch;

    swtch = ALSA_SWITCH (object);

    if (swtch->priv->options != NULL) {
        g_list_free_full (swtch->priv->options, g_object_unref);
        swtch->priv->options = NULL;
    }

    G_OBJECT_CLASS (alsa_switch_parent_class)->dispose (object);
}

static void
alsa_switch_init (AlsaSwitch *swtch)
{
    swtch->priv = G_TYPE_INSTANCE_GET_PRIVATE (swtch,
                                               ALSA_TYPE_SWITCH,
                                               AlsaSwitchPrivate);
}

AlsaSwitch *
alsa_switch_new (AlsaStream               *stream,
                 const gchar              *name,
                 const gchar              *label,
                 MateMixerStreamSwitchRole role,
                 GList                    *options)
{
    AlsaSwitch *swtch;

    g_return_val_if_fail (ALSA_IS_STREAM (stream), NULL);
    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (options != NULL, NULL);

    swtch = g_object_new (ALSA_TYPE_SWITCH,
                          "name", name,
                          "label", label,
                          "role", role,
                          "stream", stream,
                          NULL);

    /* Takes ownership of options */
    swtch->priv->options = options;
    return swtch;
}

static gboolean
alsa_switch_set_active_option (MateMixerSwitch *mms, MateMixerSwitchOption *mmso)
{
    AlsaSwitch                  *swtch;
    guint                        index;
    gboolean                     set_item = FALSE;
    snd_mixer_selem_channel_id_t channel;

    g_return_val_if_fail (ALSA_IS_SWITCH (mms), FALSE);
    g_return_val_if_fail (ALSA_IS_SWITCH_OPTION (mmso), FALSE);

    swtch = ALSA_SWITCH (mms);

    if G_UNLIKELY (swtch->priv->element == NULL)
        return FALSE;

    /* The channel mask is created when reading the active option the first
     * time, so a successful load must be done before changing the option */
    if G_UNLIKELY (swtch->priv->channel_mask == 0) {
        g_debug ("Not setting active switch option, channel mask unknown");
        return FALSE;
    }

    index = alsa_switch_option_get_id (ALSA_SWITCH_OPTION (mmso));

    for (channel = 0; channel < SND_MIXER_SCHN_LAST; channel++) {
        /* The option is set per-channel, make sure to set it only for channels
         * we successfully read the value from */
        if (swtch->priv->channel_mask & (1 << channel)) {
            gint ret = snd_mixer_selem_set_enum_item (swtch->priv->element,
                                                      channel,
                                                      index);
            if (ret == 0)
                set_item = TRUE;
            else
                g_warning ("Failed to set active option of switch %s: %s",
                           snd_mixer_selem_get_name (swtch->priv->element),
                           snd_strerror (ret));
        }
    }
    return set_item;
}

static const GList *
alsa_switch_list_options (MateMixerSwitch *mms)
{
    g_return_val_if_fail (ALSA_IS_SWITCH (mms), NULL);

    return ALSA_SWITCH (mms)->priv->options;
}

static snd_mixer_elem_t *
alsa_switch_get_snd_element (AlsaElement *element)
{
    g_return_val_if_fail (ALSA_IS_SWITCH (element), NULL);

    return ALSA_SWITCH (element)->priv->element;
}

static void
alsa_switch_set_snd_element (AlsaElement *element, snd_mixer_elem_t *el)
{
    g_return_if_fail (ALSA_IS_SWITCH (element));

    ALSA_SWITCH (element)->priv->element = el;
}

static gboolean
alsa_switch_load (AlsaElement *element)
{
    AlsaSwitch                  *swtch;
    GList                       *list;
    guint                        item;
    gint                         ret;
    snd_mixer_selem_channel_id_t c;

    g_return_val_if_fail (ALSA_IS_SWITCH (element), FALSE);

    swtch = ALSA_SWITCH (element);

    if G_UNLIKELY (swtch->priv->element == NULL)
        return FALSE;

    /* When reading the first time we try all the channels, otherwise only the
     * ones which returned success before */
    if (swtch->priv->channel_mask == 0) {
        for (c = 0; c < SND_MIXER_SCHN_LAST; c++) {
            ret = snd_mixer_selem_get_enum_item (swtch->priv->element, c, &item);

            /* The active enum option is set per-channel, so when reading it the
             * first time, create a mask of all channels for which we read the
             * value successfully */
            if (ret == 0)
                swtch->priv->channel_mask |= 1 << c;
        }

        /* The last ALSA call might have failed, but it doesn't matter if we have
         * a channel mask */
        if (swtch->priv->channel_mask > 0)
            ret = 0;
    } else {
        for (c = 0; !(swtch->priv->channel_mask & (1 << c)); c++)
            ;

        /* When not reading the mask, the first usable channel is enough, we don't
         * support per-channel selections anyway */
        ret = snd_mixer_selem_get_enum_item (swtch->priv->element, c, &item);
    }

    if (ret < 0) {
        g_warning ("Failed to read active option of switch %s: %s",
                   snd_mixer_selem_get_name (swtch->priv->element),
                   snd_strerror (ret));
        return FALSE;
    }

    list = swtch->priv->options;
    while (list != NULL) {
        AlsaSwitchOption *option = ALSA_SWITCH_OPTION (list->data);

        /* Mark the selected option when we find it, ALSA indentifies them
         * by numeric indices */
        if (alsa_switch_option_get_id (option) == item) {
            _mate_mixer_switch_set_active_option (MATE_MIXER_SWITCH (swtch),
                                                  MATE_MIXER_SWITCH_OPTION (option));
            return TRUE;
        }
        list = list->next;
    }

    g_warning ("Unknown active option of switch %s: %d",
               mate_mixer_switch_get_name (MATE_MIXER_SWITCH (swtch)),
               item);

    return FALSE;
}
