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

#ifndef MATEMIXER_PRIVATE_H
#define MATEMIXER_PRIVATE_H

#include <glib.h>

#include "matemixer-enums.h"

#include "matemixer-app-info-private.h"
#include "matemixer-backend.h"
#include "matemixer-backend-module.h"
#include "matemixer-stream-private.h"
#include "matemixer-stream-control-private.h"
#include "matemixer-switch-private.h"
#include "matemixer-switch-option-private.h"

G_BEGIN_DECLS

#define MATE_MIXER_IS_LEFT_CHANNEL(c)                   \
    ((c) == MATE_MIXER_CHANNEL_FRONT_LEFT ||            \
     (c) == MATE_MIXER_CHANNEL_BACK_LEFT ||             \
     (c) == MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER ||     \
     (c) == MATE_MIXER_CHANNEL_SIDE_LEFT ||             \
     (c) == MATE_MIXER_CHANNEL_TOP_FRONT_LEFT ||        \
     (c) == MATE_MIXER_CHANNEL_TOP_BACK_LEFT)

#define MATE_MIXER_IS_RIGHT_CHANNEL(c)                  \
    ((c) == MATE_MIXER_CHANNEL_FRONT_RIGHT ||           \
     (c) == MATE_MIXER_CHANNEL_BACK_RIGHT ||            \
     (c) == MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER ||    \
     (c) == MATE_MIXER_CHANNEL_SIDE_RIGHT ||            \
     (c) == MATE_MIXER_CHANNEL_TOP_FRONT_RIGHT ||       \
     (c) == MATE_MIXER_CHANNEL_TOP_BACK_RIGHT)

#define MATE_MIXER_IS_FRONT_CHANNEL(c)                  \
    ((c) == MATE_MIXER_CHANNEL_FRONT_LEFT ||            \
     (c) == MATE_MIXER_CHANNEL_FRONT_RIGHT ||           \
     (c) == MATE_MIXER_CHANNEL_FRONT_CENTER ||          \
     (c) == MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER ||     \
     (c) == MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER ||    \
     (c) == MATE_MIXER_CHANNEL_TOP_FRONT_LEFT ||        \
     (c) == MATE_MIXER_CHANNEL_TOP_FRONT_RIGHT ||       \
     (c) == MATE_MIXER_CHANNEL_TOP_FRONT_CENTER)

#define MATE_MIXER_IS_BACK_CHANNEL(c)                   \
    ((c) == MATE_MIXER_CHANNEL_BACK_LEFT ||             \
     (c) == MATE_MIXER_CHANNEL_BACK_RIGHT ||            \
     (c) == MATE_MIXER_CHANNEL_BACK_CENTER ||           \
     (c) == MATE_MIXER_CHANNEL_TOP_BACK_LEFT ||         \
     (c) == MATE_MIXER_CHANNEL_TOP_BACK_RIGHT ||        \
     (c) == MATE_MIXER_CHANNEL_TOP_BACK_CENTER)

#define MATE_MIXER_CHANNEL_MASK_LEFT                    \
    ((1 << MATE_MIXER_CHANNEL_FRONT_LEFT) |             \
     (1 << MATE_MIXER_CHANNEL_BACK_LEFT) |              \
     (1 << MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER) |      \
     (1 << MATE_MIXER_CHANNEL_SIDE_LEFT) |              \
     (1 << MATE_MIXER_CHANNEL_TOP_FRONT_LEFT) |         \
     (1 << MATE_MIXER_CHANNEL_TOP_BACK_LEFT))

#define MATE_MIXER_CHANNEL_MASK_RIGHT                   \
    ((1 << MATE_MIXER_CHANNEL_FRONT_RIGHT) |            \
     (1 << MATE_MIXER_CHANNEL_BACK_RIGHT) |             \
     (1 << MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER) |     \
     (1 << MATE_MIXER_CHANNEL_SIDE_RIGHT) |             \
     (1 << MATE_MIXER_CHANNEL_TOP_FRONT_RIGHT) |        \
     (1 << MATE_MIXER_CHANNEL_TOP_BACK_RIGHT))

#define MATE_MIXER_CHANNEL_MASK_FRONT                   \
    ((1 << MATE_MIXER_CHANNEL_FRONT_LEFT) |             \
     (1 << MATE_MIXER_CHANNEL_FRONT_RIGHT) |            \
     (1 << MATE_MIXER_CHANNEL_FRONT_CENTER) |           \
     (1 << MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER) |      \
     (1 << MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER) |     \
     (1 << MATE_MIXER_CHANNEL_TOP_FRONT_LEFT) |         \
     (1 << MATE_MIXER_CHANNEL_TOP_FRONT_RIGHT) |        \
     (1 << MATE_MIXER_CHANNEL_TOP_FRONT_CENTER))

#define MATE_MIXER_CHANNEL_MASK_BACK                    \
    ((1 << MATE_MIXER_CHANNEL_BACK_LEFT) |              \
     (1 << MATE_MIXER_CHANNEL_BACK_RIGHT) |             \
     (1 << MATE_MIXER_CHANNEL_BACK_CENTER) |            \
     (1 << MATE_MIXER_CHANNEL_TOP_BACK_LEFT) |          \
     (1 << MATE_MIXER_CHANNEL_TOP_BACK_RIGHT) |         \
     (1 << MATE_MIXER_CHANNEL_TOP_BACK_CENTER))

#define MATE_MIXER_CHANNEL_MASK_HAS_CHANNEL(m,c)    ((m) & (1 << (c)))
#define MATE_MIXER_CHANNEL_MASK_HAS_LEFT(m)         ((m) & MATE_MIXER_CHANNEL_MASK_LEFT)
#define MATE_MIXER_CHANNEL_MASK_HAS_RIGHT(m)        ((m) & MATE_MIXER_CHANNEL_MASK_RIGHT)
#define MATE_MIXER_CHANNEL_MASK_HAS_FRONT(m)        ((m) & MATE_MIXER_CHANNEL_MASK_FRONT)
#define MATE_MIXER_CHANNEL_MASK_HAS_BACK(m)         ((m) & MATE_MIXER_CHANNEL_MASK_BACK)

const GList *_mate_mixer_list_modules        (void);

guint32      _mate_mixer_create_channel_mask (MateMixerChannelPosition *positions,
                                              guint                     n) G_GNUC_PURE;

G_END_DECLS

#endif /* MATEMIXER_PRIVATE_H */
