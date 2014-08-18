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

#include <glib.h>
#include <glib-object.h>
#include <alsa/asoundlib.h>

#include "alsa-element.h"

G_DEFINE_INTERFACE (AlsaElement, alsa_element, G_TYPE_OBJECT)

static void
alsa_element_default_init (AlsaElementInterface *iface)
{
}

snd_mixer_elem_t *
alsa_element_get_snd_element (AlsaElement *element)
{
    g_return_val_if_fail (ALSA_IS_ELEMENT (element), NULL);

    return ALSA_ELEMENT_GET_INTERFACE (element)->get_snd_element (element);
}

void
alsa_element_set_snd_element (AlsaElement *element, snd_mixer_elem_t *el)
{
    g_return_if_fail (ALSA_IS_ELEMENT (element));

    ALSA_ELEMENT_GET_INTERFACE (element)->set_snd_element (element, el);
}

gboolean
alsa_element_load (AlsaElement *element)
{
    g_return_val_if_fail (ALSA_IS_ELEMENT (element), FALSE);

    return ALSA_ELEMENT_GET_INTERFACE (element)->load (element);
}

void
alsa_element_close (AlsaElement *element)
{
    AlsaElementInterface *iface;

    g_return_if_fail (ALSA_IS_ELEMENT (element));

    /* Close the element by unsetting the ALSA element and optionally calling
     * a closing function */
    alsa_element_set_snd_element (element, NULL);

    iface = ALSA_ELEMENT_GET_INTERFACE (element);
    if (iface->close != NULL)
        iface->close (element);
}
