/* STLWRT - A fork of GTK+ 2 supporting future applications as well
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998 Elliot Lee
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



#include "config.h"

#include <stlwrt.h>
#include <stdlib.h>
#include <gtkhandlebox.h>
#include <gtkinvisible.h>
#include <gtkmain.h>

#include <gtkwindow.h>
#include <gtkprivate.h>
#include <gtkintl.h>


typedef struct _GtkHandleBoxPrivate GtkHandleBoxPrivate;

struct _GtkHandleBoxPrivate
{
  gint orig_x;
  gint orig_y;
};

enum {
  PROP_0,
  PROP_SHADOW,
  PROP_SHADOW_TYPE,
  PROP_HANDLE_POSITION,
  PROP_SNAP_EDGE,
  PROP_SNAP_EDGE_SET,
  PROP_CHILD_DETACHED
};

#define DRAG_HANDLE_SIZE 10
#define CHILDLESS_SIZE	25
#define GHOST_HEIGHT 3
#define TOLERANCE 5

enum {
  SIGNAL_CHILD_ATTACHED,
  SIGNAL_CHILD_DETACHED,
  SIGNAL_LAST
};

/* The algorithm for docking and redocking implemented here
 * has a couple of nice properties:
 *
 * 1) During a single drag, docking always occurs at the
 *    the same cursor position. This means that the users
 *    motions are reversible, and that you won't
 *    undock/dock oscillations.
 *
 * 2) Docking generally occurs at user-visible features.
 *    The user, once they figure out to redock, will
 *    have useful information about doing it again in
 *    the future.
 *
 * Please try to preserve these properties if you
 * change the algorithm. (And the current algorithm
 * is far from ideal). Briefly, the current algorithm
 * for deciding whether the handlebox is docked or not:
 *
 * 1) The decision is done by comparing two rectangles - the
 *    allocation if the widget at the start of the drag,
 *    and the boundary of hb->bin_window at the start of
 *    of the drag offset by the distance that the cursor
 *    has moved.
 *
 * 2) These rectangles must have one edge, the "snap_edge"
 *    of the handlebox, aligned within TOLERANCE.
 * 
 * 3) On the other dimension, the extents of one rectangle
 *    must be contained in the extents of the other,
 *    extended by tolerance. That is, either we can have:
 *
 * <-TOLERANCE-|--------bin_window--------------|-TOLERANCE->
 *         <--------float_window-------------------->
 *
 * or we can have:
 *
 * <-TOLERANCE-|------float_window--------------|-TOLERANCE->
 *          <--------bin_window-------------------->
 */

static void     gtk_handle_box_set_property  (GObject        *object,
                                              guint           param_id,
                                              const GValue   *value,
                                              GParamSpec     *pspec);
static void     gtk_handle_box_get_property  (GObject        *object,
                                              guint           param_id,
                                              GValue         *value,
                                              GParamSpec     *pspec);
static void     gtk_handle_box_map           (GtkWidget      *widget);
static void     gtk_handle_box_unmap         (GtkWidget      *widget);
static void     gtk_handle_box_realize       (GtkWidget      *widget);
static void     gtk_handle_box_unrealize     (GtkWidget      *widget);
static void     gtk_handle_box_style_set     (GtkWidget      *widget,
                                              GtkStyle       *previous_style);
static void     gtk_handle_box_size_request  (GtkWidget      *widget,
                                              GtkRequisition *requisition);
static void     gtk_handle_box_size_allocate (GtkWidget      *widget,
                                              GtkAllocation  *real_allocation);
static void     gtk_handle_box_add           (GtkContainer   *container,
                                              GtkWidget      *widget);
static void     gtk_handle_box_remove        (GtkContainer   *container,
                                              GtkWidget      *widget);
static void     gtk_handle_box_draw_ghost    (GtkHandleBox   *hb);
static void     gtk_handle_box_paint         (GtkWidget      *widget,
                                              GdkEventExpose *event,
                                              GdkRectangle   *area);
static gboolean gtk_handle_box_expose        (GtkWidget      *widget,
                                              GdkEventExpose *event);
static gboolean gtk_handle_box_button_press  (GtkWidget      *widget,
                                              GdkEventButton *event);
static gboolean gtk_handle_box_motion        (GtkWidget      *widget,
                                              GdkEventMotion *event);
static gboolean gtk_handle_box_delete_event  (GtkWidget      *widget,
                                              GdkEventAny    *event);
static void     gtk_handle_box_reattach      (GtkHandleBox   *hb);
static void     gtk_handle_box_end_drag      (GtkHandleBox   *hb,
                                              guint32         time);

static guint handle_box_signals[SIGNAL_LAST] = { 0 };

STLWRT_DEFINE_VTYPE (GtkHandleBox, gtk_handle_box, GTK_TYPE_BIN, G_TYPE_FLAG_NONE,
                     G_ADD_PRIVATE (GtkHandleBox))

