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
#include <glib/gi18n.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include "oss-device.h"
#include "oss-stream.h"
#include "oss-stream-control.h"
#include "oss-switch.h"

#define OSS_STREAM_SWITCH_NAME "port"

struct _OssStreamPrivate
{
    OssSwitch *swtch;
    GList     *switches;
    GList     *controls;
};

static void oss_stream_class_init (OssStreamClass *klass);
static void oss_stream_init       (OssStream      *stream);
static void oss_stream_dispose    (GObject        *object);

G_DEFINE_TYPE (OssStream, oss_stream, MATE_MIXER_TYPE_STREAM)

static const GList *oss_stream_list_controls (MateMixerStream *mms);
static const GList *oss_stream_list_switches (MateMixerStream *mms);

static void
oss_stream_class_init (OssStreamClass *klass)
{
    GObjectClass         *object_class;
    MateMixerStreamClass *stream_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = oss_stream_dispose;

    stream_class = MATE_MIXER_STREAM_CLASS (klass);
    stream_class->list_controls = oss_stream_list_controls;
    stream_class->list_switches = oss_stream_list_switches;

    g_type_class_add_private (object_class, sizeof (OssStreamPrivate));
}

static void
oss_stream_init (OssStream *stream)
{
    stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (stream,
                                                OSS_TYPE_STREAM,
                                                OssStreamPrivate);
}

static void
oss_stream_dispose (GObject *object)
{
    OssStream *stream;

    stream = OSS_STREAM (object);

    if (stream->priv->controls != NULL) {
        g_list_free_full (stream->priv->controls, g_object_unref);
        stream->priv->controls = NULL;
    }
    if (stream->priv->switches != NULL) {
        g_list_free_full (stream->priv->switches, g_object_unref);
        stream->priv->switches = NULL;
    }

    g_clear_object (&stream->priv->swtch);

    G_OBJECT_CLASS (oss_stream_parent_class)->dispose (object);
}

OssStream *
oss_stream_new (const gchar       *name,
                MateMixerDevice   *device,
                MateMixerDirection direction)
{
    const gchar *label;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (OSS_IS_DEVICE (device), NULL);

    label = mate_mixer_device_get_label (device);

    return g_object_new (OSS_TYPE_STREAM,
                         "name", name,
                         "label", label,
                         "device", device,
                         "direction", direction,
                         NULL);
}

void
oss_stream_add_control (OssStream *stream, OssStreamControl *control)
{
    const gchar *name;

    g_return_if_fail (OSS_IS_STREAM (stream));
    g_return_if_fail (OSS_IS_STREAM_CONTROL (control));

    name = mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (control));

    stream->priv->controls =
        g_list_append (stream->priv->controls, g_object_ref (control));

    g_signal_emit_by_name (G_OBJECT (stream),
                           "control-added",
                           name);
}

void
oss_stream_load (OssStream *stream)
{
    GList *list;

    g_return_if_fail (OSS_IS_STREAM (stream));

    list = stream->priv->controls;
    while (list != NULL) {
        OssStreamControl *control = OSS_STREAM_CONTROL (list->data);

        oss_stream_control_load (control);
        list = list->next;
    }

    if (stream->priv->swtch != NULL)
        oss_switch_load (stream->priv->swtch);
}

gboolean
oss_stream_has_controls (OssStream *stream)
{
    g_return_val_if_fail (OSS_IS_STREAM (stream), FALSE);

    if (stream->priv->controls != NULL)
        return TRUE;

    return FALSE;
}

gboolean
oss_stream_has_default_control (OssStream *stream)
{
    g_return_val_if_fail (OSS_IS_STREAM (stream), FALSE);

    if (mate_mixer_stream_get_default_control (MATE_MIXER_STREAM (stream)) != NULL)
        return TRUE;

    return FALSE;
}

OssStreamControl *
oss_stream_get_default_control (OssStream *stream)
{
    MateMixerStreamControl *control;

    g_return_val_if_fail (OSS_IS_STREAM (stream), NULL);

    control = mate_mixer_stream_get_default_control (MATE_MIXER_STREAM (stream));
    if (control != NULL)
        return OSS_STREAM_CONTROL (control);

    return NULL;
}

void
oss_stream_set_default_control (OssStream *stream, OssStreamControl *control)
{
    g_return_if_fail (OSS_IS_STREAM (stream));
    g_return_if_fail (control == NULL || OSS_IS_STREAM_CONTROL (control));

    if (control == NULL)
        _mate_mixer_stream_set_default_control (MATE_MIXER_STREAM (stream), NULL);
    else
        _mate_mixer_stream_set_default_control (MATE_MIXER_STREAM (stream),
                                                MATE_MIXER_STREAM_CONTROL (control));
}

void
oss_stream_set_switch_data (OssStream *stream, gint fd, GList *options)
{
    g_return_if_fail (OSS_IS_STREAM (stream));
    g_return_if_fail (fd != -1);
    g_return_if_fail (options != NULL);

    /* Function may only be called once */
    if G_UNLIKELY (stream->priv->swtch != NULL) {
        g_warn_if_reached ();
        return;
    }

    /* Takes ownership of options */
    stream->priv->swtch = oss_switch_new (stream,
                                          OSS_STREAM_SWITCH_NAME,
                                          _("Connector"),
                                          fd,
                                          options);
    if G_UNLIKELY (stream->priv->swtch == NULL)
        return;

    /* Read the active selection */
    oss_switch_load (stream->priv->swtch);

    stream->priv->switches = g_list_prepend (NULL, g_object_ref (stream->priv->swtch));
    g_signal_emit_by_name (G_OBJECT (stream),
                           "switch-added",
                           OSS_STREAM_SWITCH_NAME);
}

void
oss_stream_remove_all (OssStream *stream)
{
    GList *list;

    g_return_if_fail (OSS_IS_STREAM (stream));

    list = stream->priv->controls;
    while (list != NULL) {
        MateMixerStreamControl *control = MATE_MIXER_STREAM_CONTROL (list->data);
        GList *next = list->next;

        oss_stream_control_close (OSS_STREAM_CONTROL (control));

        stream->priv->controls = g_list_delete_link (stream->priv->controls, list);
        g_signal_emit_by_name (G_OBJECT (stream),
                               "control-removed",
                               mate_mixer_stream_control_get_name (control));

        g_object_unref (control);
        list = next;
    }

    /* Unset the default stream control */
    oss_stream_set_default_control (stream, NULL);

    if (stream->priv->swtch != NULL) {
        oss_switch_close (stream->priv->swtch);

        g_list_free_full (stream->priv->switches, g_object_unref);
        stream->priv->switches = NULL;

        g_signal_emit_by_name (G_OBJECT (stream),
                               "switch-removed",
                               OSS_STREAM_SWITCH_NAME);

        g_clear_object (&stream->priv->swtch);
    }
}

static const GList *
oss_stream_list_controls (MateMixerStream *mms)
{
    g_return_val_if_fail (OSS_IS_STREAM (mms), NULL);

    return OSS_STREAM (mms)->priv->controls;
}

static const GList *
oss_stream_list_switches (MateMixerStream *mms)
{
    g_return_val_if_fail (OSS_IS_STREAM (mms), NULL);

    return OSS_STREAM (mms)->priv->switches;
}
