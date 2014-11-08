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

#ifndef ALSA_ELEMENT_H
#define ALSA_ELEMENT_H

#include <glib.h>
#include <glib-object.h>
#include <alsa/asoundlib.h>

#include "alsa-types.h"

G_BEGIN_DECLS

#define ALSA_TYPE_ELEMENT                       \
        (alsa_element_get_type ())
#define ALSA_ELEMENT(o)                         \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), ALSA_TYPE_ELEMENT, AlsaElement))
#define ALSA_IS_ELEMENT(o)                      \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ALSA_TYPE_ELEMENT))
#define ALSA_ELEMENT_GET_INTERFACE(o)           \
        (G_TYPE_INSTANCE_GET_INTERFACE ((o), ALSA_TYPE_ELEMENT, AlsaElementInterface))

typedef struct _AlsaElementInterface  AlsaElementInterface;

struct _AlsaElementInterface
{
    GTypeInterface parent_iface;

    /*< private >*/
    snd_mixer_elem_t *(*get_snd_element) (AlsaElement      *element);
    void              (*set_snd_element) (AlsaElement      *element,
                                          snd_mixer_elem_t *el);

    gboolean          (*load)            (AlsaElement      *element);
    void              (*close)           (AlsaElement      *element);
};

GType             alsa_element_get_type        (void) G_GNUC_CONST;

snd_mixer_elem_t *alsa_element_get_snd_element (AlsaElement      *element);
void              alsa_element_set_snd_element (AlsaElement      *element,
                                                snd_mixer_elem_t *el);

gboolean          alsa_element_load            (AlsaElement      *element);

void              alsa_element_close           (AlsaElement      *element);

G_END_DECLS

#endif /* ALSA_ELEMENT_H */
