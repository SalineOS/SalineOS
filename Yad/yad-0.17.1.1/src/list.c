/*
 * This file is part of YAD.
 *
 * YAD is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * YAD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with YAD. If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2008-2012, Victor Ananjevsky <ananasik@gmail.com>
 */

#include <string.h>
#include <stdlib.h>

#include <glib/gprintf.h>
#include <gdk/gdkkeysyms.h>

#include "yad.h"

static GtkWidget *list_view;

static gint fore_col, back_col, font_col;

static gboolean
list_activate_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
#if GTK_CHECK_VERSION(2,24,0)
  if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)
#else
  if (event->keyval == GDK_Return || event->keyval == GDK_KP_Enter)
#endif
    {
      if (options.list_data.dclick_action)
	{
	  /* FIXME: check this under gtk-3.0 */
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_dialog_response (GTK_DIALOG (data), YAD_RESPONSE_OK);
	  else
	    return FALSE;
	}
      else
	gtk_dialog_response (GTK_DIALOG (data), YAD_RESPONSE_OK);

      return TRUE;
    }
  return FALSE;
}

static void
toggled_cb (GtkCellRendererToggle *cell,
            gchar *path_str, gpointer data)
{
  gint column;
  gboolean fixed;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));

  column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, column, &fixed, -1);

  fixed ^= 1;

  gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, fixed, -1);

  gtk_tree_path_free (path);
}

static void
cell_edited_cb (GtkCellRendererText * cell,
                const gchar * path_string,
                const gchar * new_text, gpointer data)
{
  gint column;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));
  YadColumn *col;

  column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));
  gtk_tree_model_get_iter (model, &iter, path);
  col = (YadColumn *) g_slist_nth_data (options.list_data.columns, column);

  if (col->type == YAD_COLUMN_NUM)
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, g_ascii_strtoll (new_text, NULL, 10), -1);
  else if (col->type == YAD_COLUMN_FLOAT)
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, g_ascii_strtod (new_text, NULL), -1);
  else
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, new_text, -1);

  gtk_tree_path_free (path);
}

static void
tooltip_cb (GtkWidget * w, gint x, gint y,
	    gboolean kmode, GtkTooltip * tip,
	    gpointer data)
{
  GtkTreeModel *model = NULL;
  GtkTreePath *path = NULL;
  GtkTreeIter iter;
  gint cnum = -1;

  if (gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (list_view), &x, &y, kmode,
					 &model, &path, &iter))
    {
      GtkTreeViewColumn *col = NULL;
      GList *cols, *node;
      guint colx = 0;

      /* find current column */
      cols = gtk_tree_view_get_columns (GTK_TREE_VIEW (list_view));
      for (node = cols;  node != NULL && col == NULL;  node = node->next)
	{
	  GtkTreeViewColumn *checkcol = (GtkTreeViewColumn*) node->data;

	  cnum++;
	  if (x >= colx  &&  x < (colx + gtk_tree_view_column_get_width (checkcol)))
	    col = checkcol;
	  else
	    colx += gtk_tree_view_column_get_width (checkcol);

	}
      g_list_free(cols);

      /* set tolltip */
      if (col)
	{
	  YadColumn *yc;
	  gchar *text = NULL;

	  yc = (YadColumn *) g_slist_nth_data (options.list_data.columns, cnum);
	  switch (yc->type)
	    {
	    case YAD_COLUMN_NUM:
	      {
		gint64 nval;
		gtk_tree_model_get (model, &iter, cnum, &nval, -1);
		text = g_strdup_printf ("%ld", nval);
		break;
	      }
	    case YAD_COLUMN_FLOAT:
	      {
		gdouble nval;
		gtk_tree_model_get (model, &iter, cnum, &nval, -1);
		text = g_strdup_printf ("%lf", nval);
		break;
	      }
	    case YAD_COLUMN_TOOLTIP:
	    case YAD_COLUMN_TEXT:
	      {
		gchar *cval;
		gtk_tree_model_get (model, &iter, cnum, &cval, -1);
		text = g_strdup_printf ("%s", cval);
		break;
	      }
	    }

	  if (text)
	    {
	      gtk_tooltip_set_text (tip, text);
	      g_free (text);
	    }
	}
    }
}

