/*
 * screen.h - screen management header
 *
 * Copyright © 2007-2009 Julien Danjou <julien@danjou.info>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef AWESOME_OBJECTS_SCREEN_H
#define AWESOME_OBJECTS_SCREEN_H

#include "area.h"
#include "objects/window.h"

typedef struct screen_output_t screen_output_t;
ARRAY_TYPE(screen_output_t, screen_output)

typedef struct
{
    LUA_OBJECT_HEADER
    /** Screen geometry */
    area_t geometry;
    /** Tag list */
    tag_array_t tags;
    /** Screen's root window */
    window_t *root;
    /** True if the banning on this screen needs to be updated */
    bool need_lazy_banning;
    /** The screen outputs informations */
    screen_output_array_t outputs;
} screen_t;
DO_ARRAY(screen_t, screen, DO_NOTHING)

void screen_emit_signal(lua_State *, screen_t *, const char *, int);

void screen_class_setup(lua_State *);
void screen_scan(lua_State *);
screen_t *screen_getbycoord(int, int);
area_t screen_area_get(screen_t *, bool);

/** All screens */
screen_array_t _G_screens;

#endif
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
