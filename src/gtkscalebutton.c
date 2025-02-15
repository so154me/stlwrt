/* STLWRT - A fork of GTK+ 2 supporting future applications as well
 * Copyright (C) 2005 Ronald S. Bultje
 * Copyright (C) 2006, 2007 Christian Persch
 * Copyright (C) 2006 Jan Arne Petersen
 * Copyright (C) 2005-2007 Red Hat, Inc.
 *
 * Authors:
 * - Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * - Bastien Nocera <bnocera@redhat.com>
 * - Jan Arne Petersen <jpetersen@jpetersen.org>
 * - Christian Persch <chpe@svn.gnome.org>
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
 * Modified by the GTK+ Team and others 2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <stlwrt.h>

#ifndef _WIN32
#define _GNU_SOURCE
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdkkeysyms.h>

#include <gtkbindings.h>
#include <gtkframe.h>
#include <gtkmain.h>

#include <gtkorientable.h>
#include <gtkprivate.h>
#include <gtkscale.h>
#include <gtkscalebutton.h>
#include <gtkstock.h>
#include <gtkvbox.h>
#include <gtkwindow.h>

#include <gtkintl.h>


#define SCALE_SIZE 100
#define CLICK_TIMEOUT 250

enum
{
  VALUE_CHANGED,
  POPUP,
  POPDOWN,

  LAST_SIGNAL
};

enum
{
  PROP_0,

  PROP_ORIENTATION,
  PROP_VALUE,
  PROP_SIZE,
  PROP_ADJUSTMENT,
  PROP_ICONS
};




static GObject* gtk_scale_button_constructor    (GType                  type,
                                                 guint                  n_construct_properties,
                                                 GObjectConstructParam *construct_params);
static void	gtk_scale_button_dispose	(GObject             *object);
static void     gtk_scale_button_finalize       (GObject             *object);
static void	gtk_scale_button_set_property	(GObject             *object,
						 guint                prop_id,
						 const GValue        *value,
						 GParamSpec          *pspec);
static void	gtk_scale_button_get_property	(GObject             *object,
						 guint                prop_id,
						 GValue              *value,
						 GParamSpec          *pspec);
static void __gtk_scale_button_set_orientation_private (GtkScaleButton *button,
                                                      GtkOrientation  orientation);
static gboolean	gtk_scale_button_scroll		(GtkWidget           *widget,
						 GdkEventScroll      *event);
static void gtk_scale_button_screen_changed	(GtkWidget           *widget,
						 GdkScreen           *previous_screen);
static gboolean	gtk_scale_button_press		(GtkWidget           *widget,
						 GdkEventButton      *event);
static gboolean gtk_scale_button_key_release	(GtkWidget           *widget,
    						 GdkEventKey         *event);
static void     gtk_scale_button_popup          (GtkWidget           *widget);
static void     gtk_scale_button_popdown        (GtkWidget           *widget);
static gboolean cb_dock_button_press		(GtkWidget           *widget,
						 GdkEventButton      *event,
						 gpointer             user_data);
static gboolean cb_dock_key_release		(GtkWidget           *widget,
						 GdkEventKey         *event,
						 gpointer             user_data);
static gboolean cb_button_press			(GtkWidget           *widget,
						 GdkEventButton      *event,
						 gpointer             user_data);
static gboolean cb_button_release		(GtkWidget           *widget,
						 GdkEventButton      *event,
						 gpointer             user_data);
static void cb_dock_grab_notify			(GtkWidget           *widget,
						 gboolean             was_grabbed,
						 gpointer             user_data);
static gboolean cb_dock_grab_broken_event	(GtkWidget           *widget,
						 gboolean             was_grabbed,
						 gpointer             user_data);
static void cb_scale_grab_notify		(GtkWidget           *widget,
						 gboolean             was_grabbed,
						 gpointer             user_data);
static void gtk_scale_button_update_icon	(GtkScaleButton      *button);
static void gtk_scale_button_scale_value_changed(GtkRange            *range);

/* see below for scale definitions */
static GtkWidget *gtk_scale_button_scale_new    (GtkScaleButton      *button);

struct _GtkScaleButtonPrivate
{
  GtkWidget *dock;
  GtkWidget *box;
  GtkWidget *scale;
  GtkWidget *image;

  GtkIconSize size;
  GtkOrientation orientation;

  guint click_id;
  gint click_timeout;
  guint timeout : 1;
  gdouble direction;
  guint32 pop_time;

  gchar **icon_list;

  GtkAdjustment *adjustment; /* needed because it must be settable in init() */
};

STLWRT_DEFINE_VTYPE (GtkScaleButton, gtk_scale_button, GTK_TYPE_BUTTON, G_TYPE_FLAG_NONE,
                     G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                     G_ADD_PRIVATE (GtkScaleButton))

static guint signals[LAST_SIGNAL] = { 0, };