static void
gtk_handle_box_class_init (GtkHandleBoxClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  gobject_class->set_property = gtk_handle_box_set_property;
  gobject_class->get_property = gtk_handle_box_get_property;
  
  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW,
                                   g_param_spec_enum ("shadow", NULL,
                                                      P_("Deprecated property, use shadow_type instead"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_OUT,
                                                      GTK_PARAM_READWRITE | G_PARAM_DEPRECATED));
  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow-type",
                                                      P_("Shadow type"),
                                                      P_("Appearance of the shadow that surrounds the container"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_OUT,
                                                      GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_HANDLE_POSITION,
                                   g_param_spec_enum ("handle-position",
                                                      P_("Handle position"),
                                                      P_("Position of the handle relative to the child widget"),
						      GTK_TYPE_POSITION_TYPE,
						      GTK_POS_LEFT,
                                                      GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_SNAP_EDGE,
                                   g_param_spec_enum ("snap-edge",
                                                      P_("Snap edge"),
                                                      P_("Side of the handlebox that's lined up with the docking point to dock the handlebox"),
						      GTK_TYPE_POSITION_TYPE,
						      GTK_POS_TOP,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SNAP_EDGE_SET,
                                   g_param_spec_boolean ("snap-edge-set",
							 P_("Snap edge set"),
							 P_("Whether to use the value from the snap_edge property or a value derived from handle_position"),
							 FALSE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_CHILD_DETACHED,
                                   g_param_spec_boolean ("child-detached",
							 P_("Child Detached"),
							 P_("A boolean value indicating whether the handlebox's child is attached or detached."),
							 FALSE,
							 GTK_PARAM_READABLE));

  widget_class->map = gtk_handle_box_map;
  widget_class->unmap = gtk_handle_box_unmap;
  widget_class->realize = gtk_handle_box_realize;
  widget_class->unrealize = gtk_handle_box_unrealize;
  widget_class->style_set = gtk_handle_box_style_set;
  widget_class->size_request = gtk_handle_box_size_request;
  widget_class->size_allocate = gtk_handle_box_size_allocate;
  widget_class->expose_event = gtk_handle_box_expose;
  widget_class->button_press_event = gtk_handle_box_button_press;
  widget_class->delete_event = gtk_handle_box_delete_event;

  container_class->add = gtk_handle_box_add;
  container_class->remove = gtk_handle_box_remove;

  class->child_attached = NULL;
  class->child_detached = NULL;

  handle_box_signals[SIGNAL_CHILD_ATTACHED] =
    g_signal_new (I_("child-attached"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkHandleBoxClass, child_attached),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);
  handle_box_signals[SIGNAL_CHILD_DETACHED] =
    g_signal_new (I_("child-detached"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkHandleBoxClass, child_detached),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);
}


static void
gtk_handle_box_init (GtkHandleBox *handle_box)
{
  __gtk_widget_set_has_window (GTK_WIDGET (handle_box), TRUE);

  gtk_handle_box_get_props (handle_box)->bin_window = NULL;
  gtk_handle_box_get_props (handle_box)->float_window = NULL;
  gtk_handle_box_get_props (handle_box)->shadow_type = GTK_SHADOW_OUT;
  gtk_handle_box_get_props (handle_box)->handle_position = GTK_POS_LEFT;
  gtk_handle_box_get_props (handle_box)->float_window_mapped = FALSE;
  gtk_handle_box_get_props (handle_box)->child_detached = FALSE;
  gtk_handle_box_get_props (handle_box)->in_drag = FALSE;
  gtk_handle_box_get_props (handle_box)->shrink_on_detach = TRUE;
  gtk_handle_box_get_props (handle_box)->snap_edge = -1;
}

static void 
gtk_handle_box_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
  GtkHandleBox *handle_box = GTK_HANDLE_BOX (object);

  switch (prop_id)
    {
    case PROP_SHADOW:
    case PROP_SHADOW_TYPE:
      __gtk_handle_box_set_shadow_type (handle_box, g_value_get_enum (value));
      break;
    case PROP_HANDLE_POSITION:
      __gtk_handle_box_set_handle_position (handle_box, g_value_get_enum (value));
      break;
    case PROP_SNAP_EDGE:
      __gtk_handle_box_set_snap_edge (handle_box, g_value_get_enum (value));
      break;
    case PROP_SNAP_EDGE_SET:
      if (!g_value_get_boolean (value))
	__gtk_handle_box_set_snap_edge (handle_box, (GtkPositionType)-1);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_handle_box_get_property (GObject         *object,
			     guint            prop_id,
			     GValue          *value,
			     GParamSpec      *pspec)
{
  GtkHandleBox *handle_box = GTK_HANDLE_BOX (object);
  
  switch (prop_id)
    {
    case PROP_SHADOW:
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, gtk_handle_box_get_props (handle_box)->shadow_type);
      break;
    case PROP_HANDLE_POSITION:
      g_value_set_enum (value, gtk_handle_box_get_props (handle_box)->handle_position);
      break;
    case PROP_SNAP_EDGE:
      g_value_set_enum (value,
			(gtk_handle_box_get_props (handle_box)->snap_edge == -1 ?
			 GTK_POS_TOP : gtk_handle_box_get_props (handle_box)->snap_edge));
      break;
    case PROP_SNAP_EDGE_SET:
      g_value_set_boolean (value, gtk_handle_box_get_props (handle_box)->snap_edge != -1);
      break;
    case PROP_CHILD_DETACHED:
      g_value_set_boolean (value, gtk_handle_box_get_props (handle_box)->child_detached);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}
 
GtkWidget*
__gtk_handle_box_new (void)
{
  return g_object_new (GTK_TYPE_HANDLE_BOX, NULL);
}

static void
gtk_handle_box_map (GtkWidget *widget)
{
  GtkBin *bin;
  GtkHandleBox *hb;

  __gtk_widget_set_mapped (widget, TRUE);

  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);

  if (gtk_bin_get_props (bin)->child &&
      __gtk_widget_get_visible (gtk_bin_get_props (bin)->child) &&
      !__gtk_widget_get_mapped (gtk_bin_get_props (bin)->child))
    __gtk_widget_map (gtk_bin_get_props (bin)->child);

  if (gtk_handle_box_get_props (hb)->child_detached && !gtk_handle_box_get_props (hb)->float_window_mapped)
    {
      __gdk_window_show (gtk_handle_box_get_props (hb)->float_window);
      gtk_handle_box_get_props (hb)->float_window_mapped = TRUE;
    }

  __gdk_window_show (gtk_handle_box_get_props (hb)->bin_window);
  __gdk_window_show (gtk_widget_get_props (widget)->window);
}

static void
gtk_handle_box_unmap (GtkWidget *widget)
{
  GtkHandleBox *hb;

  __gtk_widget_set_mapped (widget, FALSE);

  hb = GTK_HANDLE_BOX (widget);

  __gdk_window_hide (gtk_widget_get_props (widget)->window);
  if (gtk_handle_box_get_props (hb)->float_window_mapped)
    {
      __gdk_window_hide (gtk_handle_box_get_props (hb)->float_window);
      gtk_handle_box_get_props (hb)->float_window_mapped = FALSE;
    }
}

static void
gtk_handle_box_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkHandleBox *hb;

  hb = GTK_HANDLE_BOX (widget);

  __gtk_widget_set_realized (widget, TRUE);

  attributes.x = gtk_widget_get_props (widget)->allocation.x;
  attributes.y = gtk_widget_get_props (widget)->allocation.y;
  attributes.width = gtk_widget_get_props (widget)->allocation.width;
  attributes.height = gtk_widget_get_props (widget)->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = __gtk_widget_get_visual (widget);
  attributes.colormap = __gtk_widget_get_colormap (widget);
  attributes.event_mask = (__gtk_widget_get_events (widget)
			   | GDK_EXPOSURE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  gtk_widget_get_props (widget)->window = __gdk_window_new (__gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  __gdk_window_set_user_data (gtk_widget_get_props (widget)->window, widget);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = gtk_widget_get_props (widget)->allocation.width;
  attributes.height = gtk_widget_get_props (widget)->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = (__gtk_widget_get_events (widget) |
			   GDK_EXPOSURE_MASK |
			   GDK_BUTTON1_MOTION_MASK |
			   GDK_POINTER_MOTION_HINT_MASK |
			   GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  gtk_handle_box_get_props (hb)->bin_window = __gdk_window_new (gtk_widget_get_props (widget)->window, &attributes, attributes_mask);
  __gdk_window_set_user_data (gtk_handle_box_get_props (hb)->bin_window, widget);
  if (gtk_bin_get_props (GTK_BIN (hb))->child)
    __gtk_widget_set_parent_window (gtk_bin_get_props (hb)->child, gtk_handle_box_get_props (hb)->bin_window);
  
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = gtk_widget_get_props (widget)->requisition.width;
  attributes.height = gtk_widget_get_props (widget)->requisition.height;
  attributes.window_type = GDK_WINDOW_TOPLEVEL;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = __gtk_widget_get_visual (widget);
  attributes.colormap = __gtk_widget_get_colormap (widget);
  attributes.event_mask = (__gtk_widget_get_events (widget) |
			   GDK_KEY_PRESS_MASK |
			   GDK_ENTER_NOTIFY_MASK |
			   GDK_LEAVE_NOTIFY_MASK |
			   GDK_FOCUS_CHANGE_MASK |
			   GDK_STRUCTURE_MASK);
  attributes.type_hint = GDK_WINDOW_TYPE_HINT_TOOLBAR;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP | GDK_WA_TYPE_HINT;
  gtk_handle_box_get_props (hb)->float_window = __gdk_window_new (__gtk_widget_get_root_window (widget),
				     &attributes, attributes_mask);
  __gdk_window_set_user_data (gtk_handle_box_get_props (hb)->float_window, widget);
  __gdk_window_set_decorations (gtk_handle_box_get_props (hb)->float_window, 0);
  __gdk_window_set_type_hint (gtk_handle_box_get_props (hb)->float_window, GDK_WINDOW_TYPE_HINT_TOOLBAR);
  
  gtk_widget_get_props (widget)->style = __gtk_style_attach (gtk_widget_get_props (widget)->style, gtk_widget_get_props (widget)->window);
  __gtk_style_set_background (gtk_widget_get_props (widget)->style, gtk_widget_get_props (widget)->window, __gtk_widget_get_state (widget));
  __gtk_style_set_background (gtk_widget_get_props (widget)->style, gtk_handle_box_get_props (hb)->bin_window, __gtk_widget_get_state (widget));
  __gtk_style_set_background (gtk_widget_get_props (widget)->style, gtk_handle_box_get_props (hb)->float_window, __gtk_widget_get_state (widget));
  __gdk_window_set_back_pixmap (gtk_widget_get_props (widget)->window, NULL, TRUE);
}

static void
gtk_handle_box_unrealize (GtkWidget *widget)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);

  __gdk_window_set_user_data (gtk_handle_box_get_props (hb)->bin_window, NULL);
  __gdk_window_destroy (gtk_handle_box_get_props (hb)->bin_window);
  gtk_handle_box_get_props (hb)->bin_window = NULL;
  __gdk_window_set_user_data (gtk_handle_box_get_props (hb)->float_window, NULL);
  __gdk_window_destroy (gtk_handle_box_get_props (hb)->float_window);
  gtk_handle_box_get_props (hb)->float_window = NULL;

  GTK_WIDGET_CLASS (gtk_handle_box_parent_class)->unrealize (widget);
}

static void
gtk_handle_box_style_set (GtkWidget *widget,
			  GtkStyle  *previous_style)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);

  if (__gtk_widget_get_realized (widget) &&
      __gtk_widget_get_has_window (widget))
    {
      __gtk_style_set_background (gtk_widget_get_props (widget)->style, gtk_widget_get_props (widget)->window,
				gtk_widget_get_props (widget)->state);
      __gtk_style_set_background (gtk_widget_get_props (gtk_widget_get_props (widget))->style, gtk_handle_box_get_props (hb)->bin_window, gtk_widget_get_props (gtk_widget_get_props (widget))->state);
      __gtk_style_set_background (gtk_widget_get_props (gtk_widget_get_props (widget))->style, gtk_handle_box_get_props (hb)->float_window, gtk_widget_get_props (gtk_widget_get_props (widget))->state);
    }
}

static int
effective_handle_position (GtkHandleBox *hb)
{
  int handle_position;

  if (__gtk_widget_get_direction (GTK_WIDGET (hb)) == GTK_TEXT_DIR_LTR)
    handle_position = gtk_handle_box_get_props (hb)->handle_position;
  else
    {
      switch (gtk_handle_box_get_props (hb)->handle_position) 
	{
	case GTK_POS_LEFT:
	  handle_position = GTK_POS_RIGHT;
	  break;
	case GTK_POS_RIGHT:
	  handle_position = GTK_POS_LEFT;
	  break;
	default:
	  handle_position = gtk_handle_box_get_props (hb)->handle_position;
	  break;
	}
    }

  return handle_position;
}

static void
gtk_handle_box_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
  GtkBin *bin;
  GtkHandleBox *hb;
  GtkRequisition child_requisition;
  gint handle_position;

  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);

  handle_position = effective_handle_position (hb);

  if (handle_position == GTK_POS_LEFT ||
      handle_position == GTK_POS_RIGHT)
    {
      requisition->width = DRAG_HANDLE_SIZE;
      requisition->height = 0;
    }
  else
    {
      requisition->width = 0;
      requisition->height = DRAG_HANDLE_SIZE;
    }

  /* if our child is not visible, we still request its size, since we
   * won't have any useful hint for our size otherwise.
   */
  if (gtk_bin_get_props (bin)->child)
    __gtk_widget_size_request (gtk_bin_get_props (bin)->child, &child_requisition);
  else
    {
      child_requisition.width = 0;
      child_requisition.height = 0;
    }      

  if (gtk_handle_box_get_props (hb)->child_detached)
    {
      /* FIXME: This doesn't work currently */
      if (!gtk_handle_box_get_props (hb)->shrink_on_detach)
	{
	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    requisition->height += child_requisition.height;
	  else
	    requisition->width += child_requisition.width;
	}
      else
	{
	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    requisition->height += gtk_widget_get_props (widget)->style->ythickness;
	  else
	    requisition->width += gtk_widget_get_props (widget)->style->xthickness;
	}
    }
  else
    {
      requisition->width += gtk_container_get_props (GTK_CONTAINER (widget))->border_width * 2;
      requisition->height += gtk_container_get_props (GTK_CONTAINER (widget))->border_width * 2;
      
      if (gtk_bin_get_props (bin)->child)
	{
	  requisition->width += child_requisition.width;
	  requisition->height += child_requisition.height;
	}
      else
	{
	  requisition->width += CHILDLESS_SIZE;
	  requisition->height += CHILDLESS_SIZE;
	}
    }
}

