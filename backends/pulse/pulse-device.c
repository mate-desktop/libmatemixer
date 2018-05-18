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
#include <glib/gi18n.h>
#include <glib-object.h>

#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include <pulse/pulseaudio.h>

#include "pulse-connection.h"
#include "pulse-device.h"
#include "pulse-device-profile.h"
#include "pulse-device-switch.h"
#include "pulse-port.h"
#include "pulse-stream.h"

struct _PulseDevicePrivate
{
    guint32            index;
    GHashTable        *ports;
    GHashTable        *streams;
    GList             *streams_list;
    PulseConnection   *connection;
    PulseDeviceSwitch *pswitch;
    GList             *pswitch_list;
};

enum {
    PROP_0,
    PROP_INDEX,
    PROP_CONNECTION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void pulse_device_class_init   (PulseDeviceClass *klass);

static void pulse_device_get_property (GObject          *object,
                                       guint             param_id,
                                       GValue           *value,
                                       GParamSpec       *pspec);
static void pulse_device_set_property (GObject          *object,
                                       guint             param_id,
                                       const GValue     *value,
                                       GParamSpec       *pspec);

static void pulse_device_init         (PulseDevice      *device);
static void pulse_device_dispose      (GObject          *object);
static void pulse_device_finalize     (GObject          *object);

G_DEFINE_TYPE (PulseDevice, pulse_device, MATE_MIXER_TYPE_DEVICE)

static MateMixerStream *pulse_device_get_stream    (MateMixerDevice    *mmd,
                                                    const gchar        *name);

static const GList *    pulse_device_list_streams  (MateMixerDevice    *mmd);
static const GList *    pulse_device_list_switches (MateMixerDevice    *mmd);

static void             pulse_device_load          (PulseDevice        *device,
                                                    const pa_card_info *info);

static void             free_list_streams          (PulseDevice        *device);

static void
pulse_device_class_init (PulseDeviceClass *klass)
{
    GObjectClass         *object_class;
    MateMixerDeviceClass *device_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = pulse_device_dispose;
    object_class->finalize     = pulse_device_finalize;
    object_class->get_property = pulse_device_get_property;
    object_class->set_property = pulse_device_set_property;

    device_class = MATE_MIXER_DEVICE_CLASS (klass);
    device_class->get_stream         = pulse_device_get_stream;
    device_class->list_streams       = pulse_device_list_streams;
    device_class->list_switches      = pulse_device_list_switches;

    properties[PROP_INDEX] =
        g_param_spec_uint ("index",
                           "Index",
                           "Index of the device",
                           0,
                           G_MAXUINT,
                           0,
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

    properties[PROP_CONNECTION] =
        g_param_spec_object ("connection",
                             "Connection",
                             "PulseAudio connection",
                             PULSE_TYPE_CONNECTION,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    g_type_class_add_private (object_class, sizeof (PulseDevicePrivate));
}

static void
pulse_device_get_property (GObject    *object,
                           guint       param_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    PulseDevice *device;

    device = PULSE_DEVICE (object);

    switch (param_id) {
    case PROP_INDEX:
        g_value_set_uint (value, device->priv->index);
        break;
    case PROP_CONNECTION:
        g_value_set_object (value, device->priv->connection);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_device_set_property (GObject      *object,
                           guint         param_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    PulseDevice *device;

    device = PULSE_DEVICE (object);

    switch (param_id) {
    case PROP_INDEX:
        device->priv->index = g_value_get_uint (value);
        break;
    case PROP_CONNECTION:
        device->priv->connection = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_device_init (PulseDevice *device)
{
    device->priv = G_TYPE_INSTANCE_GET_PRIVATE (device,
                                                PULSE_TYPE_DEVICE,
                                                PulseDevicePrivate);

    device->priv->ports = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 g_object_unref);

    device->priv->streams = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   g_free,
                                                   g_object_unref);
}

static void
pulse_device_dispose (GObject *object)
{
    PulseDevice *device;

    device = PULSE_DEVICE (object);

    g_hash_table_remove_all (device->priv->ports);
    g_hash_table_remove_all (device->priv->streams);

    g_clear_object (&device->priv->connection);
    g_clear_object (&device->priv->pswitch);

    free_list_streams (device);

    if (device->priv->pswitch_list != NULL) {
        g_list_free (device->priv->pswitch_list);
        device->priv->pswitch_list = NULL;
    }
    G_OBJECT_CLASS (pulse_device_parent_class)->dispose (object);
}

static void
pulse_device_finalize (GObject *object)
{
    PulseDevice *device;

    device = PULSE_DEVICE (object);

    g_hash_table_unref (device->priv->ports);
    g_hash_table_unref (device->priv->streams);

    G_OBJECT_CLASS (pulse_device_parent_class)->finalize (object);
}

PulseDevice *
pulse_device_new (PulseConnection *connection, const pa_card_info *info)
{
    PulseDevice *device;
    const gchar *label;
    const gchar *icon;

    g_return_val_if_fail (PULSE_IS_CONNECTION (connection), NULL);
    g_return_val_if_fail (info != NULL, NULL);

    label = pa_proplist_gets (info->proplist, PA_PROP_DEVICE_DESCRIPTION);
    if G_UNLIKELY (label == NULL)
        label = info->name;

    icon = pa_proplist_gets (info->proplist, PA_PROP_DEVICE_ICON_NAME);
    if G_UNLIKELY (icon == NULL)
        icon = "audio-card";

    /* Consider the device index as unchanging parameter */
    device = g_object_new (PULSE_TYPE_DEVICE,
                           "index", info->index,
                           "connection", connection,
                           "name", info->name,
                           "label", label,
                           "icon", icon,
                           NULL);

    pulse_device_load (device, info);
    pulse_device_update (device, info);

    return device;
}

void
pulse_device_update (PulseDevice *device, const pa_card_info *info)
{
    g_return_if_fail (PULSE_IS_DEVICE (device));
    g_return_if_fail (info != NULL);

    if G_LIKELY (info->active_profile2 != NULL)
        pulse_device_switch_set_active_profile_by_name (device->priv->pswitch,
                                                        info->active_profile2->name);
}

void
pulse_device_add_stream (PulseDevice *device, PulseStream *stream)
{
    const gchar *name;

    g_return_if_fail (PULSE_IS_DEVICE (device));
    g_return_if_fail (PULSE_IS_STREAM (stream));

    name = mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream));

    g_hash_table_insert (device->priv->streams,
                         g_strdup (name),
                         g_object_ref (stream));

    free_list_streams (device);

    g_signal_emit_by_name (G_OBJECT (device),
                           "stream-added",
                           name);
}

void
pulse_device_remove_stream (PulseDevice *device, PulseStream *stream)
{
    const gchar *name;

    g_return_if_fail (PULSE_IS_DEVICE (device));
    g_return_if_fail (PULSE_IS_STREAM (stream));

    name = mate_mixer_stream_get_name (MATE_MIXER_STREAM (stream));

    free_list_streams (device);

    g_hash_table_remove (device->priv->streams, name);
    g_signal_emit_by_name (G_OBJECT (device),
                           "stream-removed",
                           name);
}

guint32
pulse_device_get_index (PulseDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), 0);

    return device->priv->index;
}

PulseConnection *
pulse_device_get_connection (PulseDevice *device)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);

    return device->priv->connection;
}

