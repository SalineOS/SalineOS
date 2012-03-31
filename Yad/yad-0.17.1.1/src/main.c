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

#include <locale.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>

#ifndef G_OS_WIN32
# include <gdk/gdkx.h>
#endif

#include "yad.h"

YadOptions options;
GtkWidget *dialog = NULL;

#ifndef G_OS_WIN32
static void
sa_usr1 (gint sig)
{
  gtk_dialog_response (GTK_DIALOG (dialog), YAD_RESPONSE_OK);
}

static void
sa_usr2 (gint sig)
{
  gtk_dialog_response (GTK_DIALOG (dialog), YAD_RESPONSE_CANCEL);
}
#endif

static gboolean
timeout_cb (gpointer data)
{
  GtkWidget *w = (GtkWidget *) data;

  gtk_dialog_response (GTK_DIALOG (w), YAD_RESPONSE_TIMEOUT);

  return FALSE;
}

static void
btn_cb (GtkButton *b, gchar *c)
{
  gchar *cmd;
  gint pid;

#if !defined(_WIN32)
  pid = getpid ();
#endif
    
  cmd = g_strdup_printf (c, pid);
  g_spawn_command_line_async (cmd, NULL);
  g_free (cmd);
}

static gboolean
timeout_indicator_cb (gpointer data)
{
  static guint count = 1;
  gdouble percent;
  GtkWidget *w = (GtkWidget *) data;

  if (!w)
    return FALSE;

  percent =
    ((gdouble) options.data.timeout - count) / (gdouble) options.data.timeout;
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (w), percent);
  if (settings.show_remain)
    {
      gchar *lbl =
        g_strdup_printf (_("%d sec"), options.data.timeout - count);
      gtk_progress_bar_set_text (GTK_PROGRESS_BAR (w), lbl);
      g_free (lbl);
    }
  count++;

  return TRUE;
}

static void
text_size_allocate_cb (GtkWidget *w, GtkAllocation *al, gpointer data)
{
  gtk_widget_set_size_request (w, al->width, -1);
}

GtkWidget *
create_dialog ()
{
  GtkWidget *dlg;
  GtkWidget *hbox, *vbox, *hbox2, *bbox;
  GtkWidget *image;
  GtkWidget *text;
  GtkWidget *main_widget = NULL;
  GtkWidget *topb = NULL;

  /* create dialog window */
  dlg = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dlg), options.data.dialog_title);
#if !GTK_CHECK_VERSION(2,22,0)
  gtk_dialog_set_has_separator (GTK_DIALOG (dlg), options.data.dialog_sep);
#endif
  gtk_widget_set_name (dlg, "yad-dialog-window");

  /* get buttons container */
  bbox = gtk_dialog_get_action_area (GTK_DIALOG (dlg));

  /* set window icon */
  if (options.data.window_icon)
    {
      if (g_file_test (options.data.window_icon, G_FILE_TEST_EXISTS))
	gtk_window_set_icon_from_file (GTK_WINDOW (dlg), options.data.window_icon, NULL);
      else
	gtk_window_set_icon_name (GTK_WINDOW (dlg), options.data.window_icon);
    }

  /* set window borders */
  if (options.data.borders == -1)
    options.data.borders = (gint) gtk_container_get_border_width (GTK_CONTAINER (dlg));
  gtk_container_set_border_width (GTK_CONTAINER (dlg), (guint) options.data.borders);

  /* set window size and position */
  if (!options.data.geometry)
    {
      gtk_window_set_default_size (GTK_WINDOW (dlg),
                                   options.data.width, options.data.height);
      if (options.data.center)
        gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
      else if (options.data.mouse)
        gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_MOUSE);
      gtk_widget_realize (dlg);
    }
  else
    {
      /* parse geometry, if given. must be after showing widget */
      gtk_widget_realize (dlg);
      gtk_window_parse_geometry (GTK_WINDOW (dlg), options.data.geometry);
    }

  /* set window behavior */
  if (options.data.sticky)
    gtk_window_stick (GTK_WINDOW (dlg));
  gtk_window_set_resizable (GTK_WINDOW (dlg), !options.data.fixed);
  gtk_window_set_keep_above (GTK_WINDOW (dlg), options.data.ontop);
  gtk_window_set_decorated (GTK_WINDOW (dlg), !options.data.undecorated);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dlg), options.data.skip_taskbar);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (dlg), options.data.skip_taskbar);

  /* create timeout indicator widget */
  if (options.data.timeout)
    {
      if (G_LIKELY (options.data.to_indicator) &&
          g_ascii_strcasecmp (options.data.to_indicator, "none"))
        {
          topb = gtk_progress_bar_new ();
          gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (topb), 1.0);
          gtk_widget_set_name (topb, "yad-timeout-indicator");
        }
    }

  /* add top label widgets */
