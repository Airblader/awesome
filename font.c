/*
 * font.c - font functions
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

#include <cairo/cairo-xcb.h>
#include <pango/pangocairo.h>

#include "font.h"
#include "awesome.h"
#include "objects/screen.h"

/** Init a new Pango font.
 * \param font The font to init.
 * \param fontname Pango fontname (e.g. [FAMILY-LIST] [STYLE-OPTIONS] [SIZE]).
 */
void
font_init(font_t *font, const char *fontname)
{
    cairo_surface_t *surface;
    cairo_t *cr;
    PangoLayout *layout;

    /* Create a dummy cairo surface, cairo context and pango layout in
     * order to get font informations */
    surface = cairo_xcb_surface_create(_G_connection,
                                       _G_default_screen,
                                       _G_visual, 1, 1);

    cr = cairo_create(surface);
    layout = pango_cairo_create_layout(cr);

    /* Get the font description used to set text on a PangoLayout */
    font->desc = pango_font_description_from_string(fontname);
    pango_layout_set_font_description(layout, font->desc);

    /* Get height */
    pango_layout_get_pixel_size(layout, NULL, &font->height);

    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

/** Delete a font.
 * \param font Font to delete.
 */
void
font_wipe(font_t *font)
{
    pango_font_description_free(font->desc);
}

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