static void
gtk_handle_box_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkHandleBox *hb;
  GtkRequisition child_requisition;
  gint handle_position;
  
  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);
  
  handle_position = effective_handle_position (hb);

  if (gtk_bin_get_props (bin)->child)
    __gtk_widget_get_child_requisition (gtk_bin_get_props (bin)->child, &child_requisition);
  else
    {
      child_requisition.width = 0;
      child_requisition.height = 0;
    }      
      
  gtk_widget_get_props (widget)->allocation = *allocation;

  if (__gtk_widget_get_realized (widget))
    __gdk_window_move_resize (gtk_widget_get_props (widget)->window,
			    gtk_widget_get_props (widget)->allocation.x,
			    gtk_widget_get_props (widget)->allocation.y,
			    gtk_widget_get_props (widget)->allocation.width,
			    gtk_widget_get_props (widget)->allocation.height);


  if (gtk_bin_get_props (bin)->child && __gtk_widget_get_visible (gtk_bin_get_props (bin)->child))
    {
      GtkAllocation child_allocation;
      guint border_width;

      border_width = gtk_container_get_props (GTK_CONTAINER (widget))->border_width;

      child_allocation.x = border_width;
      child_allocation.y = border_width;
      if (handle_position == GTK_POS_LEFT)
	child_allocation.x += DRAG_HANDLE_SIZE;
      else if (handle_position == GTK_POS_TOP)
	child_allocation.y += DRAG_HANDLE_SIZE;

      if (gtk_handle_box_get_props (hb)->child_detached)
	{
	  guint float_width;
	  guint float_height;
	  
	  child_allocation.width = child_requisition.width;
	  child_allocation.height = child_requisition.height;
	  
	  float_width = child_allocation.width + 2 * border_width;
	  float_height = child_allocation.height + 2 * border_width;
	  
	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    float_width += DRAG_HANDLE_SIZE;
	  else
	    float_height += DRAG_HANDLE_SIZE;

	  if (__gtk_widget_get_realized (widget))
	    {
	      __gdk_window_resize (gtk_handle_box_get_props (hb)->float_window,
				 float_width,
				 float_height);
	      __gdk_window_move_resize (gtk_handle_box_get_props (hb)->bin_window,
				      0,
				      0,
				      float_width,
				      float_height);
	    }
	}
      else
	{
	  child_allocation.width = MAX (1, (gint)gtk_widget_get_props (widget)->allocation.width - 2 * border_width);
	  child_allocation.height = MAX (1, (gint)gtk_widget_get_props (widget)->allocation.height - 2 * border_width);

	  if (handle_position == GTK_POS_LEFT ||
	      handle_position == GTK_POS_RIGHT)
	    child_allocation.width -= DRAG_HANDLE_SIZE;
	  else
	    child_allocation.height -= DRAG_HANDLE_SIZE;
	  
	  if (__gtk_widget_get_realized (widget))
	    __gdk_window_move_resize (gtk_handle_box_get_props (hb)->bin_window,
				    0,
				    0,
				    gtk_widget_get_props (widget)->allocation.width,
				    gtk_widget_get_props (widget)->allocation.height);
	}

      __gtk_widget_size_allocate (gtk_bin_get_props (bin)->child, &child_allocation);
    }
}