#if !GTK_CHECK_VERSION(3,0,0)
  hbox = hbox2 = gtk_hbox_new (FALSE, 0);
#else
  hbox = hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
#endif
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))),
                      hbox, TRUE, TRUE, 5);
#if !GTK_CHECK_VERSION(3,0,0)
  vbox = gtk_vbox_new (FALSE, 0);
#else
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
#endif

  /* add timeout indicator */
  if (topb)
    {
      if (g_ascii_strcasecmp (options.data.to_indicator, "top") == 0)
	{
#if !GTK_CHECK_VERSION(3,0,0)
	  gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (topb), GTK_PROGRESS_LEFT_TO_RIGHT);
#endif
	  gtk_box_pack_start (GTK_BOX (vbox), topb, FALSE, FALSE, 2);
	}
      else if (g_ascii_strcasecmp (options.data.to_indicator, "bottom") == 0)
	{
#if !GTK_CHECK_VERSION(3,0,0)
	  gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (topb), GTK_PROGRESS_LEFT_TO_RIGHT);
#endif
	  gtk_box_pack_end (GTK_BOX (vbox), topb, FALSE, FALSE, 2);
	}
      else if (g_ascii_strcasecmp (options.data.to_indicator, "left") == 0)
	{
#if !GTK_CHECK_VERSION(3,0,0)
	  gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (topb), GTK_PROGRESS_BOTTOM_TO_TOP);
#endif
	  gtk_box_pack_start (GTK_BOX (hbox), topb, FALSE, FALSE, 2);
	}
      else if (g_ascii_strcasecmp (options.data.to_indicator, "right") == 0)
	{
#if !GTK_CHECK_VERSION(3,0,0)
	  gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (topb), GTK_PROGRESS_BOTTOM_TO_TOP);
#endif
	  gtk_box_pack_end (GTK_BOX (hbox), topb, FALSE, FALSE, 2);
	}
      if (settings.show_remain)
        {
          gchar *lbl = g_strdup_printf (_("%d sec"), options.data.timeout);
#if GTK_CHECK_VERSION(3,0,0)
	  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (topb), TRUE);
