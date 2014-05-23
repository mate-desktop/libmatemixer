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

#ifndef MATEMIXER_PULSE_H
#define MATEMIXER_PULSE_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-backend.h>

#define MATE_MIXER_TYPE_PULSE                   \
        (mate_mixer_pulse_get_type ())
#define MATE_MIXER_PULSE(o)                     \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_PULSE, MateMixerPulse))
#define MATE_MIXER_IS_PULSE(o)                  \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_PULSE))
#define MATE_MIXER_PULSE_CLASS(k)               \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_PULSE, MateMixerPulseClass))
#define MATE_MIXER_IS_PULSE_CLASS(k)            \
        (G_TYPE_CLASS_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_PULSE))
#define MATE_MIXER_PULSE_GET_CLASS(o)           \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_PULSE, MateMixerPulseClass))

typedef struct _MateMixerPulse         MateMixerPulse;
typedef struct _MateMixerPulseClass    MateMixerPulseClass;
typedef struct _MateMixerPulsePrivate  MateMixerPulsePrivate;

struct _MateMixerPulse
{
    GObject parent;

    MateMixerPulsePrivate *priv;
};

struct _MateMixerPulseClass
{
    GObjectClass parent;
};

GType mate_mixer_pulse_get_type (void) G_GNUC_CONST;

gboolean      mate_mixer_pulse_open          (MateMixerBackend *backend);
void          mate_mixer_pulse_close         (MateMixerBackend *backend);
GList        *mate_mixer_pulse_list_devices  (MateMixerBackend *backend);

#endif /* MATEMIXER_PULSE_H */
