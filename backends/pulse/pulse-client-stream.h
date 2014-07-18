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

#ifndef PULSE_CLIENT_STREAM_H
#define PULSE_CLIENT_STREAM_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-client-stream.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-stream.h>

#include "pulse-stream.h"

G_BEGIN_DECLS

#define PULSE_TYPE_CLIENT_STREAM                       \
        (pulse_client_stream_get_type ())
#define PULSE_CLIENT_STREAM(o)                         \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_CLIENT_STREAM, PulseClientStream))
#define PULSE_IS_CLIENT_STREAM(o)                      \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_CLIENT_STREAM))
#define PULSE_CLIENT_STREAM_CLASS(k)                   \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_CLIENT_STREAM, PulseClientStreamClass))
#define PULSE_IS_CLIENT_STREAM_CLASS(k)                \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_CLIENT_STREAM))
#define PULSE_CLIENT_STREAM_GET_CLASS(o)               \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_CLIENT_STREAM, PulseClientStreamClass))

typedef struct _PulseClientStream         PulseClientStream;
typedef struct _PulseClientStreamClass    PulseClientStreamClass;
typedef struct _PulseClientStreamPrivate  PulseClientStreamPrivate;

struct _PulseClientStream
{
    PulseStream parent;

    /*< private >*/
    PulseClientStreamPrivate *priv;
};

struct _PulseClientStreamClass
{
    PulseStreamClass parent_class;

    /*< private >*/
    /* Virtual table */
    gboolean (*set_parent) (PulseClientStream *pclient,
                            PulseStream       *pstream);

    gboolean (*remove)     (PulseClientStream *pclient);

    /* Signals */
    void     (*removed)    (PulseClientStream *pclient);
};

GType    pulse_client_stream_get_type           (void) G_GNUC_CONST;

gboolean pulse_client_stream_update_flags       (PulseClientStream         *pclient,
                                                 MateMixerClientStreamFlags flags);

gboolean pulse_client_stream_update_role        (PulseClientStream         *pclient,
                                                 MateMixerClientStreamRole  role);

gboolean pulse_client_stream_update_parent      (PulseClientStream         *pclient,
                                                 MateMixerStream           *parent);

gboolean pulse_client_stream_update_app_name    (PulseClientStream         *pclient,
                                                 const gchar               *app_name);
gboolean pulse_client_stream_update_app_id      (PulseClientStream         *pclient,
                                                 const gchar               *app_id);
gboolean pulse_client_stream_update_app_version (PulseClientStream         *pclient,
                                                 const gchar               *app_version);
gboolean pulse_client_stream_update_app_icon    (PulseClientStream         *pclient,
                                                 const gchar               *app_icon);

G_END_DECLS

#endif /* PULSE_CLIENT_STREAM_H */