static void
gtk_scale_button_class_init (GtkScaleButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set;

  gobject_class->constructor = gtk_scale_button_constructor;
  gobject_class->finalize = gtk_scale_button_finalize;
  gobject_class->dispose = gtk_scale_button_dispose;
  gobject_class->set_property = gtk_scale_button_set_property;
  gobject_class->get_property = gtk_scale_button_get_property;

  widget_class->button_press_event = gtk_scale_button_press;
  widget_class->key_release_event = gtk_scale_button_key_release;
  widget_class->scroll_event = gtk_scale_button_scroll;
  widget_class->screen_changed = gtk_scale_button_screen_changed;

  /**
   * GtkScaleButton:orientation:
   *
   * The orientation of the #GtkScaleButton's popup window.
   *
   * Note that since GTK+ 2.16, #GtkScaleButton implements the
   * #GtkOrientable interface which has its own @orientation
   * property. However we redefine the property here in order to
   * override its default horizontal orientation.
   *
   * Since: 2.14
   **/
  g_object_class_override_property (gobject_class,
				    PROP_ORIENTATION,
				    "orientation");

  g_object_class_install_property (gobject_class,
				   PROP_VALUE,
				   g_param_spec_double ("value",
							P_("Value"),
							P_("The value of the scale"),
							-G_MAXDOUBLE,
							G_MAXDOUBLE,
							0,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_SIZE,
				   g_param_spec_enum ("size",
						      P_("Icon size"),
						      P_("The icon size"),
						      GTK_TYPE_ICON_SIZE,
						      GTK_ICON_SIZE_SMALL_TOOLBAR,
						      GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment",
							P_("Adjustment"),
							P_("The GtkAdjustment that contains the current value of this scale button object"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkScaleButton:icons:
   *
   * The names of the icons to be used by the scale button.
   * The first item in the array will be used in the button
   * when the current value is the lowest value, the second
   * item for the highest value. All the subsequent icons will
   * be used for all the other values, spread evenly over the
   * range of values.
   *
   * If there's only one icon name in the @icons array, it will
   * be used for all the values. If only two icon names are in
   * the @icons array, the first one will be used for the bottom
   * 50% of the scale, and the second one for the top 50%.
   *
   * It is recommended to use at least 3 icons so that the
   * #GtkScaleButton reflects the current value of the scale
   * better for the users.
   *
   * Since: 2.12
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ICONS,
                                   g_param_spec_boxed ("icons",
                                                       P_("Icons"),
                                                       P_("List of icon names"),
                                                       G_TYPE_STRV,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkScaleButton::value-changed:
   * @button: the object which received the signal
   * @value: the new value
   *
   * The ::value-changed signal is emitted when the value field has
   * changed.
   *
   * Since: 2.12
   */
  signals[VALUE_CHANGED] =
    g_signal_new (I_("value-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkScaleButtonClass, value_changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  /**
   * GtkScaleButton::popup:
   * @button: the object which received the signal
   *
   * The ::popup signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to popup the scale widget.
   *
   * The default bindings for this signal are Space, Enter and Return.
   *
   * Since: 2.12
   */
  signals[POPUP] =
    g_signal_new_class_handler (I_("popup"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_scale_button_popup),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /**
   * GtkScaleButton::popdown:
   * @button: the object which received the signal
   *
   * The ::popdown signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to popdown the scale widget.
   *
   * The default binding for this signal is Escape.
   *
   * Since: 2.12
   */
  signals[POPDOWN] =
    g_signal_new_class_handler (I_("popdown"),
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_scale_button_popdown),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /* Key bindings */
  binding_set = __gtk_binding_set_by_class (widget_class);

  __gtk_binding_entry_add_signal (binding_set, GDK_space, 0,
				"popup", 0);
  __gtk_binding_entry_add_signal (binding_set, GDK_KP_Space, 0,
				"popup", 0);
  __gtk_binding_entry_add_signal (binding_set, GDK_Return, 0,
				"popup", 0);
  __gtk_binding_entry_add_signal (binding_set, GDK_ISO_Enter, 0,
				"popup", 0);
  __gtk_binding_entry_add_signal (binding_set, GDK_KP_Enter, 0,
				"popup", 0);
  __gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0,
				"popdown", 0);
}

static void
gtk_scale_button_init (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv;
  GtkWidget *frame;

  gtk_scale_button_get_props (button)->priv = priv = gtk_scale_button_get_instance_private (button);

  priv->timeout = FALSE;
  priv->click_id = 0;
  priv->click_timeout = CLICK_TIMEOUT;
  priv->orientation = GTK_ORIENTATION_VERTICAL;

  __gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  __gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);

  /* image */
  priv->image = __gtk_image_new ();
  __gtk_container_add (GTK_CONTAINER (button), priv->image);
  __gtk_widget_show_all (priv->image);

  /* window */
  priv->dock = __gtk_window_new (GTK_WINDOW_POPUP);
  __gtk_widget_set_name (priv->dock, "gtk-scalebutton-popup-window");
  g_signal_connect (priv->dock, "button-press-event",
		    G_CALLBACK (cb_dock_button_press), button);
  g_signal_connect (priv->dock, "key-release-event",
		    G_CALLBACK (cb_dock_key_release), button);
  g_signal_connect (priv->dock, "grab-notify",
		    G_CALLBACK (cb_dock_grab_notify), button);
  g_signal_connect (priv->dock, "grab-broken-event",
		    G_CALLBACK (cb_dock_grab_broken_event), button);
  __gtk_window_set_decorated (GTK_WINDOW (priv->dock), FALSE);

  /* frame */
  frame = __gtk_frame_new (NULL);
  __gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  __gtk_container_add (GTK_CONTAINER (priv->dock), frame);

  /* box for scale and +/- buttons */
  priv->box = __gtk_vbox_new (FALSE, 0);
  __gtk_container_add (GTK_CONTAINER (frame), priv->box);

  /* + */
  gtk_scale_button_get_props (button)->plus_button = __gtk_button_new_with_label ("+");
  __gtk_button_set_relief (GTK_BUTTON (gtk_scale_button_get_props (button)->plus_button), GTK_RELIEF_NONE);
  g_signal_connect (gtk_scale_button_get_props (button)->plus_button, "button-press-event",
		    G_CALLBACK (cb_button_press), button);
  g_signal_connect (gtk_scale_button_get_props (button)->plus_button, "button-release-event",
		    G_CALLBACK (cb_button_release), button);
  __gtk_box_pack_start (GTK_BOX (priv->box), gtk_scale_button_get_props (button)->plus_button, FALSE, FALSE, 0);

  /* - */
  gtk_scale_button_get_props (button)->minus_button = __gtk_button_new_with_label ("-");
  __gtk_button_set_relief (GTK_BUTTON (gtk_scale_button_get_props (button)->minus_button), GTK_RELIEF_NONE);
  g_signal_connect (gtk_scale_button_get_props (button)->minus_button, "button-press-event",
		   G_CALLBACK (cb_button_press), button);
  g_signal_connect (gtk_scale_button_get_props (button)->minus_button, "button-release-event",
		    G_CALLBACK (cb_button_release), button);
  __gtk_box_pack_end (GTK_BOX (priv->box), gtk_scale_button_get_props (button)->minus_button, FALSE, FALSE, 0);

  priv->adjustment = GTK_ADJUSTMENT (__gtk_adjustment_new (0.0, 0.0, 100.0, 2, 20, 0));
  g_object_ref_sink (priv->adjustment);

  /* the scale */
  priv->scale = gtk_scale_button_scale_new (button);
  __gtk_container_add (GTK_CONTAINER (priv->box), priv->scale);
}

static GObject *
gtk_scale_button_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_params)
{
  GObject *object;
  GtkScaleButton *button;
  GtkScaleButtonPrivate *priv;

  object = G_OBJECT_CLASS (gtk_scale_button_parent_class)->constructor (type, n_construct_properties, construct_params);

  button = GTK_SCALE_BUTTON (object);

  priv = gtk_scale_button_get_props (button)->priv;

  /* set button text and size */
  priv->size = GTK_ICON_SIZE_SMALL_TOOLBAR;
  gtk_scale_button_update_icon (button);

  return object;
}

static void
gtk_scale_button_set_property (GObject       *object,
			       guint          prop_id,
			       const GValue  *value,
			       GParamSpec    *pspec)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      __gtk_scale_button_set_orientation_private (button, g_value_get_enum (value));
      break;
    case PROP_VALUE:
      __gtk_scale_button_set_value (button, g_value_get_double (value));
      break;
    case PROP_SIZE:
      {
	GtkIconSize size;
	size = g_value_get_enum (value);
	if (gtk_scale_button_get_props (button)->priv->size != size)
	  {
	    gtk_scale_button_get_props (button)->priv->size = size;
	    gtk_scale_button_update_icon (button);
	  }
      }
      break;
    case PROP_ADJUSTMENT:
      __gtk_scale_button_set_adjustment (button, g_value_get_object (value));
      break;
    case PROP_ICONS:
      __gtk_scale_button_set_icons (button,
                                  (const gchar **)g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scale_button_get_property (GObject     *object,
			       guint        prop_id,
			       GValue      *value,
			       GParamSpec  *pspec)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_props (button)->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_VALUE:
      g_value_set_double (value, __gtk_scale_button_get_value (button));
      break;
    case PROP_SIZE:
      g_value_set_enum (value, priv->size);
      break;
    case PROP_ADJUSTMENT:
      g_value_set_object (value, __gtk_scale_button_get_adjustment (button));
      break;
    case PROP_ICONS:
      g_value_set_boxed (value, priv->icon_list);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scale_button_finalize (GObject *object)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_props (button)->priv;

  if (priv->icon_list)
    {
      g_strfreev (priv->icon_list);
      priv->icon_list = NULL;
    }

  if (priv->adjustment)
    {
      g_object_unref (priv->adjustment);
      priv->adjustment = NULL;
    }

  G_OBJECT_CLASS (gtk_scale_button_parent_class)->finalize (object);
}

static void
gtk_scale_button_dispose (GObject *object)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_props (button)->priv;

  if (priv->dock)
    {
      __gtk_widget_destroy (priv->dock);
      priv->dock = NULL;
    }

  if (priv->click_id != 0)
    {
      g_source_remove (priv->click_id);
      priv->click_id = 0;
    }

  G_OBJECT_CLASS (gtk_scale_button_parent_class)->dispose (object);
}

/**
 * __gtk_scale_button_new:
 * @size: (int): a stock icon size
 * @min: the minimum value of the scale (usually 0)
 * @max: the maximum value of the scale (usually 100)
 * @step: the stepping of value when a scroll-wheel event,
 *        or up/down arrow event occurs (usually 2)
 * @icons: (allow-none) (array zero-terminated=1): a %NULL-terminated
 *         array of icon names, or %NULL if you want to set the list
 *         later with __gtk_scale_button_set_icons()
 *
 * Creates a #GtkScaleButton, with a range between @min and @max, with
 * a stepping of @step.
 *
 * Return value: a new #GtkScaleButton
 *
 * Since: 2.12
 */
GtkWidget *
__gtk_scale_button_new (GtkIconSize   size,
		      gdouble       min,
		      gdouble       max,
		      gdouble       step,
		      const gchar **icons)
{
  GtkScaleButton *button;
  GObject *adj;

  adj = __gtk_adjustment_new (min, min, max, step, 10 * step, 0);

  button = g_object_new (GTK_TYPE_SCALE_BUTTON,
                         "adjustment", adj,
                         "icons", icons,
                         "size", size,
                         NULL);

  return GTK_WIDGET (button);
}

/**
 * __gtk_scale_button_get_value:
 * @button: a #GtkScaleButton
 *
 * Gets the current value of the scale button.
 *
 * Return value: current value of the scale button
 *
 * Since: 2.12
 */
gdouble
__gtk_scale_button_get_value (GtkScaleButton * button)
{
  GtkScaleButtonPrivate *priv;

  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), 0);

  priv = gtk_scale_button_get_props (button)->priv;

  return __gtk_adjustment_get_value (priv->adjustment);
}

/**
 * __gtk_scale_button_set_value:
 * @button: a #GtkScaleButton
 * @value: new value of the scale button
 *
 * Sets the current value of the scale; if the value is outside
 * the minimum or maximum range values, it will be clamped to fit
 * inside them. The scale button emits the #GtkScaleButton::value-changed
 * signal if the value changes.
 *
 * Since: 2.12
 */
void
__gtk_scale_button_set_value (GtkScaleButton *button,
			    gdouble         value)
{
  GtkScaleButtonPrivate *priv;

  g_return_if_fail (GTK_IS_SCALE_BUTTON (button));

  priv = gtk_scale_button_get_props (button)->priv;

  __gtk_range_set_value (GTK_RANGE (priv->scale), value);
}

/**
 * __gtk_scale_button_set_icons:
 * @button: a #GtkScaleButton
 * @icons: (array zero-terminated=1): a %NULL-terminated array of icon names
 *
 * Sets the icons to be used by the scale button.
 * For details, see the #GtkScaleButton:icons property.
 *
 * Since: 2.12
 */
void
__gtk_scale_button_set_icons (GtkScaleButton  *button,
			    const gchar    **icons)
{
  GtkScaleButtonPrivate *priv;
  gchar **tmp;

  g_return_if_fail (GTK_IS_SCALE_BUTTON (button));

  priv = gtk_scale_button_get_props (button)->priv;

  tmp = priv->icon_list;
  priv->icon_list = g_strdupv ((gchar **) icons);
  g_strfreev (tmp);
  gtk_scale_button_update_icon (button);

  g_object_notify (G_OBJECT (button), "icons");
}

/**
 * __gtk_scale_button_get_adjustment:
 * @button: a #GtkScaleButton
 *
 * Gets the #GtkAdjustment associated with the #GtkScaleButton's scale.
 * See __gtk_range_get_adjustment() for details.
 *
 * Returns: (transfer none): the adjustment associated with the scale
 *
 * Since: 2.12
 */
GtkAdjustment*
__gtk_scale_button_get_adjustment	(GtkScaleButton *button)
{
  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), NULL);

  return gtk_scale_button_get_props (button)->priv->adjustment;
}

