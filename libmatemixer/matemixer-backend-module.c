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
#include <gmodule.h>

#include "matemixer-backend.h"
#include "matemixer-backend-module.h"

G_DEFINE_TYPE (MateMixerBackendModule, mate_mixer_backend_module, G_TYPE_TYPE_MODULE);

struct _MateMixerBackendModulePrivate
{
    GModule  *gmodule;
    gchar    *path;

    void (*init) (GTypeModule *type_module);
    void (*free) (void);

    const MateMixerBackendModuleInfo *(*get_info) (void);
};

static gboolean  mate_mixer_backend_module_load   (GTypeModule *gmodule);
static void      mate_mixer_backend_module_unload (GTypeModule *gmodule);

static void
mate_mixer_backend_module_init (MateMixerBackendModule *module)
{
    module->priv = G_TYPE_INSTANCE_GET_PRIVATE (
        module,
        MATE_MIXER_TYPE_BACKEND_MODULE,
        MateMixerBackendModulePrivate);
}

static void
mate_mixer_backend_module_finalize (GObject *object)
{
    MateMixerBackendModule *module;

    module = MATE_MIXER_BACKEND_MODULE (object);

    g_free (module->priv->path);

    G_OBJECT_CLASS (mate_mixer_backend_module_parent_class)->finalize (object);
}

static void
mate_mixer_backend_module_class_init (MateMixerBackendModuleClass *klass)
{
    GObjectClass     *object_class;
    GTypeModuleClass *gtype_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = mate_mixer_backend_module_finalize;

    gtype_class  = G_TYPE_MODULE_CLASS (klass);
    gtype_class->load   = mate_mixer_backend_module_load;
    gtype_class->unload = mate_mixer_backend_module_unload;

    g_type_class_add_private (object_class, sizeof (MateMixerBackendModulePrivate));
}

static gboolean
mate_mixer_backend_module_load (GTypeModule *type_module)
{
    MateMixerBackendModule *module;

    module = MATE_MIXER_BACKEND_MODULE (type_module);

    module->priv->gmodule = g_module_open (
        module->priv->path,
        G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

    if (module->priv->gmodule == NULL) {
        g_warning ("Failed to load backend module %s: %s",
                   module->priv->path,
                   g_module_error ());

        return FALSE;
    }

    /* Validate library symbols that each backend module must provide */
    if (!g_module_symbol (module->priv->gmodule,
                          "backend_module_init",
                          (gpointer *) &module->priv->init) ||
        !g_module_symbol (module->priv->gmodule,
                          "backend_module_free",
                          (gpointer *) &module->priv->free) ||
        !g_module_symbol (module->priv->gmodule,
                          "backend_module_get_info",
                          (gpointer *) &module->priv->get_info)) {
        g_warning ("Failed to load backend module %s: %s",
                   module->priv->path,
                   g_module_error ());

        g_module_close (module->priv->gmodule);
        return FALSE;
    }

    g_debug ("Loaded backend module %s", module->priv->path);

    module->priv->init (type_module);
    return TRUE;
}

static void
mate_mixer_backend_module_unload (GTypeModule *type_module)
{
    MateMixerBackendModule *module;

    module = MATE_MIXER_BACKEND_MODULE (type_module);
    module->priv->free ();

    g_module_close (module->priv->gmodule);
}

MateMixerBackendModule *
mate_mixer_backend_module_new (const gchar *path)
{
    MateMixerBackendModule *module;

    g_return_val_if_fail (path != NULL, NULL);

    module = g_object_newv (MATE_MIXER_TYPE_BACKEND_MODULE, 0, NULL);
    module->priv->path = g_strdup (path);

    g_type_module_set_name (G_TYPE_MODULE (module), path);

    return module;
}

const MateMixerBackendModuleInfo *
mate_mixer_backend_module_get_info (MateMixerBackendModule *module)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND_MODULE (module), NULL);

    return module->priv->get_info ();
}

const gchar *
mate_mixer_backend_module_get_path (MateMixerBackendModule *module)
{
    g_return_val_if_fail (MATE_MIXER_IS_BACKEND_MODULE (module), NULL);

    return module->priv->path;
}
