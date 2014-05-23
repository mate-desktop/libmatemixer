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

#ifndef MATEMIXER_DEVICE_H
#define MATEMIXER_DEVICE_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_DEVICE                  \
        (mate_mixer_device_get_type ())
#define MATE_MIXER_DEVICE(o)                    \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_DEVICE, MateMixerDevice))
#define MATE_MIXER_IS_DEVICE(o)                 \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_DEVICE))
#define MATE_MIXER_DEVICE_GET_INTERFACE(o)      \
        (G_TYPE_INSTANCE_GET_INTERFACE ((o), MATE_MIXER_TYPE_DEVICE, MateMixerDeviceInterface))

typedef struct _MateMixerDevice           MateMixerDevice; /* dummy object */
typedef struct _MateMixerDeviceInterface  MateMixerDeviceInterface;

struct _MateMixerDeviceInterface
{
    GTypeInterface parent;

    const GList *(*list_tracks) (MateMixerDevice *device);
};

GType mate_mixer_device_get_type (void) G_GNUC_CONST;

const GList *mate_mixer_device_list_tracks (MateMixerDevice *device);

G_END_DECLS

#endif /* MATEMIXER_DEVICE_H */
