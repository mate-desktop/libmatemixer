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
#include "matemixer-stream-control-private.h"

/**
 * SECTION:matemixer-stream-control
 * @include: libmatemixer/matemixer.h
 */

struct _MateMixerStreamControlPrivate
{
    gchar                      *name;
    gchar                      *label;
    gboolean                    mute;
    gfloat                      balance;
    gfloat                      fade;
    MateMixerStreamControlFlags flags;
    MateMixerStreamControlRole  role;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_LABEL,
    PROP_FLAGS,
    PROP_ROLE,
    PROP_MUTE,
    PROP_VOLUME,
    PROP_BALANCE,
    PROP_FADE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void mate_mixer_stream_control_class_init   (MateMixerStreamControlClass *klass);

static void mate_mixer_stream_control_get_property (GObject                     *object,
                                                    guint                        param_id,
                                                    GValue                      *value,
                                                    GParamSpec                  *pspec);
static void mate_mixer_stream_control_set_property (GObject                     *object,
                                                    guint                        param_id,
                                                    const GValue                *value,
                                                    GParamSpec                  *pspec);

static void mate_mixer_stream_control_init         (MateMixerStreamControl      *control);
static void mate_mixer_stream_control_finalize     (GObject                     *object);

G_DEFINE_ABSTRACT_TYPE (MateMixerStreamControl, mate_mixer_stream_control, G_TYPE_OBJECT)

static void
mate_mixer_stream_control_class_init (MateMixerStreamControlClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize     = mate_mixer_stream_control_finalize;
    object_class->get_property = mate_mixer_stream_control_get_property;
    object_class->set_property = mate_mixer_stream_control_set_property;

    properties[PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "Name of the stream control",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_LABEL] =
        g_param_spec_string ("label",
                             "Label",
                             "Label of the stream control",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    properties[PROP_FLAGS] =
        g_param_spec_flags ("flags",
                            "Flags",
                            "Capability flags of the stream control",
                            MATE_MIXER_TYPE_STREAM_CONTROL_FLAGS,
                            MATE_MIXER_STREAM_CONTROL_NO_FLAGS,
                            G_PARAM_READWRITE |
                            G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_STATIC_STRINGS);

    properties[PROP_ROLE] =
        g_param_spec_enum ("role",
                            "Role",
                            "Role of the stream control",
                            MATE_MIXER_TYPE_STREAM_CONTROL_ROLE,
                            MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN,
                            G_PARAM_READWRITE |
                            G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_STATIC_STRINGS);

    properties[PROP_MUTE] =
        g_param_spec_boolean ("mute",
                              "Mute",
                              "Mute state of the stream control",
                              FALSE,
                              G_PARAM_READABLE |
                              G_PARAM_STATIC_STRINGS);

    properties[PROP_VOLUME] =
        g_param_spec_uint ("volume",
                           "Volume",
                           "Volume of the stream control",
                           0,
                           G_MAXUINT,
                           0,
                           G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS);

    properties[PROP_BALANCE] =
        g_param_spec_float ("balance",
                            "Balance",
                            "Balance value of the stream control",
                            -1.0f,
                            1.0f,
                            0.0f,
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_STRINGS);

    properties[PROP_FADE] =
        g_param_spec_float ("fade",
                            "Fade",
                            "Fade value of the stream control",
                            -1.0f,
                            1.0f,
                            0.0f,
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    g_type_class_add_private (object_class, sizeof (MateMixerStreamControlPrivate));
}

static void
mate_mixer_stream_control_get_property (GObject    *object,
                                        guint       param_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
    MateMixerStreamControl *control;

    control = MATE_MIXER_STREAM_CONTROL (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, control->priv->name);
        break;
    case PROP_LABEL:
        g_value_set_string (value, control->priv->label);
        break;
    case PROP_FLAGS:
        g_value_set_flags (value, control->priv->flags);
        break;
    case PROP_ROLE:
        g_value_set_enum (value, control->priv->role);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_stream_control_set_property (GObject      *object,
                                        guint         param_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
    MateMixerStreamControl *control;

    control = MATE_MIXER_STREAM_CONTROL (object);

    switch (param_id) {
    case PROP_NAME:
        /* Construct-only string */
        control->priv->name = g_value_dup_string (value);
        break;
    case PROP_LABEL:
        /* Construct-only string */
        control->priv->label = g_value_dup_string (value);
        break;
    case PROP_FLAGS:
        control->priv->flags = g_value_get_flags (value);
        break;
    case PROP_ROLE:
        control->priv->role = g_value_get_enum (value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_stream_control_init (MateMixerStreamControl *control)
{
    control->priv = G_TYPE_INSTANCE_GET_PRIVATE (control,
                                                 MATE_MIXER_TYPE_STREAM_CONTROL,
                                                 MateMixerStreamControlPrivate);
}

static void
mate_mixer_stream_control_finalize (GObject *object)
{
    MateMixerStreamControl *control;

    control = MATE_MIXER_STREAM_CONTROL (object);

    g_free (control->priv->name);
    g_free (control->priv->label);

    G_OBJECT_CLASS (mate_mixer_stream_control_parent_class)->finalize (object);
}

const gchar *
mate_mixer_stream_control_get_name (MateMixerStreamControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), NULL);

    return control->priv->name;
}

const gchar *
mate_mixer_stream_control_get_label (MateMixerStreamControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), NULL);

    return control->priv->label;
}

MateMixerStreamControlFlags
mate_mixer_stream_control_get_flags (MateMixerStreamControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), MATE_MIXER_STREAM_CONTROL_NO_FLAGS);

    return control->priv->flags;
}

MateMixerStreamControlRole
mate_mixer_stream_control_get_role (MateMixerStreamControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), MATE_MIXER_STREAM_CONTROL_ROLE_UNKNOWN);

    return control->priv->role;
}

