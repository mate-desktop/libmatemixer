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
#include <glib-object.h>
#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include "oss-common.h"
#include "oss-stream.h"
#include "oss-switch.h"
#include "oss-switch-option.h"

struct _OssSwitchPrivate
{
    gint   fd;
    GList *options;
};

static void oss_switch_class_init (OssSwitchClass *klass);
static void oss_switch_init       (OssSwitch      *swtch);
static void oss_switch_dispose    (GObject        *object);
static void oss_switch_finalize   (GObject        *object);

G_DEFINE_TYPE (OssSwitch, oss_switch, MATE_MIXER_TYPE_STREAM_SWITCH)

static gboolean         oss_switch_set_active_option (MateMixerSwitch       *mms,
                                                      MateMixerSwitchOption *mmso);

static const GList *    oss_switch_list_options      (MateMixerSwitch       *mms);

static OssSwitchOption *choose_default_option        (OssSwitch             *swtch);

static void
oss_switch_class_init (OssSwitchClass *klass)
{
    GObjectClass         *object_class;
    MateMixerSwitchClass *switch_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose  = oss_switch_dispose;
    object_class->finalize = oss_switch_finalize;

    switch_class = MATE_MIXER_SWITCH_CLASS (klass);
    switch_class->set_active_option = oss_switch_set_active_option;
    switch_class->list_options      = oss_switch_list_options;

    g_type_class_add_private (G_OBJECT_CLASS (klass), sizeof (OssSwitchPrivate));
}

static void
oss_switch_init (OssSwitch *swtch)
{
    swtch->priv = G_TYPE_INSTANCE_GET_PRIVATE (swtch,
                                               OSS_TYPE_SWITCH,
                                               OssSwitchPrivate);
}

static void
oss_switch_dispose (GObject *object)
{
    OssSwitch *swtch;

    swtch = OSS_SWITCH (object);

    if (swtch->priv->options != NULL) {
        g_list_free_full (swtch->priv->options, g_object_unref);
        swtch->priv->options = NULL;
    }

    G_OBJECT_CLASS (oss_switch_parent_class)->dispose (object);
}

static void
oss_switch_finalize (GObject *object)
{
    OssSwitch *swtch;

    swtch = OSS_SWITCH (object);

    if (swtch->priv->fd != -1)
        close (swtch->priv->fd);

    G_OBJECT_CLASS (oss_switch_parent_class)->finalize (object);
}

OssSwitch *
oss_switch_new (OssStream   *stream,
                const gchar *name,
                const gchar *label,
                gint         fd,
                GList       *options)
{
    OssSwitch *swtch;
    gint       newfd;

    g_return_val_if_fail (OSS_IS_STREAM (stream), NULL);
    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (options != NULL, NULL);

    newfd = dup (fd);
    if (newfd == -1) {
        g_warning ("Failed to duplicate file descriptor: %s",
                   g_strerror (errno));
        return NULL;
    }

    swtch = g_object_new (OSS_TYPE_SWITCH,
                          "name", name,
                          "label", label,
                          "role", MATE_MIXER_STREAM_SWITCH_ROLE_PORT,
                          "stream", stream,
                          NULL);

    /* Takes ownership of options */
    swtch->priv->fd      = newfd;
    swtch->priv->options = options;

    return swtch;
}

