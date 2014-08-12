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
#include "matemixer-enums.h"
#include "matemixer-enum-types.h"
#include "matemixer-stream.h"
#include "matemixer-stream-control.h"
#include "matemixer-switch.h"

/**
 * SECTION:matemixer-stream
 * @include: libmatemixer/matemixer.h
 */

struct _MateMixerStreamPrivate
{
    gchar                  *name;
    GList                  *controls;
    GList                  *switches;
    gboolean                monitor_enabled;
    MateMixerDevice        *device;
    MateMixerStreamFlags    flags;
    MateMixerStreamState    state;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_DEVICE,
    PROP_FLAGS,
    PROP_STATE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
    MONITOR_VALUE,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void mate_mixer_stream_class_init   (MateMixerStreamClass *klass);

static void mate_mixer_stream_get_property (GObject              *object,
                                            guint                 param_id,
                                            GValue               *value,
                                            GParamSpec           *pspec);
static void mate_mixer_stream_set_property (GObject              *object,
                                            guint                 param_id,
                                            const GValue         *value,
                                            GParamSpec           *pspec);

static void mate_mixer_stream_init         (MateMixerStream      *stream);
static void mate_mixer_stream_dispose      (GObject              *object);
static void mate_mixer_stream_finalize     (GObject              *object);

G_DEFINE_ABSTRACT_TYPE (MateMixerStream, mate_mixer_stream, G_TYPE_OBJECT)

static MateMixerStreamControl *mate_mixer_stream_real_get_control (MateMixerStream *stream,
                                                                   const gchar     *name);
static MateMixerSwitch *       mate_mixer_stream_real_get_switch  (MateMixerStream *stream,
                                                                   const gchar     *name);

static void
mate_mixer_stream_class_init (MateMixerStreamClass *klass)
{
    GObjectClass *object_class;

    klass->get_control = mate_mixer_stream_real_get_control;
    klass->get_switch  = mate_mixer_stream_real_get_switch;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = mate_mixer_stream_dispose;
    object_class->finalize     = mate_mixer_stream_finalize;
    object_class->get_property = mate_mixer_stream_get_property;
    object_class->set_property = mate_mixer_stream_set_property;

    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "Name of the stream",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_DEVICE] =
        g_param_spec_object ("device",
                             "Device",
                             "Device the stream belongs to",
                             MATE_MIXER_TYPE_DEVICE,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_FLAGS] =
        g_param_spec_flags ("flags",
                            "Flags",
                            "Capability flags of the stream",
                            MATE_MIXER_TYPE_STREAM_FLAGS,
                            MATE_MIXER_STREAM_NO_FLAGS,
                            G_PARAM_READWRITE |
                            G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_STATIC_STRINGS);

    properties[PROP_STATE] =
        g_param_spec_enum ("state",
                           "State",
                           "Current state of the stream",
                           MATE_MIXER_TYPE_STREAM_STATE,
                           MATE_MIXER_STREAM_STATE_UNKNOWN,
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    signals[MONITOR_VALUE] =
        g_signal_new ("monitor-value",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (MateMixerStreamClass, monitor_value),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__DOUBLE,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_DOUBLE);

    g_type_class_add_private (object_class, sizeof (MateMixerStreamPrivate));
}

static void
mate_mixer_stream_get_property (GObject    *object,
                                guint       param_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
    MateMixerStream *stream;

    stream = MATE_MIXER_STREAM (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, stream->priv->name);
        break;
    case PROP_DEVICE:
        g_value_set_object (value, stream->priv->device);
        break;
    case PROP_FLAGS:
        g_value_set_flags (value, stream->priv->flags);
        break;
    case PROP_STATE:
        g_value_set_enum (value, stream->priv->state);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_stream_set_property (GObject      *object,
                                guint         param_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
    MateMixerStream *stream;

    stream = MATE_MIXER_STREAM (object);

    switch (param_id) {
    case PROP_NAME:
        /* Construct-only string */
        stream->priv->name = g_value_dup_string (value);
        break;
    case PROP_DEVICE:
        /* Construct-only object */
        stream->priv->device = g_value_dup_object (value);
        break;
    case PROP_FLAGS:
        stream->priv->flags = g_value_get_flags (value);
        break;
    case PROP_STATE:
        stream->priv->state = g_value_get_enum (value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_stream_init (MateMixerStream *stream)
{
    stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (stream,
                                                MATE_MIXER_TYPE_STREAM,
                                                MateMixerStreamPrivate);
}

static void
mate_mixer_stream_dispose (GObject *object)
{
    MateMixerStream *stream;

    stream = MATE_MIXER_STREAM (object);

    g_clear_object (&stream->priv->device);

    G_OBJECT_CLASS (mate_mixer_stream_parent_class)->dispose (object);
}

static void
mate_mixer_stream_finalize (GObject *object)
{
    MateMixerStream *stream;

    stream = MATE_MIXER_STREAM (object);

    g_free (stream->priv->name);

    G_OBJECT_CLASS (mate_mixer_stream_parent_class)->finalize (object);
}

const gchar *
mate_mixer_stream_get_name (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    return stream->priv->name;
}

MateMixerDevice *
mate_mixer_stream_get_device (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    return stream->priv->device;
}

MateMixerStreamFlags
mate_mixer_stream_get_flags (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), MATE_MIXER_STREAM_NO_FLAGS);

    return stream->priv->flags;
}

MateMixerStreamState
mate_mixer_stream_get_state (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), MATE_MIXER_STREAM_STATE_UNKNOWN);

    return stream->priv->state;
}

MateMixerStreamControl *
mate_mixer_stream_get_control (MateMixerStream *stream, const gchar *name)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return MATE_MIXER_STREAM_GET_CLASS (stream)->get_control (stream, name);
}

