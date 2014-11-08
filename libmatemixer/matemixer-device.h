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

#ifndef MATEMIXER_DEVICE_H
#define MATEMIXER_DEVICE_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer-types.h>

G_BEGIN_DECLS

#define MATE_MIXER_TYPE_DEVICE                  \
        (mate_mixer_device_get_type ())
#define MATE_MIXER_DEVICE(o)                    \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), MATE_MIXER_TYPE_DEVICE, MateMixerDevice))
#define MATE_MIXER_IS_DEVICE(o)                 \
        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MATE_MIXER_TYPE_DEVICE))
#define MATE_MIXER_DEVICE_CLASS(k)              \
        (G_TYPE_CHECK_CLASS_CAST ((k), MATE_MIXER_TYPE_DEVICE, MateMixerDeviceClass))
#define MATE_MIXER_IS_DEVICE_CLASS(k)           \
        (G_TYPE_CHECK_CLASS_TYPE ((k), MATE_MIXER_TYPE_DEVICE))
#define MATE_MIXER_DEVICE_GET_CLASS(o)          \
        (G_TYPE_INSTANCE_GET_CLASS ((o), MATE_MIXER_TYPE_DEVICE, MateMixerDeviceClass))

typedef struct _MateMixerDeviceClass    MateMixerDeviceClass;
typedef struct _MateMixerDevicePrivate  MateMixerDevicePrivate;

/**
 * MateMixerDevice:
 *
 * The #MateMixerDevice structure contains only private data and should only
 * be accessed using the provided API.
 */
struct _MateMixerDevice
{
    GObject object;

    /*< private >*/
    MateMixerDevicePrivate *priv;
};

/**
 * MateMixerDeviceClass:
 * @parent_class: The parent class.
 *
 * The class structure for #MateMixerDevice.
 */
struct _MateMixerDeviceClass
{
    GObjectClass parent_class;

    /*< private >*/
    MateMixerStream       *(*get_stream)    (MateMixerDevice *device,
                                             const gchar     *name);
    MateMixerDeviceSwitch *(*get_switch)    (MateMixerDevice *device,
                                             const gchar     *name);

    const GList           *(*list_streams)  (MateMixerDevice *device);
    const GList           *(*list_switches) (MateMixerDevice *device);

    /* Signals */
    void (*stream_added)   (MateMixerDevice *device,
                            const gchar     *name);
    void (*stream_removed) (MateMixerDevice *device,
                            const gchar     *name);
    void (*switch_added)   (MateMixerDevice *device,
                            const gchar     *name);
    void (*switch_removed) (MateMixerDevice *device,
                            const gchar     *name);
};

GType                  mate_mixer_device_get_type      (void) G_GNUC_CONST;

const gchar *          mate_mixer_device_get_name      (MateMixerDevice *device);
const gchar *          mate_mixer_device_get_label     (MateMixerDevice *device);
const gchar *          mate_mixer_device_get_icon      (MateMixerDevice *device);

MateMixerStream *      mate_mixer_device_get_stream    (MateMixerDevice *device,
                                                        const gchar     *name);

MateMixerDeviceSwitch *mate_mixer_device_get_switch    (MateMixerDevice *device,
                                                        const gchar     *name);

const GList *          mate_mixer_device_list_streams  (MateMixerDevice *device);
const GList *          mate_mixer_device_list_switches (MateMixerDevice *device);

G_END_DECLS

#endif /* MATEMIXER_DEVICE_H */
