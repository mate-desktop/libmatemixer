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

#ifndef MATEMIXER_BACKEND_H
#define MATEMIXER_BACKEND_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-enums.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_BACKEND                 \
        (mate_mixer_backend_get_type ())
#define MATE_MIXER_BACKEND(o)                   \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_BACKEND, MateMixerBackend))
#define MATE_MIXER_IS_BACKEND(o)                \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_BACKEND))
#define MATE_MIXER_BACKEND_GET_INTERFACE(o)     \
        (G_TYPE_INSTANCE_GET_INTERFACE ((o), MATE_MIXER_TYPE_BACKEND, MateMixerBackendInterface))

typedef struct _MateMixerBackend           MateMixerBackend; /* dummy object */
typedef struct _MateMixerBackendInterface  MateMixerBackendInterface;

struct _MateMixerBackendInterface
{
    GTypeInterface parent;

    gboolean      (*open)          (MateMixerBackend *backend);
    void          (*close)         (MateMixerBackend *backend);
    const GList  *(*list_devices)  (MateMixerBackend *backend);
    const GList  *(*list_tracks)   (MateMixerBackend *backend);
};

GType mate_mixer_backend_get_type (void) G_GNUC_CONST;

gboolean        mate_mixer_backend_open          (MateMixerBackend *backend);
void            mate_mixer_backend_close         (MateMixerBackend *backend);
const GList    *mate_mixer_backend_list_devices  (MateMixerBackend *backend);
const GList    *mate_mixer_backend_list_tracks   (MateMixerBackend *backend);

G_END_DECLS

#endif /* MATEMIXER_BACKEND_H */
