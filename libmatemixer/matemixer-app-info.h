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

#ifndef MATEMIXER_APP_INFO_H
#define MATEMIXER_APP_INFO_H

#include <glib.h>
#include <glib-object.h>

#include "matemixer-types.h"

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_APP_INFO (mate_mixer_app_info_get_type ())

GType        mate_mixer_app_info_get_type    (void) G_GNUC_CONST;

const gchar *mate_mixer_app_info_get_name    (MateMixerAppInfo *info);
const gchar *mate_mixer_app_info_get_id      (MateMixerAppInfo *info);
const gchar *mate_mixer_app_info_get_version (MateMixerAppInfo *info);
const gchar *mate_mixer_app_info_get_icon    (MateMixerAppInfo *info);

G_END_DECLS

#endif /* MATEMIXER_APP_INFO_H */
