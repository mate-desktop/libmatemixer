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

#ifndef MATEMIXER_PULSE_DEVICE_H
#define MATEMIXER_PULSE_DEVICE_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-device.h>
#include <libmatemixer/matemixer-device-profile.h>

#include <pulse/pulseaudio.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_PULSE_DEVICE            \
        (mate_mixer_pulse_device_get_type ())
#define MATE_MIXER_PULSE_DEVICE(o)              \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_PULSE_DEVICE, MateMixerPulseDevice))
#define MATE_MIXER_IS_PULSE_DEVICE(o)           \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_PULSE_DEVICE))
#define MATE_MIXER_PULSE_DEVICE_CLASS(k)        \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_PULSE_DEVICE, MateMixerPulseDeviceClass))
#define MATE_MIXER_IS_PULSE_DEVICE_CLASS(k)     \
        (G_TYPE_CLASS_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_PULSE_DEVICE))
#define MATE_MIXER_PULSE_DEVICE_GET_CLASS(o)    \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_PULSE_DEVICE, MateMixerPulseDeviceClass))

typedef struct _MateMixerPulseDevice         MateMixerPulseDevice;
typedef struct _MateMixerPulseDeviceClass    MateMixerPulseDeviceClass;
typedef struct _MateMixerPulseDevicePrivate  MateMixerPulseDevicePrivate;

struct _MateMixerPulseDevice
{
    GObject parent;

    MateMixerPulseDevicePrivate *priv;
};

struct _MateMixerPulseDeviceClass
{
    GObjectClass parent;    
};

GType mate_mixer_pulse_device_get_type (void) G_GNUC_CONST;

MateMixerPulseDevice    *mate_mixer_pulse_device_new (const pa_card_info *info);
GList                   *mate_mixer_pulse_device_list_tracks (MateMixerPulseDevice *device);

const GList             *mate_mixer_pulse_device_get_ports (MateMixerPulseDevice *device);
const GList             *mate_mixer_pulse_device_get_profiles (MateMixerPulseDevice *device);

MateMixerDeviceProfile  *mate_mixer_pulse_device_get_active_profile (MateMixerPulseDevice *device);

gboolean                 mate_mixer_pulse_device_set_active_profile (MateMixerPulseDevice *device,
                                                                     MateMixerDeviceProfile *profile);

gboolean                 mate_mixer_pulse_device_update (MateMixerPulseDevice *device,
                                                         const pa_card_info *info);

G_END_DECLS

#endif /* MATEMIXER_PULSE_DEVICE_H */
