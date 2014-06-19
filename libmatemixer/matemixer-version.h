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

#ifndef MATEMIXER_VERSION_H
#define MATEMIXER_VERSION_H

G_BEGIN_DECLS

/**
 * LIBMATEMIXER_CHECK_VERSION:
 * @major: major version number
 * @minor: minor version number
 * @micro: micro version number
 *
 * Compile-time version checking. Evaluates to %TRUE if the version of the
 * library is greater than the required one.
 */
#define LIBMATEMIXER_CHECK_VERSION(major, minor, micro) \
        (LIBMATEMIXER_MAJOR_VERSION > (major) || \
         (LIBMATEMIXER_MAJOR_VERSION == (major) && LIBMATEMIXER_MINOR_VERSION > (minor)) || \
         (LIBMATEMIXER_MAJOR_VERSION == (major) && LIBMATEMIXER_MINOR_VERSION == (minor) && \
          LIBMATEMIXER_MICRO_VERSION >= (micro)))

G_END_DECLS

#endif /* MATEMIXER_VERSION_H */