MateMixerSwitch *
mate_mixer_stream_get_switch (MateMixerStream *stream, const gchar *name)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return MATE_MIXER_STREAM_GET_CLASS (stream)->get_switch (stream, name);
}

MateMixerStreamControl *
mate_mixer_stream_get_default_control (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    return MATE_MIXER_STREAM_GET_CLASS (stream)->get_default_control (stream);
}

const GList *
mate_mixer_stream_list_controls (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    if (stream->priv->controls == NULL)
        stream->priv->controls = MATE_MIXER_STREAM_GET_CLASS (stream)->list_controls (stream);

    return (const GList *) stream->priv->controls;
}

const GList *
mate_mixer_stream_list_switches (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

    if (stream->priv->switches == NULL) {
        MateMixerStreamClass *klass = MATE_MIXER_STREAM_GET_CLASS (stream);

        if (klass->list_switches != NULL)
            stream->priv->switches = klass->list_switches (stream);
    }

    return (const GList *) stream->priv->switches;
}

gboolean
mate_mixer_stream_suspend (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    if (stream->priv->state == MATE_MIXER_STREAM_STATE_SUSPENDED)
        return TRUE;

    if (stream->priv->flags & MATE_MIXER_STREAM_CAN_SUSPEND)
        return MATE_MIXER_STREAM_GET_CLASS (stream)->suspend (stream);

    return FALSE;
}

gboolean
mate_mixer_stream_resume (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    if (stream->priv->state != MATE_MIXER_STREAM_STATE_SUSPENDED)
        return TRUE;

    if (stream->priv->flags & MATE_MIXER_STREAM_CAN_SUSPEND)
        return MATE_MIXER_STREAM_GET_CLASS (stream)->resume (stream);

    return FALSE;
}

gboolean
mate_mixer_stream_monitor_get_enabled (MateMixerStream *stream)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    return stream->priv->monitor_enabled;
}

gboolean
mate_mixer_stream_monitor_set_enabled (MateMixerStream *stream, gboolean enabled)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), FALSE);

    if (stream->priv->monitor_enabled == enabled)
        return TRUE;

    if (stream->priv->flags & MATE_MIXER_STREAM_HAS_MONITOR) {
        if (enabled)
            return MATE_MIXER_STREAM_GET_CLASS (stream)->monitor_start (stream);
        else
            return MATE_MIXER_STREAM_GET_CLASS (stream)->monitor_stop (stream);
    }

    return FALSE;
}

static MateMixerStreamControl *
mate_mixer_stream_real_get_control (MateMixerStream *stream, const gchar *name)
{
    const GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = mate_mixer_stream_list_controls (stream);
    while (list != NULL) {
        MateMixerStreamControl *control = MATE_MIXER_STREAM_CONTROL (list->data);

        if (strcmp (name, mate_mixer_stream_control_get_name (control)) == 0)
            return control;

        list = list->next;
    }
    return NULL;
}

static MateMixerSwitch *
mate_mixer_stream_real_get_switch (MateMixerStream *stream, const gchar *name)
{
    const GList *list;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    list = mate_mixer_stream_list_switches (stream);
    while (list != NULL) {
        MateMixerSwitch *swtch = MATE_MIXER_SWITCH (list->data);

        if (strcmp (name, mate_mixer_switch_get_name (swtch)) == 0)
            return swtch;

        list = list->next;
    }
    return NULL;
}
