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
#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include "pulse-connection.h"
#include "pulse-device.h"
#include "pulse-device-profile.h"
#include "pulse-device-switch.h"

struct _PulseDeviceSwitchPrivate
{
    GList *profiles;
};

static void pulse_device_switch_class_init   (PulseDeviceSwitchClass *klass);
static void pulse_device_switch_init         (PulseDeviceSwitch      *swtch);
static void pulse_device_switch_dispose      (GObject                *object);

G_DEFINE_TYPE (PulseDeviceSwitch, pulse_device_switch, MATE_MIXER_TYPE_DEVICE_SWITCH)

static gboolean     pulse_device_switch_set_active_option (MateMixerSwitch       *mms,
                                                           MateMixerSwitchOption *mmso);

static const GList *pulse_device_switch_list_options      (MateMixerSwitch       *mms);

static gint         compare_profiles                      (gconstpointer          a,
                                                           gconstpointer          b);
static gint         compare_profile_name                  (gconstpointer          a,
                                                           gconstpointer          b);

static void
pulse_device_switch_class_init (PulseDeviceSwitchClass *klass)
{
    GObjectClass         *object_class;
    MateMixerSwitchClass *switch_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = pulse_device_switch_dispose;

    switch_class = MATE_MIXER_SWITCH_CLASS (klass);
    switch_class->set_active_option = pulse_device_switch_set_active_option;
    switch_class->list_options      = pulse_device_switch_list_options;

    g_type_class_add_private (G_OBJECT_CLASS (klass), sizeof (PulseDeviceSwitchPrivate));
}

static void
pulse_device_switch_init (PulseDeviceSwitch *swtch)
{
    swtch->priv = G_TYPE_INSTANCE_GET_PRIVATE (swtch,
                                               PULSE_TYPE_DEVICE_SWITCH,
                                               PulseDeviceSwitchPrivate);
}

static void
pulse_device_switch_dispose (GObject *object)
{
    PulseDeviceSwitch *swtch;

    swtch = PULSE_DEVICE_SWITCH (object);

    if (swtch->priv->profiles != NULL) {
        g_list_free_full (swtch->priv->profiles, g_object_unref);
        swtch->priv->profiles = NULL;
    }
    G_OBJECT_CLASS (pulse_device_switch_parent_class)->dispose (object);
}

PulseDeviceSwitch *
pulse_device_switch_new (const gchar *name, const gchar *label, PulseDevice *device)
{
    g_return_val_if_fail (name  != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return g_object_new (PULSE_TYPE_DEVICE_SWITCH,
                         "name", name,
                         "label", label,
                         "role", MATE_MIXER_DEVICE_SWITCH_ROLE_PROFILE,
                         "device", device,
                         NULL);
}

void
pulse_device_switch_add_profile (PulseDeviceSwitch *swtch, PulseDeviceProfile *profile)
{
    g_return_if_fail (PULSE_IS_DEVICE_SWITCH (swtch));
    g_return_if_fail (PULSE_IS_DEVICE_PROFILE (profile));

    swtch->priv->profiles = g_list_insert_sorted (swtch->priv->profiles,
                                                  g_object_ref (profile),
                                                  compare_profiles);
}

void
pulse_device_switch_set_active_profile (PulseDeviceSwitch  *swtch,
                                        PulseDeviceProfile *profile)
{
    g_return_if_fail (PULSE_IS_DEVICE_SWITCH (swtch));
    g_return_if_fail (PULSE_IS_DEVICE_PROFILE (profile));

    _mate_mixer_switch_set_active_option (MATE_MIXER_SWITCH (swtch),
                                          MATE_MIXER_SWITCH_OPTION (profile));
}

void
pulse_device_switch_set_active_profile_by_name (PulseDeviceSwitch *swtch, const gchar *name)
{
    GList *item;

    g_return_if_fail (PULSE_IS_DEVICE_SWITCH (swtch));
    g_return_if_fail (name != NULL);

    item = g_list_find_custom (swtch->priv->profiles, name, compare_profile_name);
    if G_UNLIKELY (item == NULL) {
        g_debug ("Invalid device switch profile name %s", name);
        return;
    }
    pulse_device_switch_set_active_profile (swtch, PULSE_DEVICE_PROFILE (item->data));
}

static gboolean
pulse_device_switch_set_active_option (MateMixerSwitch *mms, MateMixerSwitchOption *mmso)
{
    MateMixerDevice *device;
    const gchar     *device_name;
    const gchar     *profile_name;

    g_return_val_if_fail (PULSE_IS_DEVICE_SWITCH (mms), FALSE);
    g_return_val_if_fail (PULSE_IS_DEVICE_PROFILE (mmso), FALSE);

    device = mate_mixer_device_switch_get_device (MATE_MIXER_DEVICE_SWITCH (mms));
    if G_UNLIKELY (device == NULL)
        return FALSE;

    device_name  = mate_mixer_device_get_name (device);
    profile_name = mate_mixer_switch_option_get_name (mmso);

    return pulse_connection_set_card_profile (pulse_device_get_connection (PULSE_DEVICE (device)),
                                              device_name,
                                              profile_name);
}

static const GList *
pulse_device_switch_list_options (MateMixerSwitch *swtch)
{
    g_return_val_if_fail (PULSE_IS_DEVICE_SWITCH (swtch), NULL);

    return PULSE_DEVICE_SWITCH (swtch)->priv->profiles;
}

static gint
compare_profiles (gconstpointer a, gconstpointer b)
{
    return pulse_device_profile_get_priority (PULSE_DEVICE_PROFILE (b)) -
           pulse_device_profile_get_priority (PULSE_DEVICE_PROFILE (a));
}

static gint
compare_profile_name (gconstpointer a, gconstpointer b)
{
    PulseDeviceProfile *profile = PULSE_DEVICE_PROFILE (a);
    const gchar        *name    = (const gchar *) b;

    return strcmp (pulse_device_profile_get_name (profile), name);
}