#endif
          gtk_progress_bar_set_text (GTK_PROGRESS_BAR (topb), lbl);
          g_free (lbl);
        }
    }

  /* must be after indicator! */
  gtk_box_pack_end (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  if (options.data.image_on_top)
    {
#if !GTK_CHECK_VERSION(3,0,0)
      hbox2 = gtk_hbox_new (FALSE, 0);
#else
      hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
#endif
      gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0);
    }

  if (options.data.dialog_image)
    {
      GdkPixbuf *pb = NULL;

      pb = get_pixbuf (options.data.dialog_image, YAD_BIG_ICON);
      image = gtk_image_new_from_pixbuf (pb);
      if (pb)
        g_object_unref (pb);

      gtk_widget_set_name (image, "yad-dialog-image");
      gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
      gtk_box_pack_start (GTK_BOX (hbox2), image, FALSE, FALSE, 2);
    }
  if (options.data.dialog_text)
    {
      /* for dnd's tooltip we don't need text label */
      if (options.mode != YAD_MODE_DND || !options.dnd_data.tooltip)
	{
	  gchar *buf = g_strcompress (options.data.dialog_text);

	  text = gtk_label_new (NULL);
	  if (!options.data.no_markup)
	    gtk_label_set_markup (GTK_LABEL (text), buf);
	  else
	    gtk_label_set_text (GTK_LABEL (text), buf);
	  gtk_widget_set_name (text, "yad-dialog-label");
	  gtk_label_set_selectable (GTK_LABEL (text), options.data.selectable_labels);
	  gtk_misc_set_alignment (GTK_MISC (text), options.data.text_align, 0.5);
	  if (options.data.geometry || options.data.width != -1)
	    gtk_label_set_line_wrap (GTK_LABEL (text), TRUE);
	  if (options.data.image_on_top)
	    gtk_box_pack_start (GTK_BOX (hbox2), text, TRUE, TRUE, 2);
	  else
	    gtk_box_pack_start (GTK_BOX (vbox), text, FALSE, FALSE, 2);
	  g_signal_connect (G_OBJECT (text), "size-allocate",
			    G_CALLBACK (text_size_allocate_cb), NULL);

	  g_free (buf);
	}
    }

  /* add main widget */
  switch (options.mode)
    {
    case YAD_MODE_CALENDAR:
      main_widget = calendar_create_widget (dlg);
      break;
    case YAD_MODE_COLOR:
      main_widget = color_create_widget (dlg);
      break;
    case YAD_MODE_DND:
      dnd_init (dlg);
      break;
    case YAD_MODE_ENTRY:
      main_widget = entry_create_widget (dlg);
      break;
    case YAD_MODE_FILE:
      main_widget = file_create_widget (dlg);
      break;
    case YAD_MODE_FONT:
      main_widget = font_create_widget (dlg);
      break;
    case YAD_MODE_FORM:
      main_widget = form_create_widget (dlg);
      break;
    case YAD_MODE_ICONS:
      main_widget = icons_create_widget (dlg);
      break;
    case YAD_MODE_LIST:
      main_widget = list_create_widget (dlg);
      break;
    case YAD_MODE_MULTI_PROGRESS:
      main_widget = multi_progress_create_widget (dlg);
      break;
    case YAD_MODE_PROGRESS:
      main_widget = progress_create_widget (dlg);
      break;
    case YAD_MODE_SCALE:
      main_widget = scale_create_widget (dlg);
      break;
    case YAD_MODE_TEXTINFO:
      main_widget = text_create_widget (dlg);
      break;
    }
  if (main_widget)
    {
      if (options.data.expander)
	{
	  GtkWidget *exp;

	  exp = gtk_expander_new_with_mnemonic (options.data.expander);
	  gtk_expander_set_expanded (GTK_EXPANDER (exp), FALSE);
	  gtk_container_add (GTK_CONTAINER (exp), main_widget);
	  gtk_box_pack_start (GTK_BOX (vbox), exp, TRUE, TRUE, 2);
	}
      else
	gtk_box_pack_start (GTK_BOX (vbox), main_widget, TRUE, TRUE, 2);
    }

  /* add buttons */
  if (!options.data.no_buttons)
    {
      if (options.data.buttons)
        {
          GSList *tmp = options.data.buttons;
          do
            {
              YadButton *b = (YadButton *) tmp->data;
	      if (b->cmd)
		{
		  GtkWidget *btn = gtk_button_new_from_stock (b->name);
		  g_signal_connect (G_OBJECT (btn), "clicked", G_CALLBACK (btn_cb), b->cmd);
		  gtk_box_pack_start (GTK_BOX (bbox), btn, FALSE, FALSE, 0);
		}
	      else
		gtk_dialog_add_button (GTK_DIALOG (dlg), b->name, b->response);
              tmp = tmp->next;
            }
          while (tmp != NULL);
        }
      else
        {
	  if (options.mode == YAD_MODE_PROGRESS || options.mode == YAD_MODE_MULTI_PROGRESS)
	    {
	      gtk_dialog_add_buttons (GTK_DIALOG (dlg),
				      GTK_STOCK_CLOSE, YAD_RESPONSE_OK,
				      NULL);
	    }
	  else
	    {
	      if (gtk_alternative_dialog_button_order (NULL))
		gtk_dialog_add_buttons (GTK_DIALOG (dlg),
					GTK_STOCK_OK, YAD_RESPONSE_OK,
					GTK_STOCK_CANCEL, YAD_RESPONSE_CANCEL,
					NULL);
	      else
		gtk_dialog_add_buttons (GTK_DIALOG (dlg),
					GTK_STOCK_CANCEL, YAD_RESPONSE_CANCEL,
					GTK_STOCK_OK, YAD_RESPONSE_OK, NULL);
	    }
          gtk_dialog_set_default_response (GTK_DIALOG (dlg), YAD_RESPONSE_OK);
        }
    }

  /* show widgets */
  gtk_widget_show_all (dlg);
  if (options.data.no_buttons)
    gtk_widget_hide (bbox);

  /* set timeout */
  if (options.data.timeout)
    {
      g_timeout_add_seconds (options.data.timeout, timeout_cb, dlg);
      g_timeout_add_seconds (1, timeout_indicator_cb, topb);
    }

  /* print xid */
#ifndef G_OS_WIN32
  if (options.print_xid)
    {
      fprintf (stderr, "0x%X", GDK_WINDOW_XID (gtk_widget_get_window (dlg)));
      fflush (stderr);
    }
#endif

  return dlg;
}

void
print_result (void)
{
  switch (options.mode)
    {
    case YAD_MODE_CALENDAR:
      calendar_print_result ();
      break;
    case YAD_MODE_COLOR:
      color_print_result ();
      break;
    case YAD_MODE_ENTRY:
      entry_print_result ();
      break;
    case YAD_MODE_FILE:
      file_print_result ();
      break;
    case YAD_MODE_FONT:
      font_print_result ();
      break;
    case YAD_MODE_FORM:
      form_print_result ();
      break;
    case YAD_MODE_LIST:
      list_print_result ();
      break;
    case YAD_MODE_SCALE:
      scale_print_result ();
      break;
    case YAD_MODE_TEXTINFO:
      text_print_result ();
      break;
    }
}

