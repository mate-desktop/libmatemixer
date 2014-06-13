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

#ifndef PULSE_ENUMS_H
#define PULSE_ENUMS_H

typedef enum {
    PULSE_CONNECTION_DISCONNECTED = 0,
    PULSE_CONNECTION_CONNECTING,
    PULSE_CONNECTION_AUTHORIZING,
    PULSE_CONNECTION_LOADING,
    PULSE_CONNECTION_CONNECTED
} PulseConnectionState;

#endif /* PULSE_ENUMS_H */
