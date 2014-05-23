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
#include "matemixer-track.h"

struct _MateMixerControlPrivate
{
    MateMixerBackend  *backend;
    GHashTable        *devices;
    GHashTable        *tracks;
};

G_DEFINE_TYPE (MateMixerControl, mate_mixer_control, G_TYPE_OBJECT);

G_LOCK_DEFINE_STATIC (mixer_control_get_modules_lock);

static MateMixerBackend *mixer_control_init_module (MateMixerBackendModule *module);

static void
mate_mixer_control_init (MateMixerControl *control)
{
    control->priv = G_TYPE_INSTANCE_GET_PRIVATE (
        control,
        MATE_MIXER_TYPE_CONTROL,
        MateMixerControlPrivate);

    control->priv->devices = g_hash_table_new_full (
        g_direct_hash,
        g_direct_equal,
        NULL,
        g_object_unref);

    control->priv->tracks = g_hash_table_new_full (
        g_direct_hash,
        g_direct_equal,
        NULL,
        g_object_unref);
}

static void
mate_mixer_control_finalize (GObject *object)
{
    MateMixerControl *control;

    control = MATE_MIXER_CONTROL (object);

    g_object_unref (control->priv->backend);
    g_hash_table_destroy (control->priv->devices);
    g_hash_table_destroy (control->priv->tracks);

    G_OBJECT_CLASS (mate_mixer_control_parent_class)->finalize (object);
}

static void
mate_mixer_control_class_init (MateMixerControlClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = mate_mixer_control_finalize;

    g_type_class_add_private (object_class, sizeof (MateMixerControlPrivate));
}

MateMixerControl *
mate_mixer_control_new (void)
{
    GList            *modules;
    MateMixerControl *control;
    MateMixerBackend *backend = NULL;

    if (!mate_mixer_is_initialized ()) {
        g_critical ("The library has not been initialized");
        return NULL;
    }

    G_LOCK (mixer_control_get_modules_lock);

    modules = mate_mixer_get_modules ();
    while (modules) {
        MateMixerBackendModule *module;

        module  = MATE_MIXER_BACKEND_MODULE (modules->data);
        backend = mixer_control_init_module (module);
        if (backend != NULL)
            break;

        modules = modules->next;
    }

    G_UNLOCK (mixer_control_get_modules_lock);

    /* The last module in the priority list is the "null" module which
     * should always be initialized correctly, but in case "null" is absent
     * all the other modules might fail their initializations */
    if (backend == NULL)
        return NULL;

    control = g_object_new (MATE_MIXER_TYPE_CONTROL, NULL);
    control->priv->backend = backend;

    return control;
}

MateMixerControl *
mate_mixer_control_new_backend (MateMixerBackendType backend_type)
{
    GList            *modules;
    MateMixerControl *control;
    MateMixerBackend *backend = NULL;

    if (!mate_mixer_is_initialized ()) {
        g_critical ("The library has not been initialized");
        return NULL;
    }

    G_LOCK (mixer_control_get_modules_lock);

    modules = mate_mixer_get_modules ();

    while (modules) {
        MateMixerBackendModule            *module;
        const MateMixerBackendModuleInfo  *info;

        module = MATE_MIXER_BACKEND_MODULE (modules->data);
        info   = mate_mixer_backend_module_get_info (module);

        if (info->backend_type == backend_type) {
            backend = mixer_control_init_module (module);
            break;
        }
        modules = modules->next;
    }

    G_UNLOCK (mixer_control_get_modules_lock);

    /* The initialization might fail or the selected module might be absent */
    if (backend == NULL)
        return NULL;

    control = g_object_new (MATE_MIXER_TYPE_CONTROL, NULL);
    control->priv->backend = backend;

    return control;
}

const GList *
mate_mixer_control_list_devices (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);

    // XXX
    // this is the midpoint between MateMixerDevice implementation and
    // the application, probably the list could be cached here but the
    // implementation uses a hash table, figure out how to do this properly

    return mate_mixer_backend_list_devices (MATE_MIXER_BACKEND (control->priv->backend));
}

const GList *
mate_mixer_control_list_tracks (MateMixerControl *control)
{
    g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);

    return mate_mixer_backend_list_tracks (MATE_MIXER_BACKEND (control->priv->backend));
}

static MateMixerBackend *
mixer_control_init_module (MateMixerBackendModule *module)
{
    MateMixerBackend                  *backend;
    const MateMixerBackendModuleInfo  *info;

    info = mate_mixer_backend_module_get_info (module);
    if (G_UNLIKELY (info == NULL)) {
        g_warning ("Backend module %s does not provide module information",
            mate_mixer_backend_module_get_path (module));
        return NULL;
    }

    backend = g_object_newv (info->g_type, 0, NULL);

    if (!mate_mixer_backend_open (backend)) {
        g_object_unref (backend);
        return NULL;
    }
    return backend;
}