static gboolean
regex_search (GtkTreeModel *model, gint col, const gchar *key,
	      GtkTreeIter *iter, gpointer data)
{
  static GRegex *pattern = NULL;
  static guint pos = 0;
  gchar *str;

  if (key[pos])
    {
      if (pattern)
	g_regex_unref (pattern);
      pattern = g_regex_new (key, G_REGEX_CASELESS | G_REGEX_EXTENDED | G_REGEX_OPTIMIZE,
			     G_REGEX_MATCH_NOTEMPTY, NULL);
      pos = strlen (key);
    }

  if (pattern)
    {
      gboolean ret;

      gtk_tree_model_get (model, iter, col, &str, -1);

      ret = g_regex_match (pattern, str, G_REGEX_MATCH_NOTEMPTY, NULL);
      /* if get it, clear key end position */
      if (!ret)
        pos = 0;

      return !ret;
    }
  else
    return TRUE;
}

static GtkTreeModel *
create_model (gint n_columns)
{
  GtkListStore *store;
  GType *ctypes;
  gint i;

  ctypes = g_new0 (GType, n_columns + 1);

  if (options.list_data.checkbox)
    {
      YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, 0);
      col->type = YAD_COLUMN_CHECK;
    }

  for (i = 0; i < n_columns; i++)
    {
      YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, i);

      switch (col->type)
        {
        case YAD_COLUMN_CHECK:
          ctypes[i] = G_TYPE_BOOLEAN;
          break;
        case YAD_COLUMN_NUM:
          ctypes[i] = G_TYPE_INT64;
          break;
        case YAD_COLUMN_FLOAT:
          ctypes[i] = G_TYPE_DOUBLE;
          break;
        case YAD_COLUMN_IMAGE:
          ctypes[i] = GDK_TYPE_PIXBUF;
          break;
	case YAD_COLUMN_ATTR_FORE:
	  ctypes[i] = G_TYPE_STRING;
	  fore_col = i;
	  break;
	case YAD_COLUMN_ATTR_BACK:
	  ctypes[i] = G_TYPE_STRING;
	  back_col = i;
	  break;
	case YAD_COLUMN_ATTR_FONT:
	  ctypes[i] = G_TYPE_STRING;
	  font_col = i;
	  break;
        case YAD_COLUMN_TOOLTIP:
        case YAD_COLUMN_TEXT:
        default:
          ctypes[i] = G_TYPE_STRING;
          break;
        }
    }
  ctypes[n_columns] = PANGO_TYPE_ELLIPSIZE_MODE;

  store = gtk_list_store_newv (n_columns + 1, ctypes);

  return GTK_TREE_MODEL (store);
}

