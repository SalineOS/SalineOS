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

#include "yad.h"

static GtkWidget *filechooser;

static void
file_activated_cb (GtkFileChooser *chooser, gpointer *data)
{
  gtk_dialog_response (GTK_DIALOG (data), YAD_RESPONSE_OK);
}

void
confirm_overwrite_cb (GtkDialog *dlg, gint id, gpointer data)
{
  if (id != YAD_RESPONSE_OK)
    return;

  if (options.file_data.save &&
      options.file_data.confirm_overwrite &&
      !options.common_data.multi)
    {
      gchar *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));

      if (g_file_test (filename, G_FILE_TEST_EXISTS))
	{
	  GtkWidget *d;
	  gint r;
	  gchar *buf;

	  buf = g_strcompress (options.file_data.confirm_text);
	  d = gtk_message_dialog_new (GTK_WINDOW (dlg), GTK_DIALOG_DESTROY_WITH_PARENT,
				      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
				      "%s", buf);
	  g_free (buf);
	  r = gtk_dialog_run (GTK_DIALOG (d));
	  gtk_widget_destroy (d);
	  if (r != GTK_RESPONSE_YES)
	    g_signal_stop_emission_by_name (dlg, "response");
	}
    }
}

GtkWidget *
file_create_widget (GtkWidget *dlg)
{
  GtkWidget *w;
  gchar *dir, *basename;
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;

  if (options.file_data.directory)
    {
      if (options.file_data.save)
        action = GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER;
      else
        action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
    }
  else
    {
      if (options.file_data.save)
        action = GTK_FILE_CHOOSER_ACTION_SAVE;
    }

  w = filechooser = gtk_file_chooser_widget_new (action);
  gtk_widget_set_name (w, "yad-file-widget");

  if (options.common_data.uri)
    {
      dir = g_path_get_dirname (options.common_data.uri);

      if (g_path_is_absolute (options.common_data.uri) == TRUE)
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (w), dir);

      if (options.common_data.uri[strlen (options.common_data.uri) - 1] != '/')
        {
          basename = g_path_get_basename (options.common_data.uri);
          if (options.file_data.save)
            gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (w),
                                               basename);
          else
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (w),
					   options.common_data.uri);
          g_free (basename);
        }
      g_free (dir);
    }

  if (options.common_data.multi)
    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (w), TRUE);

  if (options.file_data.filter)
    {
      /* Filter format: Executables | *.exe *.bat *.com */
      gint filter_i;

      for (filter_i = 0; options.file_data.filter[filter_i]; filter_i++)
        {
          GtkFileFilter *filter = gtk_file_filter_new ();
          gchar *filter_str = options.file_data.filter[filter_i];
          gchar **pattern, **patterns;
          gchar *name = NULL;
          gint i;

          /* Set name */
          for (i = 0; filter_str[i] != '\0'; i++)
	    {
	      if (filter_str[i] == '|')
		break;
	    }

          if (filter_str[i] == '|')
	    name = g_strstrip (g_strndup (filter_str, i));

          if (name)
            {
              gtk_file_filter_set_name (filter, name);

              /* Point i to the right position for split */
              for (++i; filter_str[i] == ' '; i++);
            }
          else
            {
              gtk_file_filter_set_name (filter, filter_str);
              i = 0;
            }

          /* Get patterns */
          patterns = g_strsplit_set (filter_str + i, " ", -1);

          for (pattern = patterns; *pattern; pattern++)
            gtk_file_filter_add_pattern (filter, *pattern);

	  g_free (name);
          g_strfreev (patterns);

          gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (w), filter);
        }
    }

  g_signal_connect (w, "file-activated",
  		    G_CALLBACK (file_activated_cb), dlg);

  return w;
}

void
file_print_result (void)
{
  GSList *selections, *iter;

  selections = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (filechooser));
  for (iter = selections; iter != NULL; iter = iter->next)
    {
      g_print ("%s",
	       g_filename_to_utf8 ((gchar *) iter->data, -1, NULL, NULL,
				   NULL));
      g_free (iter->data);
      if (iter->next != NULL)
	g_print ("%s", options.common_data.separator);
    }
  g_print ("\n");
  g_slist_free (selections);
}
