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

#include <libmatemixer/matemixer-client-stream.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-stream.h>

#include <pulse/pulseaudio.h>

#include "pulse-client-stream.h"
#include "pulse-sink.h"
#include "pulse-source.h"
#include "pulse-stream.h"

struct _PulseClientStreamPrivate
{
    gchar                     *app_name;
    gchar                     *app_id;
    gchar                     *app_version;
    gchar                     *app_icon;
    MateMixerStream           *parent;
    MateMixerClientStreamFlags flags;
    MateMixerClientStreamRole  role;
};

enum {
    PROP_0,
    PROP_CLIENT_FLAGS,
    PROP_ROLE,
    PROP_PARENT,
    PROP_APP_NAME,
    PROP_APP_ID,
    PROP_APP_VERSION,
    PROP_APP_ICON
};

enum {
    REMOVED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void mate_mixer_client_stream_interface_init (MateMixerClientStreamInterface *iface);

static void pulse_client_stream_class_init   (PulseClientStreamClass *klass);

static void pulse_client_stream_get_property (GObject                *object,
                                              guint                   param_id,
                                              GValue                 *value,
                                              GParamSpec             *pspec);

static void pulse_client_stream_init         (PulseClientStream      *client);
static void pulse_client_stream_dispose      (GObject                *object);
static void pulse_client_stream_finalize     (GObject                *object);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PulseClientStream, pulse_client_stream, PULSE_TYPE_STREAM,
                                  G_IMPLEMENT_INTERFACE (MATE_MIXER_TYPE_CLIENT_STREAM,
                                                         mate_mixer_client_stream_interface_init))

static MateMixerClientStreamFlags pulse_client_stream_get_flags       (MateMixerClientStream *client);
static MateMixerClientStreamRole  pulse_client_stream_get_role        (MateMixerClientStream *client);

static MateMixerStream *          pulse_client_stream_get_parent      (MateMixerClientStream *client);
static gboolean                   pulse_client_stream_set_parent      (MateMixerClientStream *client,
                                                                       MateMixerStream       *parent);

static gboolean                   pulse_client_stream_remove          (MateMixerClientStream *client);

static const gchar *              pulse_client_stream_get_app_name    (MateMixerClientStream *client);
static const gchar *              pulse_client_stream_get_app_id      (MateMixerClientStream *client);
static const gchar *              pulse_client_stream_get_app_version (MateMixerClientStream *client);
static const gchar *              pulse_client_stream_get_app_icon    (MateMixerClientStream *client);

static void
mate_mixer_client_stream_interface_init (MateMixerClientStreamInterface *iface)
{
    iface->get_flags       = pulse_client_stream_get_flags;
    iface->get_role        = pulse_client_stream_get_role;
    iface->get_parent      = pulse_client_stream_get_parent;
    iface->set_parent      = pulse_client_stream_set_parent;
    iface->remove          = pulse_client_stream_remove;
    iface->get_app_name    = pulse_client_stream_get_app_name;
    iface->get_app_id      = pulse_client_stream_get_app_id;
    iface->get_app_version = pulse_client_stream_get_app_version;
    iface->get_app_icon    = pulse_client_stream_get_app_icon;
}

static void
pulse_client_stream_class_init (PulseClientStreamClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose      = pulse_client_stream_dispose;
    object_class->finalize     = pulse_client_stream_finalize;
    object_class->get_property = pulse_client_stream_get_property;

    signals[REMOVED] =
        g_signal_new ("removed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (PulseClientStreamClass, removed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0,
                      G_TYPE_NONE);

    g_object_class_override_property (object_class, PROP_CLIENT_FLAGS, "client-flags");
    g_object_class_override_property (object_class, PROP_ROLE, "role");
    g_object_class_override_property (object_class, PROP_PARENT, "parent");
    g_object_class_override_property (object_class, PROP_APP_NAME, "app-name");
    g_object_class_override_property (object_class, PROP_APP_ID, "app-id");
    g_object_class_override_property (object_class, PROP_APP_VERSION, "app-version");
    g_object_class_override_property (object_class, PROP_APP_ICON, "app-icon");

    g_type_class_add_private (object_class, sizeof (PulseClientStreamPrivate));
}