static void
add_columns (gint n_columns)
{
  gint i;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  for (i = 0; i < n_columns; i++)
    {
      YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, i);

      if (i == options.list_data.hide_column - 1 ||
	  i == fore_col || i == back_col || i == font_col)
        continue;

      switch (col->type)
        {
        case YAD_COLUMN_CHECK:
          renderer = gtk_cell_renderer_toggle_new ();
          g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (i));
          g_signal_connect (renderer, "toggled", G_CALLBACK (toggled_cb), NULL);
          column = gtk_tree_view_column_new_with_attributes (col->name, renderer, "active", i, NULL);
	  if (back_col != -1)
	    gtk_tree_view_column_add_attribute (column, renderer, "cell-background", back_col);
          break;
        case YAD_COLUMN_IMAGE:
          renderer = gtk_cell_renderer_pixbuf_new ();
          column = gtk_tree_view_column_new_with_attributes (col->name, renderer, "pixbuf", i, NULL);
	  if (back_col != -1)
	    gtk_tree_view_column_add_attribute (column, renderer, "cell-background", back_col);
          break;
        case YAD_COLUMN_NUM:
        case YAD_COLUMN_FLOAT:
          renderer = gtk_cell_renderer_text_new ();
          if (options.common_data.editable)
            {
              g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
              g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (i));
              g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited_cb), NULL);
            }
          column = gtk_tree_view_column_new_with_attributes (col->name, renderer, "text", i, NULL);
	  if (fore_col != -1)
	    gtk_tree_view_column_add_attribute (column, renderer, "foreground", fore_col);
	  if (back_col != -1)
	    gtk_tree_view_column_add_attribute (column, renderer, "cell-background", back_col);
	  if (font_col != -1)
	    gtk_tree_view_column_add_attribute (column, renderer, "font", font_col);
          gtk_tree_view_column_set_sort_column_id (column, i);
          gtk_tree_view_column_set_resizable  (column, TRUE);
          break;
        case YAD_COLUMN_TEXT:
        case YAD_COLUMN_TOOLTIP:
        default:
          renderer = gtk_cell_renderer_text_new ();
          if (options.common_data.editable)
            {
              g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
              g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (i));
              g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited_cb), NULL);
            }
          column = gtk_tree_view_column_new_with_attributes (col->name, renderer, "markup", i, NULL);
	  gtk_tree_view_column_add_attribute (column, renderer, "ellipsize", n_columns);
	  if (fore_col != -1)
	    gtk_tree_view_column_add_attribute (column, renderer, "foreground", fore_col);
	  if (back_col != -1)
	    gtk_tree_view_column_add_attribute (column, renderer, "cell-background", back_col);
	  if (font_col != -1)
	    gtk_tree_view_column_add_attribute (column, renderer, "font", font_col);
          gtk_tree_view_column_set_sort_column_id (column, i);
          gtk_tree_view_column_set_resizable (column, TRUE);
          break;
        }
      gtk_tree_view_append_column (GTK_TREE_VIEW (list_view), column);

      if (col->type != YAD_COLUMN_CHECK || col->type != YAD_COLUMN_IMAGE)
	{
	  if (i == options.list_data.expand_column - 1 ||
	      options.list_data.expand_column == 0)
	    gtk_tree_view_column_set_expand (column, TRUE);
	}
    }

  if (options.list_data.checkbox && !options.list_data.search_column)
    options.list_data.search_column += 1;
  if (options.list_data.search_column <= n_columns)
    {
      options.list_data.search_column -= 1;
      gtk_tree_view_set_search_column (GTK_TREE_VIEW (list_view), options.list_data.search_column);
    }
}

