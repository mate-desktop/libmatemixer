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

#include "matemixer-app-info.h"
#include "matemixer-app-info-private.h"

/**
 * SECTION:matemixer-app-info
 * @short_description: Application information
 * @include: libmatemixer/matemixer.h
 * @see_also: #MateMixerStreamControl
 *
 * The #MateMixerAppInfo structure describes application properties.
 *
 * See #MateMixerStreamControl and the mate_mixer_stream_control_get_app_info()
 * function for more information.
 */

/**
 * MateMixerAppInfo:
 *
 * The #MateMixerAppInfo structure contains only private data and should only
 * be accessed using the provided API.
 */
G_DEFINE_BOXED_TYPE (MateMixerAppInfo, mate_mixer_app_info,
                     _mate_mixer_app_info_copy,
                     _mate_mixer_app_info_free)

/**
 * mate_mixer_app_info_get_name:
 * @info: a #MateMixerAppInfo
 *
 * Gets the name of the application described by @info.
 *
 * Returns: name of the application or %NULL if it is unknown.
 */
const gchar *
mate_mixer_app_info_get_name (MateMixerAppInfo *info)
{
    g_return_val_if_fail (info != NULL, NULL);

    return info->name;
}

/**
 * mate_mixer_app_info_get_id:
 * @info: a #MateMixerAppInfo
 *
 * Gets the identifier of the application described by @info
 * (e.g. org.example.app).
 *
 * Returns: identifier of the application or %NULL if it is unknown.
 */
const gchar *
mate_mixer_app_info_get_id (MateMixerAppInfo *info)
{
    g_return_val_if_fail (info != NULL, NULL);

    return info->id;
}

/**
 * mate_mixer_app_info_get_version:
 * @info: a #MateMixerAppInfo
 *
 * Gets the version of the application described by @info.
 *
 * Returns: version of the application or %NULL if it is unknown.
 */
const gchar *
mate_mixer_app_info_get_version (MateMixerAppInfo *info)
{
    g_return_val_if_fail (info != NULL, NULL);

    return info->version;
}

/**
 * mate_mixer_app_info_get_icon:
 * @info: a #MateMixerAppInfo
 *
 * Gets the XDG icon name of the application described by @info.
 *
 * Returns: icon name of the application or %NULL if it is unknown.
 */
const gchar *
mate_mixer_app_info_get_icon (MateMixerAppInfo *info)
{
    g_return_val_if_fail (info != NULL, NULL);

    return info->icon;
}

/**
 * _mate_mixer_app_info_new:
 *
 * Creates a new empty #MateMixerAppInfo structure.
 *
 * Returns: a new #MateMixerAppInfo.
 */
MateMixerAppInfo *
_mate_mixer_app_info_new (void)
{
    return g_slice_new0 (MateMixerAppInfo);
}

/**
 * _mate_mixer_app_info_set_name:
 * @info: a #MateMixerAppInfo
 * @name: the application name to set
 *
 * Sets the name of the application described by @info.
 */
void
_mate_mixer_app_info_set_name (MateMixerAppInfo *info, const gchar *name)
{
    g_return_if_fail (info != NULL);

    g_free (info->name);

    info->name = g_strdup (name);
}

/**
 * _mate_mixer_app_info_set_id:
 * @info: a #MateMixerAppInfo
 * @id: the application identifier to set
 *
 * Sets the identifier of the application described by @info.
 */
void
_mate_mixer_app_info_set_id (MateMixerAppInfo *info, const gchar *id)
{
    g_return_if_fail (info != NULL);

    g_free (info->id);

    info->id = g_strdup (id);
}

/**
 * _mate_mixer_app_info_set_version:
 * @info: a #MateMixerAppInfo
 * @version: the application version to set
 *
 * Sets the version of the application described by @info.
 */
void
_mate_mixer_app_info_set_version (MateMixerAppInfo *info, const gchar *version)
{
    g_return_if_fail (info != NULL);

    g_free (info->version);

    info->version = g_strdup (version);
}

/**
 * _mate_mixer_app_info_set_version:
 * @info: a #MateMixerAppInfo
 * @icon: the application icon name to set
 *
 * Sets the XDG icon name of the application described by @info.
 */
void
_mate_mixer_app_info_set_icon (MateMixerAppInfo *info, const gchar *icon)
{
    g_return_if_fail (info != NULL);

    g_free (info->icon);

    info->icon = g_strdup (icon);
}

/**
 * _mate_mixer_app_info_copy:
 * @info: a #MateMixerAppInfo
 *
 * Creates a copy of the #MateMixerAppInfo.
 *
 * Returns: a copy of the given @info.
 */
MateMixerAppInfo *
_mate_mixer_app_info_copy (MateMixerAppInfo *info)
{
    MateMixerAppInfo *info2;

    g_return_val_if_fail (info != NULL, NULL);

    info2 = _mate_mixer_app_info_new ();
    info2->name     = g_strdup (info->name);
    info2->id       = g_strdup (info->id);
    info2->version  = g_strdup (info->version);
    info2->icon     = g_strdup (info->icon);

    return info2;
}

/**
 * _mate_mixer_app_info_free:
 * @info: a #MateMixerAppInfo
 *
 * Frees the #MateMixerAppInfo.
 */
void
_mate_mixer_app_info_free (MateMixerAppInfo *info)
{
    g_return_if_fail (info != NULL);

    g_free (info->name);
    g_free (info->id);
    g_free (info->version);
    g_free (info->icon);

    g_slice_free (MateMixerAppInfo, info);
}
