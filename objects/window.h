/*
 * window.h - window object header
 *
 * Copyright © 2009 Julien Danjou <julien@danjou.info>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef AWESOME_OBJECTS_WINDOW_H
#define AWESOME_OBJECTS_WINDOW_H

#include "strut.h"
#include "objects/button.h"
#include "common/luaclass.h"

#define WINDOW_OBJECT_HEADER \
    LUA_OBJECT_HEADER \
    /** The X window number */ \
    xcb_window_t window; \
    /** Opacity */ \
    double opacity; \
    /** Strut */ \
    strut_t strut; \
    /** Client logical screen */ \
    screen_t *screen; \
    /** Button bindings */ \
    button_array_t buttons; \
    /** Border color */ \
    xcolor_t border_color; \
    /** Border width */ \
    uint16_t border_width; \
    /** Window tags */ \
    tag_array_t tags; \
    /** True if the window is sticky */ \
    bool sticky; \
    /** True if the window is banned from the view */ \
    bool banned; \
    /** True if the window can have focus */ \
    bool focusable;

/** Window structure */
typedef struct
{
    WINDOW_OBJECT_HEADER
} window_t;

lua_class_t window_class;

void window_class_setup(lua_State *);

void window_set_opacity(lua_State *, int, double);
void window_set_border_width(lua_State *, int, int);
void window_set_sticky(lua_State *, int, bool);

DO_ARRAY(window_t *, window, DO_NOTHING)

#endif
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
