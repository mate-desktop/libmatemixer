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

#ifndef MATEMIXER_PULSE_CONNECTION_H
#define MATEMIXER_PULSE_CONNECTION_H

#include <glib.h>
#include <glib-object.h>

#include <pulse/pulseaudio.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_PULSE_CONNECTION            \
        (mate_mixer_pulse_connection_get_type ())
#define MATE_MIXER_PULSE_CONNECTION(o)              \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_PULSE_CONNECTION, MateMixerPulseConnection))
#define MATE_MIXER_IS_PULSE_CONNECTION(o)           \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_PULSE_CONNECTION))
#define MATE_MIXER_PULSE_CONNECTION_CLASS(k)        \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_PULSE_CONNECTION, MateMixerPulseConnectionClass))
#define MATE_MIXER_IS_PULSE_CONNECTION_CLASS(k)     \
        (G_TYPE_CLASS_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_PULSE_CONNECTION))
#define MATE_MIXER_PULSE_CONNECTION_GET_CLASS(o)    \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_PULSE_CONNECTION, MateMixerPulseConnectionClass))

typedef struct _MateMixerPulseConnection         MateMixerPulseConnection;
typedef struct _MateMixerPulseConnectionClass    MateMixerPulseConnectionClass;
typedef struct _MateMixerPulseConnectionPrivate  MateMixerPulseConnectionPrivate;

struct _MateMixerPulseConnection
{
    GObject parent;

    MateMixerPulseConnectionPrivate *priv;
};

struct _MateMixerPulseConnectionClass
{
    GObjectClass parent;

    void (*disconnected) (MateMixerPulseConnection *connection);
    void (*reconnected)  (MateMixerPulseConnection *connection);

    void (*list_item_card) (MateMixerPulseConnection *connection,
                            const pa_card_info *info);
    void (*list_item_sink) (MateMixerPulseConnection *connection,
                            const pa_sink_info *info);
    void (*list_item_sink_input) (MateMixerPulseConnection *connection,
                            const pa_sink_input_info *info);
    void (*list_item_source) (MateMixerPulseConnection *connection,
                            const pa_source_info *info);
    void (*list_item_source_output) (MateMixerPulseConnection *connection,
                            const pa_source_output_info *info);
};

GType mate_mixer_pulse_connection_get_type (void) G_GNUC_CONST;

MateMixerPulseConnection *mate_mixer_pulse_connection_new (const gchar *server,
                                                           const gchar *app_name);

gboolean mate_mixer_pulse_connection_connect (MateMixerPulseConnection *connection);

void mate_mixer_pulse_connection_disconnect (MateMixerPulseConnection *connection);

gboolean mate_mixer_pulse_connection_get_server_info (MateMixerPulseConnection *connection);

gboolean mate_mixer_pulse_connection_get_card_list (MateMixerPulseConnection *connection);

gboolean mate_mixer_pulse_connection_get_sink_list (MateMixerPulseConnection *connection);

gboolean mate_mixer_pulse_connection_get_sink_input_list (MateMixerPulseConnection *connection);

gboolean mate_mixer_pulse_connection_get_source_list (MateMixerPulseConnection *connection);

gboolean mate_mixer_pulse_connection_get_source_output_list (MateMixerPulseConnection *connection);

gboolean mate_mixer_pulse_connection_set_card_profile (MateMixerPulseConnection *connection,
                                                       const gchar              *device,
                                                       const gchar              *profile);

gboolean mate_mixer_pulse_connection_set_sink_mute (MateMixerPulseConnection *connection,
                                                    guint32 index,
                                                    gboolean mute);

G_END_DECLS

#endif /* MATEMIXER_PULSE_CONNECTION_H */
