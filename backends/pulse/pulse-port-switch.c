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

#include <libmatemixer/matemixer.h>
#include <libmatemixer/matemixer-private.h>

#include "pulse-connection.h"
#include "pulse-port.h"
#include "pulse-port-switch.h"
#include "pulse-stream.h"

struct _PulsePortSwitchPrivate
{
    GList *ports;
};

static void pulse_port_switch_class_init   (PulsePortSwitchClass *klass);
static void pulse_port_switch_init         (PulsePortSwitch         *swtch);
static void pulse_port_switch_dispose      (GObject                 *object);

G_DEFINE_ABSTRACT_TYPE (PulsePortSwitch, pulse_port_switch, MATE_MIXER_TYPE_STREAM_SWITCH)

static gboolean     pulse_port_switch_set_active_option (MateMixerSwitch       *mms,
                                                         MateMixerSwitchOption *mmso);

static const GList *pulse_port_switch_list_options      (MateMixerSwitch       *mms);

static gint         compare_ports                       (gconstpointer          a,
                                                         gconstpointer          b);
static gint         compare_port_name                   (gconstpointer          a,
                                                         gconstpointer          b);

static void
pulse_port_switch_class_init (PulsePortSwitchClass *klass)
{
    GObjectClass         *object_class;
    MateMixerSwitchClass *switch_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = pulse_port_switch_dispose;

    switch_class = MATE_MIXER_SWITCH_CLASS (klass);
    switch_class->set_active_option = pulse_port_switch_set_active_option;
    switch_class->list_options      = pulse_port_switch_list_options;

    g_type_class_add_private (G_OBJECT_CLASS (klass), sizeof (PulsePortSwitchPrivate));
}

static void
pulse_port_switch_init (PulsePortSwitch *swtch)
{
    swtch->priv = G_TYPE_INSTANCE_GET_PRIVATE (swtch,
                                               PULSE_TYPE_PORT_SWITCH,
                                               PulsePortSwitchPrivate);
}

static void
pulse_port_switch_dispose (GObject *object)
{
    PulsePortSwitch *swtch;

    swtch = PULSE_PORT_SWITCH (object);

    if (swtch->priv->ports != NULL) {
        g_list_free_full (swtch->priv->ports, g_object_unref);
        swtch->priv->ports = NULL;
    }
    G_OBJECT_CLASS (pulse_port_switch_parent_class)->dispose (object);
}

PulseStream *
pulse_port_switch_get_stream (PulsePortSwitch *swtch)
{
    g_return_val_if_fail (PULSE_IS_PORT_SWITCH (swtch), NULL);

    return PULSE_STREAM (mate_mixer_stream_switch_get_stream (MATE_MIXER_STREAM_SWITCH (swtch)));
}

void
pulse_port_switch_add_port (PulsePortSwitch *swtch, PulsePort *port)
{
    g_return_if_fail (PULSE_IS_PORT_SWITCH (swtch));
    g_return_if_fail (PULSE_IS_PORT (port));

    swtch->priv->ports = g_list_insert_sorted (swtch->priv->ports,
                                               port,
                                               compare_ports);
}

void
pulse_port_switch_set_active_port (PulsePortSwitch *swtch, PulsePort *port)
{
    g_return_if_fail (PULSE_IS_PORT_SWITCH (swtch));
    g_return_if_fail (PULSE_IS_PORT (port));

    _mate_mixer_switch_set_active_option (MATE_MIXER_SWITCH (swtch),
                                          MATE_MIXER_SWITCH_OPTION (port));
}

void
pulse_port_switch_set_active_port_by_name (PulsePortSwitch *swtch, const gchar *name)
{
    GList *item;

    g_return_if_fail (PULSE_IS_PORT_SWITCH (swtch));
    g_return_if_fail (name != NULL);

    item = g_list_find_custom (swtch->priv->ports, name, compare_port_name);
    if G_UNLIKELY (item == NULL) {
        g_debug ("Invalid switch port name %s", name);
        return;
    }
    pulse_port_switch_set_active_port (swtch, PULSE_PORT (item->data));
}

static gboolean
pulse_port_switch_set_active_option (MateMixerSwitch *mms, MateMixerSwitchOption *mmso)
{
    PulsePortSwitchClass *klass;

    g_return_val_if_fail (PULSE_IS_PORT_SWITCH (mms), FALSE);
    g_return_val_if_fail (PULSE_IS_PORT (mmso), FALSE);

    klass = PULSE_PORT_SWITCH_GET_CLASS (PULSE_PORT_SWITCH (mms));

    return klass->set_active_port (PULSE_PORT_SWITCH (mms),
                                   PULSE_PORT (mmso));
}

static const GList *
pulse_port_switch_list_options (MateMixerSwitch *swtch)
{
    g_return_val_if_fail (PULSE_IS_PORT_SWITCH (swtch), NULL);

    return PULSE_PORT_SWITCH (swtch)->priv->ports;
}

static gint
compare_ports (gconstpointer a, gconstpointer b)
{
    return pulse_port_get_priority (PULSE_PORT (b)) -
           pulse_port_get_priority (PULSE_PORT (a));
}

static gint
compare_port_name (gconstpointer a, gconstpointer b)
{
    PulsePort   *port = PULSE_PORT (a);
    const gchar *name = (const gchar *) b;

    return strcmp (pulse_port_get_name (port), name);
}
