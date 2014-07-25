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

#ifndef MATEMIXER_STREAM_H
#define MATEMIXER_STREAM_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-device.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-port.h>
#include <libmatemixer/matemixer-stream-control.h>

G_BEGIN_DECLS

#ifdef INFINITY
#define MATE_MIXER_INFINITY INFINITY
#else
#define MATE_MIXER_INFINITY G_MAXDOUBLE
#endif

#define MATE_MIXER_TYPE_STREAM                  \
        (mate_mixer_stream_get_type ())
#define MATE_MIXER_STREAM(o)                    \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_STREAM, MateMixerStream))
#define MATE_MIXER_IS_STREAM(o)                 \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_STREAM))
#define MATE_MIXER_STREAM_GET_INTERFACE(o)      \
        (G_TYPE_INSTANCE_GET_INTERFACE ((o), MATE_MIXER_TYPE_STREAM, MateMixerStreamInterface))

typedef struct _MateMixerStream           MateMixerStream; /* dummy object */
typedef struct _MateMixerStreamInterface  MateMixerStreamInterface;

struct _MateMixerStreamInterface
{
    GTypeInterface parent_iface;

    /*< private >*/
    const gchar *           (*get_name)            (MateMixerStream *stream);
    const gchar *           (*get_description)     (MateMixerStream *stream);

    MateMixerStreamControl *(*get_control)         (MateMixerStream *stream,
                                                    const gchar     *name);
    MateMixerStreamControl *(*get_default_control) (MateMixerStream *stream);

    MateMixerPort *         (*get_port)            (MateMixerStream *stream,
                                                    const gchar     *name);

    gboolean                (*set_active_port)     (MateMixerStream *stream,
                                                    MateMixerPort   *port);

    const GList *           (*list_controls)       (MateMixerStream *stream);
    const GList *           (*list_ports)          (MateMixerStream *stream);

    gboolean                (*suspend)             (MateMixerStream *stream);
    gboolean                (*resume)              (MateMixerStream *stream);

    gboolean                (*monitor_set_enabled) (MateMixerStream *stream,
                                                    gboolean         enabled);

    const gchar *           (*monitor_get_name)    (MateMixerStream *stream);
    gboolean                (*monitor_set_name)    (MateMixerStream *stream,
                                                    const gchar     *name);

    /* Signals */
    void                    (*monitor_value)       (MateMixerStream *stream,
                                                    gdouble          value);
};

GType                   mate_mixer_stream_get_type            (void) G_GNUC_CONST;

const gchar *           mate_mixer_stream_get_name            (MateMixerStream *stream);
const gchar *           mate_mixer_stream_get_description     (MateMixerStream *stream);
MateMixerDevice *       mate_mixer_stream_get_device          (MateMixerStream *stream);
MateMixerStreamFlags    mate_mixer_stream_get_flags           (MateMixerStream *stream);
MateMixerStreamState    mate_mixer_stream_get_state           (MateMixerStream *stream);

MateMixerStreamControl *mate_mixer_stream_get_control         (MateMixerStream *stream,
                                                               const gchar     *name);
MateMixerStreamControl *mate_mixer_stream_get_default_control (MateMixerStream *stream);

MateMixerPort *         mate_mixer_stream_get_port            (MateMixerStream *stream,
                                                               const gchar     *name);

MateMixerPort *         mate_mixer_stream_get_active_port     (MateMixerStream *stream);
gboolean                mate_mixer_stream_set_active_port     (MateMixerStream *stream,
                                                               MateMixerPort   *port);

const GList *           mate_mixer_stream_list_controls       (MateMixerStream *stream);
const GList *           mate_mixer_stream_list_ports          (MateMixerStream *stream);

gboolean                mate_mixer_stream_suspend             (MateMixerStream *stream);
gboolean                mate_mixer_stream_resume              (MateMixerStream *stream);

gboolean                mate_mixer_stream_monitor_get_enabled (MateMixerStream *stream);
gboolean                mate_mixer_stream_monitor_set_enabled (MateMixerStream *stream,
                                                               gboolean         enabled);

const gchar *           mate_mixer_stream_monitor_get_name    (MateMixerStream *stream);
gboolean                mate_mixer_stream_monitor_set_name    (MateMixerStream *stream,
                                                               const gchar     *name);

G_END_DECLS

#endif /* MATEMIXER_STREAM_H */
