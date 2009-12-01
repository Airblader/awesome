/*
 * screen.c - screen management
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

#include <stdio.h>

#include <xcb/xcb.h>
#include <xcb/xinerama.h>
#include <xcb/randr.h>

#include "awesome.h"
#include "screen.h"
#include "ewmh.h"
#include "objects/tag.h"
#include "objects/client.h"
#include "objects/wibox.h"
#include "luaa.h"
#include "common/xutil.h"

static lua_class_t screen_class;
LUA_OBJECT_FUNCS(&screen_class, screen_t, screen)

struct screen_output_t
{
    /** The XRandR names of the output */
    char *name;
    /** The size in millimeters */
    uint32_t mm_width, mm_height;
};

ARRAY_FUNCS(screen_output_t, screen_output, DO_NOTHING)

static inline area_t
screen_xsitoarea(xcb_xinerama_screen_info_t si)
{
    area_t a =
    {
        .x = si.x_org,
        .y = si.y_org,
        .width = si.width,
        .height = si.height
    };
    return a;
}

static xcb_visualtype_t *
screen_default_visual(xcb_screen_t *s)
{
    xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(s);

    if(depth_iter.data)
        for(; depth_iter.rem; xcb_depth_next (&depth_iter))
            for(xcb_visualtype_iterator_t visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
                visual_iter.rem; xcb_visualtype_next (&visual_iter))
                if(s->root_visual == visual_iter.data->visual_id)
                    return visual_iter.data;

    return NULL;
}

static void
protocol_screen_scan(void)
{
    xcb_screen_t *xcb_screen = xutil_screen_get(_G_connection, _G_default_screen);
    _G_visual = screen_default_visual(xcb_screen);

    /* Create root window */
    luaA_object_new(globalconf.L, &window_class);
    _G_root = luaA_object_ref(globalconf.L, -1);
    _G_root->focusable = true;
    _G_root->window = xcb_screen->root;

    /* The default GC is just a newly created associated to the root window */
    _G_gc = xcb_generate_id(_G_connection);
    xcb_create_gc(_G_connection, _G_gc, xcb_screen->root,
                  XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
                  (const uint32_t[]) { xcb_screen->black_pixel, xcb_screen->white_pixel });
}

/** Scan screen information using XRandR protocol.
 */
static bool
screen_scan_xrandr(void)
{
    /* Check for extension before checking for XRandR */
    if(!xcb_get_extension_data(_G_connection, &xcb_randr_id)->present)
        return false;

    /* We require at least version 1.1 */
    xcb_randr_query_version_reply_t *version_reply =
        xcb_randr_query_version_reply(_G_connection,
                                      xcb_randr_query_version(_G_connection, 1, 1), 0);
    if(!version_reply)
        return false;

    /* A quick XRandR recall:
     * You have CRTC that manages a part of a SCREEN.
     * Each CRTC can draw stuff on one or more OUTPUT.
     * So in awesome, we map our screen_t on XRandR CRTCs.
     */

    xcb_randr_get_screen_resources_cookie_t screen_res_c = xcb_randr_get_screen_resources(_G_connection, _G_root->window);
    xcb_randr_get_screen_resources_reply_t *screen_res_r = xcb_randr_get_screen_resources_reply(_G_connection, screen_res_c, NULL);

    /* We go through CRTC, and build a screen for each one. */
    xcb_randr_crtc_t *randr_crtcs = xcb_randr_get_screen_resources_crtcs(screen_res_r);

    for(int i = 0; i < screen_res_r->num_crtcs; i++)
    {
        /* Get info on the output crtc */
        xcb_randr_get_crtc_info_cookie_t crtc_info_c = xcb_randr_get_crtc_info(_G_connection, randr_crtcs[i], XCB_CURRENT_TIME);
        xcb_randr_get_crtc_info_reply_t *crtc_info_r = xcb_randr_get_crtc_info_reply(_G_connection, crtc_info_c, NULL);

        /* If CRTC has no OUTPUT, ignore it */
        if(!xcb_randr_get_crtc_info_outputs_length(crtc_info_r))
            continue;

        /* Prepare the new screen */
        screen_t new_screen;
        p_clear(&new_screen, 1);
        new_screen.geometry.x = crtc_info_r->x;
        new_screen.geometry.y = crtc_info_r->y;
        new_screen.geometry.width= crtc_info_r->width;
        new_screen.geometry.height= crtc_info_r->height;

        xcb_randr_output_t *randr_outputs = xcb_randr_get_crtc_info_outputs(crtc_info_r);

        for(int j = 0; j < xcb_randr_get_crtc_info_outputs_length(crtc_info_r); j++)
        {
            xcb_randr_get_output_info_cookie_t output_info_c = xcb_randr_get_output_info(_G_connection, randr_outputs[j], XCB_CURRENT_TIME);
            xcb_randr_get_output_info_reply_t *output_info_r = xcb_randr_get_output_info_reply(_G_connection, output_info_c, NULL);

            int len = xcb_randr_get_output_info_name_length(output_info_r);
            /* name is not NULL terminated */
            char *name = memcpy(p_new(char *, len + 1), xcb_randr_get_output_info_name(output_info_r), len);
            name[len] = '\0';

            screen_output_array_append(&new_screen.outputs,
                                       (screen_output_t) { .name = name,
                                                           .mm_width = output_info_r->mm_width,
                                                           .mm_height = output_info_r->mm_height });

            p_delete(&output_info_r);
        }

        screen_array_append(&globalconf.screens, new_screen);

        p_delete(&crtc_info_r);
    }

    p_delete(&screen_res_r);

    return true;
}

