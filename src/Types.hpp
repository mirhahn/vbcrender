/*
 * vbcrender - Command line tool to render videos from VBC files.
 * Copyright (C) 2019 Mirko Hahn
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __VBC_TYPES_HPP
#define __VBC_TYPES_HPP

#include <cairo.h>

// Forward declaration for the drawing context
typedef cairo_t Canvas;

// Scalar type preferred by the drawing library
typedef double Scalar;

// Rectangle type useful for the drawing library
struct Rect {
    Scalar x0, y0, x1, y1;
};

// RGB color structure
struct Color {
    Scalar r, g, b;
};

#endif /* end of include guard: __VBC_TYPES_HPP */
