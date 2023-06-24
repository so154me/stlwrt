/* STLWRT - A fork of GTK+ 2 supporting future applications as well
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <stlwrt.h>
#include <gtkvbbox.h>
#include <gtkorientable.h>
#include <gtkintl.h>


/**
 * SECTION:gtkvbbox
 * @Short_description: A container for arranging buttons vertically
 * @Title: GtkVButtonBox
 * @See_also: #GtkBox, #GtkButtonBox, #GtkHButtonBox
 *
 * A button box should be used to provide a consistent layout of buttons
 * throughout your application. The layout/spacing can be altered by the
 * programmer, or if desired, by the user to alter the 'feel' of a
 * program to a small degree.
 *
 * A #GtkVButtonBox is created with __gtk_vbutton_box_new(). Buttons are
 * packed into a button box the same way widgets are added to any other
 * container, using __gtk_container_add(). You can also use
 * __gtk_box_pack_start() or __gtk_box_pack_end(), but for button boxes both
 * these functions work just like __gtk_container_add(), ie., they pack the
 * button in a way that depends on the current layout style and on
 * whether the button has had __gtk_button_box_set_child_secondary() called
 * on it.
 *
 * The spacing between buttons can be set with __gtk_box_set_spacing(). The
 * arrangement and layout of the buttons can be changed with
 * __gtk_button_box_set_layout().
 */

static gint default_spacing = 10;
static GtkButtonBoxStyle default_layout_style = GTK_BUTTONBOX_EDGE;

STLWRT_DEFINE_FTYPE_VPARENT (GtkVButtonBox, gtk_vbutton_box, GTK_TYPE_BUTTON_BOX,
                             G_TYPE_FLAG_NONE, ;)

static void
gtk_vbutton_box_class_init (GtkVButtonBoxClass *class)
{
}

static void
gtk_vbutton_box_init (GtkVButtonBox *vbutton_box)
{
  __gtk_orientable_set_orientation (GTK_ORIENTABLE (vbutton_box),
                                  GTK_ORIENTATION_VERTICAL);
}

/**
 * __gtk_vbutton_box_new:
 *
 * Creates a new vertical button box.
 *
 * Returns: a new button box #GtkWidget.
 */
GtkWidget *
__gtk_vbutton_box_new (void)
{
  return g_object_new (GTK_TYPE_VBUTTON_BOX, NULL);
}

/**
 * __gtk_vbutton_box_set_spacing_default:
 * @spacing: an integer value.
 *
 * Changes the default spacing that is placed between widgets in an
 * vertical button box.
 *
 * Deprecated: 2.0: Use __gtk_box_set_spacing() instead.
 */
void
__gtk_vbutton_box_set_spacing_default (gint spacing)
{
  default_spacing = spacing;
}

/**
 * __gtk_vbutton_box_set_layout_default:
 * @layout: a new #GtkButtonBoxStyle.
 *
 * Sets a new layout mode that will be used by all button boxes.
 *
 * Deprecated: 2.0: Use __gtk_button_box_set_layout() instead.
 */
void
__gtk_vbutton_box_set_layout_default (GtkButtonBoxStyle layout)
{
  g_return_if_fail (layout >= GTK_BUTTONBOX_DEFAULT_STYLE &&
		    layout <= GTK_BUTTONBOX_CENTER);

  default_layout_style = layout;
}

/**
 * __gtk_vbutton_box_get_spacing_default:
 *
 * Retrieves the current default spacing for vertical button boxes. This is the number of pixels
 * to be placed between the buttons when they are arranged.
 *
 * Returns: the default number of pixels between buttons.
 *
 * Deprecated: 2.0: Use __gtk_box_get_spacing() instead.
 */
gint
__gtk_vbutton_box_get_spacing_default (void)
{
  return default_spacing;
}

/**
 * ___gtk_vbutton_box_get_layout_default:
 *
 * Retrieves the current layout used to arrange buttons in button box widgets.
 *
 * Returns: the current #GtkButtonBoxStyle.
 *
 * Deprecated: 2.0: Use __gtk_button_box_get_layout() instead.
 */
GtkButtonBoxStyle
___gtk_vbutton_box_get_layout_default (void)
{
  return default_layout_style;
}
