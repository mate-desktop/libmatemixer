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

#ifndef MATEMIXER_H
#define MATEMIXER_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-types.h>

#include <libmatemixer/matemixer-app-info.h>
#include <libmatemixer/matemixer-context.h>
#include <libmatemixer/matemixer-device.h>
#include <libmatemixer/matemixer-device-switch.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-enum-types.h>
#include <libmatemixer/matemixer-stored-control.h>
#include <libmatemixer/matemixer-stream.h>
#include <libmatemixer/matemixer-stream-control.h>
#include <libmatemixer/matemixer-stream-switch.h>
#include <libmatemixer/matemixer-stream-toggle.h>
#include <libmatemixer/matemixer-switch.h>
#include <libmatemixer/matemixer-switch-option.h>
#include <libmatemixer/matemixer-version.h>

G_BEGIN_DECLS

gboolean mate_mixer_init           (void);
gboolean mate_mixer_is_initialized (void);

G_END_DECLS

#endif /* MATEMIXER_H */