/**
 * __gtk_scale_button_set_adjustment:
 * @button: a #GtkScaleButton
 * @adjustment: a #GtkAdjustment
 *
 * Sets the #GtkAdjustment to be used as a model
 * for the #GtkScaleButton's scale.
 * See __gtk_range_set_adjustment() for details.
 *
 * Since: 2.12
 */
void
__gtk_scale_button_set_adjustment	(GtkScaleButton *button,
				 GtkAdjustment  *adjustment)
{
  g_return_if_fail (GTK_IS_SCALE_BUTTON (button));
  if (!adjustment)
    adjustment = (GtkAdjustment*) __gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  else
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (gtk_scale_button_get_props (button)->priv->adjustment != adjustment)
    {
      if (gtk_scale_button_get_props (button)->priv->adjustment)
        g_object_unref (gtk_scale_button_get_props (button)->priv->adjustment);
      gtk_scale_button_get_props (button)->priv->adjustment = g_object_ref_sink (adjustment);

      if (gtk_scale_button_get_props (button)->priv->scale)
        __gtk_range_set_adjustment (GTK_RANGE (gtk_scale_button_get_props (button)->priv->scale), adjustment);

      g_object_notify (G_OBJECT (button), "adjustment");
    }
}

/**
 * __gtk_scale_button_get_orientation:
 * @button: a #GtkScaleButton
 *
 * Gets the orientation of the #GtkScaleButton's popup window.
 *
 * Returns: the #GtkScaleButton's orientation.
 *
 * Since: 2.14
 *
 * Deprecated: 2.16: Use __gtk_orientable_get_orientation() instead.
 **/