/** Scan screen information using Xinerama protocol.
 * \return True if informations where gathered successfully, false otherwise.
 */
static bool
screen_scan_xinerama(void)
{
    bool xinerama_is_active = false;

    /* Check for extension before checking for Xinerama */
    if(xcb_get_extension_data(_G_connection, &xcb_xinerama_id)->present)
    {
        xcb_xinerama_is_active_reply_t *xia;
        xia = xcb_xinerama_is_active_reply(_G_connection, xcb_xinerama_is_active(_G_connection), NULL);
        xinerama_is_active = xia->state;
        p_delete(&xia);
    }

    if(!xinerama_is_active)
        return false;

    xcb_xinerama_query_screens_reply_t *xsq;
    xcb_xinerama_screen_info_t *xsi;
    int xinerama_screen_number;

    xsq = xcb_xinerama_query_screens_reply(_G_connection,
                                           xcb_xinerama_query_screens_unchecked(_G_connection),
                                           NULL);

    xsi = xcb_xinerama_query_screens_screen_info(xsq);
    xinerama_screen_number = xcb_xinerama_query_screens_screen_info_length(xsq);

    for(int screen = 0; screen < xinerama_screen_number; screen++)
    {
        /* now check if screens overlaps (same x,y): if so, we take only the biggest one */
        bool drop = false;
        foreach(screen_to_test, globalconf.screens)
            if(xsi[screen].x_org == screen_to_test->geometry.x
               && xsi[screen].y_org == screen_to_test->geometry.y)
                {
                    /* we already have a screen for this area, just check if
                     * it's not bigger and drop it */
                    drop = true;
                    int i = screen_array_indexof(&globalconf.screens, screen_to_test);
                    screen_to_test->geometry.width =
                        MAX(xsi[screen].width, xsi[i].width);
                    screen_to_test->geometry.height =
                        MAX(xsi[screen].height, xsi[i].height);
                }

        if(!drop)
        {
            screen_t new_screen;
            p_clear(&new_screen, 1);
            new_screen.geometry = screen_xsitoarea(xsi[screen]);
            screen_array_append(&globalconf.screens, new_screen);
        }
    }

    p_delete(&xsq);

    return true;
}

/** Get screens informations and fill global configuration.
 */
void
screen_scan(void)
{
    /* Scan screen protocol first */
    protocol_screen_scan();

    /* If Xrandr fails... */
    if(!screen_scan_xrandr())
        /* ...try Xinerama... */
        if(!screen_scan_xinerama())
            /* ... or then try the good old standard way */
        {
            xcb_screen_t *xcb_screen = xutil_screen_get(_G_connection, _G_default_screen);
            screen_t s;
            p_clear(&s, 1);
            s.geometry.x = 0;
            s.geometry.y = 0;
            s.geometry.width = xcb_screen->width_in_pixels;
            s.geometry.height = xcb_screen->height_in_pixels;
            screen_array_append(&globalconf.screens, s);
        }

    /* Transforms all screen in lightuserdata */
    foreach(screen, globalconf.screens)
        screen_make_light(globalconf.L, screen);
}

/** Return the Xinerama screen number where the coordinates belongs to.
 * \param screen The logical screen number.
 * \param x X coordinate
 * \param y Y coordinate
 * \return Screen pointer or screen param if no match or no multi-head.
 */
screen_t *
screen_getbycoord(int x, int y)
{
    foreach(s, globalconf.screens)
        if((x < 0 || (x >= s->geometry.x && x < s->geometry.x + s->geometry.width))
           && (y < 0 || (y >= s->geometry.y && y < s->geometry.y + s->geometry.height)))
            return s;
    return NULL;
}

/** Get screens info.
 * \param screen Screen.
 * \param strut Honor windows strut.
 * \return The screen area.
 */
