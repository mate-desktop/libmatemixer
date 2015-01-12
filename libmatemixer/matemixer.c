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

/**
 * SECTION:matemixer
 * @short_description: Library initialization and support functions
 * @include: libmatemixer/matemixer.h
 * @see_also: #MateMixerContext
 *
 * The libmatemixer library must be initialized before it is used by an
 * application. The initialization function loads dynamic modules which provide
 * access to sound systems (also called backends) and it only succeeds if there
 * is at least one usable module present on the target system.
 *
 * To connect to a sound system and access the mixer functionality after the
 * library is initialized, create a #MateMixerContext using the
 * mate_mixer_context_new() function.
 */

static void       load_modules     (void);
static gint       compare_modules  (gconstpointer a,
                                    gconstpointer b);

static GList     *modules = NULL;
static gboolean   initialized = FALSE;

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
    if (initialized == TRUE)
        return TRUE;

    load_modules ();

    if (modules != NULL) {
        GList *list = modules;

        while (list != NULL) {
            GTypeModule *module = G_TYPE_MODULE (list->data);
            GList       *next = list->next;

            /* Load the plugin and remove it from the list if it fails */
            if (g_type_module_use (module) == FALSE) {
                g_object_unref (module);
                modules = g_list_delete_link (modules, list);
            }
            list = next;
        }

        if (modules != NULL) {
            /* Sort the usable modules by priority */
            modules = g_list_sort (modules, compare_modules);
            initialized = TRUE;
        } else
            g_critical ("No usable backend modules have been found");
    } else
        g_critical ("No backend modules have been found");

    return initialized;
}

/**
 * mate_mixer_is_initialized:
 *
 * Returns %TRUE if the library has been initialized.
 *
 * Returns: %TRUE or %FALSE.
 */
gboolean
mate_mixer_is_initialized (void)
{
    return initialized;
}

/**
 * _mate_mixer_list_modules:
 *
 * Gets a list of loaded backend modules.
 *
 * Returns: a #GList.
 */
const GList *
_mate_mixer_list_modules (void)
{
    return (const GList *) modules;
}

/**
 * _mate_mixer_create_channel_mask:
 * @positions: an array of channel positions
 * @n: number of channel positions in the array
 *
 * Creates a channel mask using the given list of channel positions.
 *
 * Returns: a channel mask.
 */
guint32
_mate_mixer_create_channel_mask (MateMixerChannelPosition *positions, guint n)
{
    guint32 mask = 0;
    guint   i = 0;

    for (i = 0; i < n; i++) {
        if (positions[i] > MATE_MIXER_CHANNEL_UNKNOWN &&
            positions[i] < MATE_MIXER_CHANNEL_MAX)
            mask |= 1 << positions[i];
    }
    return mask;
}

static void
load_modules (void)
{
    static gboolean loaded = FALSE;

    if (loaded == TRUE)
        return;

    if G_LIKELY (g_module_supported () == TRUE) {
        GDir   *dir;
        GError *error = NULL;

        /* Read the directory which contains module libraries and create a list
         * of those that are likely to be usable backend modules */
        dir = g_dir_open (LIBMATEMIXER_BACKEND_DIR, 0, &error);
        if (dir != NULL) {
            const gchar *name;

            while ((name = g_dir_read_name (dir)) != NULL) {
                gchar *file;

                if (g_str_has_suffix (name, "." G_MODULE_SUFFIX) == FALSE)
                    continue;

                file = g_build_filename (LIBMATEMIXER_BACKEND_DIR, name, NULL);
                modules = g_list_prepend (modules,
                                          mate_mixer_backend_module_new (file));
                g_free (file);
            }

            g_dir_close (dir);
        } else {
            g_critical ("%s", error->message);
            g_error_free (error);
        }
    } else {
        g_critical ("Unable to load backend modules: Not supported");
    }

    loaded = TRUE;
}

/* Backend modules sorting function, higher priority number means higher priority
 * of the backend module */
static gint
compare_modules (gconstpointer a, gconstpointer b)
{
    const MateMixerBackendInfo *info1, *info2;

    info1 = mate_mixer_backend_module_get_info (MATE_MIXER_BACKEND_MODULE (a));
    info2 = mate_mixer_backend_module_get_info (MATE_MIXER_BACKEND_MODULE (b));

    return info2->priority - info1->priority;
}
