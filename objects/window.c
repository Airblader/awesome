/*
 * window.c - window object
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

#include "luaa.h"
#include "xwindow.h"
#include "ewmh.h"
#include "screen.h"
#include "objects/window.h"
#include "objects/tag.h"
#include "common/luaobject.h"
#include "common/xutil.h"

LUA_CLASS_FUNCS(window, &window_class)

static void
window_wipe(window_t *window)
{
    button_array_wipe(&window->buttons);
}

bool
window_isvisible(lua_State *L, int idx)
{
    window_t *window = luaA_checkudata(L, idx, (lua_class_t *) &window_class);
    lua_interface_window_t *interface = (lua_interface_window_t *) luaA_class_get(L, idx);
    if(interface->isvisible)
        return interface->isvisible(window);
    return true;
}

/** Prepare banning a window by running all needed Lua events.
 * \param c The client.
 */
void
window_ban_unfocus(window_t *window)
{
    /* Wait until the last moment to take away the focus from the window. */
    if(globalconf.screens.tab[window->screen->phys_screen].focused_window == window)
    {
        xcb_window_t root_win = xutil_screen_get(globalconf.connection, window->screen->phys_screen)->root;
        /* Set focus on root window, so no events leak to the current window. */
        xcb_set_input_focus(globalconf.connection, XCB_INPUT_FOCUS_PARENT,
                            root_win, XCB_CURRENT_TIME);
    }
}

/** Ban window.
 * \param window The window.
 */
void
window_ban(window_t *window)
{
    if(!window->banned)
    {
        xcb_unmap_window(globalconf.connection, window->window);
        window->banned = true;
        window_ban_unfocus(window);
    }
}


/** Unban a window.
 * \param window The window.
 */
void
window_unban(window_t *window)
{
    if(window->banned)
    {
        xcb_map_window(globalconf.connection, window->window);
        window->banned = false;
    }
}

/** Record that a window got focus.
 * \param window The window that got focus.
 */
void
window_focus_update(window_t *window)
{
    globalconf.screen_focus = &globalconf.screens.tab[window->screen->phys_screen];
    globalconf.screen_focus->focused_window = window;
    luaA_object_push(globalconf.L, window);
    luaA_object_emit_signal(globalconf.L, -1, "focus", 0);
    lua_pop(globalconf.L, 1);
}

/** Give focus to window.
 * \param L The Lua VM state.
 * \param idx The window index on the stack.
 */
void
window_focus(lua_State *L, int idx)
{
    window_t *window = luaA_checkudata(L, idx, (lua_class_t *) &window_class);
    /* If the window is banned but isvisible, unban it right now because you
     * can't set focus on unmapped window */
    if(window_isvisible(L, idx))
        window_unban(window);
    else
        return;

    /* Sets focus on window - using xcb_set_input_focus or WM_TAKE_FOCUS */
    if(window->focusable)
        xcb_set_input_focus(globalconf.connection, XCB_INPUT_FOCUS_PARENT,
                            window->window, XCB_CURRENT_TIME);

}

/** Get or set mouse buttons bindings on a window.
 * \param L The Lua VM state.
 * \return The number of elements pushed on the stack.
 */
static int
luaA_window_buttons(lua_State *L)
{
    window_t *window = luaA_checkudata(L, 1, &window_class);

    if(lua_gettop(L) == 2)
    {
        luaA_button_array_set(L, 1, 2, &window->buttons);
        luaA_object_emit_signal(L, 1, "property::buttons", 0);
        xwindow_buttons_grab(window->window, &window->buttons);
    }

    return luaA_button_array_get(L, 1, &window->buttons);
}

/** Return window struts (reserved space at the edge of the screen).
 * \param L The Lua VM state.
 * \return The number of elements pushed on stack.
 */
static int
luaA_window_struts(lua_State *L)
{
    window_t *window = luaA_checkudata(L, 1, &window_class);

    if(lua_gettop(L) == 2)
    {
        luaA_tostrut(L, 2, &window->strut);
        ewmh_update_strut(window->window, &window->strut);
        luaA_object_emit_signal(L, 1, "property::struts", 0);
        if(window->screen)
            screen_emit_signal(L, window->screen, "property::workarea", 0);
    }

    return luaA_pushstrut(L, window->strut);
}

/** Set a window opacity.
 * \param L The Lua VM state.
 * \param idx The index of the window on the stack.
 * \param opacity The opacity value.
 */
void
window_set_opacity(lua_State *L, int idx, double opacity)
{
    window_t *window = luaA_checkudata(L, idx, &window_class);

    if(window->opacity != opacity)
    {
        window->opacity = opacity;
        xwindow_set_opacity(window->window, opacity);
        luaA_object_emit_signal(L, idx, "property::opacity", 0);
    }
}

/** Set a window opacity.
 * \param L The Lua VM state.
 * \param window The window object.
 * \return The number of elements pushed on stack.
 */
static int
luaA_window_set_opacity(lua_State *L, window_t *window)
{
    if(lua_isnil(L, -1))
        window_set_opacity(L, -3, -1);
    else
    {
        double d = luaL_checknumber(L, -1);
        if(d >= 0 && d <= 1)
            window_set_opacity(L, -3, d);
    }
    return 0;
}

/** Get the window opacity.
 * \param L The Lua VM state.
 * \param window The window object.
 * \return The number of elements pushed on stack.
 */
static int
luaA_window_get_opacity(lua_State *L, window_t *window)
{
    if(window->opacity >= 0)
    {
        lua_pushnumber(L, window->opacity);
        return 1;
    }
    return 0;
}

/** Set the window border color.
 * \param L The Lua VM state.
 * \param window The window object.
 * \return The number of elements pushed on stack.
 */
