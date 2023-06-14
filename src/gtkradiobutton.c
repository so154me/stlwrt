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



#include "config.h"

#include <stlwrt.h>
#include <gtklabel.h>

#include <gtkradiobutton.h>
#include <gtkprivate.h>
#include <gtkintl.h>



enum {
  PROP_0,
  PROP_GROUP
};


static gboolean gtk_radio_button_focus          (GtkWidget           *widget,
						 GtkDirectionType     direction);
static void     gtk_radio_button_clicked        (GtkButton           *button);
static void     gtk_radio_button_draw_indicator (GtkCheckButton      *check_button,
						 GdkRectangle        *area);
static void     gtk_radio_button_set_property   (GObject             *object,
						 guint                prop_id,
						 const GValue        *value,
						 GParamSpec          *pspec);
static void     gtk_radio_button_get_property   (GObject             *object,
						 guint                prop_id,
						 GValue              *value,
						 GParamSpec          *pspec);

STLWRT_DEFINE_FTYPE_VPARENT (GtkRadioButton, gtk_radio_button, GTK_TYPE_CHECK_BUTTON,
                             G_TYPE_FLAG_NONE, ;)

static guint group_changed_signal = 0;

static void
gtk_radio_button_class_init (GtkRadioButtonClass *class)
{
  GObjectClass *gobject_class;
  GtkButtonClass *button_class;
  GtkCheckButtonClass *check_button_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  button_class = (GtkButtonClass*) class;
  check_button_class = (GtkCheckButtonClass*) class;

  gobject_class->set_property = gtk_radio_button_set_property;
  gobject_class->get_property = gtk_radio_button_get_property;

  g_object_class_install_property (gobject_class,
				   PROP_GROUP,
				   g_param_spec_object ("group",
							P_("Group"),
							P_("The radio button whose group this widget belongs to."),
							GTK_TYPE_RADIO_BUTTON,
							GTK_PARAM_WRITABLE));

  widget_class->focus = gtk_radio_button_focus;

  button_class->clicked = gtk_radio_button_clicked;

  check_button_class->draw_indicator = gtk_radio_button_draw_indicator;

  class->group_changed = NULL;

  /**
   * GtkRadioButton::group-changed:
   * @style: the object which received the signal
   *
   * Emitted when the group of radio buttons that a radio button belongs
   * to changes. This is emitted when a radio button switches from
   * being alone to being part of a group of 2 or more buttons, or
   * vice-versa, and when a button is moved from one group of 2 or
   * more buttons to a different one, but not when the composition
   * of the group that a button belongs to changes.
   *
   * Since: 2.4
   */
  group_changed_signal = g_signal_new (I_("group-changed"),
				       G_OBJECT_CLASS_TYPE (gobject_class),
				       G_SIGNAL_RUN_FIRST,
				       G_STRUCT_OFFSET (GtkRadioButtonClass, group_changed),
				       NULL, NULL,
				       NULL,
				       G_TYPE_NONE, 0);
}

static void
gtk_radio_button_init (GtkRadioButton *radio_button)
{
  __gtk_widget_set_has_window (GTK_WIDGET (radio_button), FALSE);
  __gtk_widget_set_receives_default (GTK_WIDGET (radio_button), FALSE);

  gtk_toggle_button_get_props (GTK_TOGGLE_BUTTON (radio_button))->active = TRUE;

  gtk_button_get_props (GTK_BUTTON (radio_button))->depress_on_activate = FALSE;

  gtk_radio_button_get_props (radio_button)->group = g_slist_prepend (NULL, gtk_radio_button_get_props (radio_button));

  ___gtk_button_set_depressed (GTK_BUTTON (radio_button), TRUE);
  __gtk_widget_set_state (GTK_WIDGET (radio_button), GTK_STATE_ACTIVE);
}

