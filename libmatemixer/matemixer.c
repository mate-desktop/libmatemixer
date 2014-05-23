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

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include "matemixer.h"
#include "matemixer-private.h"
#include "matemixer-backend-module.h"

static gboolean  mixer_load_backends    (void);
static gint      mixer_compare_modules  (gconstpointer a, gconstpointer b);

static GList    *mixer_modules = NULL;
static gboolean  mixer_initialized = FALSE;

gboolean
mate_mixer_init (void)
{
    if (!mixer_initialized)
        mixer_initialized = mixer_load_backends ();

    return mixer_initialized;
}

void
mate_mixer_deinit (void)
{
    GList *list;

    if (!mixer_initialized)
        return;

    list = mixer_modules;
    while (list) {
        MateMixerBackendModule *module;

        module = MATE_MIXER_BACKEND_MODULE (list->data);

        g_type_module_unuse (G_TYPE_MODULE (module));

        // XXX it is not possible to unref the module, figure out how
        // to handle repeated initialization
        // g_object_unref (module);

        list = list->next;
    }
    g_list_free (mixer_modules);

    mixer_initialized = FALSE;
}

/* Internal function: return a *shared* list of loaded backend modules */
GList *
mate_mixer_get_modules (void)
{
    return mixer_modules;
}

/* Internal function: return TRUE if the library has been initialized */
gboolean
mate_mixer_is_initialized (void)
{
    gboolean initialized;

    G_LOCK_DEFINE_STATIC (mixer_initialized_lock);

    G_LOCK (mixer_initialized_lock);
    initialized = mixer_initialized;
    G_UNLOCK (mixer_initialized_lock);

    return initialized;
}

static gboolean
mixer_load_backends (void)
{
    GDir   *dir;
    GError *error = NULL;

    if (!g_module_supported ()) {
        g_critical ("Unable to load backend modules: GModule not supported");
        return FALSE;
    }

    /* Read the directory which contains module libraries and load them */
    dir = g_dir_open (LIBMATEMIXER_BACKEND_DIR, 0, &error);
    if (dir != NULL) {
        const gchar *name;

        while ((name = g_dir_read_name (dir)) != NULL) {
            gchar *file;
            MateMixerBackendModule *module;

            if (!g_str_has_suffix (name, "." G_MODULE_SUFFIX))
                continue;

            file = g_build_filename (LIBMATEMIXER_BACKEND_DIR, name, NULL);
            module = mate_mixer_backend_module_new (file);

            /* Load the backend module and make sure it includes all the
             * required symbols */
            if (g_type_module_use (G_TYPE_MODULE (module)))
                mixer_modules = g_list_prepend (mixer_modules, module);
            else
                g_object_unref (module);

            g_free (file);
        }

        if (mixer_modules == NULL)
            g_critical ("No usable backend modules have been found");

        g_dir_close (dir);
    } else {
        g_critical ("%s", error->message);
        g_error_free (error);
    }

    if (mixer_modules) {
        mixer_modules = g_list_sort (mixer_modules, mixer_compare_modules);
        return TRUE;
    }

    /* As we include a "null" backend module to provide no functionality,
     * we fail the initialization in case no module could be loaded */
    return FALSE;
}

/* GCompareFunc function to sort backend modules by the priority, lower
 * number means higher priority */
static gint
mixer_compare_modules (gconstpointer a, gconstpointer b)
{
    const MateMixerBackendModuleInfo *info1, *info2;

    info1 = mate_mixer_backend_module_get_info (MATE_MIXER_BACKEND_MODULE (a));
    info2 = mate_mixer_backend_module_get_info (MATE_MIXER_BACKEND_MODULE (b));

    return info1->priority - info2->priority;
}
