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
#include "oss-switch.h"
#include "oss-switch-option.h"

struct _OssSwitchPrivate
{
    gint   fd;
    GList *options;
};

static void oss_switch_class_init      (OssSwitchClass      *klass);
static void oss_switch_init            (OssSwitch           *swtch);
static void oss_switch_dispose         (GObject             *object);
static void oss_switch_finalize        (GObject             *object);

G_DEFINE_TYPE (OssSwitch, oss_switch, MATE_MIXER_TYPE_SWITCH)

static gboolean     oss_switch_set_active_option (MateMixerSwitch       *mms,
                                                  MateMixerSwitchOption *mmso);

static const GList *oss_switch_list_options      (MateMixerSwitch       *mms);

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
oss_switch_new (const gchar *name,
                const gchar *label,
                gint         fd,
                GList       *options)
{
    OssSwitch *swtch;

    swtch = g_object_new (OSS_TYPE_SWITCH,
                          "name", name,
                          "label", label,
                          "flags", MATE_MIXER_SWITCH_ALLOWS_NO_ACTIVE_OPTION,
                          "role", MATE_MIXER_SWITCH_ROLE_PORT,
                          NULL);

    /* Takes ownership of options */
    swtch->priv->fd      = dup (fd);
    swtch->priv->options = options;

    return swtch;
}

void
oss_switch_load (OssSwitch *swtch)
{
    GList *list;
    gint   recsrc;
    gint   ret;

    g_return_if_fail (OSS_IS_SWITCH (swtch));

    if G_UNLIKELY (swtch->priv->fd == -1)
        return;

    ret = ioctl (swtch->priv->fd, MIXER_READ (SOUND_MIXER_RECSRC), &recsrc);
    if (ret == -1)
        return;
    if (recsrc == 0) {
        _mate_mixer_switch_set_active_option (MATE_MIXER_SWITCH (swtch), NULL);
        return;
    }

    list = swtch->priv->options;
    while (list != NULL) {
        OssSwitchOption *option = OSS_SWITCH_OPTION (list->data);
        gint             devnum = oss_switch_option_get_devnum (option);

        /* Mark the selected option when we find it */
        if (recsrc & (1 << devnum)) {
            _mate_mixer_switch_set_active_option (MATE_MIXER_SWITCH (swtch),
                                                  MATE_MIXER_SWITCH_OPTION (option));
            return;
        }
        list = list->next;
    }

    g_warning ("Unknown active option of switch %s",
               mate_mixer_switch_get_name (MATE_MIXER_SWITCH (swtch)));
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
    gint       devnum;

    g_return_val_if_fail (OSS_IS_SWITCH (mms), FALSE);
    g_return_val_if_fail (OSS_IS_SWITCH_OPTION (mmso), FALSE);

    swtch = OSS_SWITCH (mms);

    if G_UNLIKELY (swtch->priv->fd == -1)
        return FALSE;

    devnum = oss_switch_option_get_devnum (OSS_SWITCH_OPTION (mmso));
    recsrc = 1 << devnum;

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