static void
gtk_handle_box_draw_ghost (GtkHandleBox *hb)
{
  GtkWidget *widget;
  guint x;
  guint y;
  guint width;
  guint height;
  gint handle_position;

  widget = GTK_WIDGET (hb);
  
  handle_position = effective_handle_position (hb);
  if (handle_position == GTK_POS_LEFT ||
      handle_position == GTK_POS_RIGHT)
    {
      x = handle_position == GTK_POS_LEFT ? 0 : gtk_widget_get_props (widget)->allocation.width - DRAG_HANDLE_SIZE;
      y = 0;
      width = DRAG_HANDLE_SIZE;
      height = gtk_widget_get_props (widget)->allocation.height;
    }
  else
    {
      x = 0;
      y = handle_position == GTK_POS_TOP ? 0 : gtk_widget_get_props (widget)->allocation.height - DRAG_HANDLE_SIZE;
      width = gtk_widget_get_props (widget)->allocation.width;
      height = DRAG_HANDLE_SIZE;
    }
  __gtk_paint_shadow (gtk_widget_get_props (widget)->style,
		    gtk_widget_get_props (widget)->window,
		    __gtk_widget_get_state (widget),
		    GTK_SHADOW_ETCHED_IN,
		    NULL, widget, "handle",
		    x,
		    y,
		    width,
		    height);
   if (handle_position == GTK_POS_LEFT ||
       handle_position == GTK_POS_RIGHT)
     __gtk_paint_hline (gtk_widget_get_props (widget)->style,
		      gtk_widget_get_props (widget)->window,
		      __gtk_widget_get_state (widget),
		      NULL, widget, "handlebox",
		      handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : 0,
		      handle_position == GTK_POS_LEFT ? gtk_widget_get_props (widget)->allocation.width : gtk_widget_get_props (widget)->allocation.width - DRAG_HANDLE_SIZE,
		      gtk_widget_get_props (widget)->allocation.height / 2);
   else
     __gtk_paint_vline (gtk_widget_get_props (widget)->style,
		      gtk_widget_get_props (widget)->window,
		      __gtk_widget_get_state (widget),
		      NULL, widget, "handlebox",
		      handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : 0,
		      handle_position == GTK_POS_TOP ? gtk_widget_get_props (widget)->allocation.height : gtk_widget_get_props (widget)->allocation.height - DRAG_HANDLE_SIZE,
		      gtk_widget_get_props (widget)->allocation.width / 2);
}