static void
gtk_radio_button_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  GtkRadioButton *radio_button;

  radio_button = GTK_RADIO_BUTTON (object);

  switch (prop_id)
    {
      GSList *slist;
      GtkRadioButton *button;

    case PROP_GROUP:
        button = g_value_get_object (value);

      if (button)
	slist = __gtk_radio_button_get_group (button);
      else
	slist = NULL;
      __gtk_radio_button_set_group (radio_button, slist);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_radio_button_get_property (GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * __gtk_radio_button_set_group:
 * @radio_button: a #GtkRadioButton.
 * @group: (transfer none) (element-type GtkRadioButton): an existing radio
 *     button group, such as one returned from __gtk_radio_button_get_group().
 *
 * Sets a #GtkRadioButton's group. It should be noted that this does not change
 * the layout of your interface in any way, so if you are changing the group,
 * it is likely you will need to re-arrange the user interface to reflect these
 * changes.
 */
void
__gtk_radio_button_set_group (GtkRadioButton *radio_button,
			    GSList         *group)
{
  GtkWidget *old_group_singleton = NULL;
  GtkWidget *new_group_singleton = NULL;
  
  g_return_if_fail (GTK_IS_RADIO_BUTTON (radio_button));
  g_return_if_fail (!g_slist_find (group, radio_button));

  if (gtk_radio_button_get_props (radio_button)->group)
    {
      GSList *slist;

      gtk_radio_button_get_props (radio_button)->group = g_slist_remove (gtk_radio_button_get_props (radio_button)->group, gtk_radio_button_get_props (radio_button));
      
      if (gtk_radio_button_get_props (radio_button)->group && !gtk_radio_button_get_props (radio_button)->group->next)
	old_group_singleton = g_object_ref (gtk_radio_button_get_props (radio_button)->group->data);
	  
      for (slist = gtk_radio_button_get_props (radio_button)->group; slist; slist = slist->next)
	{
	  GtkRadioButton *tmp_button;
	  
	  tmp_button = slist->data;
	  
	  gtk_radio_button_get_props (tmp_button)->group = gtk_radio_button_get_props (radio_button)->group;
	}
    }
  
  if (group && !group->next)
    new_group_singleton = g_object_ref (group->data);
  
  gtk_radio_button_get_props (radio_button)->group = g_slist_prepend (group, gtk_radio_button_get_props (radio_button));
  
  if (group)
    {
      GSList *slist;
      
      for (slist = group; slist; slist = slist->next)
	{
	  GtkRadioButton *tmp_button;
	  
	  tmp_button = slist->data;
	  
	  gtk_radio_button_get_props (tmp_button)->group = gtk_radio_button_get_props (radio_button)->group;
	}
    }

  g_object_ref (radio_button);
  
  g_object_notify (G_OBJECT (radio_button), "group");
  g_signal_emit (radio_button, group_changed_signal, 0);
  if (old_group_singleton)
    {
      g_signal_emit (old_group_singleton, group_changed_signal, 0);
      g_object_unref (old_group_singleton);
    }
  if (new_group_singleton)
    {
      g_signal_emit (new_group_singleton, group_changed_signal, 0);
      g_object_unref (new_group_singleton);
    }

  __gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), group == NULL);

  g_object_unref (radio_button);
}

/**
 * __gtk_radio_button_new:
 * @group: (allow-none): an existing radio button group, or %NULL if you are
 *  creating a new group.
 *
 * Creates a new #GtkRadioButton. To be of any practical value, a widget should
 * then be packed into the radio button.
 *
 * Returns: a new radio button
 */
GtkWidget*
__gtk_radio_button_new (GSList *group)
{
  GtkRadioButton *radio_button;

  radio_button = g_object_new (GTK_TYPE_RADIO_BUTTON, NULL);

  if (group)
    __gtk_radio_button_set_group (radio_button, group);

  return GTK_WIDGET (radio_button);
}

/**
 * __gtk_radio_button_new_with_label:
 * @group: (allow-none): an existing radio button group, or %NULL if you are
 *  creating a new group.
 * @label: the text label to display next to the radio button.
 *
 * Creates a new #GtkRadioButton with a text label.
 *
 * Returns: a new radio button.
 */