static void
pulse_client_stream_get_property (GObject    *object,
                                  guint       param_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
    PulseClientStream *client;

    client = PULSE_CLIENT_STREAM (object);

    switch (param_id) {
    case PROP_CLIENT_FLAGS:
        g_value_set_flags (value, client->priv->flags);
        break;
    case PROP_ROLE:
        g_value_set_enum (value, client->priv->role);
        break;
    case PROP_PARENT:
        g_value_set_object (value, client->priv->parent);
        break;
    case PROP_APP_NAME:
        g_value_set_string (value, client->priv->app_name);
        break;
    case PROP_APP_ID:
        g_value_set_string (value, client->priv->app_id);
        break;
    case PROP_APP_VERSION:
        g_value_set_string (value, client->priv->app_version);
        break;
    case PROP_APP_ICON:
        g_value_set_string (value, client->priv->app_icon);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
pulse_client_stream_init (PulseClientStream *client)
{
    client->priv = G_TYPE_INSTANCE_GET_PRIVATE (client,
                                                PULSE_TYPE_CLIENT_STREAM,
                                                PulseClientStreamPrivate);
}

static void
pulse_client_stream_dispose (GObject *object)
{
    PulseClientStream *client;

    client = PULSE_CLIENT_STREAM (object);

    g_clear_object (&client->priv->parent);

    G_OBJECT_CLASS (pulse_client_stream_parent_class)->dispose (object);
}

static void
pulse_client_stream_finalize (GObject *object)
{
    PulseClientStream *client;

    client = PULSE_CLIENT_STREAM (object);

    g_free (client->priv->app_name);
    g_free (client->priv->app_id);
    g_free (client->priv->app_version);
    g_free (client->priv->app_icon);

    G_OBJECT_CLASS (pulse_client_stream_parent_class)->finalize (object);
}

gboolean
pulse_client_stream_update_flags (PulseClientStream         *pclient,
                                  MateMixerClientStreamFlags flags)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (pclient), FALSE);

    if (pclient->priv->flags != flags) {
        pclient->priv->flags = flags;

        g_object_notify (G_OBJECT (pclient), "client-flags");
    }
    return TRUE;
}

gboolean
pulse_client_stream_update_parent (PulseClientStream *pclient, MateMixerStream *parent)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (pclient), FALSE);

    if (pclient->priv->parent != parent) {
        g_clear_object (&pclient->priv->parent);

        if (G_LIKELY (parent != NULL))
            pclient->priv->parent = g_object_ref (parent);

        g_object_notify (G_OBJECT (pclient), "parent");
    }
    return TRUE;
}

gboolean
pulse_client_stream_update_role (PulseClientStream        *pclient,
                                 MateMixerClientStreamRole role)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (pclient), FALSE);

    if (pclient->priv->role != role) {
        pclient->priv->role = role;

        g_object_notify (G_OBJECT (pclient), "role");
    }
    return TRUE;
}

gboolean
pulse_client_stream_update_app_name (PulseClientStream *pclient, const gchar *app_name)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (pclient), FALSE);

    if (g_strcmp0 (pclient->priv->app_name, app_name) != 0) {
        g_free (pclient->priv->app_name);
        pclient->priv->app_name = g_strdup (app_name);

        g_object_notify (G_OBJECT (pclient), "app-name");
    }
    return TRUE;
}

gboolean
pulse_client_stream_update_app_id (PulseClientStream *pclient, const gchar *app_id)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (pclient), FALSE);

    if (g_strcmp0 (pclient->priv->app_id, app_id) != 0) {
        g_free (pclient->priv->app_id);
        pclient->priv->app_id = g_strdup (app_id);

        g_object_notify (G_OBJECT (pclient), "app-id");
    }
    return TRUE;
}

gboolean
pulse_client_stream_update_app_version (PulseClientStream *pclient, const gchar *app_version)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (pclient), FALSE);

    if (g_strcmp0 (pclient->priv->app_version, app_version) != 0) {
        g_free (pclient->priv->app_version);
        pclient->priv->app_version = g_strdup (app_version);

        g_object_notify (G_OBJECT (pclient), "app-version");
    }
    return TRUE;
}