static gboolean
handle_stdin (GIOChannel * channel,
              GIOCondition condition, gpointer data)
{
  static GtkTreeIter iter;
  static gint column_count = 0;
  static gint row_count = 0;
  static gboolean first_time = TRUE;
  gint n_columns = GPOINTER_TO_INT (data);
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));

  if (first_time)
    {
      first_time = FALSE;
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    }

  if ((condition == G_IO_IN) || (condition == G_IO_IN + G_IO_HUP))
    {
      GError *err = NULL;
      GString *string = g_string_new (NULL);

      while (channel->is_readable != TRUE) ;

      do
        {
          YadColumn *col;
          GdkPixbuf *pb;
          gint status;
          gchar *val;

          do
            {
              status =
                g_io_channel_read_line_string (channel, string, NULL, &err);

              while (gtk_events_pending ())
                gtk_main_iteration ();
            }
          while (status == G_IO_STATUS_AGAIN);

          if (status != G_IO_STATUS_NORMAL)
            {
              if (err)
                {
                  g_printerr ("yad_list_handle_stdin(): %s\n", err->message);
                  g_error_free (err);
                  err = NULL;
                }
              continue;
            }

          strip_new_line (string->str);
          if (string->str[0] == '\014')
            {
              /* clear list if ^L received */
              gtk_list_store_clear (GTK_LIST_STORE (model));
              row_count = column_count = 0;
              continue;
            }

          if (column_count == n_columns)
            {
	      /* add eliipsize */
              gtk_list_store_set (GTK_LIST_STORE (model), &iter, column_count,
				  options.list_data.ellipsize, -1);
              /* We're starting a new row */
              column_count = 0;
              row_count++;
	      if (options.list_data.limit && row_count >= options.list_data.limit)
		{
		  gtk_tree_model_get_iter_first (model, &iter);
		  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		}
              gtk_list_store_append (GTK_LIST_STORE (model), &iter);
            }

          col = (YadColumn *) g_slist_nth_data (options.list_data.columns, column_count);

          switch (col->type)
            {
            case YAD_COLUMN_CHECK:
              if (g_ascii_strcasecmp (string->str, "true") == 0)
                gtk_list_store_set (GTK_LIST_STORE (model), &iter, column_count, TRUE, -1);
              else
                gtk_list_store_set (GTK_LIST_STORE (model), &iter, column_count, FALSE, -1);
              break;
            case YAD_COLUMN_NUM:
              gtk_list_store_set (GTK_LIST_STORE (model), &iter, column_count,
				  g_ascii_strtoll (string->str, NULL, 10), -1);
              break;
            case YAD_COLUMN_FLOAT:
              gtk_list_store_set (GTK_LIST_STORE (model), &iter, column_count,
				  g_ascii_strtod (string->str, NULL), -1);
              break;
            case YAD_COLUMN_IMAGE:
              pb = get_pixbuf (string->str, YAD_SMALL_ICON);
              gtk_list_store_set (GTK_LIST_STORE (model), &iter, column_count, pb, -1);
              if (pb)
                g_object_unref (pb);
              break;
	    case YAD_COLUMN_ATTR_FORE:
	    case YAD_COLUMN_ATTR_BACK:
	    case YAD_COLUMN_ATTR_FONT:
              gtk_list_store_set (GTK_LIST_STORE (model), &iter, column_count, string->str, -1);
	      break;
            default:
              val = escape_markup (string->str);
              gtk_list_store_set (GTK_LIST_STORE (model), &iter, column_count, val, -1);
              g_free (val);
              break;
            }

          column_count++;
        }
      while (g_io_channel_get_buffer_condition (channel) == G_IO_IN);
      g_string_free (string, TRUE);
    }

  if ((condition != G_IO_IN) && (condition != G_IO_IN + G_IO_HUP))
    {
      g_io_channel_shutdown (channel, TRUE, NULL);
      return FALSE;
    }

  return TRUE;
}

