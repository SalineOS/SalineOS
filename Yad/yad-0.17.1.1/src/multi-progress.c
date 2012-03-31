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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "yad.h"

static GSList *progress_bars = NULL;
static guint nbars = 0;

static gboolean
handle_stdin (GIOChannel * channel, GIOCondition condition, gpointer data)
{
  float percentage = 0.0;

  if ((condition == G_IO_IN) || (condition == G_IO_IN + G_IO_HUP))
    {
      GString *string;
      gchar **value;
      GError *err = NULL;

      string = g_string_new (NULL);

      while (channel->is_readable != TRUE) ;

      do
        {
          gint status, num;
	  GtkProgressBar *pb;
	  YadProgressBar *b;

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
                  g_printerr ("yad_multi_progress_handle_stdin(): %s\n", err->message);
                  g_error_free (err);
                  err = NULL;
                }
              continue;
            }

	  value = g_strsplit(string->str, ":", 2);
	  num = atoi (value[0]) - 1;
	  if (num < 0 || num > nbars)
	    continue;

	  pb = GTK_PROGRESS_BAR (g_slist_nth_data (progress_bars, num));
	  b = (YadProgressBar *) g_slist_nth_data (options.multi_progress_data.bars, num);
	  
	  if (b->type == YAD_PROGRESS_PULSE)
	    gtk_progress_bar_pulse (pb);
	  else
	    {
	      if (value[1] && value[1][0] == '#')
		{
		  gchar *match;

		  /* We have a comment, so let's try to change the label */
		  match = g_strcompress (g_strstrip (string->str + 1));
		  gtk_progress_bar_set_text (pb, match);
		  g_free (match);
		}
	      else
		{
		  if (!value[1] || !g_ascii_isdigit (*value[1]))
		    continue;

		  /* Now try to convert the thing to a number */
		  percentage = atoi (value[1]);
		  if (percentage >= 100)
		    gtk_progress_bar_set_fraction (pb, 1.0);
		  else
		    gtk_progress_bar_set_fraction (pb, percentage / 100.0);
		}
	    }
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

GtkWidget *
multi_progress_create_widget (GtkWidget * dlg)
{
  GtkWidget *table;
  GIOChannel *channel;
  GSList *b;
  gint i = 0;

  nbars = g_slist_length (options.multi_progress_data.bars);
  if (nbars < 1)
    return NULL;
  if (options.common_data.vertical)
    table = gtk_table_new (2, nbars, FALSE);
  else
    table = gtk_table_new (nbars, 2, FALSE);

  for (b = options.multi_progress_data.bars; b; b = b->next)
    {
      GtkWidget *l, *w;
      YadProgressBar *p = (YadProgressBar *) b->data;

      /* add label */
      l = gtk_label_new (p->name);
      if (options.common_data.vertical)
	gtk_table_attach (GTK_TABLE (table), l, i, i + 1, 1, 2, GTK_FILL, 0, 2, 2);
      else
	gtk_table_attach (GTK_TABLE (table), l, 0, 1, i, i + 1, GTK_FILL, 0, 2, 2);

      /* add progress bar */
      w = gtk_progress_bar_new ();
      gtk_widget_set_name (w, "yad-progress-widget");
#if GTK_CHECK_VERSION(3,0,0)
      gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (w), TRUE);
#endif

      if (p->type != YAD_PROGRESS_PULSE)
	{
	  if (options.extra_data && options.extra_data[i])
	    {
	      if (g_ascii_isdigit (*options.extra_data[i]))
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (w),
					       atoi (options.extra_data[i]) / 100.0);
	    }
	}
      else
	{
	  if (options.extra_data && options.extra_data[i])
	    {
	      if (g_ascii_isdigit (*options.extra_data[i]))
		gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (w), 
						 atoi (options.extra_data[i]) / 100.0);
	    }
	}

#if GTK_CHECK_VERSION(3,0,0)
      gtk_progress_bar_set_inverted (GTK_PROGRESS_BAR (w),
				     p->type == YAD_PROGRESS_RTL);
      if (options.common_data.vertical)
	{
	  gtk_orientable_set_orientation (GTK_ORIENTABLE (w),
					  GTK_ORIENTATION_VERTICAL);
	}
#else
      if (p->type == YAD_PROGRESS_RTL)
	{
	  if (options.common_data.vertical)
	    gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (w),
					      GTK_PROGRESS_TOP_TO_BOTTOM);
	  else
	    gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (w),
					      GTK_PROGRESS_RIGHT_TO_LEFT);
	}
      else
	{
	  if (options.common_data.vertical)
	    gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (w),
					      GTK_PROGRESS_BOTTOM_TO_TOP);
	}
#endif
      if (options.common_data.vertical)
	gtk_table_attach (GTK_TABLE (table), w, i, i + 1, 0, 1, 0, GTK_FILL | GTK_EXPAND, 2, 2);
      else
	gtk_table_attach (GTK_TABLE (table), w, 1, 2, i, i + 1, GTK_FILL | GTK_EXPAND, 0, 2, 2);
	
      progress_bars = g_slist_append (progress_bars, w);

      i++;
    }

  channel = g_io_channel_unix_new (0);
  g_io_channel_set_encoding (channel, NULL, NULL);
  g_io_channel_set_flags (channel, G_IO_FLAG_NONBLOCK, NULL);
  g_io_add_watch (channel, G_IO_IN | G_IO_HUP, handle_stdin, dlg);

  return table;
}