gboolean
pulse_client_stream_update_app_icon (PulseClientStream *pclient, const gchar *app_icon)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (pclient), FALSE);

    if (g_strcmp0 (pclient->priv->app_icon, app_icon) != 0) {
        g_free (pclient->priv->app_icon);
        pclient->priv->app_icon = g_strdup (app_icon);

        g_object_notify (G_OBJECT (pclient), "app-icon");
    }
    return TRUE;
}

static MateMixerClientStreamFlags
pulse_client_stream_get_flags (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), MATE_MIXER_CLIENT_STREAM_NO_FLAGS);

    return PULSE_CLIENT_STREAM (client)->priv->flags;
}

static MateMixerClientStreamRole
pulse_client_stream_get_role (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), MATE_MIXER_CLIENT_STREAM_ROLE_NONE);

    return PULSE_CLIENT_STREAM (client)->priv->role;
}

static MateMixerStream *
pulse_client_stream_get_parent (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), NULL);

    return PULSE_CLIENT_STREAM (client)->priv->parent;
}

static gboolean
pulse_client_stream_set_parent (MateMixerClientStream *client, MateMixerStream *parent)
{
    MateMixerStreamFlags    flags;
    PulseClientStream      *pclient;
    PulseClientStreamClass *klass;

    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), FALSE);
    g_return_val_if_fail (PULSE_IS_STREAM (parent), FALSE);

    pclient = PULSE_CLIENT_STREAM (client);
    klass   = PULSE_CLIENT_STREAM_GET_CLASS (pclient);

    if (pclient->priv->parent == parent)
        return TRUE;

    flags = mate_mixer_stream_get_flags (MATE_MIXER_STREAM (pclient));

    /* Validate the parent stream */
    if (flags & MATE_MIXER_STREAM_INPUT && !PULSE_IS_SOURCE (parent)) {
        g_warning ("Could not change stream parent to %s: not a parent input stream",
                   mate_mixer_stream_get_name (MATE_MIXER_STREAM (parent)));
        return FALSE;
    } else if (flags & MATE_MIXER_STREAM_OUTPUT && !PULSE_IS_SINK (parent)) {
        g_warning ("Could not change stream parent to %s: not a parent output stream",
                   mate_mixer_stream_get_name (MATE_MIXER_STREAM (parent)));
        return FALSE;
    }

    /* Set the parent */
    if (klass->set_parent (pclient, PULSE_STREAM (parent)) == FALSE)
        return FALSE;

    if (pclient->priv->parent != NULL)
        g_object_unref (pclient->priv->parent);

    /* It is allowed for the parent to be NULL when the instance is created, but
     * changing the parent requires a valid parent stream */
    pclient->priv->parent = g_object_ref (parent);

    g_object_notify (G_OBJECT (client), "parent");
    return TRUE;
}

static gboolean
pulse_client_stream_remove (MateMixerClientStream *client)
{
    PulseClientStream      *pclient;
    PulseClientStreamClass *klass;

    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), FALSE);

    pclient = PULSE_CLIENT_STREAM (client);
    klass   = PULSE_CLIENT_STREAM_GET_CLASS (pclient);

    if (klass->remove (pclient) == FALSE)
        return FALSE;

    // XXX handle this in the backend
    g_signal_emit (G_OBJECT (client),
                   signals[REMOVED],
                   0);

    return TRUE;
}

static const gchar *
pulse_client_stream_get_app_name (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), NULL);

    return PULSE_CLIENT_STREAM (client)->priv->app_name;
}

static const gchar *
pulse_client_stream_get_app_id (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), NULL);

    return PULSE_CLIENT_STREAM (client)->priv->app_id;
}

static const gchar *
pulse_client_stream_get_app_version (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), NULL);

    return PULSE_CLIENT_STREAM (client)->priv->app_version;
}

static const gchar *
pulse_client_stream_get_app_icon (MateMixerClientStream *client)
{
    g_return_val_if_fail (PULSE_IS_CLIENT_STREAM (client), NULL);

    return PULSE_CLIENT_STREAM (client)->priv->app_icon;
}