gint
main (gint argc, gchar ** argv)
{
  GOptionContext *ctx;
  GError *err = NULL;
  gint w, h;
  gint ret = 0;
  gchar *tmp_sep;
  struct sigaction sa;

  setlocale (LC_ALL, "");

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  g_type_init ();
  read_settings ();

  gtk_init (&argc, &argv);
  g_set_application_name ("YAD");
  yad_options_init ();

  /* set default icons and icon theme */
  if (options.data.icon_theme)
    {
      settings.icon_theme = gtk_icon_theme_new ();
      gtk_icon_theme_set_custom_theme (settings.icon_theme,
				       options.data.icon_theme);
    }
  else
    settings.icon_theme = gtk_icon_theme_get_default ();
  gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &w, &h);
  settings.big_fallback_image =
    gtk_icon_theme_load_icon (settings.icon_theme, "yad", MIN (w, h),
                              GTK_ICON_LOOKUP_GENERIC_FALLBACK, NULL);
  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
  settings.small_fallback_image =
    gtk_icon_theme_load_icon (settings.icon_theme, "yad", MIN (w, h),
                              GTK_ICON_LOOKUP_GENERIC_FALLBACK, NULL);

  ctx = yad_create_context ();
  g_option_context_parse (ctx, &argc, &argv, &err);
  if (err)
    {
      g_printerr (_("Unable parse command line: %s\n"), err->message);
      return -1;
    }
  yad_set_mode ();

  /* correct separators */
  tmp_sep = g_strcompress (options.common_data.separator);
  options.common_data.separator = tmp_sep;
  tmp_sep = g_strcompress (options.common_data.item_separator);
  options.common_data.item_separator = tmp_sep;
  
  /* loads an extra arguments, if specified */
  if (options.rest_file)
    {
      GIOChannel *ioc;
      gchar *buf;
      guint len, line = 0;
     
      g_strfreev (options.extra_data);
      options.extra_data = NULL;

      ioc = g_io_channel_new_file (options.rest_file, "r", NULL);
      while (TRUE)
	{
	  gint status = g_io_channel_read_line (ioc, &buf, NULL, NULL, NULL);
	  
	  if (status != G_IO_STATUS_NORMAL)
	    break;

	  /* remove \n at the end of string */
	  len = strlen (buf);
	  if (buf[len - 1] == '\n')
	    buf[len - 1] = '\0';

	  /* add line to arguments array */
	  options.extra_data = g_realloc (options.extra_data, (line + 2) * sizeof (gchar *));  
	  options.extra_data[line] = g_strcompress (buf);
	  options.extra_data[line + 1] = NULL;

	  g_free (buf);
	  line++;
	}      
      g_io_channel_shutdown (ioc, FALSE, NULL);
    }

#ifndef G_OS_WIN32
  /* set signal handlers */
  bzero (&sa, sizeof (struct sigaction));
  sa.sa_handler = sa_usr1;
  sigaction (SIGUSR1, &sa, NULL);
  sa.sa_handler = sa_usr2;
  sigaction (SIGUSR2, &sa, NULL);
#endif

  switch (options.mode)
    {
    case YAD_MODE_ABOUT:
      ret = yad_about ();
      break;
    case YAD_MODE_VERSION:
      g_print ("%s\n", VERSION);
      break;
    case YAD_MODE_NOTIFICATION:
      ret = yad_notification_run ();
      break;
    case YAD_MODE_PRINT:
      ret = yad_print_run ();
      break;
    default:
      dialog = create_dialog ();
      if (options.mode == YAD_MODE_FILE)
        {
          /* show custom confirmation dialog */
          g_signal_connect (G_OBJECT (dialog), "response",
                            G_CALLBACK (confirm_overwrite_cb), NULL);
        }
      ret = gtk_dialog_run (GTK_DIALOG (dialog));
      if (options.data.always_print)
        print_result ();
      else if (ret != YAD_RESPONSE_TIMEOUT && ret != YAD_RESPONSE_ESC)
	{
	  /* standard OK button pressed */
	  if (ret == YAD_RESPONSE_OK && options.data.buttons == NULL)
	    print_result ();
	  /* custom even button pressed */
	  else if (options.data.buttons && !(ret & 1))
	    print_result ();
	}
#ifndef G_OS_WIN32
      /* autokill option for progress dialog */
      if (!options.kill_parent)
	{
	  if (options.mode == YAD_MODE_PROGRESS &&
	      options.progress_data.autokill &&
	      ret != YAD_RESPONSE_OK)
	    kill (getppid (), 1);
	}
#endif
    }

#ifndef G_OS_WIN32
  if (options.kill_parent)
    kill (getppid (), SIGTERM);
#endif

  return ret;
}
