
/* gtkcellrenderertext.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_CELL_RENDERER_TEXT_H__
#define __GTK_CELL_RENDERER_TEXT_H__


#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (STLWRT_COMPILATION)
#error "Only <gtk.h> can be included directly."
#endif

#include <gtkcellrenderer.h>


G_BEGIN_DECLS


#define GTK_TYPE_CELL_RENDERER_TEXT		(gtk_cell_renderer_text_get_type ())
#define GTK_CELL_RENDERER_TEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_RENDERER_TEXT, GtkCellRendererText))
#define GTK_CELL_RENDERER_TEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_RENDERER_TEXT, GtkCellRendererTextClass))
#define GTK_IS_CELL_RENDERER_TEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_RENDERER_TEXT))
#define GTK_IS_CELL_RENDERER_TEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_RENDERER_TEXT))
#define GTK_CELL_RENDERER_TEXT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CELL_RENDERER_TEXT, GtkCellRendererTextClass))

typedef struct _GtkCellRendererText      GtkCellRendererTextFat;
typedef struct _GtkCellRendererText      GtkCellRendererTextSkinny;
typedef struct _GtkCellRendererTextClass GtkCellRendererTextClassFat;
typedef struct _GtkCellRendererTextClass GtkCellRendererTextClassSkinny;

struct _GtkCellRendererText
{
  GtkCellRenderer parent;

  /*< private >*/
  gchar * (text);
  PangoFontDescription * (font);
  gdouble  (font_scale);
  PangoColor  (foreground);
  PangoColor  (background);

  PangoAttrList * (extra_attrs);

  PangoUnderline  (underline_style);

  gint  (rise);
  gint  (fixed_height_rows);

  guint  (strikethrough) : 1;

  guint  (editable)  : 1;

  guint  (scale_set) : 1;

  guint  (foreground_set) : 1;
  guint  (background_set) : 1;

  guint  (underline_set) : 1;

  guint  (rise_set) : 1;

  guint  (strikethrough_set) : 1;

  guint  (editable_set) : 1;
  guint  (calc_fixed_height) : 1;
};

struct _GtkCellRendererTextClass
{
  GtkCellRendererClass parent_class;

  void (* edited) (GtkCellRendererText *cell_renderer_text,
		   const gchar         *path,
		   const gchar         *new_text);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType            _T2_gtk_cell_renderer_text_get_type (void) G_GNUC_CONST;
GType            _3T_gtk_cell_renderer_text_get_type (void) G_GNUC_CONST;
/* Supplied in the STLWRT public libraries */
GType            gtk_cell_renderer_text_get_type (void) G_GNUC_CONST;
GtkCellRenderer *__gtk_cell_renderer_text_new      (void);

void             __gtk_cell_renderer_text_set_fixed_height_from_font (GtkCellRendererText *renderer,
								    gint                 number_of_rows);


G_END_DECLS

#endif /* __GTK_CELL_RENDERER_TEXT_H__ */
