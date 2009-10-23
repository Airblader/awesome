/*
 * systray.c - systray handling
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

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>

#include "screen.h"
#include "systray.h"
#include "xwindow.h"
#include "objects/widget.h"
#include "common/atoms.h"
#include "common/xutil.h"

#define SYSTEM_TRAY_REQUEST_DOCK 0 /* Begin icon docking */

/** Initialize systray information in X.
 * \param pscreen The protocol screen.
 */
void
systray_init(protocol_screen_t *pscreen)
{
    xcb_client_message_event_t ev;
    int phys_screen = protocol_screen_array_indexof(&protocol_screens, pscreen);
    xcb_screen_t *xscreen = xutil_screen_get(globalconf.connection, phys_screen);
    char *atom_name;
    xcb_intern_atom_cookie_t atom_systray_q;
    xcb_intern_atom_reply_t *atom_systray_r;
    xcb_atom_t atom_systray;

    /* Get atom */
    if(!(atom_name = xcb_atom_name_by_screen("_NET_SYSTEM_TRAY", phys_screen)))
    {
        warn("error getting systray atom");
        return;
    }

    atom_systray_q = xcb_intern_atom_unchecked(globalconf.connection, false,
                                               a_strlen(atom_name), atom_name);

    p_delete(&atom_name);

    /* Create systray window */
    window_new(globalconf.L);
    pscreen->systray = luaA_object_ref(globalconf.L, -1);
    pscreen->systray->parent = pscreen->root;

    pscreen->systray->geometry = (area_t) { 25, 25, 100, 100 };
    pscreen->systray->window = xcb_generate_id(globalconf.connection);
    xcb_create_window(globalconf.connection, xscreen->root_depth,
                      pscreen->systray->window, xscreen->root,
                      25, 25, 100, 100, 0,
                      XCB_COPY_FROM_PARENT, xscreen->root_visual, 0, NULL);
    xcb_map_window(globalconf.connection, pscreen->systray->window);

    /* Fill event */
    p_clear(&ev, 1);
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = xscreen->root;
    ev.format = 32;
    ev.type = MANAGER;
    ev.data.data32[0] = XCB_CURRENT_TIME;
    ev.data.data32[2] = pscreen->systray->window;
    ev.data.data32[3] = ev.data.data32[4] = 0;

    if(!(atom_systray_r = xcb_intern_atom_reply(globalconf.connection, atom_systray_q, NULL)))
    {
        warn("error getting systray atom");
        return;
    }

    ev.data.data32[1] = atom_systray = atom_systray_r->atom;

    p_delete(&atom_systray_r);

    xcb_set_selection_owner(globalconf.connection,
                            pscreen->systray->window,
                            atom_systray,
                            XCB_CURRENT_TIME);

    xcb_send_event(globalconf.connection, false, xscreen->root, 0xFFFFFF, (char *) &ev);
}

/** Remove systray information in X.
 * \param pscreen Protocol screen to cleanup.
 */
void
systray_cleanup(protocol_screen_t *pscreen)
{
    int phys_screen = protocol_screen_array_indexof(&protocol_screens, pscreen);
    xcb_intern_atom_reply_t *atom_systray_r;
    char *atom_name;

    if(!(atom_name = xcb_atom_name_by_screen("_NET_SYSTEM_TRAY", phys_screen))
       || !(atom_systray_r = xcb_intern_atom_reply(globalconf.connection,
                                                   xcb_intern_atom_unchecked(globalconf.connection,
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

    xcb_set_selection_owner(globalconf.connection,
                            XCB_NONE,
                            atom_systray_r->atom,
                            XCB_CURRENT_TIME);

    p_delete(&atom_systray_r);
}

/** Handle a systray request.
 * \param embed_win The window to embed.
 * \param pscreen The protocol screen to display on.
 * \param info The embedding info
 * \return 0 on no error.
 */
int
systray_request_handle(xcb_window_t embed_win, protocol_screen_t *pscreen, xembed_info_t *info)
{
    xembed_window_t em;
    xcb_get_property_cookie_t em_cookie;
    const uint32_t select_input_val[] =
    {
        XCB_EVENT_MASK_STRUCTURE_NOTIFY
            | XCB_EVENT_MASK_PROPERTY_CHANGE
            | XCB_EVENT_MASK_ENTER_WINDOW
    };

    /* check if not already trayed */
    if(xembed_getbywin(&pscreen->embedded, embed_win))
        return -1;

    p_clear(&em_cookie, 1);

    if(!info)
        em_cookie = xembed_info_get_unchecked(globalconf.connection, embed_win);

    xcb_change_window_attributes(globalconf.connection, embed_win, XCB_CW_EVENT_MASK,
                                 select_input_val);
    xwindow_set_state(embed_win, XCB_WM_STATE_WITHDRAWN);

    xcb_reparent_window(globalconf.connection, embed_win,
                        pscreen->systray->window, 0, 0);

    em.window = embed_win;

    if(info)
        em.info = *info;
    else
        xembed_info_get_reply(globalconf.connection, em_cookie, &em.info);

    xembed_embedded_notify(globalconf.connection, em.window, pscreen->systray->window,
                           MIN(XEMBED_VERSION, em.info.version));

    xembed_window_array_append(&pscreen->embedded, em);

    systray_reorganize(pscreen);

    return 0;
}

void
systray_reorganize(protocol_screen_t *pscreen)
{
    /* check len to avoid division by zero */
    if(!pscreen->embedded.len)
        return;

    uint32_t config_win_vals[] = { 0,
                                   0,
                                   pscreen->systray->geometry.height / pscreen->embedded.len,
                                   pscreen->systray->geometry.height };

    foreach(em, pscreen->embedded)
    {
        xcb_configure_window(globalconf.connection, em->window,
                             XCB_CONFIG_WINDOW_X
                             | XCB_CONFIG_WINDOW_Y
                             | XCB_CONFIG_WINDOW_WIDTH
                             | XCB_CONFIG_WINDOW_HEIGHT,
                             config_win_vals);
        config_win_vals[0] += config_win_vals[2];
    }
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
        geom_c = xcb_get_geometry_unchecked(globalconf.connection, ev->window);

        if(!(geom_r = xcb_get_geometry_reply(globalconf.connection, geom_c, NULL)))
            return -1;

        foreach(screen, protocol_screens)
            if(screen->root->window == geom_r->root)
            {
                ret = systray_request_handle(ev->data.data32[2], screen, NULL);
                break;
            }

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
    kde_check_q = xcb_get_property_unchecked(globalconf.connection, false, w,
                                             _KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR,
                                             WINDOW, 0, 1);

    kde_check = xcb_get_property_reply(globalconf.connection, kde_check_q, NULL);

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
        xembed_focus_in(globalconf.connection, ev->window, XEMBED_FOCUS_CURRENT);
        break;
    }
    return 0;
}

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