GtkOrientation
__gtk_scale_button_get_orientation (GtkScaleButton *button)
{
  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), GTK_ORIENTATION_VERTICAL);

  return gtk_scale_button_get_props (button)->priv->orientation;
}

/**
 * __gtk_scale_button_set_orientation:
 * @button: a #GtkScaleButton
 * @orientation: the new orientation
 *
 * Sets the orientation of the #GtkScaleButton's popup window.
 *
 * Since: 2.14
 *
 * Deprecated: 2.16: Use __gtk_orientable_set_orientation() instead.
 **/
void
__gtk_scale_button_set_orientation (GtkScaleButton *button,
                                  GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_SCALE_BUTTON (button));

  __gtk_scale_button_set_orientation_private (button, orientation);
}

/**
 * __gtk_scale_button_get_plus_button:
 * @button: a #GtkScaleButton
 *
 * Retrieves the plus button of the #GtkScaleButton.
 *
 * Returns: (transfer none): the plus button of the #GtkScaleButton
 *
 * Since: 2.14
 */
GtkWidget *
__gtk_scale_button_get_plus_button (GtkScaleButton *button)
{
  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), NULL);

  return gtk_scale_button_get_props (button)->plus_button;
}

/**
 * __gtk_scale_button_get_minus_button:
 * @button: a #GtkScaleButton
 *
 * Retrieves the minus button of the #GtkScaleButton.
 *
 * Returns: (transfer none): the minus button of the #GtkScaleButton
 *
 * Since: 2.14
 */
GtkWidget *
__gtk_scale_button_get_minus_button (GtkScaleButton *button)
{
  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), NULL);

  return gtk_scale_button_get_props (button)->minus_button;
}

/**
 * __gtk_scale_button_get_popup:
 * @button: a #GtkScaleButton
 *
 * Retrieves the popup of the #GtkScaleButton.
 *
 * Returns: (transfer none): the popup of the #GtkScaleButton
 *
 * Since: 2.14
 */
GtkWidget *
__gtk_scale_button_get_popup (GtkScaleButton *button)
{
  g_return_val_if_fail (GTK_IS_SCALE_BUTTON (button), NULL);

  return gtk_scale_button_get_props (button)->priv->dock;
}

static void
__gtk_scale_button_set_orientation_private (GtkScaleButton *button,
                                          GtkOrientation  orientation)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_props (button)->priv;

  if (orientation != priv->orientation)
    {
      priv->orientation = orientation;

      __gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->box),
                                      orientation);
      __gtk_container_child_set (GTK_CONTAINER (priv->box),
                               gtk_scale_button_get_props (button)->plus_button,
                               "pack-type",
                               orientation == GTK_ORIENTATION_VERTICAL ?
                               GTK_PACK_START : GTK_PACK_END,
                               NULL);
      __gtk_container_child_set (GTK_CONTAINER (priv->box),
                               gtk_scale_button_get_props (button)->minus_button,
                               "pack-type",
                               orientation == GTK_ORIENTATION_VERTICAL ?
                               GTK_PACK_END : GTK_PACK_START,
                               NULL);

      __gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->scale),
                                      orientation);

      if (orientation == GTK_ORIENTATION_VERTICAL)
        {
          __gtk_widget_set_size_request (GTK_WIDGET (priv->scale),
                                       -1, SCALE_SIZE);
          __gtk_range_set_inverted (GTK_RANGE (priv->scale), TRUE);
        }
      else
        {
          __gtk_widget_set_size_request (GTK_WIDGET (priv->scale),
                                       SCALE_SIZE, -1);
          __gtk_range_set_inverted (GTK_RANGE (priv->scale), FALSE);
        }

      /* FIXME: without this, the popup window appears as a square
       * after changing the orientation
       */
      __gtk_window_resize (GTK_WINDOW (priv->dock), 1, 1);

      g_object_notify (G_OBJECT (button), "orientation");
    }
}

