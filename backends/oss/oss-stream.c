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

#include <libmatemixer/matemixer-device.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-stream.h>

#include "oss-stream.h"
#include "oss-stream-control.h"

struct _OssStreamPrivate
{
    gchar                 *name;
    gchar                 *description;
    MateMixerDevice       *device;
    MateMixerStreamFlags   flags;
    MateMixerStreamState   state;
    GHashTable            *ports;
    GList                 *ports_list;
    GHashTable            *controls;
    GList                 *controls_list;
    OssStreamControl      *control;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_DEVICE,
    PROP_FLAGS,
    PROP_STATE,
    PROP_ACTIVE_PORT,
};

static void mate_mixer_stream_interface_init (MateMixerStreamInterface *iface);

static void oss_stream_class_init   (OssStreamClass *klass);

static void oss_stream_get_property (GObject        *object,
                                     guint           param_id,
                                     GValue         *value,
                                     GParamSpec     *pspec);

static void oss_stream_init         (OssStream      *ostream);
static void oss_stream_dispose      (GObject        *object);
static void oss_stream_finalize     (GObject        *object);

G_DEFINE_TYPE_WITH_CODE (OssStream, oss_stream, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_STREAM,
                                                mate_mixer_stream_interface_init))

static const gchar *           oss_stream_get_name            (MateMixerStream *stream);
static const gchar *           oss_stream_get_description     (MateMixerStream *stream);

static MateMixerStreamControl *oss_stream_get_control         (MateMixerStream *stream,
                                                               const gchar     *name);
static MateMixerStreamControl *oss_stream_get_default_control (MateMixerStream *stream);

static const GList *           oss_stream_list_controls       (MateMixerStream *stream);
static const GList *           oss_stream_list_ports          (MateMixerStream *stream);

static void
mate_mixer_stream_interface_init (MateMixerStreamInterface *iface)
{
    iface->get_name            = oss_stream_get_name;
    iface->get_description     = oss_stream_get_description;
    iface->get_control         = oss_stream_get_control;
    iface->get_default_control = oss_stream_get_default_control;
    iface->list_controls       = oss_stream_list_controls;
    iface->list_ports          = oss_stream_list_ports;
}

static void
oss_stream_class_init (OssStreamClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = oss_stream_dispose;
    object_class->finalize     = oss_stream_finalize;
    object_class->get_property = oss_stream_get_property;

    g_object_class_override_property (object_class, PROP_NAME, "name");
    g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (object_class, PROP_DEVICE, "device");
    g_object_class_override_property (object_class, PROP_FLAGS, "flags");
    g_object_class_override_property (object_class, PROP_STATE, "state");
    g_object_class_override_property (object_class, PROP_ACTIVE_PORT, "active-port");

    g_type_class_add_private (object_class, sizeof (OssStreamPrivate));
}

static void
oss_stream_get_property (GObject    *object,
                         guint       param_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    OssStream *ostream;

    ostream = OSS_STREAM (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, ostream->priv->name);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, ostream->priv->description);
        break;
    case PROP_DEVICE:
        g_value_set_object (value, ostream->priv->device);
        break;
    case PROP_FLAGS:
        g_value_set_flags (value, ostream->priv->flags);
        break;
    case PROP_STATE:
        /* Not supported */
        g_value_set_enum (value, MATE_MIXER_STREAM_STATE_UNKNOWN);
        break;
    case PROP_ACTIVE_PORT:
        // XXX
        g_value_set_object (value, NULL);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
oss_stream_init (OssStream *ostream)
{
    ostream->priv = G_TYPE_INSTANCE_GET_PRIVATE (ostream,
                                                 OSS_TYPE_STREAM,
                                                 OssStreamPrivate);

    ostream->priv->controls = g_hash_table_new_full (g_str_hash,
                                                     g_str_equal,
                                                     g_free,
                                                     g_object_unref);

    ostream->priv->ports = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  g_object_unref);
}

static void
oss_stream_dispose (GObject *object)
{
    OssStream *ostream;

    ostream = OSS_STREAM (object);

    g_hash_table_remove_all (ostream->priv->controls);
    g_hash_table_remove_all (ostream->priv->ports);

    G_OBJECT_CLASS (oss_stream_parent_class)->finalize (object);
}

