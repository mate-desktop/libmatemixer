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

#ifndef MATEMIXER_DEVICE_PROFILE_H
#define MATEMIXER_DEVICE_PROFILE_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_DEVICE_PROFILE              \
        (mate_mixer_device_profile_get_type ())
#define MATE_MIXER_DEVICE_PROFILE(o)                \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_DEVICE_PROFILE, MateMixerDeviceProfile))
#define MATE_MIXER_IS_DEVICE_PROFILE(o)             \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_DEVICE_PROFILE))
#define MATE_MIXER_DEVICE_PROFILE_CLASS(k)          \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_DEVICE_PROFILE, MateMixerDeviceProfileClass))
#define MATE_MIXER_IS_DEVICE_PROFILE_CLASS(k)       \
        (G_TYPE_CLASS_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_DEVICE_PROFILE))
#define MATE_MIXER_DEVICE_PROFILE_GET_CLASS(o)      \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_DEVICE_PROFILE, MateMixerDeviceProfileClass))

typedef struct _MateMixerDeviceProfile         MateMixerDeviceProfile;
typedef struct _MateMixerDeviceProfileClass    MateMixerDeviceProfileClass;
typedef struct _MateMixerDeviceProfilePrivate  MateMixerDeviceProfilePrivate;

struct _MateMixerDeviceProfile
{
    GObject parent;

    MateMixerDeviceProfilePrivate *priv;
};

struct _MateMixerDeviceProfileClass
{
    GObjectClass parent;    
};

GType mate_mixer_device_profile_get_type (void) G_GNUC_CONST;

MateMixerDeviceProfile *mate_mixer_device_profile_new (const gchar  *identifier,
                                                       const gchar  *name,
                                                       guint32       priority);

const gchar            *mate_mixer_device_profile_get_identifier (MateMixerDeviceProfile *profile);
const gchar            *mate_mixer_device_profile_get_name       (MateMixerDeviceProfile *profile);
guint32                 mate_mixer_device_profile_get_priority   (MateMixerDeviceProfile *profile);

G_END_DECLS

#endif /* MATEMIXER_PROFILE_H */
