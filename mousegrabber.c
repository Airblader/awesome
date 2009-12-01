/*
 * mousegrabber.c - mouse pointer grabbing
 *
 * Copyright © 2008-2009 Julien Danjou <julien@danjou.info>
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

#include <unistd.h>

#include "awesome.h"
#include "screen.h"
#include "mouse.h"
#include "luaa.h"
#include "mousegrabber.h"
#include "objects/window.h"
#include "common/xcursor.h"

/** Grab the mouse.
 * \param cursor The cursor to use while grabbing.
 * \return True if mouse was grabbed.
 */
static bool
mousegrabber_grab(xcb_cursor_t cursor)
{
    for(int i = 1000; i; i--)
    {
        xcb_grab_pointer_reply_t *grab_ptr_r;
        xcb_grab_pointer_cookie_t grab_ptr_c =
            xcb_grab_pointer_unchecked(_G_connection, false, _G_root->window,
                                       XCB_EVENT_MASK_BUTTON_PRESS
                                       | XCB_EVENT_MASK_BUTTON_RELEASE
                                       | XCB_EVENT_MASK_POINTER_MOTION,
                                       XCB_GRAB_MODE_ASYNC,
                                       XCB_GRAB_MODE_ASYNC,
                                       _G_root->window, cursor, XCB_CURRENT_TIME);

        if((grab_ptr_r = xcb_grab_pointer_reply(_G_connection, grab_ptr_c, NULL)))
        {
            p_delete(&grab_ptr_r);
            return true;
        }
        usleep(1000);
    }
    return false;
}

/** Handle mouse motion events.
 * \param L Lua stack to push the pointer motion.
 * \param x The received mouse event x component.
 * \param y The received mouse event y component.
 * \param mask The received mouse event bit mask.
 */
void
mousegrabber_handleevent(lua_State *L, int x, int y, uint16_t mask)
{
    luaA_mouse_pushstatus(L, x, y, mask);
}

/** Grab the mouse pointer and list motions, calling callback function at each
 * motion. The callback function must return a boolean value: true to
 * continue grabbing, false to stop.
 * The function is called with one argument:
 * a table containing modifiers pointer coordinates.
 *
 * \param L The Lua VM state.
 * \return The number of elements pushed on stack.
 *
 * \luastack
 *
 * \lparam A callback function as described above.
 */
static int
luaA_mousegrabber_run(lua_State *L)
{
    if(_G_mousegrabber)
        luaL_error(L, "mousegrabber already running");

    uint16_t cfont = xcursor_font_fromstr(luaL_checkstring(L, 2));

    if(cfont)
    {
        luaA_checkfunction(L, 1);
        xcb_cursor_t cursor = xcursor_new(_G_connection, cfont);

        _G_mousegrabber = luaA_object_ref(L, 1);

        if(!mousegrabber_grab(cursor))
        {
            luaA_object_unref(L, _G_mousegrabber);
            luaL_error(L, "unable to grab mouse pointer");
        }
    }
    else
        luaA_warn(L, "invalid cursor");

    return 0;
}

/** Stop grabbing the mouse pointer.
 * \param L The Lua VM state.
 * \return The number of elements pushed on stack.
 */
int
luaA_mousegrabber_stop(lua_State *L)
{
    xcb_ungrab_pointer(_G_connection, XCB_CURRENT_TIME);
    luaA_object_unref(L, _G_mousegrabber);
    return 0;
}

const struct luaL_reg awesome_mousegrabber_lib[] =
{
    { "run", luaA_mousegrabber_run },
    { "stop", luaA_mousegrabber_stop },
    { NULL, NULL }
};

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