gboolean
mate_mixer_stream_control_get_mute (MateMixerStreamControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), FALSE);

    return control->priv->mute;
}

gboolean
mate_mixer_stream_control_set_mute (MateMixerStreamControl *control, gboolean mute)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), FALSE);

    if (control->priv->mute == mute)
        return TRUE;

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_HAS_MUTE) {
        MateMixerStreamControlClass *klass;

        klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

        if (G_LIKELY (klass->set_mute != NULL))
            return klass->set_mute (control, mute);
    }
    return FALSE;
}

guint
mate_mixer_stream_control_get_num_channels (MateMixerStreamControl *control)
{
    MateMixerStreamControlClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), 0);

    klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

    if (klass->get_num_channels != NULL)
        return klass->get_num_channels (control);

    return 0;
}

guint
mate_mixer_stream_control_get_volume (MateMixerStreamControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), 0);

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_HAS_VOLUME) {
        MateMixerStreamControlClass *klass;

        klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

        if G_LIKELY (klass->get_volume != NULL)
            return klass->get_volume (control);
    }
    return 0;
}

gboolean
mate_mixer_stream_control_set_volume (MateMixerStreamControl *control, guint volume)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), FALSE);

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_HAS_VOLUME) {
        MateMixerStreamControlClass *klass;

        klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

        if (G_LIKELY (klass->set_volume != NULL))
            return klass->set_volume (control, volume);
    }
    return FALSE;
}

gdouble
mate_mixer_stream_control_get_decibel (MateMixerStreamControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), -MATE_MIXER_INFINITY);

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL) {
        MateMixerStreamControlClass *klass;

        klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

        if (G_LIKELY (klass->get_decibel != NULL))
            return klass->get_decibel (control);
    }
    return -MATE_MIXER_INFINITY;
}

gboolean
mate_mixer_stream_control_set_decibel (MateMixerStreamControl *control, gdouble decibel)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), FALSE);

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL) {
        MateMixerStreamControlClass *klass;

        klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

        if (G_LIKELY (klass->set_decibel != NULL))
            return klass->set_decibel (control, decibel);
    }
    return FALSE;
}

MateMixerChannelPosition
mate_mixer_stream_control_get_channel_position (MateMixerStreamControl *control, guint channel)
{
    MateMixerStreamControlClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), MATE_MIXER_CHANNEL_UNKNOWN);

    klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

    if (klass->get_channel_position != NULL)
        return klass->get_channel_position (control, channel);

    return MATE_MIXER_CHANNEL_UNKNOWN;
}

guint
mate_mixer_stream_control_get_channel_volume (MateMixerStreamControl *control, guint channel)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), 0);

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_HAS_VOLUME) {
        MateMixerStreamControlClass *klass;

        klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

        if (G_LIKELY (klass->get_channel_volume != NULL))
            return klass->get_channel_volume (control, channel);
    }
    return 0;
}

