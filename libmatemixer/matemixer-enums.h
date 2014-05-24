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

#ifndef MATEMIXER_ENUMS_H
#define MATEMIXER_ENUMS_H

typedef enum {
    MATE_MIXER_BACKEND_TYPE_UNKNOWN,
    MATE_MIXER_BACKEND_TYPE_PULSE,
    MATE_MIXER_BACKEND_TYPE_NULL
} MateMixerBackendType;

typedef enum { /*< flags >*/
    MATE_MIXER_DEVICE_PORT_DIRECTION_INPUT  = 1 << 0,
    MATE_MIXER_DEVICE_PORT_DIRECTION_OUTPUT = 1 << 1
} MateMixerDevicePortDirection;

typedef enum { /*< flags >*/
    MATE_MIXER_DEVICE_PORT_STATUS_AVAILABLE = 1 << 0
} MateMixerDevicePortStatus;

#endif /* MATEMIXER_ENUMS_H */
