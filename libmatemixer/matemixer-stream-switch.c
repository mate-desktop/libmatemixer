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

#include "matemixer-stream.h"
#include "matemixer-switch.h"
#include "matemixer-stream-switch.h"

/**
 * SECTION:matemixer-stream-switch
 * @include: libmatemixer/matemixer.h
 */

struct _MateMixerStreamSwitchPrivate
{
    MateMixerStream *stream;
};

enum {
    PROP_0,
    PROP_STREAM,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void mate_mixer_stream_switch_class_init   (MateMixerStreamSwitchClass *klass);

static void mate_mixer_stream_switch_get_property (GObject                    *object,
                                                   guint                       param_id,
                                                   GValue                     *value,
                                                   GParamSpec                 *pspec);
static void mate_mixer_stream_switch_set_property (GObject                    *object,
                                                   guint                       param_id,
                                                   const GValue               *value,
                                                   GParamSpec                 *pspec);

static void mate_mixer_stream_switch_init         (MateMixerStreamSwitch      *swtch);

G_DEFINE_ABSTRACT_TYPE (MateMixerStreamSwitch, mate_mixer_stream_switch, MATE_MIXER_TYPE_SWITCH)

static void
mate_mixer_stream_switch_class_init (MateMixerStreamSwitchClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->get_property = mate_mixer_stream_switch_get_property;
    object_class->set_property = mate_mixer_stream_switch_set_property;

    properties[PROP_STREAM] =
        g_param_spec_object ("stream",
                             "Stream",
                             "Stream owning the switch",
                             MATE_MIXER_TYPE_STREAM,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    g_type_class_add_private (object_class, sizeof (MateMixerStreamSwitchPrivate));
}

static void
mate_mixer_stream_switch_get_property (GObject    *object,
                                       guint       param_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    MateMixerStreamSwitch *swtch;

    swtch = MATE_MIXER_STREAM_SWITCH (object);

    switch (param_id) {
    case PROP_STREAM:
        g_value_set_object (value, swtch->priv->stream);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_stream_switch_set_property (GObject      *object,
                                       guint         param_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    MateMixerStreamSwitch *swtch;

    swtch = MATE_MIXER_STREAM_SWITCH (object);

    switch (param_id) {
    case PROP_STREAM:
        /* Construct-only object */
        swtch->priv->stream = g_value_get_object (value);

        if (swtch->priv->stream != NULL)
            g_object_add_weak_pointer (G_OBJECT (swtch->priv->stream),
                                       (gpointer *) &swtch->priv->stream);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_stream_switch_init (MateMixerStreamSwitch *swtch)
{
    swtch->priv = G_TYPE_INSTANCE_GET_PRIVATE (swtch,
                                               MATE_MIXER_TYPE_STREAM_SWITCH,
                                               MateMixerStreamSwitchPrivate);
}

/**
 * mate_mixer_stream_switch_get_stream:
 * @swtch: a #MateMixerStreamSwitch
 */
MateMixerStream *
mate_mixer_stream_switch_get_stream (MateMixerStreamSwitch *swtch)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_SWITCH (swtch), NULL);

    return swtch->priv->stream;
}