static int
luaA_window_set_border_color(lua_State *L, window_t *window)
{
    size_t len;
    const char *color_name = luaL_checklstring(L, -1, &len);

    if(color_name &&
       xcolor_init_reply(xcolor_init_unchecked(&window->border_color, color_name, len)))
    {
        xwindow_set_border_color(window->window, &window->border_color);
        luaA_object_emit_signal(L, -3, "property::border_color", 0);
    }

    return 0;
}

/** Set a window border width.
 * \param L The Lua VM state.
 * \param idx The window index.
 * \param width The border width.
 */
void
window_set_border_width(lua_State *L, int idx, int width)
{
    window_t *window = luaA_checkudata(L, idx, &window_class);

    if(width == window->border_width || width < 0)
        return;

    xcb_configure_window(globalconf.connection, window->window,
                         XCB_CONFIG_WINDOW_BORDER_WIDTH,
                         (uint32_t[]) { width });

    window->border_width = width;

    luaA_object_emit_signal(L, idx, "property::border_width", 0);
}

static int
luaA_window_set_border_width(lua_State *L, window_t *c)
{
    window_set_border_width(L, -3, luaL_checknumber(L, -1));
    return 0;
}

/** Access or set the window tags.
 * \param L The Lua VM state.
 * \return The number of elements pushed on stack.
 * \lparam A table with tags to set, or none to get the current tags table.
 * \return The window tag.
 */
static int
luaA_window_tags(lua_State *L)
{
    window_t *c = luaA_checkudata(L, 1, &window_class);

    if(lua_gettop(L) == 2)
    {
        luaA_checktable(L, 2);
        foreach(tag, c->tags)
        {
            luaA_object_push_item(L, 1, *tag);
            untag_window(L, 1, -1);
            /* remove tag */
            lua_pop(L, 1);
        }
        lua_pushnil(L);
        while(lua_next(L, 2))
        {
            tag_window(L, 1, -1);
            /* remove value (tag) */
            lua_pop(L, 1);
        }
    }

    int i = 0;
    lua_createtable(L, c->tags.len, 0);
    foreach(tag, c->tags)
    {
        luaA_object_push_item(L, 1, *tag);
        lua_rawseti(L, -2, ++i);
    }

    return 1;
}

LUA_OBJECT_DO_SET_PROPERTY_FUNC(window, &window_class, window_t, sticky)
static LUA_OBJECT_DO_SET_PROPERTY_FUNC(window, &window_class, window_t, focusable)

static int
luaA_window_set_sticky(lua_State *L, window_t *c)
{
    window_set_sticky(L, -3, luaA_checkboolean(L, -1));
    return 0;
}

static int
luaA_window_set_focusable(lua_State *L, window_t *c)
{
    window_set_focusable(L, -3, luaA_checkboolean(L, -1));
    return 0;
}

/** Give the focus to a window.
 * \param L The Lua VM state.
 * \return The number of elements pushed on stack.
 */
static int
luaA_window_focus(lua_State *L)
{
    window_focus(L, 1);
    return 0;
}

static LUA_OBJECT_EXPORT_PROPERTY(window, window_t, window, lua_pushnumber)
static LUA_OBJECT_EXPORT_PROPERTY(window, window_t, border_color, luaA_pushxcolor)
static LUA_OBJECT_EXPORT_PROPERTY(window, window_t, border_width, lua_pushnumber)
static LUA_OBJECT_EXPORT_PROPERTY(window, window_t, sticky, lua_pushboolean)
LUA_OBJECT_EXPORT_PROPERTY(window, window_t, focusable, lua_pushboolean)

void
window_class_setup(lua_State *L)
{
    static const struct luaL_reg window_methods[] =
    {
        { NULL, NULL }
    };

    static const struct luaL_reg window_meta[] =
    {
        { "struts", luaA_window_struts },
        { "buttons", luaA_window_buttons },
        { "tags", luaA_window_tags },
        { "focus", luaA_window_focus },
        { NULL, NULL }
    };

    luaA_class_setup(L, &window_class, "window", NULL,
                     NULL, (lua_class_collector_t) window_wipe, NULL,
                     luaA_class_index_miss_property, luaA_class_newindex_miss_property,
                     window_methods, window_meta);

    luaA_class_add_property(&window_class, A_TK_WINDOW,
                            NULL,
                            (lua_class_propfunc_t) luaA_window_get_window,
                            NULL);
    luaA_class_add_property(&window_class, A_TK_OPACITY,
                            (lua_class_propfunc_t) luaA_window_set_opacity,
                            (lua_class_propfunc_t) luaA_window_get_opacity,
                            (lua_class_propfunc_t) luaA_window_set_opacity);
    luaA_class_add_property(&window_class, A_TK_BORDER_COLOR,
                            (lua_class_propfunc_t) luaA_window_set_border_color,
                            (lua_class_propfunc_t) luaA_window_get_border_color,
                            (lua_class_propfunc_t) luaA_window_set_border_color);
    luaA_class_add_property(&window_class, A_TK_BORDER_WIDTH,
                            (lua_class_propfunc_t) luaA_window_set_border_width,
                            (lua_class_propfunc_t) luaA_window_get_border_width,
                            (lua_class_propfunc_t) luaA_window_set_border_width);
    luaA_class_add_property(&window_class, A_TK_STICKY,
                            (lua_class_propfunc_t) luaA_window_set_sticky,
                            (lua_class_propfunc_t) luaA_window_get_sticky,
                            (lua_class_propfunc_t) luaA_window_set_sticky);
    luaA_class_add_property(&window_class, A_TK_FOCUSABLE,
                            (lua_class_propfunc_t) luaA_window_set_focusable,
                            (lua_class_propfunc_t) luaA_window_get_focusable,
                            (lua_class_propfunc_t) luaA_window_set_focusable);
}

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