static void
oss_stream_finalize (GObject *object)
{
    OssStream *ostream;

    ostream = OSS_STREAM (object);

    g_free (ostream->priv->name);
    g_free (ostream->priv->description);

    g_hash_table_destroy (ostream->priv->controls);
    g_hash_table_destroy (ostream->priv->ports);

    G_OBJECT_CLASS (oss_stream_parent_class)->finalize (object);
}

OssStream *
oss_stream_new (const gchar         *name,
                const gchar         *description,
                MateMixerStreamFlags flags)
{
    OssStream *stream;

    stream = g_object_new (OSS_TYPE_STREAM, NULL);

    stream->priv->name = g_strdup (name);
    stream->priv->description = g_strdup (description);
    stream->priv->flags = flags;

    return stream;
}

gboolean
oss_stream_add_control (OssStream *ostream, OssStreamControl *octl)
{
    const gchar *name;

    g_return_val_if_fail (OSS_IS_STREAM (ostream), FALSE);
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (octl), FALSE);

    name = mate_mixer_stream_control_get_name (MATE_MIXER_STREAM_CONTROL (octl));

    g_hash_table_insert (ostream->priv->controls,
                         g_strdup (name),
                         octl);
    return TRUE;
}

gboolean
oss_stream_set_default_control (OssStream *ostream, OssStreamControl *octl)
{
    g_return_val_if_fail (OSS_IS_STREAM (ostream), FALSE);
    g_return_val_if_fail (OSS_IS_STREAM_CONTROL (octl), FALSE);

    /* This function is only used internally so avoid validating that the control
     * belongs to this stream */
    if (ostream->priv->control != NULL)
        g_object_unref (ostream->priv->control);

    ostream->priv->control = g_object_ref (octl);
    return TRUE;
}

gboolean
oss_stream_add_port (OssStream *ostream, MateMixerPort *port)
{
    g_return_val_if_fail (OSS_IS_STREAM (ostream), FALSE);
    g_return_val_if_fail (MATE_MIXER_IS_PORT (port), FALSE);

    g_hash_table_insert (ostream->priv->ports,
                         g_strdup (mate_mixer_port_get_name (port)),
                         port);
    return TRUE;
}

static const gchar *
oss_stream_get_name (MateMixerStream *stream)
{
    g_return_val_if_fail (OSS_IS_STREAM (stream), NULL);

    return OSS_STREAM (stream)->priv->name;
}

static const gchar *
oss_stream_get_description (MateMixerStream *stream)
{
    g_return_val_if_fail (OSS_IS_STREAM (stream), NULL);

    return OSS_STREAM (stream)->priv->description;
}

static MateMixerStreamControl *
oss_stream_get_control (MateMixerStream *stream, const gchar *name)
{
    g_return_val_if_fail (OSS_IS_STREAM (stream), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_hash_table_lookup (OSS_STREAM (stream)->priv->controls, name);
}

static MateMixerStreamControl *
oss_stream_get_default_control (MateMixerStream *stream)
{
    g_return_val_if_fail (OSS_IS_STREAM (stream), NULL);

    return MATE_MIXER_STREAM_CONTROL (OSS_STREAM (stream)->priv->control);
}

static const GList *
oss_stream_list_controls (MateMixerStream *stream)
{
    OssStream *ostream;

    g_return_val_if_fail (OSS_IS_STREAM (stream), NULL);

    ostream = OSS_STREAM (stream);

    if (ostream->priv->controls_list == NULL)
        ostream->priv->controls_list = g_hash_table_get_values (ostream->priv->controls);

    return ostream->priv->controls_list;
}

static const GList *
oss_stream_list_ports (MateMixerStream *stream)
{
    OssStream *ostream;

    g_return_val_if_fail (OSS_IS_STREAM (stream), NULL);

    ostream = OSS_STREAM (stream);

    if (ostream->priv->ports_list == NULL)
        ostream->priv->ports_list = g_hash_table_get_values (ostream->priv->ports);

    return ostream->priv->ports_list;
}
