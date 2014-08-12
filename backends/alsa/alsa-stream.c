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

#include "alsa-element.h"
#include "alsa-stream.h"
#include "alsa-stream-control.h"
#include "alsa-switch.h"

struct _AlsaStreamPrivate
{
    GHashTable             *switches;
    GHashTable             *controls;
    MateMixerStreamControl *control;
};

static void alsa_stream_class_init (AlsaStreamClass *klass);
static void alsa_stream_init       (AlsaStream      *stream);
static void alsa_stream_dispose    (GObject         *object);
static void alsa_stream_finalize   (GObject         *object);

G_DEFINE_TYPE (AlsaStream, alsa_stream, MATE_MIXER_TYPE_STREAM)

static MateMixerStreamControl *alsa_stream_get_control         (MateMixerStream *mms,
                                                                const gchar     *name);
static MateMixerStreamControl *alsa_stream_get_default_control (MateMixerStream *mms);

static MateMixerSwitch *       alsa_stream_get_switch          (MateMixerStream *mms,
                                                                const gchar     *name);

static GList *                 alsa_stream_list_controls       (MateMixerStream *mms);
static GList *                 alsa_stream_list_switches       (MateMixerStream *mms);

static void
alsa_stream_class_init (AlsaStreamClass *klass)
{
    GObjectClass         *object_class;
    MateMixerStreamClass *stream_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose  = alsa_stream_dispose;
    object_class->finalize = alsa_stream_finalize;

    stream_class = MATE_MIXER_STREAM_CLASS (klass);
    stream_class->get_control         = alsa_stream_get_control;
    stream_class->get_default_control = alsa_stream_get_default_control;
    stream_class->get_switch          = alsa_stream_get_switch;
    stream_class->list_controls       = alsa_stream_list_controls;
    stream_class->list_switches       = alsa_stream_list_switches;

    g_type_class_add_private (object_class, sizeof (AlsaStreamPrivate));
}

static void
alsa_stream_init (AlsaStream *stream)
{
    stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (stream,
                                                ALSA_TYPE_STREAM,
                                                AlsaStreamPrivate);

    stream->priv->controls = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    g_object_unref);

    stream->priv->switches = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    g_object_unref);
}

static void
alsa_stream_dispose (GObject *object)
{
    AlsaStream *stream;

    stream = ALSA_STREAM (object);

    g_hash_table_remove_all (stream->priv->controls);
    g_hash_table_remove_all (stream->priv->switches);

    g_clear_object (&stream->priv->control);

    G_OBJECT_CLASS (alsa_stream_parent_class)->dispose (object);
}

static void
alsa_stream_finalize (GObject *object)
{
    AlsaStream *stream;

    stream = ALSA_STREAM (object);

    g_hash_table_destroy (stream->priv->controls);
    g_hash_table_destroy (stream->priv->switches);

    G_OBJECT_CLASS (alsa_stream_parent_class)->finalize (object);
}

AlsaStream *
alsa_stream_new (const gchar         *name,
                 MateMixerDevice     *device,
                 MateMixerStreamFlags flags)
{
    return g_object_new (ALSA_TYPE_STREAM,
                         "name", name,
                         "device", device,
                         "flags", flags,
                         NULL);
}

void
alsa_stream_add_control (AlsaStream *stream, AlsaStreamControl *control)
{
    const gchar *name;

    g_return_if_fail (ALSA_IS_STREAM (stream));
    g_return_if_fail (ALSA_IS_STREAM_CONTROL (control));

    name = mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (control));
    g_hash_table_insert (stream->priv->controls,
                         g_strdup (name),
                         g_object_ref (control));
}

void
alsa_stream_add_switch (AlsaStream *stream, AlsaSwitch *swtch)
{
    const gchar *name;

    g_return_if_fail (ALSA_IS_STREAM (stream));
    g_return_if_fail (ALSA_IS_SWITCH (swtch));

    name = mate_mixer_switch_get_name (MATE_MIXER_SWITCH (swtch));
    g_hash_table_insert (stream->priv->switches,
                         g_strdup (name),
                         g_object_ref (swtch));
}

gboolean
alsa_stream_is_empty (AlsaStream *stream)
{
    g_return_val_if_fail (ALSA_IS_STREAM (stream), FALSE);

    if (g_hash_table_size (stream->priv->controls) > 0 ||
        g_hash_table_size (stream->priv->switches) > 0)
        return FALSE;

    return TRUE;
}

void
alsa_stream_set_default_control (AlsaStream *stream, AlsaStreamControl *control)
{
    g_return_if_fail (ALSA_IS_STREAM (stream));
    g_return_if_fail (ALSA_IS_STREAM_CONTROL (control));

    /* This function is only used internally so avoid validating that the control
     * belongs to this stream */
    if (stream->priv->control != NULL)
        g_object_unref (stream->priv->control);

    if (control != NULL)
        stream->priv->control = MATE_MIXER_STREAM_CONTROL (g_object_ref (control));
    else
        stream->priv->control = NULL;
}

void
alsa_stream_load_elements (AlsaStream *stream, const gchar *name)
{
    AlsaElement *element;

    g_return_if_fail (ALSA_IS_STREAM (stream));
    g_return_if_fail (name != NULL);

    element = g_hash_table_lookup (stream->priv->controls, name);
    if (element != NULL)
        alsa_element_load (element);

    element = g_hash_table_lookup (stream->priv->switches, name);
    if (element != NULL)
        alsa_element_load (element);
}

gboolean
alsa_stream_remove_elements (AlsaStream *stream, const gchar *name)
{
    gboolean removed = FALSE;

    g_return_val_if_fail (ALSA_IS_STREAM (stream), FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    if (g_hash_table_remove (stream->priv->controls, name) == TRUE)
        removed = TRUE;
    if (g_hash_table_remove (stream->priv->switches, name) == TRUE)
        removed = TRUE;

    return removed;
}

static MateMixerStreamControl *
alsa_stream_get_control (MateMixerStream *mms, const gchar *name)
{
    g_return_val_if_fail (ALSA_IS_STREAM (mms), NULL);

    return g_hash_table_lookup (ALSA_STREAM (mms)->priv->controls, name);
}

static MateMixerStreamControl *
alsa_stream_get_default_control (MateMixerStream *mms)
{
    g_return_val_if_fail (ALSA_IS_STREAM (mms), NULL);

    return ALSA_STREAM (mms)->priv->control;
}

static MateMixerSwitch *
alsa_stream_get_switch (MateMixerStream *mms, const gchar *name)
{
    g_return_val_if_fail (ALSA_IS_STREAM (mms), NULL);

    return g_hash_table_lookup (ALSA_STREAM (mms)->priv->switches, name);
}

static GList *
alsa_stream_list_controls (MateMixerStream *mms)
{
    GList *list;

    g_return_val_if_fail (ALSA_IS_STREAM (mms), NULL);

    /* Convert the hash table to a linked list, this list is expected to be
     * cached in the main library */
    list = g_hash_table_get_values (ALSA_STREAM (mms)->priv->controls);
    if (list != NULL)
        g_list_foreach (list, (GFunc) g_object_ref, NULL);

    return list;
}

static GList *
alsa_stream_list_switches (MateMixerStream *mms)
{
    GList *list;

    g_return_val_if_fail (ALSA_IS_STREAM (mms), NULL);

    /* Convert the hash table to a linked list, this list is expected to be
     * cached in the main library */
    list = g_hash_table_get_values (ALSA_STREAM (mms)->priv->switches);
    if (list != NULL)
        g_list_foreach (list, (GFunc) g_object_ref, NULL);

    return list;
}