/*
 * button callbacks.
 */

static gboolean
gtk_scale_button_scroll (GtkWidget      *widget,
			 GdkEventScroll *event)
{
  GtkScaleButton *button;
  GtkScaleButtonPrivate *priv;
  GtkAdjustment *adj;
  gdouble d;

  button = GTK_SCALE_BUTTON (widget);
  priv = gtk_scale_button_get_props (button)->priv;
  adj = priv->adjustment;

  if (event->type != GDK_SCROLL)
    return FALSE;

  d = __gtk_scale_button_get_value (button);
  if (event->direction == GDK_SCROLL_UP)
    {
      d += gtk_adjustment_get_props (adj)->step_increment;
      if (d > gtk_adjustment_get_props (adj)->upper)
	d = gtk_adjustment_get_props (adj)->upper;
    }
  else
    {
      d -= gtk_adjustment_get_props (adj)->step_increment;
      if (d < gtk_adjustment_get_props (adj)->lower)
	d = gtk_adjustment_get_props (adj)->lower;
    }
  __gtk_scale_button_set_value (button, d);

  return TRUE;
}

static void
gtk_scale_button_screen_changed (GtkWidget *widget,
				 GdkScreen *previous_screen)
{
  GtkScaleButton *button = (GtkScaleButton *) widget;
  GtkScaleButtonPrivate *priv;
  GdkScreen *screen;
  GValue value = { 0, };

  if (__gtk_widget_has_screen (widget) == FALSE)
    return;

  priv = gtk_scale_button_get_props (button)->priv;

  screen = __gtk_widget_get_screen (widget);
  g_value_init (&value, G_TYPE_INT);
  if (__gdk_screen_get_setting (screen,
			      "gtk-double-click-time",
			      &value) == FALSE)
    {
      priv->click_timeout = CLICK_TIMEOUT;
      return;
    }

  priv->click_timeout = g_value_get_int (&value);
}