GtkWidget*
__gtk_radio_button_new_with_label (GSList      *group,
				 const gchar *label)
{
  GtkWidget *radio_button;

  radio_button = g_object_new (GTK_TYPE_RADIO_BUTTON, "label", label, NULL) ;

  if (group)
    __gtk_radio_button_set_group (GTK_RADIO_BUTTON (radio_button), group);

  return radio_button;
}

/**
 * __gtk_radio_button_new_with_mnemonic:
 * @group: the radio button group
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 *
 * Creates a new #GtkRadioButton containing a label, adding it to the same 
 * group as @group. The label will be created using 
 * __gtk_label_new_with_mnemonic(), so underscores in @label indicate the 
 * mnemonic for the button.
 *
 * Returns: a new #GtkRadioButton
 **/
GtkWidget*
__gtk_radio_button_new_with_mnemonic (GSList      *group,
				    const gchar *label)
{
  GtkWidget *radio_button;

  radio_button = g_object_new (GTK_TYPE_RADIO_BUTTON, 
			       "label", label, 
			       "use-underline", TRUE, 
			       NULL);

  if (group)
    __gtk_radio_button_set_group (GTK_RADIO_BUTTON (radio_button), group);

  return radio_button;
}

/**
 * __gtk_radio_button_new_from_widget:
 * @radio_group_member: (allow-none): an existing #GtkRadioButton.
 *
 * Creates a new #GtkRadioButton, adding it to the same group as
 * @radio_group_member. As with __gtk_radio_button_new(), a widget
 * should be packed into the radio button.
 *
 * Returns: (transfer none): a new radio button.
 */
GtkWidget*
__gtk_radio_button_new_from_widget (GtkRadioButton *radio_group_member)
{
  GSList *l = NULL;
  if (radio_group_member)
    l = __gtk_radio_button_get_group (radio_group_member);
  return __gtk_radio_button_new (l);
}

/**
 * __gtk_radio_button_new_with_label_from_widget: (constructor)
 * @radio_group_member: (allow-none): widget to get radio group from or %NULL
 * @label: a text string to display next to the radio button.
 *
 * Creates a new #GtkRadioButton with a text label, adding it to
 * the same group as @radio_group_member.
 *
 * Returns: (transfer none): a new radio button.
 */
GtkWidget*
__gtk_radio_button_new_with_label_from_widget (GtkRadioButton *radio_group_member,
					     const gchar    *label)
{
  GSList *l = NULL;
  if (radio_group_member)
    l = __gtk_radio_button_get_group (radio_group_member);
  return __gtk_radio_button_new_with_label (l, label);
}

/**
 * __gtk_radio_button_new_with_mnemonic_from_widget: (constructor)
 * @radio_group_member: (allow-none): widget to get radio group from or %NULL
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 *
 * Creates a new #GtkRadioButton containing a label. The label
 * will be created using __gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the button.
 *
 * Returns: (transfer none): a new #GtkRadioButton
 **/
GtkWidget*
__gtk_radio_button_new_with_mnemonic_from_widget (GtkRadioButton *radio_group_member,
					        const gchar    *label)
{
  GSList *l = NULL;
  if (radio_group_member)
    l = __gtk_radio_button_get_group (radio_group_member);
  return __gtk_radio_button_new_with_mnemonic (l, label);
}


/**
 * __gtk_radio_button_get_group:
 * @radio_button: a #GtkRadioButton.
 *
 * Retrieves the group assigned to a radio button.
 *
 * Return value: (element-type GtkRadioButton) (transfer none): a linked list
 * containing all the radio buttons in the same group
 * as @radio_button. The returned list is owned by the radio button
 * and must not be modified or freed.
 */
GSList*
__gtk_radio_button_get_group (GtkRadioButton *radio_button)
{
  g_return_val_if_fail (GTK_IS_RADIO_BUTTON (radio_button), NULL);

  return gtk_radio_button_get_props (radio_button)->group;
}