gboolean
mate_mixer_stream_control_set_channel_volume (MateMixerStreamControl *control,
                                              guint                   channel,
                                              guint                   volume)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), FALSE);

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_HAS_VOLUME) {
        MateMixerStreamControlClass *klass;

        klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

        if (G_LIKELY (klass->set_channel_volume != NULL))
            return klass->set_channel_volume (control, channel, volume);
    }
    return FALSE;
}

gdouble
mate_mixer_stream_control_get_channel_decibel (MateMixerStreamControl *control, guint channel)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), -MATE_MIXER_INFINITY);

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL) {
        MateMixerStreamControlClass *klass;

        klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

        if (G_LIKELY (klass->get_channel_decibel != NULL))
            return klass->get_channel_decibel (control, channel);
    }
    return -MATE_MIXER_INFINITY;
}

gboolean
mate_mixer_stream_control_set_channel_decibel (MateMixerStreamControl *control,
                                               guint                   channel,
                                               gdouble                 decibel)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), FALSE);

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL) {
        MateMixerStreamControlClass *klass;

        klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

        if (G_LIKELY (klass->set_channel_decibel != NULL))
            return klass->set_channel_decibel (control, channel, decibel);
    }
    return FALSE;
}

gboolean
mate_mixer_stream_control_has_channel_position (MateMixerStreamControl   *control,
                                                MateMixerChannelPosition  position)
{
    MateMixerStreamControlClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), FALSE);

    klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

    if (klass->has_channel_position != NULL)
        return klass->has_channel_position (control, position);

    return FALSE;
}

gfloat
mate_mixer_stream_control_get_balance (MateMixerStreamControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), 0.0f);

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_CAN_BALANCE)
        return control->priv->balance;
    else
        return 0.0f;
}

gboolean
mate_mixer_stream_control_set_balance (MateMixerStreamControl *control, gfloat balance)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), FALSE);

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_CAN_BALANCE) {
        MateMixerStreamControlClass *klass;

        if (balance < -1.0f || balance > 1.0f)
            return FALSE;

        klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

        if (G_LIKELY (klass->set_balance != NULL))
            return klass->set_balance (control, balance);
    }
    return FALSE;
}

gfloat
mate_mixer_stream_control_get_fade (MateMixerStreamControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), 0.0f);

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_CAN_FADE)
        return control->priv->fade;
    else
        return 0.0f;
}

gboolean
mate_mixer_stream_control_set_fade (MateMixerStreamControl *control, gfloat fade)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), FALSE);

    if (control->priv->flags & MATE_MIXER_STREAM_CONTROL_CAN_FADE) {
        MateMixerStreamControlClass *klass;

        if (fade < -1.0f || fade > 1.0f)
            return FALSE;

        klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

        if (G_LIKELY (klass->set_fade != NULL))
            return klass->set_fade (control, fade);
    }
    return FALSE;
}

guint
mate_mixer_stream_control_get_min_volume (MateMixerStreamControl *control)
{
    MateMixerStreamControlClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), 0);

    klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

    if (klass->get_min_volume != NULL)
        return klass->get_min_volume (control);

    return 0;
}

guint
mate_mixer_stream_control_get_max_volume (MateMixerStreamControl *control)
{
    MateMixerStreamControlClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), 0);

    klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

    if (klass->get_max_volume != NULL)
        return klass->get_max_volume (control);

    return 0;
}

guint
mate_mixer_stream_control_get_normal_volume (MateMixerStreamControl *control)
{
    MateMixerStreamControlClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), 0);

    klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

    if (klass->get_normal_volume != NULL)
        return klass->get_normal_volume (control);

    return 0;
}

guint
mate_mixer_stream_control_get_base_volume (MateMixerStreamControl *control)
{
    MateMixerStreamControlClass *klass;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), 0);

    klass = MATE_MIXER_STREAM_CONTROL_GET_CLASS (control);

    if (klass->get_base_volume != NULL)
        return klass->get_base_volume (control);

    return 0;
}

void
_mate_mixer_stream_control_set_flags (MateMixerStreamControl     *control,
                                      MateMixerStreamControlFlags flags)
{
    control->priv->flags = flags;
}

void
_mate_mixer_stream_control_set_mute (MateMixerStreamControl *control, gboolean mute)
{
    control->priv->mute = mute;
}

void
_mate_mixer_stream_control_set_balance (MateMixerStreamControl *control, gfloat balance)
{
    control->priv->balance = balance;
}

void
_mate_mixer_stream_control_set_fade (MateMixerStreamControl *control, gfloat fade)
{
    control->priv->fade = fade;
}
