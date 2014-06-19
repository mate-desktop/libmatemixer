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

#ifndef MATEMIXER_CONTROL_H
#define MATEMIXER_CONTROL_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-device.h>
#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-stream.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_CONTROL                 \
        (mate_mixer_control_get_type ())
#define MATE_MIXER_CONTROL(o)                   \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_CONTROL, MateMixerControl))
#define MATE_MIXER_IS_CONTROL(o)                \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_CONTROL))
#define MATE_MIXER_CONTROL_CLASS(k)             \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_CONTROL, MateMixerControlClass))
#define MATE_MIXER_IS_CONTROL_CLASS(k)          \
        (G_TYPE_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_CONTROL))
#define MATE_MIXER_CONTROL_GET_CLASS(o)         \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_CONTROL, MateMixerControlClass))

typedef struct _MateMixerControl         MateMixerControl;
typedef struct _MateMixerControlClass    MateMixerControlClass;
typedef struct _MateMixerControlPrivate  MateMixerControlPrivate;

/**
 * MateMixerControl:
 *
 * The #MateMixerControl structure contains only private data and should only
 * be accessed using the provided API.
 */
struct _MateMixerControl
{
    GObject parent;

    /*< private >*/
    MateMixerControlPrivate *priv;
};

/**
 * MateMixerControlClass:
 *
 * The class structure of #MateMixerControl.
 */
struct _MateMixerControlClass
{
    GObjectClass parent_class;

    /*< private >*/
    void (*device_added)        (MateMixerControl *control,
                                 const gchar      *name);
    void (*device_changed)      (MateMixerControl *control,
                                 const gchar      *name);
    void (*device_removed)      (MateMixerControl *control,
                                 const gchar      *name);
    void (*stream_added)        (MateMixerControl *control,
                                 const gchar      *name);
    void (*stream_changed)      (MateMixerControl *control,
                                 const gchar      *name);
    void (*stream_removed)      (MateMixerControl *control,
                                 const gchar      *name);
};

GType                 mate_mixer_control_get_type                  (void) G_GNUC_CONST;
MateMixerControl *    mate_mixer_control_new                       (void);

gboolean              mate_mixer_control_set_backend_type          (MateMixerControl     *control,
                                                                    MateMixerBackendType  backend_type);
gboolean              mate_mixer_control_set_app_name              (MateMixerControl     *control,
                                                                    const gchar          *app_name);
gboolean              mate_mixer_control_set_app_id                (MateMixerControl     *control,
                                                                    const gchar          *app_id);
gboolean              mate_mixer_control_set_app_version           (MateMixerControl     *control,
                                                                    const gchar          *app_version);
gboolean              mate_mixer_control_set_app_icon              (MateMixerControl     *control,
                                                                    const gchar          *app_icon);
gboolean              mate_mixer_control_set_server_address        (MateMixerControl     *control,
                                                                    const gchar          *address);
gboolean              mate_mixer_control_open                      (MateMixerControl     *control);
void                  mate_mixer_control_close                     (MateMixerControl     *control);

MateMixerState        mate_mixer_control_get_state                 (MateMixerControl     *control);

MateMixerDevice *     mate_mixer_control_get_device                (MateMixerControl     *control,
                                                                    const gchar          *name);
MateMixerStream *     mate_mixer_control_get_stream                (MateMixerControl     *control,
                                                                    const gchar          *name);

const GList *         mate_mixer_control_list_devices              (MateMixerControl     *control);
const GList *         mate_mixer_control_list_streams              (MateMixerControl     *control);

MateMixerStream  *    mate_mixer_control_get_default_input_stream  (MateMixerControl     *control);
gboolean              mate_mixer_control_set_default_input_stream  (MateMixerControl     *control,
                                                                    MateMixerStream      *stream);

MateMixerStream  *    mate_mixer_control_get_default_output_stream (MateMixerControl     *control);
gboolean              mate_mixer_control_set_default_output_stream (MateMixerControl     *control,
                                                                    MateMixerStream      *stream);

const gchar *         mate_mixer_control_get_backend_name          (MateMixerControl     *control);
MateMixerBackendType  mate_mixer_control_get_backend_type          (MateMixerControl     *control);

G_END_DECLS

#endif /* MATEMIXER_CONTROL_H */