static void
get_coordinates (GtkWidget    *widget,
		 GtkWidget    *reference,
		 gint         *x,
		 gint         *y)
{
  *x = gtk_widget_get_props (widget)->allocation.x + gtk_widget_get_props (widget)->allocation.width / 2;
  *y = gtk_widget_get_props (widget)->allocation.y + gtk_widget_get_props (widget)->allocation.height / 2;
  
  __gtk_widget_translate_coordinates (widget, reference, *x, *y, x, y);
}

static gint
left_right_compare (gconstpointer a,
		    gconstpointer b,
		    gpointer      data)
{
  gint x1, y1, x2, y2;

  get_coordinates ((GtkWidget *)a, data, &x1, &y1);
  get_coordinates ((GtkWidget *)b, data, &x2, &y2);

  if (y1 == y2)
    return (x1 < x2) ? -1 : ((x1 == x2) ? 0 : 1);
  else
    return (y1 < y2) ? -1 : 1;
}

static gint
up_down_compare (gconstpointer a,
		 gconstpointer b,
		 gpointer      data)
{
  gint x1, y1, x2, y2;
  
  get_coordinates ((GtkWidget *)a, data, &x1, &y1);
  get_coordinates ((GtkWidget *)b, data, &x2, &y2);
  
  if (x1 == x2)
    return (y1 < y2) ? -1 : ((y1 == y2) ? 0 : 1);
  else
    return (x1 < x2) ? -1 : 1;
}

static gboolean
gtk_radio_button_focus (GtkWidget         *widget,
			GtkDirectionType   direction)
{
  GtkRadioButton *radio_button = GTK_RADIO_BUTTON (widget);
  GSList *tmp_slist;

  /* Radio buttons with draw_indicator unset focus "normally", since
   * they look like buttons to the user.
   */
  if (!gtk_toggle_button_get_props (GTK_TOGGLE_BUTTON (widget))->draw_indicator)
    return GTK_WIDGET_CLASS (gtk_radio_button_parent_class)->focus (widget, direction);
  
  if (__gtk_widget_is_focus (widget))
    {
      GtkSettings *settings = __gtk_widget_get_settings (widget);
      GSList *focus_list, *tmp_list;
      GtkWidget *toplevel = __gtk_widget_get_toplevel (widget);
      GtkWidget *new_focus = NULL;
      gboolean cursor_only;
      gboolean wrap_around;

      switch (direction)
	{
	case GTK_DIR_LEFT:
	case GTK_DIR_RIGHT:
	  focus_list = g_slist_copy (gtk_radio_button_get_props (radio_button)->group);
	  focus_list = g_slist_sort_with_data (focus_list, left_right_compare, toplevel);
	  break;
	case GTK_DIR_UP:
	case GTK_DIR_DOWN:
	  focus_list = g_slist_copy (gtk_radio_button_get_props (radio_button)->group);
	  focus_list = g_slist_sort_with_data (focus_list, up_down_compare, toplevel);
	  break;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_TAB_BACKWARD:
          /* fall through */
        default:
	  return FALSE;
	}

      if (direction == GTK_DIR_LEFT || direction == GTK_DIR_UP)
	focus_list = g_slist_reverse (focus_list);

      tmp_list = g_slist_find (focus_list, widget);

      if (tmp_list)
	{
	  tmp_list = tmp_list->next;
	  
	  while (tmp_list)
	    {
	      GtkWidget *child = tmp_list->data;
	      
	      if (__gtk_widget_get_mapped (child) && __gtk_widget_is_sensitive (child))
		{
		  new_focus = child;
		  break;
		}

	      tmp_list = tmp_list->next;
	    }
	}

      g_object_get (settings,
                    "gtk-keynav-cursor-only", &cursor_only,
                    "gtk-keynav-wrap-around", &wrap_around,
                    NULL);

      if (!new_focus)
	{
          if (cursor_only)
            {
              g_slist_free (focus_list);
              return FALSE;
            }

          if (!wrap_around)
            {
              g_slist_free (focus_list);
              __gtk_widget_error_bell (widget);
              return TRUE;
            }

	  tmp_list = focus_list;

	  while (tmp_list)
	    {
	      GtkWidget *child = tmp_list->data;
	      
	      if (__gtk_widget_get_mapped (child) && __gtk_widget_is_sensitive (child))
		{
		  new_focus = child;
		  break;
		}
	      
	      tmp_list = tmp_list->next;
	    }
	}
      
      g_slist_free (focus_list);

      if (new_focus)
	{
	  __gtk_widget_grab_focus (new_focus);

          if (!cursor_only)
            __gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (new_focus), TRUE);
	}

      return TRUE;
    }
  else
    {
      GtkRadioButton *selected_button = NULL;
      
      /* We accept the focus if, we don't have the focus and
       *  - we are the currently active button in the group
       *  - there is no currently active radio button.
       */
      
      tmp_slist = gtk_radio_button_get_props (radio_button)->group;
      while (tmp_slist)
	{
	  if (gtk_toggle_button_get_props (GTK_TOGGLE_BUTTON (tmp_slist->data))->active)
	    selected_button = tmp_slist->data;
	  tmp_slist = tmp_slist->next;
	}
      
      if (selected_button && selected_button != radio_button)
	return FALSE;

      __gtk_widget_grab_focus (widget);
      return TRUE;
    }
}

