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

static void mixer_load_modules    (void);
static gint mixer_compare_modules (gconstpointer a, gconstpointer b);

static GList    *mixer_modules = NULL;
static gboolean  mixer_initialized = FALSE;

/**
 * mate_mixer_init:
 *
 * Initializes the library. You must call this function before using any other
 * function from the library.
 *
 * Returns: %TRUE on success or %FALSE if the library installation does not
 * provide support for any sound system backends.
 */
gboolean
mate_mixer_init (void)
{
    if (!mixer_initialized) {
        mixer_load_modules ();

        if (mixer_modules) {
            GList *list;

            list = mixer_modules;
            while (list) {
                GTypeModule *module = G_TYPE_MODULE (list->data);
                GList *next = list->next;

                /* Attempt to load the module and remove it from the list
                 * if it isn't usable */
                if (!g_type_module_use (module)) {
                    g_object_unref (module);
                    mixer_modules = g_list_delete_link (mixer_modules, list);
                }
                list = next;
            }

            if (mixer_modules) {
                /* Sort the usable modules by their priority */
                mixer_modules = g_list_sort (mixer_modules, mixer_compare_modules);
                mixer_initialized = TRUE;
            } else
                g_critical ("No usable backend modules have been found");
        }
    }

    return mixer_initialized;
}

/**
 * mate_mixer_is_initialized:
 *
 * Returns TRUE if the library has been initialized.
 *
 * Returns: %TRUE or %FALSE
 */
gboolean
mate_mixer_is_initialized (void)
{
    return mixer_initialized;
}

/**
 * mate_mixer_deinit:
 *
 * Deinitializes the library. You should call this function when you do not need
 * to use the library any longer or before exitting the application.
 */
void
mate_mixer_deinit (void)
{
    GList *list;

    if (!mixer_initialized)
        return;

    list = mixer_modules;
    while (list) {
        g_type_module_unuse (G_TYPE_MODULE (list->data));
        list = list->next;
    }
    mixer_initialized = FALSE;
}

/* Internal function: return a list of loaded backend modules */
const GList *
mate_mixer_get_modules (void)
{
    return (const GList *) mixer_modules;
}

static void
mixer_load_modules (void)
{
    static gboolean loaded = FALSE;

    if (loaded)
        return;

    if (g_module_supported ()) {
        GDir   *dir;
        GError *error = NULL;

        /* Read the directory which contains module libraries and create a list
         * of those that are likely to be usable backend modules */
        dir = g_dir_open (LIBMATEMIXER_BACKEND_DIR, 0, &error);
        if (dir != NULL) {
            const gchar *name;

            while ((name = g_dir_read_name (dir)) != NULL) {
                gchar *file;

                if (!g_str_has_suffix (name, "." G_MODULE_SUFFIX))
                    continue;

                file = g_build_filename (LIBMATEMIXER_BACKEND_DIR, name, NULL);
                mixer_modules = g_list_prepend (mixer_modules,
                                                mate_mixer_backend_module_new (file));
                g_free (file);
            }

            if (mixer_modules == NULL)
                g_critical ("No backend modules have been found");

            g_dir_close (dir);
        } else {
            g_critical ("%s", error->message);
            g_error_free (error);
        }
    } else {
        g_critical ("Unable to load backend modules: GModule is not supported in your system");
    }

    loaded = TRUE;
}

/* GCompareFunc function to sort backend modules by the priority, higher number means
 * higher priority */
static gint
mixer_compare_modules (gconstpointer a, gconstpointer b)
{
    const MateMixerBackendInfo *info1, *info2;

    info1 = mate_mixer_backend_module_get_info (MATE_MIXER_BACKEND_MODULE (a));
    info2 = mate_mixer_backend_module_get_info (MATE_MIXER_BACKEND_MODULE (b));

    return info2->priority - info1->priority;
}
