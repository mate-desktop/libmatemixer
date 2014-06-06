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

#include "matemixer-backend.h"
#include "matemixer-backend-module.h"
#include "matemixer-control.h"
#include "matemixer-enums.h"
#include "matemixer-private.h"
#include "matemixer-stream.h"

struct _MateMixerControlPrivate
{
    GList                  *devices;
    GList                  *streams;
    MateMixerBackend       *backend;
    MateMixerBackendModule *module;
};

G_DEFINE_TYPE (MateMixerControl, mate_mixer_control, G_TYPE_OBJECT);

static void              mate_mixer_control_class_init (MateMixerControlClass  *klass);
static void              mate_mixer_control_init       (MateMixerControl       *control);
static void              mate_mixer_control_dispose    (GObject                *object);

static MateMixerBackend *mixer_control_init_module     (MateMixerBackendModule *module);

static void
mate_mixer_control_class_init (MateMixerControlClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = mate_mixer_control_dispose;

    g_type_class_add_private (object_class, sizeof (MateMixerControlPrivate));
}

static void
mate_mixer_control_init (MateMixerControl *control)
{
    control->priv = G_TYPE_INSTANCE_GET_PRIVATE (control,
                                                 MATE_MIXER_TYPE_CONTROL,
                                                 MateMixerControlPrivate);
}

static void
mate_mixer_control_dispose (GObject *object)
{
    MateMixerControl *control;

    control = MATE_MIXER_CONTROL (object);

    if (control->priv->backend) {
        mate_mixer_backend_close (control->priv->backend);
        g_clear_object (&control->priv->backend);
    }

    g_clear_object (&control->priv->module);

    if (control->priv->devices) {
        g_list_free_full (control->priv->devices, g_object_unref);
        control->priv->devices = NULL;
    }
    if (control->priv->streams) {
        g_list_free_full (control->priv->streams, g_object_unref);
        control->priv->streams = NULL;
    }

    G_OBJECT_CLASS (mate_mixer_control_parent_class)->dispose (object);
}

MateMixerControl *
mate_mixer_control_new (void)
{
    const GList            *modules;
    MateMixerControl       *control;
    MateMixerBackend       *backend = NULL;
    MateMixerBackendModule *module = NULL;

    if (!mate_mixer_is_initialized ()) {
        g_critical ("The library has not been initialized");
        return NULL;
    }

    modules = mate_mixer_get_modules ();
    while (modules) {
        module  = MATE_MIXER_BACKEND_MODULE (modules->data);
        backend = mixer_control_init_module (module);
        if (backend != NULL)
            break;

        modules = modules->next;
    }

    /* The last module in the priority list is the "null" module which
     * should always be initialized correctly, but in case "null" is absent,
     * all the other modules might fail their initializations */
    if (backend == NULL)
        return NULL;

    control = g_object_new (MATE_MIXER_TYPE_CONTROL, NULL);

    control->priv->module  = g_object_ref (module);
    control->priv->backend = backend;

    return control;
}

MateMixerControl *
mate_mixer_control_new_backend (MateMixerBackendType backend_type)
{
    const GList            *modules;
    MateMixerControl       *control;
    MateMixerBackend       *backend = NULL;
    MateMixerBackendModule *module = NULL;

    if (!mate_mixer_is_initialized ()) {
        g_critical ("The library has not been initialized");
        return NULL;
    }

    modules = mate_mixer_get_modules ();
    while (modules) {
        const MateMixerBackendInfo *info;

        module = MATE_MIXER_BACKEND_MODULE (modules->data);
        info   = mate_mixer_backend_module_get_info (module);

        if (info->backend_type == backend_type) {
            backend = mixer_control_init_module (module);
            break;
        }
        modules = modules->next;
    }

    /* The initialization might fail or the selected module might be absent */
    if (backend == NULL)
        return NULL;

    control = g_object_new (MATE_MIXER_TYPE_CONTROL, NULL);

    control->priv->module  = g_object_ref (module);
    control->priv->backend = backend;

    return control;
}

const GList *
mate_mixer_control_list_devices (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);

    /* This list is cached here and invalidated when the backend notifies us
     * about a change */
    if (control->priv->devices == NULL)
        control->priv->devices =
            mate_mixer_backend_list_devices (MATE_MIXER_BACKEND (control->priv->backend));

    return (const GList *) control->priv->devices;
}

const GList *
mate_mixer_control_list_streams (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);

    /* This list is cached here and invalidated when the backend notifies us
     * about a change */
    if (control->priv->streams == NULL)
        control->priv->streams =
            mate_mixer_backend_list_streams (MATE_MIXER_BACKEND (control->priv->backend));

    return (const GList *) control->priv->streams;
}

MateMixerStream *
mate_mixer_control_get_default_input_stream (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);

    return mate_mixer_backend_get_default_input_stream (control->priv->backend);
}

MateMixerStream *
mate_mixer_control_get_default_output_stream (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);

    return mate_mixer_backend_get_default_output_stream (control->priv->backend);
}

const gchar *
mate_mixer_control_get_backend_name (MateMixerControl *control)
{
    const MateMixerBackendInfo *info;

    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);

    info = mate_mixer_backend_module_get_info (control->priv->module);

    return info->name;
}

MateMixerBackendType
mate_mixer_control_get_backend_type (MateMixerControl *control)
{
    const MateMixerBackendInfo *info;

    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), FALSE);

    info = mate_mixer_backend_module_get_info (control->priv->module);

    return info->backend_type;
}

static MateMixerBackend *
mixer_control_init_module (MateMixerBackendModule *module)
{
    MateMixerBackend           *backend;
    const MateMixerBackendInfo *info;

    info = mate_mixer_backend_module_get_info (module);
    backend = g_object_newv (info->g_type, 0, NULL);

    if (!mate_mixer_backend_open (backend)) {
        g_object_unref (backend);
        return NULL;
    }
    return backend;
}
