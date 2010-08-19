/*
 * systray.c - systray handling
 *
 * Copyright © 2008-2009 Julien Danjou <julien@danjou.info>
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

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>

#include "awesome.h"
#include "screen.h"
#include "systray.h"
#include "xwindow.h"
#include "screen.h"
#include "common/atoms.h"
#include "common/luaclass_property.h"

#define SYSTEM_TRAY_REQUEST_DOCK 0 /* Begin icon docking */

static struct systray_t {
    xcb_window_t window;
    bool registered;
} _G_systray;

/** Initialize systray information in X.
 */
void
systray_init(void)
{
    xcb_client_message_event_t ev;
    char *atom_name;
    xcb_intern_atom_cookie_t atom_systray_q;
    xcb_intern_atom_reply_t *atom_systray_r;
    xcb_atom_t atom_systray;

    /* Send requests */
    if(!(atom_name = xcb_atom_name_by_screen("_NET_SYSTEM_TRAY", _G_default_screen)))
    {
        warn("error getting systray atom");
        return;
    }

    atom_systray_q = xcb_intern_atom_unchecked(_G_connection, false,
                                               a_strlen(atom_name), atom_name);

    p_delete(&atom_name);

    _G_systray.window = xcb_generate_id(_G_connection);
    xcb_create_window(_G_connection, XCB_COPY_FROM_PARENT,
                      _G_systray.window, _G_screen->root,
                      -1, -1, 1, 1, 0,
                      XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, 0, NULL);

    /* Fill event */
    p_clear(&ev, 1);
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = _G_screen->root,
    ev.format = 32;
    ev.type = MANAGER;
    ev.data.data32[0] = XCB_CURRENT_TIME;
    ev.data.data32[2] = _G_systray.window;
    ev.data.data32[3] = ev.data.data32[4] = 0;

    if(!(atom_systray_r = xcb_intern_atom_reply(_G_connection, atom_systray_q, NULL)))
    {
        warn("error getting systray atom");
        return;
    }

    ev.data.data32[1] = atom_systray = atom_systray_r->atom;

    p_delete(&atom_systray_r);

    xcb_set_selection_owner(_G_connection,
                            _G_systray.window,
                            atom_systray,
                            XCB_CURRENT_TIME);

    xcb_send_event(_G_connection, false, _G_screen->root, 0xFFFFFF, (char *) &ev);
}


/** Refresh all systrays registrations per physical screen
 */
void
systray_refresh(void)
{
}


/** Register systray in X.
 */
void
systray_register(void)
{
    xcb_client_message_event_t ev;
    xcb_screen_t *xscreen = _G_screen;
    char *atom_name;
    xcb_intern_atom_cookie_t atom_systray_q;
    xcb_intern_atom_reply_t *atom_systray_r;
    xcb_atom_t atom_systray;

    /* Set registered even if it fails to don't try again unless forced */
    if(_G_systray.registered)
        return;
    _G_systray.registered = true;

    /* Send requests */
    if(!(atom_name = xcb_atom_name_by_screen("_NET_SYSTEM_TRAY", _G_default_screen)))
    {
        warn("error getting systray atom");
        return;
    }

    atom_systray_q = xcb_intern_atom_unchecked(_G_connection, false,
                                               a_strlen(atom_name), atom_name);

    p_delete(&atom_name);

    /* Fill event */
    p_clear(&ev, 1);
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = xscreen->root;
    ev.format = 32;
    ev.type = MANAGER;
    ev.data.data32[0] = XCB_CURRENT_TIME;
    ev.data.data32[2] = _G_systray.window;
    ev.data.data32[3] = ev.data.data32[4] = 0;

    if(!(atom_systray_r = xcb_intern_atom_reply(_G_connection, atom_systray_q, NULL)))
    {
        warn("error getting systray atom");
        return;
    }

    ev.data.data32[1] = atom_systray = atom_systray_r->atom;

    p_delete(&atom_systray_r);

    xcb_set_selection_owner(_G_connection,
                            _G_systray.window,
                            atom_systray,
                            XCB_CURRENT_TIME);

    xcb_send_event(_G_connection, false, xscreen->root, 0xFFFFFF, (char *) &ev);
}

/** Remove systray information in X.
 */