static void
gtk_radio_button_clicked (GtkButton *button)
{
  GtkToggleButton *toggle_button;
  GtkRadioButton *radio_button;
  GtkToggleButton *tmp_button;
  GtkStateType new_state;
  GSList *tmp_list;
  gint toggled;
  gboolean depressed;

  radio_button = GTK_RADIO_BUTTON (button);
  toggle_button = GTK_TOGGLE_BUTTON (button);
  toggled = FALSE;

  g_object_ref (GTK_WIDGET (button));

  if (gtk_toggle_button_get_props (toggle_button)->active)
    {
      tmp_button = NULL;
      tmp_list = gtk_radio_button_get_props (radio_button)->group;

      while (tmp_list)
	{
	  tmp_button = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (gtk_toggle_button_get_props (tmp_button)->active && gtk_toggle_button_get_props (tmp_button) != toggle_button)
	    break;

	  tmp_button = NULL;
	}

      if (!tmp_button)
	{
	  new_state = (gtk_button_get_props (button)->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_ACTIVE);
	}
      else
	{
	  toggled = TRUE;
	  gtk_toggle_button_get_props (toggle_button)->active = !gtk_toggle_button_get_props (toggle_button)->active;
	  new_state = (gtk_button_get_props (button)->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL);
	}
    }
  else
    {
      toggled = TRUE;
      gtk_toggle_button_get_props (toggle_button)->active = !gtk_toggle_button_get_props (toggle_button)->active;
      
      tmp_list = gtk_radio_button_get_props (radio_button)->group;
      while (tmp_list)
	{
	  tmp_button = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (gtk_toggle_button_get_props (tmp_button)->active && (gtk_toggle_button_get_props (tmp_button) != toggle_button))
	    {
	      __gtk_button_clicked (GTK_BUTTON (tmp_button));
	      break;
	    }
	}

      new_state = (gtk_button_get_props (button)->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_ACTIVE);
    }

  if (gtk_toggle_button_get_props (toggle_button)->inconsistent)
    depressed = FALSE;
  else if (gtk_button_get_props (button)->in_button && gtk_button_get_props (button)->button_down)
    depressed = !gtk_toggle_button_get_props (toggle_button)->active;
  else
    depressed = gtk_toggle_button_get_props (toggle_button)->active;

  if (__gtk_widget_get_state (GTK_WIDGET (button)) != new_state)
    __gtk_widget_set_state (GTK_WIDGET (button), new_state);

  if (toggled)
    {
      __gtk_toggle_button_toggled (toggle_button);

      g_object_notify (G_OBJECT (toggle_button), "active");
    }

  ___gtk_button_set_depressed (button, depressed);

  __gtk_widget_queue_draw (GTK_WIDGET (button));

  g_object_unref (button);
}

