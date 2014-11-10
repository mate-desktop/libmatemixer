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

#include "matemixer-switch.h"
#include "matemixer-switch-option.h"
#include "matemixer-stream-toggle.h"

/**
 * SECTION:matemixer-stream-toggle
 * @short_description: On/Off switch
 * @include: libmatemixer/matemixer.h
 */

struct _MateMixerStreamTogglePrivate
{
    GList                 *options;
    MateMixerSwitchOption *on;
    MateMixerSwitchOption *off;
};

enum {
    PROP_0,
    PROP_STATE,
    PROP_ON_STATE_OPTION,
    PROP_OFF_STATE_OPTION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void mate_mixer_stream_toggle_class_init   (MateMixerStreamToggleClass *klass);

static void mate_mixer_stream_toggle_get_property (GObject                    *object,
                                                   guint                       param_id,
                                                   GValue                     *value,
                                                   GParamSpec                 *pspec);
static void mate_mixer_stream_toggle_set_property (GObject                    *object,
                                                   guint                       param_id,
                                                   const GValue               *value,
                                                   GParamSpec                 *pspec);

static void mate_mixer_stream_toggle_init         (MateMixerStreamToggle      *toggle);
static void mate_mixer_stream_toggle_dispose      (GObject                    *object);

G_DEFINE_ABSTRACT_TYPE (MateMixerStreamToggle, mate_mixer_stream_toggle, MATE_MIXER_TYPE_STREAM_SWITCH)

static MateMixerSwitchOption *mate_mixer_stream_toggle_get_option   (MateMixerSwitch *swtch,
                                                                     const gchar     *name);

static const GList *          mate_mixer_stream_toggle_list_options (MateMixerSwitch *swtch);

static void
mate_mixer_stream_toggle_class_init (MateMixerStreamToggleClass *klass)
{
    GObjectClass         *object_class;
    MateMixerSwitchClass *switch_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = mate_mixer_stream_toggle_dispose;
    object_class->get_property = mate_mixer_stream_toggle_get_property;
    object_class->set_property = mate_mixer_stream_toggle_set_property;

    switch_class = MATE_MIXER_SWITCH_CLASS (klass);
    switch_class->get_option   = mate_mixer_stream_toggle_get_option;
    switch_class->list_options = mate_mixer_stream_toggle_list_options;

    /**
     * MateMixerStreamToggle:state:
     *
     * The current state of the toggle. %TRUE corresponds to the 'on' state and
     * %FALSE to the 'off' state.
     */
    properties[PROP_STATE] =
        g_param_spec_boolean ("state",
                              "State",
                              "Current state of the toggle",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS);

    /**
     * MateMixerStreamToggle:on-state-option:
     *
     * The #MateMixerSwitchOption representing the 'on' value of the toggle.
     */
    properties[PROP_ON_STATE_OPTION] =
        g_param_spec_object ("on-state-option",
                             "On state option",
                             "On state option",
                             MATE_MIXER_TYPE_SWITCH_OPTION,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    /**
     * MateMixerStreamToggle:off-state-option:
     *
     * The #MateMixerSwitchOption representing the 'off' value of the toggle.
     */
    properties[PROP_OFF_STATE_OPTION] =
        g_param_spec_object ("off-state-option",
                             "Off state option",
                             "Off state option",
                             MATE_MIXER_TYPE_SWITCH_OPTION,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    g_type_class_add_private (object_class, sizeof (MateMixerStreamTogglePrivate));
}

static void
mate_mixer_stream_toggle_get_property (GObject    *object,
                                       guint       param_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    MateMixerStreamToggle *toggle;

    toggle = MATE_MIXER_STREAM_TOGGLE (object);

    switch (param_id) {
    case PROP_STATE:
        g_value_set_boolean (value, mate_mixer_stream_toggle_get_state (toggle));
        break;
    case PROP_ON_STATE_OPTION:
        g_value_set_object (value, toggle->priv->on);
        break;
    case PROP_OFF_STATE_OPTION:
        g_value_set_object (value, toggle->priv->off);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_stream_toggle_set_property (GObject      *object,
                                       guint         param_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    MateMixerStreamToggle *toggle;

    toggle = MATE_MIXER_STREAM_TOGGLE (object);

    switch (param_id) {
    case PROP_STATE:
        mate_mixer_stream_toggle_set_state (toggle, g_value_get_boolean (value));
        break;
    case PROP_ON_STATE_OPTION:
        /* Construct-only object */
        toggle->priv->on = g_value_dup_object (value);
        break;
    case PROP_OFF_STATE_OPTION:
        /* Construct-only object */
        toggle->priv->off = g_value_dup_object (value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
mate_mixer_stream_toggle_init (MateMixerStreamToggle *toggle)
{
    toggle->priv = G_TYPE_INSTANCE_GET_PRIVATE (toggle,
                                                MATE_MIXER_TYPE_STREAM_TOGGLE,
                                                MateMixerStreamTogglePrivate);
}

static void
mate_mixer_stream_toggle_dispose (GObject *object)
{
    MateMixerStreamToggle *toggle;

    toggle = MATE_MIXER_STREAM_TOGGLE (object);

    if (toggle->priv->options != NULL) {
        g_list_free (toggle->priv->options);
        toggle->priv->options = NULL;
    }

    g_clear_object (&toggle->priv->on);
    g_clear_object (&toggle->priv->off);

    G_OBJECT_CLASS (mate_mixer_stream_toggle_parent_class)->dispose (object);
}

/**
 * mate_mixer_stream_toggle_get_state:
 * @toggle: a #MateMixerStreamToggle
 *
 * Gets the current state of the toggle. %TRUE corresponds to the 'on' state and
 * %FALSE to the 'off' state.
 *
 * Returns: %TRUE or %FALSE.
 */
gboolean
mate_mixer_stream_toggle_get_state (MateMixerStreamToggle *toggle)
{
    MateMixerSwitchOption *active;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_TOGGLE (toggle), FALSE);

    active = mate_mixer_switch_get_active_option (MATE_MIXER_SWITCH (toggle));
    if G_UNLIKELY (active == NULL)
        return FALSE;

    if (active == toggle->priv->on)
        return TRUE;
    else
        return FALSE;
}

/**
 * mate_mixer_stream_toggle_get_state_option:
 * @toggle: a #MateMixerStreamToggle
 * @state: the state to get the #MateMixerSwitchOption for
 *
 * Gets the #MateMixerSwitchOption representing the selected state.
 *
 * Returns: a #MateMixerSwitchOption.
 */
MateMixerSwitchOption *
mate_mixer_stream_toggle_get_state_option (MateMixerStreamToggle *toggle, gboolean state)
{
    g_return_val_if_fail (MATE_MIXER_IS_STREAM_TOGGLE (toggle), NULL);

    if (state == TRUE)
        return toggle->priv->on;
    else
        return toggle->priv->off;
}

/**
 * mate_mixer_stream_toggle_set_state:
 * @toggle: a #MateMixerStreamToggle
 * @state: the state to set
 *
 * Sets the @toggle to the selected state.
 *
 * This function is equivalent to using mate_mixer_switch_set_active_option()
 * with a #MateMixerSwitchOption representing the selected state.
 *
 * Returns: %TRUE on success or %FALSE on failure.
 */
gboolean
mate_mixer_stream_toggle_set_state (MateMixerStreamToggle *toggle, gboolean state)
{
    MateMixerSwitchOption *active;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_TOGGLE (toggle), FALSE);

    if (state == TRUE)
        active = toggle->priv->on;
    else
        active = toggle->priv->off;

    if G_UNLIKELY (active == NULL)
        return FALSE;

    return mate_mixer_switch_set_active_option (MATE_MIXER_SWITCH (toggle), active);
}

static MateMixerSwitchOption *
mate_mixer_stream_toggle_get_option (MateMixerSwitch *swtch, const gchar *name)
{
    MateMixerStreamToggle *toggle;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_TOGGLE (swtch), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    toggle = MATE_MIXER_STREAM_TOGGLE (swtch);

    if (g_strcmp0 (name, mate_mixer_switch_option_get_name (toggle->priv->on)) == 0)
        return toggle->priv->on;
    if (g_strcmp0 (name, mate_mixer_switch_option_get_name (toggle->priv->off)) == 0)
        return toggle->priv->off;

    return NULL;
}

static const GList *
mate_mixer_stream_toggle_list_options (MateMixerSwitch *swtch)
{
    MateMixerStreamToggle *toggle;

    g_return_val_if_fail (MATE_MIXER_IS_STREAM_TOGGLE (swtch), NULL);

    toggle = MATE_MIXER_STREAM_TOGGLE (swtch);

    if (toggle->priv->options == NULL) {
        if G_LIKELY (toggle->priv->off != NULL)
            toggle->priv->options = g_list_prepend (toggle->priv->options,
                                                    toggle->priv->off);
        if G_LIKELY (toggle->priv->on != NULL)
            toggle->priv->options = g_list_prepend (toggle->priv->options,
                                                    toggle->priv->on);
    }
    return toggle->priv->options;
}