void
systray_cleanup(void)
{
    xcb_intern_atom_reply_t *atom_systray_r;
    char *atom_name;

    if(!_G_systray.registered)
        return;
    _G_systray.registered = false;

    if(!(atom_name = xcb_atom_name_by_screen("_NET_SYSTEM_TRAY", _G_default_screen))
       || !(atom_systray_r = xcb_intern_atom_reply(_G_connection,
                                                   xcb_intern_atom_unchecked(_G_connection,
                                                                             false,
                                                                             a_strlen(atom_name),
                                                                             atom_name),
                                                   NULL)))
    {
        warn("error getting systray atom");
        p_delete(&atom_name);
        return;
    }

    p_delete(&atom_name);

    xcb_set_selection_owner(_G_connection,
                            XCB_NONE,
                            atom_systray_r->atom,
                            XCB_CURRENT_TIME);

    p_delete(&atom_systray_r);
}

/** Handle a systray request.
 * \param embed_win The window to embed.
 * \param info The embedding info
 * \return 0 on no error.
 */
int
systray_request_handle(xcb_window_t embed_win, xembed_info_t *info)
{
    xembed_window_t em;
    xcb_get_property_cookie_t em_cookie;
    const uint32_t select_input_val[] =
    {
        XCB_EVENT_MASK_STRUCTURE_NOTIFY
            | XCB_EVENT_MASK_PROPERTY_CHANGE
            | XCB_EVENT_MASK_ENTER_WINDOW
    };

    p_clear(&em_cookie, 1);

    if(!info)
        em_cookie = xembed_info_get_unchecked(_G_connection, embed_win);

    xcb_change_window_attributes(_G_connection, embed_win, XCB_CW_EVENT_MASK,
                                 select_input_val);
    xwindow_set_state(embed_win, XCB_WM_STATE_WITHDRAWN);

    /* we grab the window, but also make sure it's automatically reparented back
     * to the root window if we should die.
     */
    xcb_change_save_set(_G_connection, XCB_SET_MODE_INSERT, embed_win);
    xcb_reparent_window(_G_connection, embed_win,
                        _G_systray.window,
                        0, 0);

    em.win = embed_win;

    if(info)
        em.info = *info;
    else
        xembed_info_get_reply(_G_connection, em_cookie, &em.info);

    xembed_embedded_notify(_G_connection, em.win,
                           _G_systray.window,
                           MIN(XEMBED_VERSION, em.info.version));

    return 0;
}

/** Handle systray message.
 * \param ev The event.
 * \return 0 on no error.
 */
int
systray_process_client_message(xcb_client_message_event_t *ev)
{
    int ret = 0;
    xcb_get_geometry_cookie_t geom_c;
    xcb_get_geometry_reply_t *geom_r;

    switch(ev->data.data32[1])
    {
      case SYSTEM_TRAY_REQUEST_DOCK:
        geom_c = xcb_get_geometry_unchecked(_G_connection, ev->window);

        if(!(geom_r = xcb_get_geometry_reply(_G_connection, geom_c, NULL)))
            return -1;

        if(_G_screen->root == geom_r->root)
            ret = systray_request_handle(ev->data.data32[2], NULL);

        p_delete(&geom_r);
        break;
    }

    return ret;
}

/** Check if a window is a KDE tray.
 * \param w The window to check.
 * \return True if it is, false otherwise.
 */
bool
systray_iskdedockapp(xcb_window_t w)
{
    xcb_get_property_cookie_t kde_check_q;
    xcb_get_property_reply_t *kde_check;
    bool ret;

    /* Check if that is a KDE tray because it does not respect fdo standards,
     * thanks KDE. */
    kde_check_q = xcb_get_property_unchecked(_G_connection, false, w,
                                             _KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR,
                                             XCB_ATOM_WINDOW, 0, 1);

    kde_check = xcb_get_property_reply(_G_connection, kde_check_q, NULL);

    /* it's a KDE systray ?*/
    ret = (kde_check && kde_check->value_len);

    p_delete(&kde_check);

    return ret;
}

/** Handle xembed client message.
 * \param ev The event.
 * \return 0 on no error.
 */
int
xembed_process_client_message(xcb_client_message_event_t *ev)
{
    switch(ev->data.data32[1])
    {
      case XEMBED_REQUEST_FOCUS:
        xembed_focus_in(_G_connection, ev->window, XEMBED_FOCUS_CURRENT);
        break;
    }
    return 0;
}

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