static gboolean
gtk_scale_popup (GtkWidget *widget,
		 GdkEvent  *event,
		 guint32    time)
{
  GtkScaleButton *button;
  GtkScaleButtonPrivate *priv;
  GtkAdjustment *adj;
  gint x, y, m, dx, dy, sx, sy, startoff;
  gdouble v;
  GdkDisplay *display;
  GdkScreen *screen;
  gboolean is_moved;

  is_moved = FALSE;
  button = GTK_SCALE_BUTTON (widget);
  priv = gtk_scale_button_get_props (button)->priv;
  adj = priv->adjustment;

  display = __gtk_widget_get_display (widget);
  screen = __gtk_widget_get_screen (widget);

  /* position roughly */
  __gtk_window_set_screen (GTK_WINDOW (priv->dock), screen);

  __gdk_window_get_origin (gtk_widget_get_props (widget)->window, &x, &y);
  x += gtk_widget_get_props (widget)->allocation.x;
  y += gtk_widget_get_props (widget)->allocation.y;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    __gtk_window_move (GTK_WINDOW (priv->dock), x, y - (SCALE_SIZE / 2));
  else
    __gtk_window_move (GTK_WINDOW (priv->dock), x - (SCALE_SIZE / 2), y);

  __gtk_widget_show_all (priv->dock);

  __gdk_window_get_origin (gtk_widget_get_props (priv->dock)->window, &dx, &dy);
  dx += gtk_widget_get_props (priv->dock)->allocation.x;
  dy += gtk_widget_get_props (priv->dock)->allocation.y;

  __gdk_window_get_origin (gtk_widget_get_props (priv->scale)->window, &sx, &sy);
  sx += gtk_widget_get_props (priv->scale)->allocation.x;
  sy += gtk_widget_get_props (priv->scale)->allocation.y;

  priv->timeout = TRUE;

  /* position (needs widget to be shown already) */
  v = __gtk_scale_button_get_value (button) / (gtk_adjustment_get_props (adj)->upper - gtk_adjustment_get_props (adj)->lower);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      startoff = sy - dy;

      x += (gtk_widget_get_props (widget)->allocation.width - gtk_widget_get_props (priv->dock)->allocation.width) / 2;
      y -= startoff;
      y -= gtk_range_get_props (GTK_RANGE (priv->scale))->min_slider_size / 2;
      m = gtk_widget_get_props (priv->scale)->allocation.height -
          gtk_range_get_props (GTK_RANGE (priv->scale))->min_slider_size;
      y -= m * (1.0 - v);
    }
  else
    {
      startoff = sx - dx;

      x -= startoff;
      y += (gtk_widget_get_props (widget)->allocation.height - gtk_widget_get_props (priv->dock)->allocation.height) / 2;
      x -= gtk_range_get_props (GTK_RANGE (priv->scale))->min_slider_size / 2;
      m = gtk_widget_get_props (priv->scale)->allocation.width -
          gtk_range_get_props (GTK_RANGE (priv->scale))->min_slider_size;
      x -= m * v;
    }

  /* Make sure the dock stays inside the monitor */
  if (event->type == GDK_BUTTON_PRESS)
    {
      int monitor;
      GdkEventButton *button_event = (GdkEventButton *) event;
      GdkRectangle rect;
      GtkWidget *d;

      d = GTK_WIDGET (priv->dock);
      monitor = __gdk_screen_get_monitor_at_point (screen,
						 button_event->x_root,
						 button_event->y_root);
      __gdk_screen_get_monitor_geometry (screen, monitor, &rect);

      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        y += button_event->y;
      else
        x += button_event->x;

      /* Move the dock, but set is_moved so we
       * don't forward the first click later on,
       * as it could make the scale go to the bottom */
      if (y < rect.y) {
	y = rect.y;
	is_moved = TRUE;
      } else if (y + gtk_widget_get_props (d)->allocation.height > rect.height + rect.y) {
	y = rect.y + rect.height - gtk_widget_get_props (d)->allocation.height;
	is_moved = TRUE;
      }

      if (x < rect.x) {
	x = rect.x;
	is_moved = TRUE;
      } else if (x + gtk_widget_get_props (d)->allocation.width > rect.width + rect.x) {
	x = rect.x + rect.width - gtk_widget_get_props (d)->allocation.width;
	is_moved = TRUE;
      }
    }

  __gtk_window_move (GTK_WINDOW (priv->dock), x, y);

  if (event->type == GDK_BUTTON_PRESS)
    GTK_WIDGET_CLASS (gtk_scale_button_parent_class)->button_press_event (widget, (GdkEventButton *) event);

  /* grab focus */
  __gtk_grab_add (priv->dock);

  if (__gdk_pointer_grab (gtk_widget_get_props (priv->dock)->window, TRUE,
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			GDK_POINTER_MOTION_MASK, NULL, NULL, time)
      != GDK_GRAB_SUCCESS)
    {
      __gtk_grab_remove (priv->dock);
      __gtk_widget_hide (priv->dock);
      return FALSE;
    }

  if (__gdk_keyboard_grab (gtk_widget_get_props (priv->dock)->window, TRUE, time) != GDK_GRAB_SUCCESS)
    {
      __gdk_display_pointer_ungrab (display, time);
      __gtk_grab_remove (priv->dock);
      __gtk_widget_hide (priv->dock);
      return FALSE;
    }

  __gtk_widget_grab_focus (priv->dock);

  if (event->type == GDK_BUTTON_PRESS && !is_moved)
    {
      GdkEventButton *e;
      GdkEventButton *button_event = (GdkEventButton *) event;

      /* forward event to the slider */
      e = (GdkEventButton *) __gdk_event_copy ((GdkEvent *) event);
      e->window = gtk_widget_get_props (priv->scale)->window;

      /* position: the X position isn't relevant, halfway will work just fine.
       * The vertical position should be *exactly* in the middle of the slider
       * of the scale; if we don't do that correctly, it'll move from its current
       * position, which means a position change on-click, which is bad.
       */
      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        {
          e->x = gtk_widget_get_props (priv->scale)->allocation.width / 2;
          m = gtk_widget_get_props (priv->scale)->allocation.height -
              gtk_range_get_props (GTK_RANGE (priv->scale))->min_slider_size;
          e->y = ((1.0 - v) * m) + gtk_range_get_props (GTK_RANGE (priv->scale))->min_slider_size / 2;
        }
      else
        {
          e->y = gtk_widget_get_props (priv->scale)->allocation.height / 2;
          m = gtk_widget_get_props (priv->scale)->allocation.width -
              gtk_range_get_props (GTK_RANGE (priv->scale))->min_slider_size;
          e->x = (v * m) + gtk_range_get_props (GTK_RANGE (priv->scale))->min_slider_size / 2;
        }

      __gtk_widget_event (priv->scale, (GdkEvent *) e);
      e->window = button_event->window;
      __gdk_event_free ((GdkEvent *) e);
    }

  __gtk_widget_grab_focus (priv->scale);

  priv->pop_time = time;

  return TRUE;
}

static gboolean
gtk_scale_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  return gtk_scale_popup (widget, (GdkEvent *) event, event->time);
}

static void
gtk_scale_button_popup (GtkWidget *widget)
{
  GdkEvent *ev;

  ev = __gdk_event_new (GDK_KEY_RELEASE);
  gtk_scale_popup (widget, ev, GDK_CURRENT_TIME);
  __gdk_event_free (ev);
}

static gboolean
gtk_scale_button_key_release (GtkWidget   *widget,
			      GdkEventKey *event)
{
  return __gtk_bindings_activate_event (G_OBJECT (widget), event);
}

/* This is called when the grab is broken for
 * either the dock, or the scale itself */
static void
gtk_scale_button_grab_notify (GtkScaleButton *button,
			      gboolean        was_grabbed)
{
  GdkDisplay *display;
  GtkScaleButtonPrivate *priv;

  if (was_grabbed != FALSE)
    return;

  priv = gtk_scale_button_get_props (button)->priv;

  if (!__gtk_widget_has_grab (priv->dock))
    return;

  if (__gtk_widget_is_ancestor (__gtk_grab_get_current (), priv->dock))
    return;

  display = __gtk_widget_get_display (priv->dock);
  __gdk_display_keyboard_ungrab (display, GDK_CURRENT_TIME);
  __gdk_display_pointer_ungrab (display, GDK_CURRENT_TIME);
  __gtk_grab_remove (priv->dock);

  /* hide again */
  __gtk_widget_hide (priv->dock);
  priv->timeout = FALSE;
}

/*
 * +/- button callbacks.
 */

static gboolean
cb_button_timeout (gpointer user_data)
{
  GtkScaleButton *button;
  GtkScaleButtonPrivate *priv;
  GtkAdjustment *adj;
  gdouble val;
  gboolean res = TRUE;

  button = GTK_SCALE_BUTTON (user_data);
  priv = gtk_scale_button_get_props (button)->priv;

  if (priv->click_id == 0)
    return FALSE;

  adj = priv->adjustment;

  val = __gtk_scale_button_get_value (button);
  val += priv->direction;
  if (val <= gtk_adjustment_get_props (adj)->lower)
    {
      res = FALSE;
      val = gtk_adjustment_get_props (adj)->lower;
    }
  else if (val > gtk_adjustment_get_props (adj)->upper)
    {
      res = FALSE;
      val = gtk_adjustment_get_props (adj)->upper;
    }
  __gtk_scale_button_set_value (button, val);

  if (!res)
    {
      g_source_remove (priv->click_id);
      priv->click_id = 0;
    }

  return res;
}

