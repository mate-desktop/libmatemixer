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

#ifndef MATEMIXER_CONTEXT_H
#define MATEMIXER_CONTEXT_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-enums.h>
#include <libmatemixer/matemixer-types.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_CONTEXT                 \
        (mate_mixer_context_get_type ())
#define MATE_MIXER_CONTEXT(o)                   \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_CONTEXT, MateMixerContext))
#define MATE_MIXER_IS_CONTEXT(o)                \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_CONTEXT))
#define MATE_MIXER_CONTEXT_CLASS(k)             \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_CONTEXT, MateMixerContextClass))
#define MATE_MIXER_IS_CONTEXT_CLASS(k)          \
        (G_TYPE_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_CONTEXT))
#define MATE_MIXER_CONTEXT_GET_CLASS(o)         \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_CONTEXT, MateMixerContextClass))

typedef struct _MateMixerContextClass    MateMixerContextClass;
typedef struct _MateMixerContextPrivate  MateMixerContextPrivate;

/**
 * MateMixerContext:
 *
 * The #MateMixerContext structure contains only private data and should only
 * be accessed using the provided API.
 */
struct _MateMixerContext
{
    GObject parent;

    /*< private >*/
    MateMixerContextPrivate *priv;
};

/**
 * MateMixerContextClass:
 * @parent_class: The parent class.
 *
 * The class structure for #MateMixerContext.
 */
struct _MateMixerContextClass
{
    GObjectClass parent_class;

    /*< private >*/
    void (*device_added)           (MateMixerContext *context,
                                    const gchar      *name);
    void (*device_removed)         (MateMixerContext *context,
                                    const gchar      *name);
    void (*stream_added)           (MateMixerContext *context,
                                    const gchar      *name);
    void (*stream_removed)         (MateMixerContext *context,
                                    const gchar      *name);
    void (*stored_control_added)   (MateMixerContext *context,
                                    const gchar      *name);
    void (*stored_control_removed) (MateMixerContext *context,
                                    const gchar      *name);
};

GType                   mate_mixer_context_get_type                  (void) G_GNUC_CONST;

MateMixerContext *      mate_mixer_context_new                       (void);

gboolean                mate_mixer_context_set_backend_type          (MateMixerContext     *context,
                                                                      MateMixerBackendType  backend_type);
gboolean                mate_mixer_context_set_app_name              (MateMixerContext     *context,
                                                                      const gchar          *app_name);
gboolean                mate_mixer_context_set_app_id                (MateMixerContext     *context,
                                                                      const gchar          *app_id);
gboolean                mate_mixer_context_set_app_version           (MateMixerContext     *context,
                                                                      const gchar          *app_version);
gboolean                mate_mixer_context_set_app_icon              (MateMixerContext     *context,
                                                                      const gchar          *app_icon);
gboolean                mate_mixer_context_set_server_address        (MateMixerContext     *context,
                                                                      const gchar          *address);

gboolean                mate_mixer_context_open                      (MateMixerContext     *context);
void                    mate_mixer_context_close                     (MateMixerContext     *context);

MateMixerState          mate_mixer_context_get_state                 (MateMixerContext     *context);

MateMixerDevice *       mate_mixer_context_get_device                (MateMixerContext     *context,
                                                                      const gchar          *name);
MateMixerStream *       mate_mixer_context_get_stream                (MateMixerContext     *context,
                                                                      const gchar          *name);
MateMixerStoredControl *mate_mixer_context_get_stored_control        (MateMixerContext     *context,
                                                                      const gchar          *name);

const GList *           mate_mixer_context_list_devices              (MateMixerContext     *context);
const GList *           mate_mixer_context_list_streams              (MateMixerContext     *context);
const GList *           mate_mixer_context_list_stored_controls      (MateMixerContext     *context);

MateMixerStream *       mate_mixer_context_get_default_input_stream  (MateMixerContext     *context);
gboolean                mate_mixer_context_set_default_input_stream  (MateMixerContext     *context,
                                                                      MateMixerStream      *stream);

MateMixerStream *       mate_mixer_context_get_default_output_stream (MateMixerContext     *context);
gboolean                mate_mixer_context_set_default_output_stream (MateMixerContext     *context,
                                                                      MateMixerStream      *stream);

const gchar *           mate_mixer_context_get_backend_name          (MateMixerContext     *context);
MateMixerBackendType    mate_mixer_context_get_backend_type          (MateMixerContext     *context);
MateMixerBackendFlags   mate_mixer_context_get_backend_flags         (MateMixerContext     *context);

G_END_DECLS

#endif /* MATEMIXER_CONTEXT_H */
