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

#ifndef MATEMIXER_BACKEND_H
#define MATEMIXER_BACKEND_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-types.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_BACKEND                 \
        (mate_mixer_backend_get_type ())
#define MATE_MIXER_BACKEND(o)                   \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_BACKEND, MateMixerBackend))
#define MATE_MIXER_IS_BACKEND(o)                \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_BACKEND))
#define MATE_MIXER_BACKEND_CLASS(k)             \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_BACKEND, MateMixerBackendClass))
#define MATE_MIXER_IS_BACKEND_CLASS(k)          \
        (G_TYPE_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_BACKEND))
#define MATE_MIXER_BACKEND_GET_CLASS(o)         \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_BACKEND, MateMixerBackendClass))

typedef struct _MateMixerBackend         MateMixerBackend;
typedef struct _MateMixerBackendClass    MateMixerBackendClass;
typedef struct _MateMixerBackendData     MateMixerBackendData;
typedef struct _MateMixerBackendPrivate  MateMixerBackendPrivate;

struct _MateMixerBackend
{
    GObject object;

    /*< private >*/
    MateMixerBackendPrivate *priv;
};

struct _MateMixerBackendClass
{
    GObjectClass parent_class;

    /*< private >*/
    void      (*set_data)                  (MateMixerBackend     *backend,
                                            MateMixerBackendData *data);

    gboolean  (*open)                      (MateMixerBackend     *backend);
    void      (*close)                     (MateMixerBackend     *backend);

    GList    *(*list_devices)              (MateMixerBackend     *backend);
    GList    *(*list_streams)              (MateMixerBackend     *backend);
    GList    *(*list_stored_streams)       (MateMixerBackend     *backend);

    gboolean  (*set_default_input_stream)  (MateMixerBackend     *backend,
                                            MateMixerStream      *stream);
    gboolean  (*set_default_output_stream) (MateMixerBackend     *backend,
                                            MateMixerStream      *stream);

    /* Signals */
    void      (*device_added)              (MateMixerBackend     *backend,
                                            const gchar          *name);
    void      (*device_removed)            (MateMixerBackend     *backend,
                                            const gchar          *name);
    void      (*stream_added)              (MateMixerBackend     *backend,
                                            const gchar          *name);
    void      (*stream_removed)            (MateMixerBackend     *backend,
                                            const gchar          *name);
    void      (*stored_stream_added)       (MateMixerBackend     *backend,
                                            const gchar          *name);
    void      (*stored_stream_removed)     (MateMixerBackend     *backend,
                                            const gchar          *name);
};

struct _MateMixerBackendData
{
    gchar *app_name;
    gchar *app_id;
    gchar *app_version;
    gchar *app_icon;
    gchar *server_address;
};

GType                 mate_mixer_backend_get_type                  (void) G_GNUC_CONST;

void                  mate_mixer_backend_set_data                  (MateMixerBackend     *backend,
                                                                    MateMixerBackendData *data);

gboolean              mate_mixer_backend_open                      (MateMixerBackend     *backend);
void                  mate_mixer_backend_close                     (MateMixerBackend     *backend);

MateMixerState        mate_mixer_backend_get_state                 (MateMixerBackend     *backend);
MateMixerBackendFlags mate_mixer_backend_get_flags                 (MateMixerBackend     *backend);

const GList *         mate_mixer_backend_list_devices              (MateMixerBackend     *backend);
const GList *         mate_mixer_backend_list_streams              (MateMixerBackend     *backend);
const GList *         mate_mixer_backend_list_stored_streams       (MateMixerBackend     *backend);

MateMixerStream *     mate_mixer_backend_get_default_input_stream  (MateMixerBackend     *backend);
gboolean              mate_mixer_backend_set_default_input_stream  (MateMixerBackend     *backend,
                                                                    MateMixerStream      *stream);

MateMixerStream *     mate_mixer_backend_get_default_output_stream (MateMixerBackend     *backend);
gboolean              mate_mixer_backend_set_default_output_stream (MateMixerBackend     *backend,
                                                                    MateMixerStream      *stream);

G_END_DECLS

#endif /* MATEMIXER_BACKEND_H */
