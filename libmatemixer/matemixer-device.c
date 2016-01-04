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

#include "matemixer-device.h"
#include "matemixer-device-switch.h"
#include "matemixer-stream.h"
#include "matemixer-switch.h"

/**
 * SECTION:matemixer-device
 * @short_description: Hardware or software device in the sound system
 * @include: libmatemixer/matemixer.h
 *
 * A #MateMixerDevice represents a sound device, most typically a sound card.
 *
 * Each device may contain an arbitrary number of streams.
 */

struct _MateMixerDevicePrivate
{
    gchar *name;
    gchar *label;
    gchar *icon;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_LABEL,
    PROP_ICON,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
    STREAM_ADDED,
    STREAM_REMOVED,
    SWITCH_ADDED,
    SWITCH_REMOVED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void mate_mixer_device_class_init   (MateMixerDeviceClass *klass);

static void mate_mixer_device_get_property (GObject              *object,
                                            guint                 param_id,
                                            GValue               *value,
                                            GParamSpec           *pspec);
static void mate_mixer_device_set_property (GObject              *object,
                                            guint                 param_id,
                                            const GValue         *value,
                                            GParamSpec           *pspec);

static void mate_mixer_device_init         (MateMixerDevice      *device);
static void mate_mixer_device_finalize     (GObject              *object);

G_DEFINE_ABSTRACT_TYPE (MateMixerDevice, mate_mixer_device, G_TYPE_OBJECT)

static MateMixerStream *      mate_mixer_device_real_get_stream (MateMixerDevice *device,
                                                                 const gchar     *name);
static MateMixerDeviceSwitch *mate_mixer_device_real_get_switch (MateMixerDevice *device,
                                                                 const gchar     *name);

static void
mate_mixer_device_class_init (MateMixerDeviceClass *klass)
{
    GObjectClass *object_class;

    klass->get_stream = mate_mixer_device_real_get_stream;
    klass->get_switch = mate_mixer_device_real_get_switch;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = mate_mixer_device_finalize;
    object_class->get_property = mate_mixer_device_get_property;
    object_class->set_property = mate_mixer_device_set_property;

    /**
     * MateMixerDevice:name:
     *
     * The name of the device. The name serves as a unique identifier and
     * in most cases it is not in a user-readable form.
     */
    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "Name of the device",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    /**
     * MateMixerDevice:label:
     *
     * The label of the device. This is a potentially translated string
     * that should be presented to users in the user interface.
     */
    properties[PROP_LABEL] =
        g_param_spec_string ("label",
                             "Label",
                             "Label of the device",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    /**
     * MateMixerDevice:icon:
     *
     * The XDG icon name of the device.
     */
    properties[PROP_ICON] =
        g_param_spec_string ("icon",
                             "Icon",
                             "XDG icon name of the device",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    /**
     * MateMixerDevice::stream-added:
     * @device: a #MateMixerDevice
     * @name: name of the added stream
     *
     * The signal is emitted each time a device stream is added.
     *
     * Note that at the time this signal is emitted, the controls and switches
     * of the stream may not yet be known.
     */
    signals[STREAM_ADDED] =
        g_signal_new ("stream-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MateMixerDeviceClass, stream_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerDevice::stream-removed:
     * @device: a #MateMixerDevice
     * @name: name of the removed stream
     *
     * The signal is emitted each time a device stream is removed.
     *
     * When this signal is emitted, the stream is no longer known to the library,
     * it will not be included in the stream list provided by the
     * mate_mixer_device_list_streams() function and it is not possible to get
     * the stream with mate_mixer_device_get_stream().
     */
    signals[STREAM_REMOVED] =
        g_signal_new ("stream-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MateMixerDeviceClass, stream_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerDevice::switch-added:
     * @device: a #MateMixerDevice
     * @name: name of the added switch
     *
     * The signal is emitted each time a device switch is added.
     */
    signals[SWITCH_ADDED] =
        g_signal_new ("switch-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MateMixerDeviceClass, switch_added),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    /**
     * MateMixerDevice::switch-removed:
     * @device: a #MateMixerDevice
     * @name: name of the removed switch
     *
     * The signal is emitted each time a device switch is removed.
     *
     * When this signal is emitted, the switch is no longer known to the library,
     * it will not be included in the switch list provided by the
     * mate_mixer_device_list_switches() function and it is not possible to get
     * the switch with mate_mixer_device_get_switch().
     */
    signals[SWITCH_REMOVED] =
        g_signal_new ("switch-removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MateMixerDeviceClass, switch_removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_STRING);

    g_type_class_add_private (object_class, sizeof (MateMixerDevicePrivate));
}

static void
mate_mixer_device_get_property (GObject    *object,
                                guint       param_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
    MateMixerDevice *device;

    device = MATE_MIXER_DEVICE (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, device->priv->name);
        break;
    case PROP_LABEL:
        g_value_set_string (value, device->priv->label);
        break;
    case PROP_ICON:
        g_value_set_string (value, device->priv->icon);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_device_set_property (GObject      *object,
                                guint         param_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
    MateMixerDevice *device;

    device = MATE_MIXER_DEVICE (object);

    switch (param_id) {
    case PROP_NAME:
        /* Construct-only string */
        device->priv->name = g_value_dup_string (value);
        break;
    case PROP_LABEL:
        /* Construct-only string */
        device->priv->label = g_value_dup_string (value);
        break;
    case PROP_ICON:
        /* Construct-only string */
        device->priv->icon = g_value_dup_string (value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_device_init (MateMixerDevice *device)
{
    device->priv = G_TYPE_INSTANCE_GET_PRIVATE (device,
                                                MATE_MIXER_TYPE_DEVICE,
                                                MateMixerDevicePrivate);
}

static void
mate_mixer_device_finalize (GObject *object)
{
    MateMixerDevice *device;

    device = MATE_MIXER_DEVICE (object);

    g_free (device->priv->name);
    g_free (device->priv->label);
    g_free (device->priv->icon);

    G_OBJECT_CLASS (mate_mixer_device_parent_class)->finalize (object);
}

/**
 * mate_mixer_device_get_name:
 * @device: a #MateMixerDevice
 *
 * Gets the name of the device.
 *
 * The name serves as a unique identifier and in most cases it is not in a
 * user-readable form.
 *
 * The returned name is guaranteed to be unique across all the known devices
 * and may be used to get the #MateMixerDevice using
 * mate_mixer_context_get_device().
 *
 * Returns: the name of the device.
 */
const gchar *
mate_mixer_device_get_name (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

    return device->priv->name;
}

/**
 * mate_mixer_device_get_label:
 * @device: a #MateMixerDevice
 *
 * Gets the label of the device.
 *
 * This is a potentially translated string that should be presented to users
 * in the user interface.
 *
 * Returns: the label of the device.
 */
const gchar *
mate_mixer_device_get_label (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

    return device->priv->label;
}

/**
 * mate_mixer_device_get_icon:
 * @device: a #MateMixerDevice
 *
 * Gets the XDG icon name of the device.
 *
 * Returns: the icon name or %NULL.
 */
const gchar *
mate_mixer_device_get_icon (MateMixerDevice *device)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

    return device->priv->icon;
}

/**
 * mate_mixer_device_get_stream:
 * @device: a #MateMixerDevice
 * @name: a stream name
 *
 * Gets the device stream with the given name.
 *
 * Returns: a #MateMixerStream or %NULL if there is no such stream.
 */
MateMixerStream *
mate_mixer_device_get_stream (MateMixerDevice *device, const gchar *name)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return MATE_MIXER_DEVICE_GET_CLASS (device)->get_stream (device, name);
}

/**
 * mate_mixer_device_get_switch:
 * @device: a #MateMixerDevice
 * @name: a switch name
 *
 * Gets the device switch with the given name.
 *
 * Note that this function will only return a switch that belongs to the device
 * and not to a stream of the device. See mate_mixer_device_list_switches() for
 * information about the difference between device and stream switches.
 *
 * To get a stream switch, rather than a device switch, use
 * mate_mixer_stream_get_switch().
 *
 * Returns: a #MateMixerDeviceSwitch or %NULL if there is no such device switch.
 */
MateMixerDeviceSwitch *
mate_mixer_device_get_switch (MateMixerDevice *device, const gchar *name)
{
    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return MATE_MIXER_DEVICE_GET_CLASS (device)->get_switch (device, name);
}

/**
 * mate_mixer_device_list_streams:
 * @device: a #MateMixerDevice
 *
 * Gets the list of streams that belong to the device.
 *
 * The returned #GList is owned by the #MateMixerDevice and may be invalidated
 * at any time.
 *
 * Returns: a #GList of the device streams or %NULL if the device does not have
 * any streams.
 */
const GList *
mate_mixer_device_list_streams (MateMixerDevice *device)
{
    MateMixerDeviceClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

    klass = MATE_MIXER_DEVICE_GET_CLASS (device);

    if G_LIKELY (klass->list_streams != NULL)
        return klass->list_streams (device);

    return NULL;
}

/**
 * mate_mixer_device_list_switches:
 * @device: a #MateMixerDevice
 *
 * Gets the list of switches the belong to the device.
 *
 * Note that a switch may belong either to a device, or to a stream. Unlike
 * stream switches, device switches returned by this function are not classified
 * as input or output (as streams are), but they operate on the whole device.
 *
 * Use mate_mixer_stream_list_switches() to get a list of switches that belong
 * to a stream.
 *
 * The returned #GList is owned by the #MateMixerDevice and may be invalidated
 * at any time.
 *
 * Returns: a #GList of the device switches or %NULL if the device does not have
 * any switches.
 */
const GList *
mate_mixer_device_list_switches (MateMixerDevice *device)
{
    MateMixerDeviceClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

    klass = MATE_MIXER_DEVICE_GET_CLASS (device);

    if G_LIKELY (klass->list_switches != NULL)
        return klass->list_switches (device);

    return NULL;
}

static MateMixerStream *
mate_mixer_device_real_get_stream (MateMixerDevice *device, const gchar *name)
{
    const GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = mate_mixer_device_list_streams (device);
    while (list != NULL) {
        MateMixerStream *stream = MATE_MIXER_STREAM (list->data);

        if (strcmp (name, mate_mixer_stream_get_name (stream)) == 0)
            return stream;

        list = list->next;
    }
    return NULL;
}

static MateMixerDeviceSwitch *
mate_mixer_device_real_get_switch (MateMixerDevice *device, const gchar *name)
{
    const GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = mate_mixer_device_list_switches (device);
    while (list != NULL) {
        MateMixerSwitch *swtch = MATE_MIXER_SWITCH (list->data);

        if (strcmp (name, mate_mixer_switch_get_name (swtch)) == 0)
            return MATE_MIXER_DEVICE_SWITCH (swtch);

        list = list->next;
    }
    return NULL;
}
