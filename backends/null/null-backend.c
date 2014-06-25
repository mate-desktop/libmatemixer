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

#include <libmatemixer/matemixer-backend.h>
#include <libmatemixer/matemixer-backend-module.h>
#include <libmatemixer/matemixer-enums.h>

#include "null-backend.h"

#define BACKEND_NAME      "Null"
#define BACKEND_PRIORITY  0

enum {
    PROP_0,
    PROP_STATE,
    PROP_DEFAULT_INPUT,
    PROP_DEFAULT_OUTPUT,
    N_PROPERTIES
};

static void mate_mixer_backend_interface_init (MateMixerBackendInterface *iface);

static void null_backend_class_init     (NullBackendClass *klass);
static void null_backend_class_finalize (NullBackendClass *klass);
static void null_backend_get_property   (GObject          *object,
                                         guint             param_id,
                                         GValue           *value,
                                         GParamSpec       *pspec);
static void null_backend_init           (NullBackend      *null);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (NullBackend, null_backend,
                                G_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (MATE_MIXER_TYPE_BACKEND,
                                                               mate_mixer_backend_interface_init))

static gboolean       backend_open      (MateMixerBackend *backend);
static MateMixerState backend_get_state (MateMixerBackend *backend);

static MateMixerBackendInfo info;

void
backend_module_init (GTypeModule *module)
{
    null_backend_register_type (module);

    info.name         = BACKEND_NAME;
    info.priority     = BACKEND_PRIORITY;
    info.g_type       = NULL_TYPE_BACKEND;
    info.backend_type = MATE_MIXER_BACKEND_NULL;
}

const MateMixerBackendInfo *
backend_module_get_info (void)
{
    return &info;
}

static void
mate_mixer_backend_interface_init (MateMixerBackendInterface *iface)
{
    iface->open      = backend_open;
    iface->get_state = backend_get_state;
}

static void
null_backend_class_init (NullBackendClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->get_property = null_backend_get_property;

    g_object_class_override_property (object_class, PROP_STATE, "state");
    g_object_class_override_property (object_class, PROP_DEFAULT_INPUT, "default-input");
    g_object_class_override_property (object_class, PROP_DEFAULT_OUTPUT, "default-output");
}

/* Called in the code generated by G_DEFINE_DYNAMIC_TYPE_EXTENDED() */
static void
null_backend_class_finalize (NullBackendClass *klass)
{
}

static void
null_backend_get_property (GObject    *object,
                           guint       param_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    switch (param_id) {
    case PROP_STATE:
        g_value_set_enum (value, MATE_MIXER_STATE_READY);
        break;
    case PROP_DEFAULT_INPUT:
    case PROP_DEFAULT_OUTPUT:
        g_value_set_object (value, NULL);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
null_backend_init (NullBackend *null)
{
}

static gboolean
backend_open (MateMixerBackend *backend)
{
    return TRUE;
}

static MateMixerState
backend_get_state (MateMixerBackend *backend)
{
    return MATE_MIXER_STATE_READY;
}
