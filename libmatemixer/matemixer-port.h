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

#ifndef MATEMIXER_PORT_H
#define MATEMIXER_PORT_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-enums.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_PORT                    \
        (mate_mixer_port_get_type ())
#define MATE_MIXER_PORT(o)                      \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_PORT, MateMixerPort))
#define MATE_MIXER_IS_PORT(o)                   \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_PORT))
#define MATE_MIXER_PORT_CLASS(k)                \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_PORT, MateMixerPortClass))
#define MATE_MIXER_IS_PORT_CLASS(k)             \
        (G_TYPE_CLASS_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_PORT))
#define MATE_MIXER_PORT_GET_CLASS(o)            \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_PORT, MateMixerPortClass))

typedef struct _MateMixerPort         MateMixerPort;
typedef struct _MateMixerPortClass    MateMixerPortClass;
typedef struct _MateMixerPortPrivate  MateMixerPortPrivate;

struct _MateMixerPort
{
    /*< private >*/
    GObject                 parent;
    MateMixerPortPrivate   *priv;
};

struct _MateMixerPortClass
{
    /*< private >*/
    GObjectClass            parent;
};

GType               mate_mixer_port_get_type        (void) G_GNUC_CONST;
MateMixerPort *     mate_mixer_port_new             (const gchar         *name,
                                                     const gchar         *description,
                                                     const gchar         *icon,
                                                     gulong               priority,
                                                     MateMixerPortStatus  status);

const gchar *       mate_mixer_port_get_name        (MateMixerPort *port);
const gchar *       mate_mixer_port_get_description (MateMixerPort *port);
const gchar *       mate_mixer_port_get_icon        (MateMixerPort *port);
gulong              mate_mixer_port_get_priority    (MateMixerPort *port);
MateMixerPortStatus mate_mixer_port_get_status      (MateMixerPort *port);

G_END_DECLS

#endif /* MATEMIXER_PORT_H */