static void
fill_data (gint n_columns)
{
  GtkTreeIter iter;
  GtkListStore *model =
    GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (list_view)));
  GIOChannel *channel;

  if (options.extra_data && *options.extra_data)
    {
      gchar **args = options.extra_data;
      gint i = 0;

      while (args[i] != NULL)
        {
          gint j;

          gtk_list_store_append (model, &iter);
          for (j = 0; j < n_columns; j++)
            {
              YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, j);
              GdkPixbuf *pb;
              gchar *val;

              if (args[i] == NULL)
                break;

              switch (col->type)
                {
                case YAD_COLUMN_CHECK:
                  if (g_ascii_strcasecmp ((gchar *) args[i], "true") == 0)
                    gtk_list_store_set (model, &iter, j, TRUE, -1);
                  else
                    gtk_list_store_set (model, &iter, j, FALSE, -1);
                  break;
                case YAD_COLUMN_NUM:
                  gtk_list_store_set (GTK_LIST_STORE (model), &iter, j,
                                      g_ascii_strtoll (args[i], NULL, 10), -1);
                  break;
                case YAD_COLUMN_FLOAT:
                  gtk_list_store_set (GTK_LIST_STORE (model), &iter, j,
                                      g_ascii_strtod (args[i], NULL), -1);
                  break;
                case YAD_COLUMN_IMAGE:
                  pb = get_pixbuf (args[i], YAD_SMALL_ICON);
                  gtk_list_store_set (GTK_LIST_STORE (model), &iter, j, pb, -1);
                  if (pb)
                    g_object_unref (pb);
                  break;
		case YAD_COLUMN_ATTR_FORE:
		case YAD_COLUMN_ATTR_BACK:
		case YAD_COLUMN_ATTR_FONT:
                  gtk_list_store_set (GTK_LIST_STORE (model), &iter, j, args[i], -1);
		  break;
                default:
                  val = escape_markup (args[i]);
                  gtk_list_store_set (GTK_LIST_STORE (model), &iter, j, val, -1);
                  g_free (val);
                  break;
                }
              i++;
            }
	  /* set ellipsize */
	  gtk_list_store_set (GTK_LIST_STORE (model), &iter, n_columns, options.list_data.ellipsize, -1);
        }

      if (settings.always_selected)
	{
	  GtkTreeIter it;
	  GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (list_view));
	  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));

	  gtk_tree_model_get_iter_first (model, &it);
	  gtk_tree_selection_select_iter (sel, &it);
	}
    }

  channel = g_io_channel_unix_new (0);
  g_io_channel_set_encoding (channel, NULL, NULL);
  g_io_channel_set_flags (channel, G_IO_FLAG_NONBLOCK, NULL);
  g_io_add_watch (channel, G_IO_IN | G_IO_HUP, handle_stdin, GINT_TO_POINTER (n_columns));
}

static void
double_click_cb (GtkTreeView *view, GtkTreePath *path,
                 GtkTreeViewColumn *column, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_tree_view_get_model (view);

  if (options.list_data.dclick_action)
    {
      gchar *cmd;
      GString *args;

      args = g_string_new ("");

      if (gtk_tree_model_get_iter (model, &iter, path))
	{
	  gint i;

	  for (i = 0; i < gtk_tree_model_get_n_columns (model) - 1; i++)
	    {
	      YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, i);
	      switch (col->type)
		{
		case YAD_COLUMN_CHECK:
		  {
		    gboolean bval;
		    gtk_tree_model_get (model, &iter, i, &bval, -1);
		    g_string_append_printf (args, " %s", bval ? "TRUE" : "FALSE");
		    break;
		  }
		case YAD_COLUMN_NUM:
		  {
		    gint64 nval;
		    gtk_tree_model_get (model, &iter, i, &nval, -1);
		    g_string_append_printf (args, " %ld", nval);
		    break;
		  }
		case YAD_COLUMN_FLOAT:
		  {
		    gdouble nval;
		    gtk_tree_model_get (model, &iter, i, &nval, -1);
		    g_string_append_printf (args, " %lf", nval);
		    break;
		  }
		case YAD_COLUMN_IMAGE:
		  {
		    g_string_append_printf (args, " ''");
		    break;
		  }
		case YAD_COLUMN_ATTR_FORE:
		case YAD_COLUMN_ATTR_BACK:
		case YAD_COLUMN_ATTR_FONT:
		  break;
		default:
		  {
		    gchar *cval, *uval;
		    gtk_tree_model_get (model, &iter, i, &cval, -1);
		    uval = unescape_markup (cval);
		    g_string_append_printf (args, " '%s'", uval);
		    g_free (uval);
		    break;
		  }
		}
	    }
	}

      if (g_strstr_len (options.list_data.dclick_action, -1, "%s"))
	{
	  static GRegex *regex = NULL;

	  if (!regex)
	    regex = g_regex_new ("\%s", G_REGEX_OPTIMIZE, 0, NULL);
	  cmd = g_regex_replace_literal (regex, options.list_data.dclick_action, -1, 0, args->str, 0, NULL);
	}
      else
	cmd = g_strdup_printf ("%s %s", options.list_data.dclick_action, args->str);
      g_string_free (args, TRUE);

      g_spawn_command_line_async (cmd, NULL);
      g_free (cmd);
    }
  else
    {
      if (options.list_data.checkbox)
	{
	  if (gtk_tree_model_get_iter (model, &iter, path))
	    {
	      gboolean chk;

	      gtk_tree_model_get (model, &iter, 0, &chk, -1);
	      chk = !chk;
	      gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, chk, -1);
	    }
	}
      else
	gtk_dialog_response (GTK_DIALOG (data), YAD_RESPONSE_OK);
    }
}