void
oss_switch_load (OssSwitch *swtch)
{
    OssSwitchOption *option;
    gint             recsrc;
    gint             ret;

    g_return_if_fail (OSS_IS_SWITCH (swtch));

    if G_UNLIKELY (swtch->priv->fd == -1)
        return;

    /* Recsrc contains a bitmask of currently enabled recording sources */
    ret = ioctl (swtch->priv->fd, MIXER_READ (SOUND_MIXER_RECSRC), &recsrc);
    if (ret == -1)
        return;

    if (recsrc != 0) {
        GList *list = swtch->priv->options;

        /* Find out which option is currently selected */
        while (list != NULL) {
            gint devnum;

            option = OSS_SWITCH_OPTION (list->data);
            devnum = oss_switch_option_get_devnum (option);

            if (recsrc & (1 << devnum)) {
                /* It is possible that some hardware might allow and have more recording
                 * sources active at the same time, but we only support one active
                 * source at a time */
                _mate_mixer_switch_set_active_option (MATE_MIXER_SWITCH (swtch),
                                                      MATE_MIXER_SWITCH_OPTION (option));
                return;
            }
            list = list->next;
        }

        g_debug ("Switch %s has an unknown device as the active option",
                 mate_mixer_switch_get_name (MATE_MIXER_SWITCH (swtch)));

        /* OSS shouldn't let a non-record device be selected, let's step in and select
         * something reasonable instead... */
    } else {
         g_debug ("Switch %s has no active device",
                  mate_mixer_switch_get_name (MATE_MIXER_SWITCH (swtch)));

        /* According to the OSS Programmer's Guide, if the recsrc value is 0, the
         * microphone will be selected implicitly.
         * Let's not assume that's true everywhere and select something explicitly... */
    }

    option = choose_default_option (swtch);

    g_debug ("Selecting default device %s as active for switch %s",
             mate_mixer_switch_option_get_name (MATE_MIXER_SWITCH_OPTION (option)),
             mate_mixer_switch_get_name (MATE_MIXER_SWITCH (swtch)));

    if (mate_mixer_switch_set_active_option (MATE_MIXER_SWITCH (swtch),
                                             MATE_MIXER_SWITCH_OPTION (option)) == FALSE) {
        g_debug ("Failed to set the default device, assuming it is selected anyway");

        _mate_mixer_switch_set_active_option (MATE_MIXER_SWITCH (swtch),
                                              MATE_MIXER_SWITCH_OPTION (option));
    }
}

void
oss_switch_close (OssSwitch *swtch)
{
    g_return_if_fail (OSS_IS_SWITCH (swtch));

    if (swtch->priv->fd == -1)
        return;

    close (swtch->priv->fd);
    swtch->priv->fd = -1;
}

static gboolean
oss_switch_set_active_option (MateMixerSwitch *mms, MateMixerSwitchOption *mmso)
{
    OssSwitch *swtch;
    gint       ret;
    gint       recsrc;

    g_return_val_if_fail (OSS_IS_SWITCH (mms), FALSE);
    g_return_val_if_fail (OSS_IS_SWITCH_OPTION (mmso), FALSE);

    swtch = OSS_SWITCH (mms);

    if G_UNLIKELY (swtch->priv->fd == -1)
        return FALSE;

    recsrc = 1 << oss_switch_option_get_devnum (OSS_SWITCH_OPTION (mmso));

    ret = ioctl (swtch->priv->fd, MIXER_WRITE (SOUND_MIXER_RECSRC), &recsrc);
    if (ret == -1)
        return FALSE;

    return TRUE;
}

static const GList *
oss_switch_list_options (MateMixerSwitch *mms)
{
    g_return_val_if_fail (OSS_IS_SWITCH (mms), NULL);

    return OSS_SWITCH (mms)->priv->options;
}

#define OSS_SWITCH_PREFERRED_DEFAULT_DEVNUM 7 /* Microphone */

static OssSwitchOption *
choose_default_option (OssSwitch *swtch)
{
    GList *list = swtch->priv->options;

    /* Search for the preferred device */
    while (list != NULL) {
        OssSwitchOption *option;
        gint             devnum;

        option = OSS_SWITCH_OPTION (list->data);
        devnum = oss_switch_option_get_devnum (option);

        if (devnum == OSS_SWITCH_PREFERRED_DEFAULT_DEVNUM)
            return option;

        list = list->next;
    }
    /* If the preferred device is not present, use the first available one */
    return OSS_SWITCH_OPTION (swtch->priv->options->data);
}
