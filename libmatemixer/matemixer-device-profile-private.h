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

#ifndef MATEMIXER_DEVICE_PROFILE_PRIVATE_H
#define MATEMIXER_DEVICE_PROFILE_PRIVATE_H

#include <glib.h>

#include "matemixer-device-profile.h"

G_BEGIN_DECLS

MateMixerDeviceProfile *_mate_mixer_device_profile_new                       (const gchar            *name,
                                                                              const gchar            *description,
                                                                              guint                   priority,
                                                                              guint                   input_streams,
                                                                              guint                   output_streams);

gboolean                _mate_mixer_device_profile_update_description        (MateMixerDeviceProfile *profile,
                                                                              const gchar            *description);
gboolean                _mate_mixer_device_profile_update_priority           (MateMixerDeviceProfile *profile,
                                                                              guint                   priority);
gboolean                _mate_mixer_device_profile_update_num_input_streams  (MateMixerDeviceProfile *profile,
                                                                              guint                   num);
gboolean                _mate_mixer_device_profile_update_num_output_streams (MateMixerDeviceProfile *profile,
                                                                              guint                   num);

G_END_DECLS

#endif /* MATEMIXER_DEVICE_PROFILE_PRIVATE_H */
