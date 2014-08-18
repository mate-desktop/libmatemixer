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

#ifndef PULSE_CONNECTION_H
#define PULSE_CONNECTION_H

#include <glib.h>
#include <glib-object.h>

#include <pulse/pulseaudio.h>
#include <pulse/ext-stream-restore.h>

#include "pulse-enums.h"
#include "pulse-types.h"

G_BEGIN_DECLS

#define PULSE_TYPE_CONNECTION                   \
        (pulse_connection_get_type ())
#define PULSE_CONNECTION(o)                     \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), PULSE_TYPE_CONNECTION, PulseConnection))
#define PULSE_IS_CONNECTION(o)                  \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PULSE_TYPE_CONNECTION))
#define PULSE_CONNECTION_CLASS(k)               \
        (G_TYPE_CHECK_CLASS_CAST ((k), PULSE_TYPE_CONNECTION, PulseConnectionClass))
#define PULSE_IS_CONNECTION_CLASS(k)            \
        (G_TYPE_CHECK_CLASS_TYPE ((k), PULSE_TYPE_CONNECTION))
#define PULSE_CONNECTION_GET_CLASS(o)           \
        (G_TYPE_INSTANCE_GET_CLASS ((o), PULSE_TYPE_CONNECTION, PulseConnectionClass))

typedef struct _PulseConnectionClass    PulseConnectionClass;
typedef struct _PulseConnectionPrivate  PulseConnectionPrivate;

struct _PulseConnection
{
    GObject parent;

    /*< private >*/
    PulseConnectionPrivate *priv;
};

struct _PulseConnectionClass
{
    GObjectClass parent_class;

    /*< private >*/
    void (*server_info)           (PulseConnection                  *connection,
                                   const pa_server_info             *info);

    void (*card_info)             (PulseConnection                  *connection,
                                   const pa_card_info               *info);
    void (*card_removed)          (PulseConnection                  *connection,
                                   guint32                           index);

    void (*sink_info)             (PulseConnection                  *connection,
                                   const pa_sink_info               *info);
    void (*sink_removed)          (PulseConnection                  *connection,
                                   guint32                           index);

    void (*sink_input_info)       (PulseConnection                  *connection,
                                   const pa_sink_input_info         *info);
    void (*sink_input_removed)    (PulseConnection                  *connection,
                                   guint32                           index);

    void (*source_info)           (PulseConnection                  *connection,
                                   const pa_source_info             *info);
    void (*source_removed)        (PulseConnection                  *connection,
                                   guint32                           index);

    void (*source_output_info)    (PulseConnection                  *connection,
                                   const pa_source_output_info      *info);
    void (*source_output_removed) (PulseConnection                  *connection,
                                   guint32                           index);

    void (*ext_stream_loading)    (PulseConnection                  *connection);
    void (*ext_stream_loaded)     (PulseConnection                  *connection);
    void (*ext_stream_info)       (PulseConnection                  *connection,
                                   const pa_ext_stream_restore_info *info);
};

GType                pulse_connection_get_type                 (void) G_GNUC_CONST;

PulseConnection *    pulse_connection_new                      (const gchar                      *app_name,
                                                                const gchar                      *app_id,
                                                                const gchar                      *app_version,
                                                                const gchar                      *app_icon,
                                                                const gchar                      *server_address);

gboolean             pulse_connection_connect                  (PulseConnection                  *connection,
                                                                gboolean                          wait_for_daemon);
void                 pulse_connection_disconnect               (PulseConnection                  *connection);

PulseConnectionState pulse_connection_get_state                (PulseConnection                  *connection);

gboolean             pulse_connection_load_server_info         (PulseConnection                  *connection);

gboolean             pulse_connection_load_card_info           (PulseConnection                  *connection,
                                                                guint32                           index);
gboolean             pulse_connection_load_card_info_name      (PulseConnection                  *connection,
                                                                const gchar                      *name);

gboolean             pulse_connection_load_sink_info           (PulseConnection                  *connection,
                                                                guint32                           index);
gboolean             pulse_connection_load_sink_info_name      (PulseConnection                  *connection,
                                                                const gchar                      *name);

gboolean             pulse_connection_load_sink_input_info     (PulseConnection                  *connection,
                                                                guint32                           index);

gboolean             pulse_connection_load_source_info         (PulseConnection                  *connection,
                                                                guint32                           index);
gboolean             pulse_connection_load_source_info_name    (PulseConnection                  *connection,
                                                                const gchar                      *name);

gboolean             pulse_connection_load_source_output_info  (PulseConnection                  *connection,
                                                                guint32                           index);

gboolean             pulse_connection_load_ext_stream_info     (PulseConnection                  *connection);

PulseMonitor *       pulse_connection_create_monitor           (PulseConnection                  *connection,
                                                                guint32                           index_source,
                                                                guint32                           index_sink_input);

gboolean             pulse_connection_set_default_sink         (PulseConnection                  *connection,
                                                                const gchar                      *name);
gboolean             pulse_connection_set_default_source       (PulseConnection                  *connection,
                                                                const gchar                      *name);

gboolean             pulse_connection_set_card_profile         (PulseConnection                  *connection,
                                                                const gchar                      *device,
                                                                const gchar                      *profile);

gboolean             pulse_connection_set_sink_mute            (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                gboolean                          mute);
gboolean             pulse_connection_set_sink_volume          (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                const pa_cvolume                 *volume);
gboolean             pulse_connection_set_sink_port            (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                const gchar                      *port);

gboolean             pulse_connection_set_sink_input_mute      (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                gboolean                          mute);
gboolean             pulse_connection_set_sink_input_volume    (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                const pa_cvolume                 *volume);

gboolean             pulse_connection_set_source_mute          (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                gboolean                          mute);
gboolean             pulse_connection_set_source_volume        (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                const pa_cvolume                 *volume);
gboolean             pulse_connection_set_source_port          (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                const gchar                      *port);

gboolean             pulse_connection_set_source_output_mute   (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                gboolean                          mute);
gboolean             pulse_connection_set_source_output_volume (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                const pa_cvolume                 *volume);

gboolean             pulse_connection_suspend_sink             (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                gboolean                          suspend);
gboolean             pulse_connection_suspend_source           (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                gboolean                          suspend);

gboolean             pulse_connection_move_sink_input          (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                guint32                           sink_index);
gboolean             pulse_connection_move_source_output       (PulseConnection                  *connection,
                                                                guint32                           index,
                                                                guint32                           source_index);

gboolean             pulse_connection_kill_sink_input          (PulseConnection                  *connection,
                                                                guint32                           index);
gboolean             pulse_connection_kill_source_output       (PulseConnection                  *connection,
                                                                guint32                           index);

gboolean             pulse_connection_write_ext_stream         (PulseConnection                  *connection,
                                                                const pa_ext_stream_restore_info *info);
gboolean             pulse_connection_delete_ext_stream        (PulseConnection                  *connection,
                                                                const gchar                      *name);

G_END_DECLS

#endif /* PULSE_CONNECTION_H */