static gboolean
cb_button_press (GtkWidget      *widget,
		 GdkEventButton *event,
		 gpointer        user_data)
{
  GtkScaleButton *button;
  GtkScaleButtonPrivate *priv;
  GtkAdjustment *adj;

  button = GTK_SCALE_BUTTON (user_data);
  priv = gtk_scale_button_get_props (button)->priv;
  adj = priv->adjustment;

  if (priv->click_id != 0)
    g_source_remove (priv->click_id);

  if (widget == gtk_scale_button_get_props (button)->plus_button)
    priv->direction = fabs (gtk_adjustment_get_props (adj)->page_increment);
  else
    priv->direction = - fabs (gtk_adjustment_get_props (adj)->page_increment);

  priv->click_id = __gdk_threads_add_timeout (priv->click_timeout,
                                            cb_button_timeout,
                                            button);
  cb_button_timeout (button);

  return TRUE;
}

static gboolean
cb_button_release (GtkWidget      *widget,
		   GdkEventButton *event,
		   gpointer        user_data)
{
  GtkScaleButton *button;
  GtkScaleButtonPrivate *priv;

  button = GTK_SCALE_BUTTON (user_data);
  priv = gtk_scale_button_get_props (button)->priv;

  if (priv->click_id != 0)
    {
      g_source_remove (priv->click_id);
      priv->click_id = 0;
    }

  return TRUE;
}

static void
cb_dock_grab_notify (GtkWidget *widget,
		     gboolean   was_grabbed,
		     gpointer   user_data)
{
  GtkScaleButton *button = (GtkScaleButton *) user_data;

  gtk_scale_button_grab_notify (button, was_grabbed);
}

static gboolean
cb_dock_grab_broken_event (GtkWidget *widget,
			   gboolean   was_grabbed,
			   gpointer   user_data)
{
  GtkScaleButton *button = (GtkScaleButton *) user_data;

  gtk_scale_button_grab_notify (button, FALSE);

  return FALSE;
}

/*
 * Scale callbacks.
 */

static void
gtk_scale_button_release_grab (GtkScaleButton *button,
			       GdkEventButton *event)
{
  GdkEventButton *e;
  GdkDisplay *display;
  GtkScaleButtonPrivate *priv;

  priv = gtk_scale_button_get_props (button)->priv;

  /* ungrab focus */
  display = __gtk_widget_get_display (GTK_WIDGET (button));
  __gdk_display_keyboard_ungrab (display, event->time);
  __gdk_display_pointer_ungrab (display, event->time);
  __gtk_grab_remove (priv->dock);

  /* hide again */
  __gtk_widget_hide (priv->dock);
  priv->timeout = FALSE;

  e = (GdkEventButton *) __gdk_event_copy ((GdkEvent *) event);
  e->window = gtk_widget_get_props (GTK_WIDGET (button))->window;
  e->type = GDK_BUTTON_RELEASE;
  __gtk_widget_event (GTK_WIDGET (button), (GdkEvent *) e);
  e->window = event->window;
  __gdk_event_free ((GdkEvent *) e);
}

static gboolean
cb_dock_button_press (GtkWidget      *widget,
		      GdkEventButton *event,
		      gpointer        user_data)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (user_data);

  if (event->type == GDK_BUTTON_PRESS)
    {
      gtk_scale_button_release_grab (button, event);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_scale_button_popdown (GtkWidget *widget)
{
  GtkScaleButton *button;
  GtkScaleButtonPrivate *priv;
  GdkDisplay *display;

  button = GTK_SCALE_BUTTON (widget);
  priv = gtk_scale_button_get_props (button)->priv;

  /* ungrab focus */
  display = __gtk_widget_get_display (widget);
  __gdk_display_keyboard_ungrab (display, GDK_CURRENT_TIME);
  __gdk_display_pointer_ungrab (display, GDK_CURRENT_TIME);
  __gtk_grab_remove (priv->dock);

  /* hide again */
  __gtk_widget_hide (priv->dock);
  priv->timeout = FALSE;
}

static gboolean
cb_dock_key_release (GtkWidget   *widget,
		     GdkEventKey *event,
		     gpointer     user_data)
{
  if (event->keyval == GDK_Escape)
    {
      gtk_scale_button_popdown (GTK_WIDGET (user_data));
      return TRUE;
    }

  if (!__gtk_bindings_activate_event (G_OBJECT (widget), event))
    {
      /* The popup hasn't managed the event, pass onto the button */
      __gtk_bindings_activate_event (G_OBJECT (user_data), event);
    }

  return TRUE;
}

static void
cb_scale_grab_notify (GtkWidget *widget,
		      gboolean   was_grabbed,
		      gpointer   user_data)
{
  GtkScaleButton *button = (GtkScaleButton *) user_data;

  gtk_scale_button_grab_notify (button, was_grabbed);
}

/*
 * Scale stuff.
 */

#define GTK_TYPE_SCALE_BUTTON_SCALE    (_gtk_scale_button_scale_get_type ())
#define GTK_SCALE_BUTTON_SCALE(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SCALE_BUTTON_SCALE, GtkScaleButtonScale))
#define GTK_IS_SCALE_BUTTON_SCALE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SCALE_BUTTON_SCALE))

typedef struct _GtkScaleButtonScale GtkScaleButtonScale;

STLWRT_DECLARE_VTYPE_VPARENT (GtkScaleButtonScale, _gtk_scale_button_scale, GtkScale,
  GtkScaleButton *button;
)

typedef struct _GtkScaleButtonScaleClass
{
  GtkScaleClass parent_class;
} GtkScaleButtonScaleClass;

static gboolean	gtk_scale_button_scale_press   (GtkWidget      *widget,
                                                GdkEventButton *event);
static gboolean gtk_scale_button_scale_release (GtkWidget      *widget,
                                                GdkEventButton *event);

STLWRT_DEFINE_VTYPE (GtkScaleButtonScale, _gtk_scale_button_scale, GTK_TYPE_SCALE,
                     G_TYPE_FLAG_NONE, ;)

static void
_gtk_scale_button_scale_class_init (GtkScaleButtonScaleClass *klass)
{
  GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS (klass);
  GtkRangeClass *gtkrange_class = GTK_RANGE_CLASS (klass);

  gtkwidget_class->button_press_event = gtk_scale_button_scale_press;
  gtkwidget_class->button_release_event = gtk_scale_button_scale_release;
  gtkrange_class->value_changed = gtk_scale_button_scale_value_changed;
}

static void
_gtk_scale_button_scale_init (GtkScaleButtonScale *scale)
{
}

static GtkWidget *
gtk_scale_button_scale_new (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_props (button)->priv;
  GtkScaleButtonScale *scale;

  scale = g_object_new (GTK_TYPE_SCALE_BUTTON_SCALE,
                        "orientation", priv->orientation,
                        "adjustment",  priv->adjustment,
                        "draw-value",  FALSE,
                        NULL);

  _gtk_scale_button_scale_get_props (button)->button = button;

  g_signal_connect (scale, "grab-notify",
                    G_CALLBACK (cb_scale_grab_notify), button);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      __gtk_widget_set_size_request (GTK_WIDGET (scale), -1, SCALE_SIZE);
      __gtk_range_set_inverted (GTK_RANGE (scale), TRUE);
    }
  else
    {
      __gtk_widget_set_size_request (GTK_WIDGET (scale), SCALE_SIZE, -1);
      __gtk_range_set_inverted (GTK_RANGE (scale), FALSE);
    }

  return GTK_WIDGET (scale);
}

