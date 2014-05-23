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

#ifndef MATEMIXER_NULL_H
#define MATEMIXER_NULL_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-backend.h>

#define MATE_MIXER_TYPE_NULL                   \
        (mate_mixer_null_get_type ())
#define MATE_MIXER_NULL(o)                     \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_NULL, MateMixerNull))
#define MATE_MIXER_IS_NULL(o)                  \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_NULL))
#define MATE_MIXER_NULL_CLASS(k)               \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_NULL, MateMixerNullClass))
#define MATE_MIXER_IS_NULL_CLASS(k)            \
        (G_TYPE_CLASS_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_NULL))
#define MATE_MIXER_NULL_GET_CLASS(o)           \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_NULL, MateMixerNullClass))

typedef struct _MateMixerNull       MateMixerNull;
typedef struct _MateMixerNullClass  MateMixerNullClass;

struct _MateMixerNull
{
    GObject parent;
};

struct _MateMixerNullClass
{
    GObjectClass parent;
};

GType mate_mixer_null_get_type (void) G_GNUC_CONST;

#endif /* MATEMIXER_NULL_H */