area_t
screen_area_get(screen_t *screen, bool strut)
{
    if(!strut)
        return screen->geometry;

    area_t area = screen->geometry;
    uint16_t top = 0, bottom = 0, left = 0, right = 0;

    foreach(ewindow, _G_ewindows)
        if(screen == screen_getbycoord((*ewindow)->geometry.x, (*ewindow)->geometry.y)
           && ewindow_isvisible(*ewindow))
        {
            if((*ewindow)->strut.top_start_x || (*ewindow)->strut.top_end_x || (*ewindow)->strut.top)
            {
                if((*ewindow)->strut.top)
                    top = MAX(top, (*ewindow)->strut.top);
                else
                    top = MAX(top, ((*ewindow)->geometry.y - area.y) + (*ewindow)->geometry.height);
            }
            if((*ewindow)->strut.bottom_start_x || (*ewindow)->strut.bottom_end_x || (*ewindow)->strut.bottom)
            {
                if((*ewindow)->strut.bottom)
                    bottom = MAX(bottom, (*ewindow)->strut.bottom);
                else
                    bottom = MAX(bottom, (area.y + area.height) - (*ewindow)->geometry.y);
            }
            if((*ewindow)->strut.left_start_y || (*ewindow)->strut.left_end_y || (*ewindow)->strut.left)
            {
                if((*ewindow)->strut.left)
                    left = MAX(left, (*ewindow)->strut.left);
                else
                    left = MAX(left, ((*ewindow)->geometry.x - area.x) + (*ewindow)->geometry.width);
            }
            if((*ewindow)->strut.right_start_y || (*ewindow)->strut.right_end_y || (*ewindow)->strut.right)
            {
                if((*ewindow)->strut.right)
                    right = MAX(right, (*ewindow)->strut.right);
                else
                    right = MAX(right, (area.x + area.width) - (*ewindow)->geometry.x);
            }
        }

    area.x += left;
    area.y += top;
    area.width -= left + right;
    area.height -= top + bottom;

    return area;
}

/** Screen module.
 * \param L The Lua VM state.
 * \return The number of elements pushed on stack.
 * \luastack
 * \lfield number The screen number, to get a screen.
 */
static int
luaA_screen_module_index(lua_State *L)
{
    const char *name;

    if((name = lua_tostring(L, 2)))
        foreach(screen, globalconf.screens)
            foreach(output, screen->outputs)
                if(!a_strcmp(output->name, name))
                {
                    lua_pushlightuserdata(L, screen);
                    return 1;
                }

    int screen = luaL_checknumber(L, 2) - 1;
    luaA_checkscreen(screen);
    lua_pushlightuserdata(L, &globalconf.screens.tab[screen]);
    return 1;
}

/** Get the screen count.
 * \param L The Lua VM state.
 * \return The number of elements pushed on stack.
 *
 * \luastack
 * \lreturn The screen count, at least 1.
 */
static int
luaA_screen_count(lua_State *L)
{
    lua_pushnumber(L, globalconf.screens.len);
    return 1;
}

static int
luaA_screen_get_index(lua_State *L, screen_t *screen)
{
    lua_pushinteger(L, screen_array_indexof(&globalconf.screens, screen) + 1);
    return 1;
}

static int
luaA_screen_get_root(lua_State *L, screen_t *screen)
{
    return luaA_object_push(L, _G_root);
}

static int
luaA_screen_get_geometry(lua_State *L, screen_t *screen)
{
    return luaA_pusharea(L, screen->geometry);
}

static int
luaA_screen_get_workarea(lua_State *L, screen_t *screen)
{
    return luaA_pusharea(L, screen_area_get(screen, true));
}

static int
luaA_screen_get_outputs(lua_State *L, screen_t *screen)
{
    lua_createtable(L, 0, screen->outputs.len);
    foreach(output, screen->outputs)
    {
        lua_createtable(L, 0, 2);
        lua_pushinteger(L, output->mm_width);
        lua_setfield(L, -2, "mm_width");
        lua_pushinteger(L, output->mm_height);
        lua_setfield(L, -2, "mm_height");
        lua_setfield(L, -2, output->name);
    }
    return 1;
}

void
screen_class_setup(lua_State *L)
{
    static const struct luaL_reg screen_methods[] =
    {
        LUA_CLASS_METHODS(screen)
        { "count", luaA_screen_count },
        { NULL, NULL }
    };

    static const struct luaL_reg screen_module_meta[] =
    {
        { "__index", luaA_screen_module_index },
        { NULL, NULL }
    };

    luaA_class_setup(L, &screen_class, "screen", NULL, sizeof(screen_t),
                     NULL, NULL, NULL,
                     luaA_class_index_miss_property, luaA_class_newindex_miss_property,
                     screen_methods, screen_module_meta, NULL);

    luaA_class_add_property(&screen_class, A_TK_INDEX,
                            NULL,
                            (lua_class_propfunc_t) luaA_screen_get_index,
                            NULL);
    luaA_class_add_property(&screen_class, A_TK_ROOT,
                            NULL,
                            (lua_class_propfunc_t) luaA_screen_get_root,
                            NULL);
    luaA_class_add_property(&screen_class, A_TK_GEOMETRY,
                            NULL,
                            (lua_class_propfunc_t) luaA_screen_get_geometry,
                            NULL);
    luaA_class_add_property(&screen_class, A_TK_WORKAREA,
                            NULL,
                            (lua_class_propfunc_t) luaA_screen_get_workarea,
                            NULL);
    luaA_class_add_property(&screen_class, A_TK_OUTPUTS,
                            NULL,
                            (lua_class_propfunc_t) luaA_screen_get_outputs,
                            NULL);
}

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
