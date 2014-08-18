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

#ifndef PULSE_TYPES_H
#define PULSE_TYPES_H

G_BEGIN_DECLS

typedef struct _PulseBackend            PulseBackend;
typedef struct _PulseConnection         PulseConnection;
typedef struct _PulseDevice             PulseDevice;
typedef struct _PulseDeviceProfile      PulseDeviceProfile;
typedef struct _PulseDeviceSwitch       PulseDeviceSwitch;
typedef struct _PulseExtStream          PulseExtStream;
typedef struct _PulseMonitor            PulseMonitor;
typedef struct _PulsePort               PulsePort;
typedef struct _PulsePortSwitch         PulsePortSwitch;
typedef struct _PulseSink               PulseSink;
typedef struct _PulseSinkControl        PulseSinkControl;
typedef struct _PulseSinkInput          PulseSinkInput;
typedef struct _PulseSinkSwitch         PulseSinkSwitch;
typedef struct _PulseSource             PulseSource;
typedef struct _PulseSourceControl      PulseSourceControl;
typedef struct _PulseSourceOutput       PulseSourceOutput;
typedef struct _PulseSourceSwitch       PulseSourceSwitch;
typedef struct _PulseStream             PulseStream;
typedef struct _PulseStreamControl      PulseStreamControl;

G_END_DECLS

#endif /* PULSE_TYPES_H */
