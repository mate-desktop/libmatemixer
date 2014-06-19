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

#ifndef MATEMIXER_CLIENT_STREAM_H
#define MATEMIXER_CLIENT_STREAM_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-stream.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_CLIENT_STREAM                  \
        (mate_mixer_client_stream_get_type ())
#define MATE_MIXER_CLIENT_STREAM(o)                    \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_CLIENT_STREAM, MateMixerClientStream))
#define MATE_MIXER_IS_CLIENT_STREAM(o)                 \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_CLIENT_STREAM))
#define MATE_MIXER_CLIENT_STREAM_GET_INTERFACE(o)      \
        (G_TYPE_INSTANCE_GET_INTERFACE ((o), MATE_MIXER_TYPE_CLIENT_STREAM, MateMixerClientStreamInterface))

typedef struct _MateMixerClientStream           MateMixerClientStream; /* dummy object */
typedef struct _MateMixerClientStreamInterface  MateMixerClientStreamInterface;

struct _MateMixerClientStreamInterface
{
    GTypeInterface parent_iface;

    /*< private >*/
    MateMixerStream *(*get_parent)      (MateMixerClientStream *client);
    gboolean         (*set_parent)      (MateMixerClientStream *client,
                                         MateMixerStream       *stream);
    gboolean         (*remove)          (MateMixerClientStream *client);
    const gchar     *(*get_app_name)    (MateMixerClientStream *client);
    const gchar     *(*get_app_id)      (MateMixerClientStream *client);
    const gchar     *(*get_app_version) (MateMixerClientStream *client);
    const gchar     *(*get_app_icon)    (MateMixerClientStream *client);
};

GType            mate_mixer_client_stream_get_type        (void) G_GNUC_CONST;
MateMixerStream *mate_mixer_client_stream_get_parent      (MateMixerClientStream *client);
gboolean         mate_mixer_client_stream_set_parent      (MateMixerClientStream *client,
                                                           MateMixerStream       *parent);
gboolean         mate_mixer_client_stream_remove          (MateMixerClientStream *client);

const gchar *    mate_mixer_client_stream_get_app_name    (MateMixerClientStream *client);
const gchar *    mate_mixer_client_stream_get_app_id      (MateMixerClientStream *client);
const gchar *    mate_mixer_client_stream_get_app_version (MateMixerClientStream *client);
const gchar *    mate_mixer_client_stream_get_app_icon    (MateMixerClientStream *client);

G_END_DECLS

#endif /* MATEMIXER_CLIENT_STREAM_H */