static gboolean
gtk_scale_button_scale_press (GtkWidget      *widget,
			      GdkEventButton *event)
{
  GtkScaleButtonPrivate *priv = gtk_scale_button_get_instance_private (_gtk_scale_button_scale_get_props (widget)->button);

  /* the scale will grab input; if we have input grabbed, all goes
   * horribly wrong, so let's not do that.
   */
  __gtk_grab_remove (priv->dock);

  return GTK_WIDGET_CLASS (_gtk_scale_button_scale_parent_class)->button_press_event (widget, event);
}

static gboolean
gtk_scale_button_scale_release (GtkWidget      *widget,
				GdkEventButton *event)
{
  GtkScaleButton *button = _gtk_scale_button_scale_get_props(GTK_SCALE_BUTTON_SCALE (widget))->button;
  gboolean res;

  if (gtk_scale_button_get_props (button)->priv->timeout)
    {
      /* if we did a quick click, leave the window open; else, hide it */
      if (event->time > gtk_scale_button_get_props (button)->priv->pop_time + gtk_scale_button_get_props (button)->priv->click_timeout)
        {

	  gtk_scale_button_release_grab (button, event);
	  GTK_WIDGET_CLASS (_gtk_scale_button_scale_parent_class)->button_release_event (widget, event);

	  return TRUE;
	}

      gtk_scale_button_get_props (button)->priv->timeout = FALSE;
    }

  res = GTK_WIDGET_CLASS (_gtk_scale_button_scale_parent_class)->button_release_event (widget, event);

  /* the scale will release input; right after that, we *have to* grab
   * it back so we can catch out-of-scale clicks and hide the popup,
   * so I basically want a g_signal_connect_after_always(), but I can't
   * find that, so we do this complex 'first-call-parent-then-do-actual-
   * action' thingy...
   */
  __gtk_grab_add (gtk_scale_button_get_props (button)->priv->dock);

  return res;
}

static void
gtk_scale_button_update_icon (GtkScaleButton *button)
{
  GtkScaleButtonPrivate *priv;
  GtkAdjustment *adj;
  gdouble value;
  const gchar *name;
  guint num_icons;

  priv = gtk_scale_button_get_props (button)->priv;

  if (!priv->icon_list || priv->icon_list[0] == '\0')
    {
      __gtk_image_set_from_stock (GTK_IMAGE (priv->image),
				GTK_STOCK_MISSING_IMAGE,
				priv->size);
      return;
    }

  num_icons = g_strv_length (priv->icon_list);

  /* The 1-icon special case */
  if (num_icons == 1)
    {
      __gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				    priv->icon_list[0],
				    priv->size);
      return;
    }

  adj = priv->adjustment;
  value = __gtk_scale_button_get_value (button);

  /* The 2-icons special case */
  if (num_icons == 2)
    {
      gdouble limit;
      limit = (gtk_adjustment_get_props (adj)->upper - gtk_adjustment_get_props (adj)->lower) / 2 + gtk_adjustment_get_props (adj)->lower;
      if (value < limit)
	name = priv->icon_list[0];
      else
	name = priv->icon_list[1];

      __gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				    name,
				    priv->size);
      return;
    }

  /* With 3 or more icons */
  if (value == gtk_adjustment_get_props (adj)->lower)
    {
      name = priv->icon_list[0];
    }
  else if (value == gtk_adjustment_get_props (adj)->upper)
    {
      name = priv->icon_list[1];
    }
  else
    {
      gdouble step;
      guint i;

      step = (gtk_adjustment_get_props (adj)->upper - gtk_adjustment_get_props (adj)->lower) / (num_icons - 2);
      i = (guint) ((value - gtk_adjustment_get_props (adj)->lower) / step) + 2;
      g_assert (i < num_icons);
      name = priv->icon_list[i];
    }

  __gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				name,
				priv->size);
}

static void
gtk_scale_button_scale_value_changed (GtkRange *range)
{
  GtkScaleButton *button = _gtk_scale_button_scale_get_props (GTK_SCALE_BUTTON_SCALE (range))->button;
  gdouble value;

  value = __gtk_range_get_value (range);

  gtk_scale_button_update_icon (button);

  g_signal_emit (button, signals[VALUE_CHANGED], 0, value);
  g_object_notify (G_OBJECT (button), "value");
}