PulsePort *
pulse_device_get_port (PulseDevice *device, const gchar *name)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (device), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_hash_table_lookup (device->priv->ports, name);
}

static MateMixerStream *
pulse_device_get_stream (MateMixerDevice *mmd, const gchar *name)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (mmd), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_hash_table_lookup (PULSE_DEVICE (mmd)->priv->streams, name);
}

static const GList *
pulse_device_list_streams (MateMixerDevice *mmd)
{
    PulseDevice *device;

    g_return_val_if_fail (PULSE_IS_DEVICE (mmd), NULL);

    device = PULSE_DEVICE (mmd);

    if (device->priv->streams_list == NULL) {
        device->priv->streams_list = g_hash_table_get_values (device->priv->streams);
        if (device->priv->streams_list != NULL)
            g_list_foreach (device->priv->streams_list, (GFunc) g_object_ref, NULL);
    }
    return device->priv->streams_list;
}

static const GList *
pulse_device_list_switches (MateMixerDevice *mmd)
{
    g_return_val_if_fail (PULSE_IS_DEVICE (mmd), NULL);

    return PULSE_DEVICE (mmd)->priv->pswitch_list;
}

static void
pulse_device_load (PulseDevice *device, const pa_card_info *info)
{
    guint i;

    for (i = 0; i < info->n_ports; i++) {
        PulsePort   *port;
        const gchar *name;
        const gchar *icon;

        name = info->ports[i]->name;
        icon = pa_proplist_gets (info->ports[i]->proplist, "device.icon_name");

        port = pulse_port_new (name,
                               info->ports[i]->description,
                               icon,
                               info->ports[i]->priority);

        g_hash_table_insert (device->priv->ports,
                             g_strdup (name),
                             port);
    }

    /* Create the device profile switch */
    if (info->n_profiles > 0) {
        device->priv->pswitch = pulse_device_switch_new ("profile",
                                                         _("Profile"),
                                                         device);

        device->priv->pswitch_list = g_list_prepend (NULL, device->priv->pswitch);
    }

    for (i = 0; i < info->n_profiles; i++) {
        PulseDeviceProfile *profile;

        pa_card_profile_info2 *p_info = info->profiles2[i];

        /* PulseAudio 5.0 includes a new pa_card_profile_info2 which only
         * differs in the new available flag, we use it not to include profiles
         * which are unavailable */
        if (p_info->available == 0)
            continue;

        profile = pulse_device_profile_new (p_info->name,
                                            p_info->description,
                                            p_info->priority);

        pulse_device_switch_add_profile (device->priv->pswitch, profile);

        g_object_unref (profile);
    }
}

static void
free_list_streams (PulseDevice *device)
{
    if (device->priv->streams_list == NULL)
        return;

    g_list_free_full (device->priv->streams_list, g_object_unref);

    device->priv->streams_list = NULL;
}
