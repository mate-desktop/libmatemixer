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

#ifndef ALSA_TYPES_H
#define ALSA_TYPES_H

G_BEGIN_DECLS

typedef struct _AlsaBackend             AlsaBackend;
typedef struct _AlsaDevice              AlsaDevice;
typedef struct _AlsaElement             AlsaElement;
typedef struct _AlsaStream              AlsaStream;
typedef struct _AlsaStreamControl       AlsaStreamControl;
typedef struct _AlsaStreamInputControl  AlsaStreamInputControl;
typedef struct _AlsaStreamOutputControl AlsaStreamOutputControl;
typedef struct _AlsaSwitch              AlsaSwitch;
typedef struct _AlsaSwitchOption        AlsaSwitchOption;
typedef struct _AlsaToggle              AlsaToggle;

G_END_DECLS

#endif /* ALSA_TYPES_H */