void
add_row_cb (GtkMenuItem *item, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));

  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
}

void
del_row_cb (GtkMenuItem *item, gpointer data)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));
  GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (list_view));

  if (gtk_tree_selection_get_selected (sel, NULL, &iter))
    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static gboolean
popup_menu_cb (GtkWidget *w, GdkEventButton *ev, gpointer data)
{
  static GtkWidget *menu = NULL;
  if (ev->button == 3)
    {
      GtkWidget *item;

      if (menu == NULL)
        {
          menu = gtk_menu_new ();
 
          item = gtk_image_menu_item_new_with_label (_("Add row"));
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                         gtk_image_new_from_stock
					 (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
          gtk_widget_show (item);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          g_signal_connect (G_OBJECT (item), "activate",
                            G_CALLBACK (add_row_cb), NULL);

          item = gtk_image_menu_item_new_with_label (_("Delete row"));
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
					 gtk_image_new_from_stock
					 (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
	  gtk_widget_show (item);
	  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	  g_signal_connect (G_OBJECT (item), "activate",
			    G_CALLBACK (del_row_cb), NULL);
	  gtk_widget_show (menu);
        }
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL,
                      NULL, ev->button, ev->time);
    }
  return FALSE;
}

GtkWidget *
list_create_widget (GtkWidget *dlg)
{
  GtkWidget *w;
  GtkTreeModel *model;
  gint i, n_columns;

  fore_col = back_col = font_col = -1;

  n_columns = g_slist_length (options.list_data.columns);

  if (n_columns == 0)
    {
      g_printerr (_("No column titles specified for List dialog.\n"));
      return NULL;
    }

  w = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (w), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (w),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  model = create_model (n_columns);

  list_view = gtk_tree_view_new_with_model (model);
  gtk_widget_set_name (list_view, "yad-list-widget");
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list_view), !options.list_data.no_headers);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (list_view), settings.rules_hint);
  gtk_tree_view_set_reorderable (GTK_TREE_VIEW (list_view), options.common_data.editable);
  g_object_unref (model);

  gtk_container_add (GTK_CONTAINER (w), list_view);

  add_columns (n_columns);

  if (options.common_data.multi && !options.list_data.checkbox)
    {
      GtkTreeSelection *sel =
        gtk_tree_view_get_selection (GTK_TREE_VIEW (list_view));
      gtk_tree_selection_set_mode (sel, GTK_SELECTION_MULTIPLE);
    }

  if (options.list_data.checkbox || !options.common_data.multi)
    {
      g_signal_connect (G_OBJECT (list_view), "row-activated",
                        G_CALLBACK (double_click_cb), dlg);
    }

  if (options.common_data.editable)
    {
      /* add popup menu */
      g_signal_connect_swapped (G_OBJECT (list_view), "button_press_event",
                                G_CALLBACK (popup_menu_cb), NULL);
    }

  /* Return submits data */
  g_signal_connect (G_OBJECT (list_view), "key-press-event",
                    G_CALLBACK (list_activate_cb), dlg);

  fill_data (n_columns);

  /* add tooltip column, if present */
  for (i = 0; i < n_columns; i++)
    {
      YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, i);

      if (col->type == YAD_COLUMN_TOOLTIP)
        {
          gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (list_view), i);
          break;
        }
    }
  /* set tooltip callback if no tooltip column */
  if (i == n_columns)
    {
      gtk_widget_set_has_tooltip (list_view, TRUE);
      g_signal_connect (G_OBJECT (list_view), "query-tooltip",
			G_CALLBACK (tooltip_cb), NULL);
    }

  /* set search function for regex search */
  if (options.list_data.search_column != -1 && options.list_data.regex_search)
    {
      YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns,
						       options.list_data.search_column);

      if (col->type == YAD_COLUMN_TEXT || col->type == YAD_COLUMN_TOOLTIP)
	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (list_view),
					     regex_search, NULL, NULL);
    }

  return w;
}