static void
draw_textured_frame (GtkWidget *widget, GdkWindow *window, GdkRectangle *rect, GtkShadowType shadow,
		     GdkRectangle *clip, GtkOrientation orientation)
{
   __gtk_paint_handle (gtk_widget_get_props (widget)->style, window, GTK_STATE_NORMAL, shadow,
		     clip, widget, "handlebox",
		     rect->x, rect->y, rect->width, rect->height, 
		     orientation);
}

void
__gtk_handle_box_set_shadow_type (GtkHandleBox  *handle_box,
				GtkShadowType  type)
{
  g_return_if_fail (GTK_IS_HANDLE_BOX (handle_box));

  if ((GtkShadowType) gtk_handle_box_get_props (handle_box)->shadow_type != type)
    {
      gtk_handle_box_get_props (handle_box)->shadow_type = type;
      g_object_notify (G_OBJECT (handle_box), "shadow-type");
      __gtk_widget_queue_resize (GTK_WIDGET (handle_box));
    }
}

/**
 * __gtk_handle_box_get_shadow_type:
 * @handle_box: a #GtkHandleBox
 * 
 * Gets the type of shadow drawn around the handle box. See
 * __gtk_handle_box_set_shadow_type().
 *
 * Return value: the type of shadow currently drawn around the handle box.
 **/
GtkShadowType
__gtk_handle_box_get_shadow_type (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), GTK_SHADOW_ETCHED_OUT);

  return gtk_handle_box_get_props (handle_box)->shadow_type;
}

void        
__gtk_handle_box_set_handle_position  (GtkHandleBox    *handle_box,
				     GtkPositionType  position)
{
  g_return_if_fail (GTK_IS_HANDLE_BOX (handle_box));

  if ((GtkPositionType) gtk_handle_box_get_props (handle_box)->handle_position != position)
    {
      gtk_handle_box_get_props (handle_box)->handle_position = position;
      g_object_notify (G_OBJECT (handle_box), "handle-position");
      __gtk_widget_queue_resize (GTK_WIDGET (handle_box));
    }
}

/**
 * __gtk_handle_box_get_handle_position:
 * @handle_box: a #GtkHandleBox
 *
 * Gets the handle position of the handle box. See
 * __gtk_handle_box_set_handle_position().
 *
 * Return value: the current handle position.
 **/
GtkPositionType
__gtk_handle_box_get_handle_position (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), GTK_POS_LEFT);

  return gtk_handle_box_get_props (handle_box)->handle_position;
}

void        
__gtk_handle_box_set_snap_edge        (GtkHandleBox    *handle_box,
				     GtkPositionType  edge)
{
  g_return_if_fail (GTK_IS_HANDLE_BOX (handle_box));

  if (gtk_handle_box_get_props (handle_box)->snap_edge != edge)
    {
      gtk_handle_box_get_props (handle_box)->snap_edge = edge;
      
      g_object_freeze_notify (G_OBJECT (handle_box));
      g_object_notify (G_OBJECT (handle_box), "snap-edge");
      g_object_notify (G_OBJECT (handle_box), "snap-edge-set");
      g_object_thaw_notify (G_OBJECT (handle_box));
    }
}

/**
 * __gtk_handle_box_get_snap_edge:
 * @handle_box: a #GtkHandleBox
 * 
 * Gets the edge used for determining reattachment of the handle box. See
 * __gtk_handle_box_set_snap_edge().
 *
 * Return value: the edge used for determining reattachment, or (GtkPositionType)-1 if this
 *               is determined (as per default) from the handle position. 
 **/
GtkPositionType
__gtk_handle_box_get_snap_edge (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), (GtkPositionType)-1);

  return gtk_handle_box_get_props (handle_box)->snap_edge;
}

/**
 * __gtk_handle_box_get_child_detached:
 * @handle_box: a #GtkHandleBox
 *
 * Whether the handlebox's child is currently detached.
 *
 * Return value: %TRUE if the child is currently detached, otherwise %FALSE
 *
 * Since: 2.14
 **/
gboolean
__gtk_handle_box_get_child_detached (GtkHandleBox *handle_box)
{
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (handle_box), FALSE);

  return gtk_handle_box_get_props (handle_box)->child_detached;
}

static void
gtk_handle_box_paint (GtkWidget      *widget,
                      GdkEventExpose *event,
		      GdkRectangle   *area)
{
  GtkBin *bin;
  GtkHandleBox *hb;
  gint width, height;
  GdkRectangle rect;
  GdkRectangle dest;
  gint handle_position;
  GtkOrientation handle_orientation;

  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX (widget);

  handle_position = effective_handle_position (hb);

  width = __gdk_window_get_width (gtk_handle_box_get_props (hb)->bin_window);
  height = __gdk_window_get_height (gtk_handle_box_get_props (hb)->bin_window);
  
  if (!event)
    __gtk_paint_box (gtk_widget_get_props (widget)->style,
		   gtk_handle_box_get_props (hb)->bin_window,
		   __gtk_widget_get_state (widget),
		   gtk_handle_box_get_props (hb)->shadow_type,
		   area, widget, "handlebox_bin",
		   0, 0, -1, -1);
  else
   __gtk_paint_box (gtk_widget_get_props (widget)->style,
		  gtk_handle_box_get_props (hb)->bin_window,
		  __gtk_widget_get_state (widget),
		  gtk_handle_box_get_props (hb)->shadow_type,
		  &event->area, widget, "handlebox_bin",
		  0, 0, -1, -1);

/* We currently draw the handle _above_ the relief of the handlebox.
 * it could also be drawn on the same level...

		 gtk_handle_box_get_props (hb)->handle_position == GTK_POS_LEFT ? DRAG_HANDLE_SIZE : 0,
		 gtk_handle_box_get_props (hb)->handle_position == GTK_POS_TOP ? DRAG_HANDLE_SIZE : 0,
		 width,
		 height);*/

  switch (handle_position)
    {
    case GTK_POS_LEFT:
      rect.x = 0;
      rect.y = 0; 
      rect.width = DRAG_HANDLE_SIZE;
      rect.height = height;
      handle_orientation = GTK_ORIENTATION_VERTICAL;
      break;
    case GTK_POS_RIGHT:
      rect.x = width - DRAG_HANDLE_SIZE; 
      rect.y = 0;
      rect.width = DRAG_HANDLE_SIZE;
      rect.height = height;
      handle_orientation = GTK_ORIENTATION_VERTICAL;
      break;
    case GTK_POS_TOP:
      rect.x = 0;
      rect.y = 0; 
      rect.width = width;
      rect.height = DRAG_HANDLE_SIZE;
      handle_orientation = GTK_ORIENTATION_HORIZONTAL;
      break;
    case GTK_POS_BOTTOM:
      rect.x = 0;
      rect.y = height - DRAG_HANDLE_SIZE;
      rect.width = width;
      rect.height = DRAG_HANDLE_SIZE;
      handle_orientation = GTK_ORIENTATION_HORIZONTAL;
      break;
    default: 
      g_assert_not_reached ();
      break;
    }

  if (__gdk_rectangle_intersect (event ? &event->area : area, &rect, &dest))
    draw_textured_frame (widget, gtk_handle_box_get_props (hb)->bin_window, &rect,
			 GTK_SHADOW_OUT,
			 event ? &event->area : area,
			 handle_orientation);

  if (gtk_bin_get_props (bin)->child && __gtk_widget_get_visible (gtk_bin_get_props (bin)->child))
    GTK_WIDGET_CLASS (gtk_handle_box_parent_class)->expose_event (widget, event);
}