static void
gtk_radio_button_draw_indicator (GtkCheckButton *check_button,
				 GdkRectangle   *area)
{
  GtkWidget *widget;
  GtkWidget *child;
  GtkButton *button;
  GtkToggleButton *toggle_button;
  GtkStateType state_type;
  GtkShadowType shadow_type;
  gint x, y;
  gint indicator_size, indicator_spacing;
  gint focus_width;
  gint focus_pad;
  gboolean interior_focus;

  widget = GTK_WIDGET (check_button);

  if (__gtk_widget_is_drawable (widget))
    {
      button = GTK_BUTTON (check_button);
      toggle_button = GTK_TOGGLE_BUTTON (check_button);

      __gtk_widget_style_get (widget,
			    "interior-focus", &interior_focus,
			    "focus-line-width", &focus_width,
			    "focus-padding", &focus_pad,
			    NULL);

      ___gtk_check_button_get_props (check_button, &indicator_size, &indicator_spacing);

      x = gtk_widget_get_props (widget)->allocation.x + indicator_spacing + gtk_container_get_props (GTK_CONTAINER (widget))->border_width;
      y = gtk_widget_get_props (widget)->allocation.y + (gtk_widget_get_props (widget)->allocation.height - indicator_size) / 2;

      child = gtk_bin_get_props (GTK_BIN (check_button))->child;
      if (!interior_focus || !(child && __gtk_widget_get_visible (child)))
	x += focus_width + focus_pad;      

      if (gtk_toggle_button_get_props (toggle_button)->inconsistent)
	shadow_type = GTK_SHADOW_ETCHED_IN;
      else if (gtk_toggle_button_get_props (toggle_button)->active)
	shadow_type = GTK_SHADOW_IN;
      else
	shadow_type = GTK_SHADOW_OUT;

      if (gtk_button_get_props (button)->activate_timeout || (gtk_button_get_props (button)->button_down && gtk_button_get_props (button)->in_button))
	state_type = GTK_STATE_ACTIVE;
      else if (gtk_button_get_props (button)->in_button)
	state_type = GTK_STATE_PRELIGHT;
      else if (!__gtk_widget_is_sensitive (widget))
	state_type = GTK_STATE_INSENSITIVE;
      else
	state_type = GTK_STATE_NORMAL;

      if (__gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	x = gtk_widget_get_props (widget)->allocation.x + gtk_widget_get_props (widget)->allocation.width - (indicator_size + x - gtk_widget_get_props (widget)->allocation.x);

      if (__gtk_widget_get_state (widget) == GTK_STATE_PRELIGHT)
	{
	  GdkRectangle restrict_area;
	  GdkRectangle new_area;
	      
	  restrict_area.x = gtk_widget_get_props (widget)->allocation.x + gtk_container_get_props (GTK_CONTAINER (widget))->border_width;
	  restrict_area.y = gtk_widget_get_props (widget)->allocation.y + gtk_container_get_props (GTK_CONTAINER (widget))->border_width;
	  restrict_area.width = gtk_widget_get_props (widget)->allocation.width - (2 * gtk_container_get_props (GTK_CONTAINER (widget))->border_width);
	  restrict_area.height = gtk_widget_get_props (widget)->allocation.height - (2 * gtk_container_get_props (GTK_CONTAINER (widget))->border_width);
	  
	  if (__gdk_rectangle_intersect (area, &restrict_area, &new_area))
	    {
	      __gtk_paint_flat_box (gtk_widget_get_props (widget)->style, gtk_widget_get_props (widget)->window, GTK_STATE_PRELIGHT,
				  GTK_SHADOW_ETCHED_OUT, 
				  area, widget, "checkbutton",
				  new_area.x, new_area.y,
				  new_area.width, new_area.height);
	    }
	}

      __gtk_paint_option (gtk_widget_get_props (widget)->style, gtk_widget_get_props (widget)->window,
			state_type, shadow_type,
			area, widget, "radiobutton",
			x, y, indicator_size, indicator_size);
    }
}
