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

G_BEGIN_DECLS

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
    GTypeInterface parent;

    const gchar *            (*get_name)               (MateMixerStream          *stream);
    const gchar *            (*get_description)        (MateMixerStream          *stream);
    const gchar *            (*get_icon)               (MateMixerStream          *stream);
    MateMixerDevice *        (*get_device)             (MateMixerStream          *stream);
    MateMixerStreamStatus    (*get_status)             (MateMixerStream          *stream);
    gboolean                 (*get_mute)               (MateMixerStream          *stream);
    gboolean                 (*set_mute)               (MateMixerStream          *stream,
                                                        gboolean                  mute);
    guint32                  (*get_volume)             (MateMixerStream          *stream);
    gboolean                 (*set_volume)             (MateMixerStream          *stream,
                                                        guint32                   volume);
    gdouble                  (*get_volume_db)          (MateMixerStream          *stream);
    gboolean                 (*set_volume_db)          (MateMixerStream          *stream,
                                                        gdouble                   volume_db);
    guint8                   (*get_num_channels)       (MateMixerStream          *stream);
    MateMixerChannelPosition (*get_channel_position)   (MateMixerStream          *stream,
                                                        guint8                    channel);
    guint32                  (*get_channel_volume)     (MateMixerStream          *stream,
                                                        guint8                    channel);
    gboolean                 (*set_channel_volume)     (MateMixerStream          *stream,
                                                        guint8                    channel,
                                                        guint32                   volume);
    gdouble                  (*get_channel_volume_db)  (MateMixerStream          *stream,
                                                        guint8                    channel);
    gboolean                 (*set_channel_volume_db)  (MateMixerStream          *stream,
                                                        guint8                    channel,
                                                        gdouble                   volume_db);
    gboolean                 (*has_position)           (MateMixerStream          *stream,
                                                        MateMixerChannelPosition  position);
    guint32                  (*get_position_volume)    (MateMixerStream          *stream,
                                                        MateMixerChannelPosition  position);
    gboolean                 (*set_position_volume)    (MateMixerStream          *stream,
                                                        MateMixerChannelPosition  position,
                                                        guint32                   volume);
    gdouble                  (*get_position_volume_db) (MateMixerStream          *stream,
                                                        MateMixerChannelPosition  position);
    gboolean                 (*set_position_volume_db) (MateMixerStream          *stream,
                                                        MateMixerChannelPosition  position,
                                                        gdouble                   volume);
    gboolean                 (*get_balance)            (MateMixerStream          *stream);
    gboolean                 (*set_balance)            (MateMixerStream          *stream,
                                                        gdouble                   balance);
    gboolean                 (*get_fade)               (MateMixerStream          *stream);
    gboolean                 (*set_fade)               (MateMixerStream          *stream,
                                                        gdouble                   fade);
    MateMixerPort *          (*get_active_port)        (MateMixerStream          *stream);
    gboolean                 (*set_active_port)        (MateMixerStream          *stream,
                                                        MateMixerPort            *port);
    const GList *            (*list_ports)             (MateMixerStream          *stream);
};

GType mate_mixer_stream_get_type (void) G_GNUC_CONST;

const gchar *            mate_mixer_stream_get_name               (MateMixerStream          *stream);
const gchar *            mate_mixer_stream_get_description        (MateMixerStream          *stream);
const gchar *            mate_mixer_stream_get_icon               (MateMixerStream          *stream);
MateMixerDevice *        mate_mixer_stream_get_device             (MateMixerStream          *stream);
MateMixerStreamStatus    mate_mixer_stream_get_status             (MateMixerStream          *stream);

gboolean                 mate_mixer_stream_get_mute               (MateMixerStream          *stream);
gboolean                 mate_mixer_stream_set_mute               (MateMixerStream          *stream,
                                                                   gboolean                  mute);

guint32                  mate_mixer_stream_get_volume             (MateMixerStream          *stream);
gboolean                 mate_mixer_stream_set_volume             (MateMixerStream          *stream,
                                                                   guint32                   volume);

gdouble                  mate_mixer_stream_get_volume_db          (MateMixerStream          *stream);
gboolean                 mate_mixer_stream_set_volume_db          (MateMixerStream          *stream,
                                                                   gdouble                   volume_db);

guint8                   mate_mixer_stream_get_num_channels       (MateMixerStream          *stream);

MateMixerChannelPosition mate_mixer_stream_get_channel_position   (MateMixerStream          *stream,
                                                                   guint8                    channel);

guint32                  mate_mixer_stream_get_channel_volume     (MateMixerStream          *stream,
                                                                   guint8                    channel);
gboolean                 mate_mixer_stream_set_channel_volume     (MateMixerStream          *stream,
                                                                   guint8                    channel,
                                                                   guint32                   volume);

gdouble                  mate_mixer_stream_get_channel_volume_db  (MateMixerStream          *stream,
                                                                   guint8                    channel);
gboolean                 mate_mixer_stream_set_channel_volume_db  (MateMixerStream          *stream,
                                                                   guint8                    channel,
                                                                   gdouble                   volume_db);

gboolean                 mate_mixer_stream_has_position           (MateMixerStream          *stream,
                                                                   MateMixerChannelPosition  position);

guint32                  mate_mixer_stream_get_position_volume    (MateMixerStream          *stream,
                                                                   MateMixerChannelPosition  position);
gboolean                 mate_mixer_stream_set_position_volume    (MateMixerStream          *stream,
                                                                   MateMixerChannelPosition  position,
                                                                   guint32                   volume);

gdouble                  mate_mixer_stream_get_position_volume_db (MateMixerStream          *stream,
                                                                   MateMixerChannelPosition  position);
gboolean                 mate_mixer_stream_set_position_volume_db (MateMixerStream          *stream,
                                                                   MateMixerChannelPosition  position,
                                                                   gdouble                   volume_db);

gdouble                  mate_mixer_stream_get_balance            (MateMixerStream          *stream);
gboolean                 mate_mixer_stream_set_balance            (MateMixerStream          *stream,
                                                                   gdouble                   balance);

gdouble                  mate_mixer_stream_get_fade               (MateMixerStream          *stream);
gboolean                 mate_mixer_stream_set_fade               (MateMixerStream          *stream,
                                                                   gdouble                   fade);

const GList *            mate_mixer_stream_list_ports             (MateMixerStream          *stream);

MateMixerPort *          mate_mixer_stream_get_active_port        (MateMixerStream          *stream);
gboolean                 mate_mixer_stream_set_active_port        (MateMixerStream          *stream,
                                                                   MateMixerPort            *port);

G_END_DECLS

#endif /* MATEMIXER_STREAM_H */