static gboolean
gtk_handle_box_expose (GtkWidget      *widget,
		       GdkEventExpose *event)
{
  GtkHandleBox *hb;

  if (__gtk_widget_is_drawable (widget))
    {
      hb = GTK_HANDLE_BOX (widget);

      if (event->window == gtk_widget_get_props (widget)->window)
	{
	  if (gtk_handle_box_get_props (hb)->child_detached)
	    gtk_handle_box_draw_ghost (hb);
	}
      else
	gtk_handle_box_paint (widget, event, NULL);
    }
  
  return FALSE;
}

static GtkWidget *
gtk_handle_box_get_invisible (void)
{
  static GtkWidget *handle_box_invisible = NULL;

  if (!handle_box_invisible)
    {
      handle_box_invisible = __gtk_invisible_new ();
      __gtk_widget_show (handle_box_invisible);
    }
  
  return handle_box_invisible;
}

static gboolean
gtk_handle_box_grab_event (GtkWidget    *widget,
			   GdkEvent     *event,
			   GtkHandleBox *hb)
{
  switch (event->type)
    {
    case GDK_BUTTON_RELEASE:
      if (gtk_handle_box_get_props (hb)->in_drag)		/* sanity check */
	{
	  gtk_handle_box_end_drag (hb, event->button.time);
	  return TRUE;
	}
      break;

    case GDK_MOTION_NOTIFY:
      return gtk_handle_box_motion (GTK_WIDGET (hb), (GdkEventMotion *)event);
      break;

    default:
      break;
    }

  return FALSE;
}

static gboolean
gtk_handle_box_button_press (GtkWidget      *widget,
                             GdkEventButton *event)
{
  GtkHandleBox *hb;
  gboolean event_handled;
  GdkCursor *fleur;
  gint handle_position;

  hb = GTK_HANDLE_BOX (widget);

  handle_position = effective_handle_position (hb);

  event_handled = FALSE;
  if ((event->button == 1) && 
      (event->type == GDK_BUTTON_PRESS || event->type == GDK_2BUTTON_PRESS))
    {
      GtkWidget *child;
      gboolean in_handle;
      
      if (event->window != gtk_handle_box_get_props (hb)->bin_window)
	return FALSE;

      child = gtk_bin_get_props (GTK_BIN (hb))->child;

      if (child)
	{
	  switch (handle_position)
	    {
	    case GTK_POS_LEFT:
	      in_handle = event->x < DRAG_HANDLE_SIZE;
	      break;
	    case GTK_POS_TOP:
	      in_handle = event->y < DRAG_HANDLE_SIZE;
	      break;
	    case GTK_POS_RIGHT:
	      in_handle = event->x > 2 * gtk_container_get_props (GTK_CONTAINER (hb))->border_width + gtk_widget_get_props (child)->allocation.width;
	      break;
	    case GTK_POS_BOTTOM:
	      in_handle = event->y > 2 * gtk_container_get_props (GTK_CONTAINER (hb))->border_width + gtk_widget_get_props (child)->allocation.height;
	      break;
	    default:
	      in_handle = FALSE;
	      break;
	    }
	}
      else
	{
	  in_handle = FALSE;
	  event_handled = TRUE;
	}
      
      if (in_handle)
	{
	  if (event->type == GDK_BUTTON_PRESS) /* Start a drag */
	    {
	      GtkHandleBoxPrivate *private = gtk_handle_box_get_instance_private (hb);
	      GtkWidget *invisible = gtk_handle_box_get_invisible ();
	      gint desk_x, desk_y;
	      gint root_x, root_y;
	      gint width, height;

              __gtk_invisible_set_screen (GTK_INVISIBLE (invisible),
                                        __gtk_widget_get_screen (GTK_WIDGET (hb)));
	      __gdk_window_get_deskrelative_origin (gtk_handle_box_get_props (hb)->bin_window, &desk_x, &desk_y);
	      __gdk_window_get_origin (gtk_handle_box_get_props (hb)->bin_window, &root_x, &root_y);
	      width = __gdk_window_get_width (gtk_handle_box_get_props (hb)->bin_window);
	      height = __gdk_window_get_height (gtk_handle_box_get_props (hb)->bin_window);
		  
	      private->orig_x = event->x_root;
	      private->orig_y = event->y_root;
		  
	      gtk_handle_box_get_props (hb)->float_allocation.x = root_x - event->x_root;
	      gtk_handle_box_get_props (hb)->float_allocation.y = root_y - event->y_root;
	      gtk_handle_box_get_props (hb)->float_allocation.width = width;
	      gtk_handle_box_get_props (hb)->float_allocation.height = height;
	      
	      gtk_handle_box_get_props (hb)->deskoff_x = desk_x - root_x;
	      gtk_handle_box_get_props (hb)->deskoff_y = desk_y - root_y;
	      
	      if (__gdk_window_is_viewable (gtk_widget_get_props (widget)->window))
		{
		  __gdk_window_get_origin (gtk_widget_get_props (widget)->window, &root_x, &root_y);
		  width = __gdk_window_get_width (gtk_widget_get_props (widget)->window);
		  height = __gdk_window_get_height (gtk_widget_get_props (widget)->window);
	      
		  gtk_handle_box_get_props (hb)->attach_allocation.x = root_x;
		  gtk_handle_box_get_props (hb)->attach_allocation.y = root_y;
		  gtk_handle_box_get_props (hb)->attach_allocation.width = width;
		  gtk_handle_box_get_props (hb)->attach_allocation.height = height;
		}
	      else
		{
		  gtk_handle_box_get_props (hb)->attach_allocation.x = -1;
		  gtk_handle_box_get_props (hb)->attach_allocation.y = -1;
		  gtk_handle_box_get_props (hb)->attach_allocation.width = 0;
		  gtk_handle_box_get_props (hb)->attach_allocation.height = 0;
		}
	      gtk_handle_box_get_props (hb)->in_drag = TRUE;
	      fleur = __gdk_cursor_new_for_display (__gtk_widget_get_display (widget),
						  GDK_FLEUR);
	      if (__gdk_pointer_grab (gtk_widget_get_props (invisible)->window,
				    FALSE,
				    (GDK_BUTTON1_MOTION_MASK |
				     GDK_POINTER_MOTION_HINT_MASK |
				     GDK_BUTTON_RELEASE_MASK),
				    NULL,
				    fleur,
				    event->time) != 0)
		{
		  gtk_handle_box_get_props (hb)->in_drag = FALSE;
		}
	      else
		{
		  __gtk_grab_add (invisible);
		  g_signal_connect (invisible, "event",
				    G_CALLBACK (gtk_handle_box_grab_event), hb);
		}
	      
	      __gdk_cursor_unref (fleur);
	      event_handled = TRUE;
	    }
	  else if (gtk_handle_box_get_props (hb)->child_detached) /* Double click */
	    {
	      gtk_handle_box_reattach (hb);
	    }
	}
    }
  
  return event_handled;
}

