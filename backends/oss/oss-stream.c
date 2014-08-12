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
#include <libmatemixer/matemixer.h>

#include "oss-device.h"
#include "oss-stream.h"
#include "oss-stream-control.h"

struct _OssStreamPrivate
{
    GHashTable       *controls;
    OssStreamControl *control;
};

static void oss_stream_class_init   (OssStreamClass *klass);

static void oss_stream_init         (OssStream      *ostream);
static void oss_stream_dispose      (GObject        *object);
static void oss_stream_finalize     (GObject        *object);

G_DEFINE_TYPE (OssStream, oss_stream, MATE_MIXER_TYPE_STREAM)

static MateMixerStreamControl *oss_stream_get_control         (MateMixerStream *stream,
                                                               const gchar     *name);
static MateMixerStreamControl *oss_stream_get_default_control (MateMixerStream *stream);

static GList *                 oss_stream_list_controls       (MateMixerStream *stream);

static void
oss_stream_class_init (OssStreamClass *klass)
{
    GObjectClass         *object_class;
    MateMixerStreamClass *stream_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose  = oss_stream_dispose;
    object_class->finalize = oss_stream_finalize;

    stream_class = MATE_MIXER_STREAM_CLASS (klass);
    stream_class->get_control         = oss_stream_get_control;
    stream_class->get_default_control = oss_stream_get_default_control;
    stream_class->list_controls       = oss_stream_list_controls;

    g_type_class_add_private (object_class, sizeof (OssStreamPrivate));
}

static void
oss_stream_init (OssStream *stream)
{
    stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (stream,
                                                OSS_TYPE_STREAM,
                                                OssStreamPrivate);

    stream->priv->controls = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    g_object_unref);
}

static void
oss_stream_dispose (GObject *object)
{
    OssStream *stream;

    stream = OSS_STREAM (object);

    g_clear_object (&stream->priv->control);
    g_hash_table_remove_all (stream->priv->controls);

    G_OBJECT_CLASS (oss_stream_parent_class)->dispose (object);
}

static void
oss_stream_finalize (GObject *object)
{
    OssStream *stream;

    stream = OSS_STREAM (object);

    g_hash_table_destroy (stream->priv->controls);

    G_OBJECT_CLASS (oss_stream_parent_class)->finalize (object);
}

OssStream *
oss_stream_new (const gchar         *name,
                MateMixerDevice     *device,
                MateMixerStreamFlags flags)
{
    OssStream *stream;

    stream = g_object_new (OSS_TYPE_STREAM,
                           "name", name,
                           "device", device,
                           "flags", flags,
                           NULL);
    return stream;
}

gboolean
oss_stream_add_control (OssStream *stream, OssStreamControl *control)
{
    const gchar *name;

    g_return_val_if_fail (OSS_IS_STREAM (stream), FALSE);
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (control), FALSE);

    name = mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (control));

    g_hash_table_insert (stream->priv->controls,
                         g_strdup (name),
                         control);
    return TRUE;
}

gboolean
oss_stream_set_default_control (OssStream *stream, OssStreamControl *control)
{
    g_return_val_if_fail (OSS_IS_STREAM (stream), FALSE);
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (control), FALSE);

    /* This function is only used internally so avoid validating that the control
     * belongs to this stream */
    if (stream->priv->control != NULL)
        g_object_unref (stream->priv->control);

    if G_LIKELY (control != NULL)
        stream->priv->control = g_object_ref (control);
    else
        stream->priv->control = NULL;

    return TRUE;
}

static MateMixerStreamControl *
oss_stream_get_control (MateMixerStream *mms, const gchar *name)
{
    g_return_val_if_fail (OSS_IS_STREAM (mms), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_hash_table_lookup (OSS_STREAM (mms)->priv->controls, name);
}

static MateMixerStreamControl *
oss_stream_get_default_control (MateMixerStream *mms)
{
    g_return_val_if_fail (OSS_IS_STREAM (mms), NULL);

    return MATE_MIXER_STREAM_CONTROL (OSS_STREAM (mms)->priv->control);
}

static GList *
oss_stream_list_controls (MateMixerStream *mms)
{
    GList *list;

    g_return_val_if_fail (OSS_IS_STREAM (mms), NULL);

    /* Convert the hash table to a linked list, this list is expected to be
     * cached in the main library */
    list = g_hash_table_get_values (OSS_STREAM (mms)->priv->controls);
    if (list != NULL)
        g_list_foreach (list, (GFunc) g_object_ref, NULL);

    return list;
}
