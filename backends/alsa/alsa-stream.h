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

#ifndef ALSA_STREAM_H
#define ALSA_STREAM_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

#include "alsa-types.h"

G_BEGIN_DECLS

#define ALSA_TYPE_STREAM                        \
        (alsa_stream_get_type ())
#define ALSA_STREAM(o)                          \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), ALSA_TYPE_STREAM, AlsaStream))
#define ALSA_IS_STREAM(o)                       \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ALSA_TYPE_STREAM))
#define ALSA_STREAM_CLASS(k)                    \
        (G_TYPE_CHECK_CLASS_CAST ((k), ALSA_TYPE_STREAM, AlsaStreamClass))
#define ALSA_IS_STREAM_CLASS(k)                 \
        (G_TYPE_CHECK_CLASS_TYPE ((k), ALSA_TYPE_STREAM))
#define ALSA_STREAM_GET_CLASS(o)                \
        (G_TYPE_INSTANCE_GET_CLASS ((o), ALSA_TYPE_STREAM, AlsaStreamClass))

typedef struct _AlsaStreamClass    AlsaStreamClass;
typedef struct _AlsaStreamPrivate  AlsaStreamPrivate;

struct _AlsaStream
{
    MateMixerStream parent;

    /*< private >*/
    AlsaStreamPrivate *priv;
};

struct _AlsaStreamClass
{
    MateMixerStreamClass parent_class;
};

GType              alsa_stream_get_type                 (void) G_GNUC_CONST;

AlsaStream *       alsa_stream_new                      (const gchar       *name,
                                                         MateMixerDevice   *device,
                                                         MateMixerDirection direction);

void               alsa_stream_add_control              (AlsaStream        *stream,
                                                         AlsaStreamControl *control);
void               alsa_stream_add_switch               (AlsaStream        *stream,
                                                         AlsaSwitch        *swtch);
void               alsa_stream_add_toggle               (AlsaStream        *stream,
                                                         AlsaToggle        *toggle);

gboolean           alsa_stream_has_controls             (AlsaStream        *stream);
gboolean           alsa_stream_has_switches             (AlsaStream        *stream);
gboolean           alsa_stream_has_controls_or_switches (AlsaStream        *stream);
gboolean           alsa_stream_has_default_control      (AlsaStream        *stream);

AlsaStreamControl *alsa_stream_get_default_control      (AlsaStream        *stream);
void               alsa_stream_set_default_control      (AlsaStream        *stream,
                                                         AlsaStreamControl *control);

void               alsa_stream_load_elements            (AlsaStream        *stream,
                                                         const gchar       *name);

gboolean           alsa_stream_remove_elements          (AlsaStream        *stream,
                                                         const gchar       *name);

void               alsa_stream_remove_all               (AlsaStream        *stream);

G_END_DECLS

#endif /* ALSA_STREAM_H */