static gboolean
gtk_handle_box_motion (GtkWidget      *widget,
		       GdkEventMotion *event)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);
  gint new_x, new_y;
  gint snap_edge;
  gboolean is_snapped = FALSE;
  gint handle_position;
  GdkGeometry geometry;
  GdkScreen *screen, *pointer_screen;

  if (!gtk_handle_box_get_props (hb)->in_drag)
    return FALSE;
  handle_position = effective_handle_position (hb);

  /* Calculate the attachment point on the float, if the float
   * were detached
   */
  new_x = 0;
  new_y = 0;
  screen = __gtk_widget_get_screen (widget);
  __gdk_display_get_pointer (__gdk_screen_get_display (screen),
			   &pointer_screen, 
			   &new_x, &new_y, NULL);
  if (pointer_screen != screen)
    {
      GtkHandleBoxPrivate *private = gtk_handle_box_get_instance_private (hb);

      new_x = private->orig_x;
      new_y = private->orig_y;
    }
  
  new_x += gtk_handle_box_get_props (hb)->float_allocation.x;
  new_y += gtk_handle_box_get_props (hb)->float_allocation.y;

  snap_edge = gtk_handle_box_get_props (hb)->snap_edge;
  if (snap_edge == -1)
    snap_edge = (handle_position == GTK_POS_LEFT ||
		 handle_position == GTK_POS_RIGHT) ?
      GTK_POS_TOP : GTK_POS_LEFT;

  if (__gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) 
    switch (snap_edge) 
      {
      case GTK_POS_LEFT:
	snap_edge = GTK_POS_RIGHT;
	break;
      case GTK_POS_RIGHT:
	snap_edge = GTK_POS_LEFT;
	break;
      default:
	break;
      }

  /* First, check if the snapped edge is aligned
   */
  switch (snap_edge)
    {
    case GTK_POS_TOP:
      is_snapped = abs (gtk_handle_box_get_props (hb)->attach_allocation.y - new_y) < TOLERANCE;
      break;
    case GTK_POS_BOTTOM:
      is_snapped = abs (gtk_handle_box_get_props (hb)->attach_allocation.y + (gint)gtk_handle_box_get_props (hb)->attach_allocation.height -
			new_y - (gint)gtk_handle_box_get_props (hb)->float_allocation.height) < TOLERANCE;
      break;
    case GTK_POS_LEFT:
      is_snapped = abs (gtk_handle_box_get_props (hb)->attach_allocation.x - new_x) < TOLERANCE;
      break;
    case GTK_POS_RIGHT:
      is_snapped = abs (gtk_handle_box_get_props (hb)->attach_allocation.x + (gint)gtk_handle_box_get_props (hb)->attach_allocation.width -
			new_x - (gint)gtk_handle_box_get_props (hb)->float_allocation.width) < TOLERANCE;
      break;
    }

  /* Next, check if coordinates in the other direction are sufficiently
   * aligned
   */
  if (is_snapped)
    {
      gint float_pos1 = 0;	/* Initialize to suppress warnings */
      gint float_pos2 = 0;
      gint attach_pos1 = 0;
      gint attach_pos2 = 0;
      
      switch (snap_edge)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  attach_pos1 = gtk_handle_box_get_props (hb)->attach_allocation.x;
	  attach_pos2 = gtk_handle_box_get_props (hb)->attach_allocation.x + gtk_handle_box_get_props (hb)->attach_allocation.width;
	  float_pos1 = new_x;
	  float_pos2 = new_x + gtk_handle_box_get_props (hb)->float_allocation.width;
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  attach_pos1 = gtk_handle_box_get_props (hb)->attach_allocation.y;
	  attach_pos2 = gtk_handle_box_get_props (hb)->attach_allocation.y + gtk_handle_box_get_props (hb)->attach_allocation.height;
	  float_pos1 = new_y;
	  float_pos2 = new_y + gtk_handle_box_get_props (hb)->float_allocation.height;
	  break;
	}

      is_snapped = ((attach_pos1 - TOLERANCE < float_pos1) && 
		    (attach_pos2 + TOLERANCE > float_pos2)) ||
	           ((float_pos1 - TOLERANCE < attach_pos1) &&
		    (float_pos2 + TOLERANCE > attach_pos2));
    }

  if (is_snapped)
    {
      if (gtk_handle_box_get_props (hb)->child_detached)
	{
	  gtk_handle_box_get_props (hb)->child_detached = FALSE;
	  __gdk_window_hide (gtk_handle_box_get_props (hb)->float_window);
	  __gdk_window_reparent (gtk_handle_box_get_props (hb)->bin_window, gtk_widget_get_props (widget)->window, 0, 0);
	  gtk_handle_box_get_props (hb)->float_window_mapped = FALSE;
	  g_signal_emit (hb,
			 handle_box_signals[SIGNAL_CHILD_ATTACHED],
			 0,
			 gtk_bin_get_props (GTK_BIN (hb))->child);
	  
	  __gtk_widget_queue_resize (widget);
	}
    }
  else
    {
      gint width, height;

      width = __gdk_window_get_width (gtk_handle_box_get_props (hb)->float_window);
      height = __gdk_window_get_height (gtk_handle_box_get_props (hb)->float_window);
      new_x += gtk_handle_box_get_props (hb)->deskoff_x;
      new_y += gtk_handle_box_get_props (hb)->deskoff_y;

      switch (handle_position)
	{
	case GTK_POS_LEFT:
	  new_y += ((gint)gtk_handle_box_get_props (hb)->float_allocation.height - height) / 2;
	  break;
	case GTK_POS_RIGHT:
	  new_x += (gint)gtk_handle_box_get_props (hb)->float_allocation.width - width;
	  new_y += ((gint)gtk_handle_box_get_props (hb)->float_allocation.height - height) / 2;
	  break;
	case GTK_POS_TOP:
	  new_x += ((gint)gtk_handle_box_get_props (hb)->float_allocation.width - width) / 2;
	  break;
	case GTK_POS_BOTTOM:
	  new_x += ((gint)gtk_handle_box_get_props (hb)->float_allocation.width - width) / 2;
	  new_y += (gint)gtk_handle_box_get_props (hb)->float_allocation.height - height;
	  break;
	}

      if (gtk_handle_box_get_props (hb)->child_detached)
	{
	  __gdk_window_move (gtk_handle_box_get_props (hb)->float_window, new_x, new_y);
	  __gdk_window_raise (gtk_handle_box_get_props (hb)->float_window);
	}
      else
	{
	  gint width;
	  gint height;
	  GtkRequisition child_requisition;

	  gtk_handle_box_get_props (hb)->child_detached = TRUE;

	  if (gtk_bin_get_props (GTK_BIN (hb))->child)
	    __gtk_widget_get_child_requisition (gtk_bin_get_props (GTK_BIN (hb))->child, &child_requisition);
	  else
	    {
	      child_requisition.width = 0;
	      child_requisition.height = 0;
	    }      

	  width = child_requisition.width + 2 * gtk_container_get_props (GTK_CONTAINER (hb))->border_width;
	  height = child_requisition.height + 2 * gtk_container_get_props (GTK_CONTAINER (hb))->border_width;

	  if (handle_position == GTK_POS_LEFT || handle_position == GTK_POS_RIGHT)
	    width += DRAG_HANDLE_SIZE;
	  else
	    height += DRAG_HANDLE_SIZE;
	  
	  __gdk_window_move_resize (gtk_handle_box_get_props (hb)->float_window, new_x, new_y, width, height);
	  __gdk_window_reparent (gtk_handle_box_get_props (hb)->bin_window, gtk_handle_box_get_props (hb)->float_window, 0, 0);
	  __gdk_window_set_geometry_hints (gtk_handle_box_get_props (hb)->float_window, &geometry, GDK_HINT_POS);
	  __gdk_window_show (gtk_handle_box_get_props (hb)->float_window);
	  gtk_handle_box_get_props (hb)->float_window_mapped = TRUE;
#if	0
	  /* this extra move is necessary if we use decorations, or our
	   * window manager insists on decorations.
	   */
	  __gdk_display_sync (__gtk_widget_get_display (widget));
	  __gdk_window_move (gtk_handle_box_get_props (hb)->float_window, new_x, new_y);
	  __gdk_display_sync (__gtk_widget_get_display (widget));
#endif	/* 0 */
	  g_signal_emit (hb,
			 handle_box_signals[SIGNAL_CHILD_DETACHED],
			 0,
			 gtk_bin_get_props (GTK_BIN (hb))->child);
	  gtk_handle_box_draw_ghost (hb);
	  
	  __gtk_widget_queue_resize (widget);
	}
    }

  return TRUE;
}

