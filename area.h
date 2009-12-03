/*
 * area.h - area definition
 *
 * Copyright © 2007-2009 Julien Danjou <julien@danjou.info>
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

#ifndef AWESOME_AREA_H
#define AWESOME_AREA_H

#include <stdint.h>
#include <lua.h>

typedef struct
{
    /** Co-ords of upper left corner */
    int16_t  x;
    int16_t  y;
    uint16_t width;
    uint16_t height;
} area_t;

#define AREA_LEFT(a)    ((a).x)
#define AREA_TOP(a)     ((a).y)
#define AREA_RIGHT(a)   ((a).x + (a).width)
#define AREA_BOTTOM(a)    ((a).y + (a).height)

/** Push a area type to a table on stack.
 * \param L The Lua VM state.
 * \param geometry The area geometry to push.
 * \return The number of elements pushed on stack.
 */
static inline int
luaA_pusharea(lua_State *L, area_t geometry)
{
    lua_createtable(L, 0, 4);
    lua_pushnumber(L, geometry.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, geometry.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, geometry.width);
    lua_setfield(L, -2, "width");
    lua_pushnumber(L, geometry.height);
    lua_setfield(L, -2, "height");
    return 1;
}

#endif
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
