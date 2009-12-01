/*
 * draw.h - draw functions header
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

#ifndef AWESOME_DRAW_H
#define AWESOME_DRAW_H

#include <cairo.h>
#include <pango/pangocairo.h>

#include <xcb/xcb.h>

#include "objects/image.h"
#include "color.h"
#include "area.h"

typedef enum
{
    AlignLeft = 0,
    AlignRight,
    AlignCenter,
    AlignTop,
    AlignBottom
} alignment_t;

typedef struct
{
    cairo_t *cr;
    cairo_surface_t *surface;
    PangoLayout *layout;
    xcolor_t fg;
    xcolor_t bg;
} draw_context_t;

void draw_context_init(draw_context_t *, xcb_pixmap_t, int, int,
                       const xcolor_t *, const xcolor_t *);
void draw_context_wipe(draw_context_t *);

bool draw_iso2utf8(const char *, size_t, char **, ssize_t *);

/** Convert a string to UTF-8.
 * \param str The string to convert.
 * \param len The string length.
 * \param dest The destination string that will be allocated.
 * \param dlen The destination string length allocated, can be NULL.
 * \return True if the conversion happened, false otherwise. In both case, dest
 * and dlen will have value and dest have to be free().
 */
static inline bool
a_iso2utf8(const char *str, ssize_t len, char **dest, ssize_t *dlen)
{
    if(draw_iso2utf8(str, len, dest, dlen))
        return true;

    *dest = a_strdup(str);
    if(dlen)
        *dlen = len;

    return false;
}

typedef struct
{
    PangoAttrList *attr_list;
    char *text;
    ssize_t len;
    PangoEllipsizeMode ellip;
    PangoWrapMode wrap;
    alignment_t align, valign;
} draw_text_context_t;

bool draw_text_context_init(draw_text_context_t *, const char *, ssize_t);
void draw_text(draw_context_t *, draw_text_context_t *, area_t);
void draw_rectangle(draw_context_t *, area_t, float, bool, const color_t *);
void draw_image(draw_context_t *, area_t, alignment_t, alignment_t, image_t *);
alignment_t draw_align_fromstr(const char *, ssize_t);
const char *draw_align_tostr(alignment_t);
alignment_t draw_valign_fromstr(const char *, ssize_t);
const char *draw_valign_tostr(alignment_t);

static inline void
draw_text_context_wipe(draw_text_context_t *pdata)
{
    if(pdata)
    {
        if(pdata->attr_list)
            pango_attr_list_unref(pdata->attr_list);
        p_delete(&pdata->text);
    }
}

#endif
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80