static void
print_col (GtkTreeModel *model, GtkTreeIter *iter, gint num)
{
  YadColumn *col = (YadColumn *) g_slist_nth_data (options.list_data.columns, num);

  /* don't print attributes */
  if (col->type == YAD_COLUMN_ATTR_FORE ||
      col->type == YAD_COLUMN_ATTR_BACK ||
      col->type == YAD_COLUMN_ATTR_FONT)
    return;

  switch (col->type)
    {
    case YAD_COLUMN_CHECK:
      {
        gboolean bval;
        gtk_tree_model_get (model, iter, num, &bval, -1);
        g_printf ("%s", bval ? "TRUE" : "FALSE");
        break;
      }
    case YAD_COLUMN_NUM:
      {
        gint64 nval;
        gtk_tree_model_get (model, iter, num, &nval, -1);
        g_printf ("%ld", nval);
        break;
      }
    case YAD_COLUMN_FLOAT:
      {
        gdouble nval;
        gtk_tree_model_get (model, iter, num, &nval, -1);
        g_printf ("%lf", nval);
        break;
      }
    case YAD_COLUMN_IMAGE:
      break;
    default:
      {
        gchar *cval, *uval;
        gtk_tree_model_get (model, iter, num, &cval, -1);
	uval = unescape_markup (cval);
        g_printf ("%s", uval);
	g_free (uval);
	break;
      }
    }
  g_printf ("%s", options.common_data.separator);
}

static void
print_selected (GtkTreeModel *model, GtkTreePath *path,
                GtkTreeIter *iter, gpointer data)
{
  gint i, col = options.list_data.print_column;

  if (col)
    print_col (model, iter, col - 1);
  else
    {
      for (i = 0; i < gtk_tree_model_get_n_columns (model) - 1; i++)
        print_col (model, iter, i);
    }
  g_printf ("\n");
}

static void
print_all (GtkTreeModel *model)
{
  GtkTreeIter iter;
  gint i, cols = gtk_tree_model_get_n_columns (model) - 1;

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          for (i = 0; i < cols; i++)
            print_col (model, &iter, i);
          g_printf ("\n");
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

void
list_print_result (void)
{
  GtkTreeModel *model;
  gint col = options.list_data.print_column;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (list_view));

  if (options.list_data.print_all)
    {
      print_all (model);
      return;
    }

  if (options.list_data.checkbox)
    {
      // don't check in cycle
      if (col)
        {
          GtkTreeIter iter;

          if (gtk_tree_model_get_iter_first (model, &iter))
            {
              do
                {
                  gboolean chk;
                  gtk_tree_model_get (model, &iter, 0, &chk, -1);
                  if (chk)
                    {
                      print_col (model, &iter, col - 1);
                      g_printf ("\n");
                    }
                }
              while (gtk_tree_model_iter_next (model, &iter));
            }
        }
      else
        {
          GtkTreeIter iter;

          if (gtk_tree_model_get_iter_first (model, &iter))
            {
              do
                {
                  gboolean chk;
                  gtk_tree_model_get (model, &iter, 0, &chk, -1);
                  if (chk)
                    {
                      gint i;
                      for (i = 0; i < gtk_tree_model_get_n_columns (model) - 1; i++)
                        print_col (model, &iter, i);
                      g_printf ("\n");
                    }
                }
              while (gtk_tree_model_iter_next (model, &iter));
            }
        }
    }
  else
    {
      GtkTreeSelection *sel =
        gtk_tree_view_get_selection (GTK_TREE_VIEW (list_view));

      gtk_tree_selection_selected_foreach (sel, print_selected, NULL);
    }
}
