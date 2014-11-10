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

#ifndef MATEMIXER_TYPES_H
#define MATEMIXER_TYPES_H

G_BEGIN_DECLS

typedef struct _MateMixerAppInfo        MateMixerAppInfo;
typedef struct _MateMixerContext        MateMixerContext;
typedef struct _MateMixerDevice         MateMixerDevice;
typedef struct _MateMixerDeviceSwitch   MateMixerDeviceSwitch;
typedef struct _MateMixerStoredControl  MateMixerStoredControl;
typedef struct _MateMixerStream         MateMixerStream;
typedef struct _MateMixerStreamControl  MateMixerStreamControl;
typedef struct _MateMixerStreamSwitch   MateMixerStreamSwitch;
typedef struct _MateMixerStreamToggle   MateMixerStreamToggle;
typedef struct _MateMixerSwitch         MateMixerSwitch;
typedef struct _MateMixerSwitchOption   MateMixerSwitchOption;

G_END_DECLS

#endif /* MATEMIXER_TYPES_H */
