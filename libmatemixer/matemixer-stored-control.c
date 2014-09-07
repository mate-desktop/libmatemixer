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

#include "matemixer-enums.h"
#include "matemixer-enum-types.h"
#include "matemixer-stream-control.h"
#include "matemixer-stored-control.h"

struct _MateMixerStoredControlPrivate
{
    MateMixerDirection direction;
};

enum {
    PROP_0,
    PROP_DIRECTION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void mate_mixer_stored_control_class_init   (MateMixerStoredControlClass *klass);

static void mate_mixer_stored_control_get_property (GObject                     *object,
                                                    guint                        param_id,
                                                    GValue                      *value,
                                                    GParamSpec                  *pspec);
static void mate_mixer_stored_control_set_property (GObject                     *object,
                                                    guint                        param_id,
                                                    const GValue                *value,
                                                    GParamSpec                  *pspec);

static void mate_mixer_stored_control_init         (MateMixerStoredControl      *control);

G_DEFINE_ABSTRACT_TYPE (MateMixerStoredControl, mate_mixer_stored_control, MATE_MIXER_TYPE_STREAM_CONTROL)

static void
mate_mixer_stored_control_class_init (MateMixerStoredControlClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->get_property = mate_mixer_stored_control_get_property;
    object_class->set_property = mate_mixer_stored_control_set_property;

    properties[PROP_DIRECTION] =
        g_param_spec_enum ("direction",
                           "Direction",
                           "Direction of the stored control",
                           MATE_MIXER_TYPE_DIRECTION,
                           MATE_MIXER_DIRECTION_UNKNOWN,
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    g_type_class_add_private (object_class, sizeof (MateMixerStoredControlPrivate));
}

static void
mate_mixer_stored_control_get_property (GObject    *object,
                                        guint       param_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
    MateMixerStoredControl *control;

    control = MATE_MIXER_STORED_CONTROL (object);

    switch (param_id) {
    case PROP_DIRECTION:
        g_value_set_enum (value, control->priv->direction);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_stored_control_set_property (GObject      *object,
                                        guint         param_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
    MateMixerStoredControl *control;

    control = MATE_MIXER_STORED_CONTROL (object);

    switch (param_id) {
    case PROP_DIRECTION:
        control->priv->direction = g_value_get_enum (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_stored_control_init (MateMixerStoredControl *control)
{
    control->priv = G_TYPE_INSTANCE_GET_PRIVATE (control,
                                                 MATE_MIXER_TYPE_STORED_CONTROL,
                                                 MateMixerStoredControlPrivate);
}

/**
 * mate_mixer_stored_control_get_direction:
 * @control: a #MateMixerStoredControl
 */
MateMixerDirection
mate_mixer_stored_control_get_direction (MateMixerStoredControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_STORED_CONTROL (control), MATE_MIXER_DIRECTION_UNKNOWN);

    return control->priv->direction;
}