static void
gtk_handle_box_add (GtkContainer *container,
		    GtkWidget    *widget)
{
  __gtk_widget_set_parent_window (widget, gtk_handle_box_get_props (GTK_HANDLE_BOX (container))->bin_window);
  GTK_CONTAINER_CLASS (gtk_handle_box_parent_class)->add (container, widget);
}

static void
gtk_handle_box_remove (GtkContainer *container,
		       GtkWidget    *widget)
{
  GTK_CONTAINER_CLASS (gtk_handle_box_parent_class)->remove (container, widget);

  gtk_handle_box_reattach (GTK_HANDLE_BOX (container));
}

static gint
gtk_handle_box_delete_event (GtkWidget *widget,
			     GdkEventAny  *event)
{
  GtkHandleBox *hb = GTK_HANDLE_BOX (widget);

  if (event->window == gtk_handle_box_get_props (hb)->float_window)
    {
      gtk_handle_box_reattach (hb);
      
      return TRUE;
    }

  return FALSE;
}

static void
gtk_handle_box_reattach (GtkHandleBox *hb)
{
  GtkWidget *widget = GTK_WIDGET (hb);
  
  if (gtk_handle_box_get_props (hb)->child_detached)
    {
      gtk_handle_box_get_props (hb)->child_detached = FALSE;
      if (__gtk_widget_get_realized (widget))
	{
	  __gdk_window_hide (gtk_handle_box_get_props (hb)->float_window);
	  __gdk_window_reparent (gtk_handle_box_get_props (hb)->bin_window, gtk_widget_get_props (widget)->window, 0, 0);

	  if (gtk_bin_get_props (GTK_BIN (hb))->child)
	    g_signal_emit (hb,
			   handle_box_signals[SIGNAL_CHILD_ATTACHED],
			   0,
			   gtk_bin_get_props (GTK_BIN (hb))->child);

	}
      gtk_handle_box_get_props (hb)->float_window_mapped = FALSE;
    }
  if (gtk_handle_box_get_props (hb)->in_drag)
    gtk_handle_box_end_drag (hb, GDK_CURRENT_TIME);

  __gtk_widget_queue_resize (GTK_WIDGET (hb));
}

static void
gtk_handle_box_end_drag (GtkHandleBox *hb,
			 guint32       time)
{
  GtkWidget *invisible = gtk_handle_box_get_invisible ();
		
  gtk_handle_box_get_props (hb)->in_drag = FALSE;

  __gtk_grab_remove (invisible);
  __gdk_pointer_ungrab (time);
  g_signal_handlers_disconnect_by_func (invisible,
					G_CALLBACK (gtk_handle_box_grab_event),
					hb);
}
