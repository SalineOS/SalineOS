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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "yad.h"

#define SETTINGS_FILE "yad.conf"

YadSettings settings;

void
read_settings (void)
{
  GKeyFile *kf;
  gchar *filename;

  /* set defaults */
  settings.width = settings.height = -1;
  settings.timeout = 0;
  settings.to_indicator = "none";
  settings.show_remain = FALSE;
  settings.rules_hint = TRUE;
  settings.always_selected = FALSE;
#if !GTK_CHECK_VERSION(2,22,0)
  settings.dlg_sep = FALSE;
#endif
  settings.combo_always_editable = FALSE;
  settings.show_gtk_palette = FALSE;
  settings.expand_palette = FALSE;
  settings.term = "xterm -e %s";
  settings.ignore_unknown = TRUE;

  filename = g_build_filename (g_get_user_config_dir (), SETTINGS_FILE, NULL);

  if (g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      kf = g_key_file_new ();

      if (g_key_file_load_from_file (kf, filename, G_KEY_FILE_NONE, NULL))
	{
#if !GTK_CHECK_VERSION(2,22,0)
	  if (g_key_file_has_key (kf, "General", "dialog_separator", NULL))
	    settings.dlg_sep = g_key_file_get_boolean (kf, "General", "dialog_separator", NULL);
#endif
	  if (g_key_file_has_key (kf, "General", "width", NULL))
	    settings.width = g_key_file_get_integer (kf, "General", "width", NULL);
	  if (g_key_file_has_key (kf, "General", "height", NULL))
	    settings.height = g_key_file_get_integer (kf, "General", "height", NULL);
	  if (g_key_file_has_key (kf, "General", "timeout", NULL))
	    settings.timeout = g_key_file_get_integer (kf, "General", "timeout", NULL);
	  if (g_key_file_has_key (kf, "General", "timeout_indicator", NULL))
	    settings.to_indicator = g_key_file_get_string (kf, "General", "timeout_indicator", NULL);
	  if (g_key_file_has_key (kf, "General", "show_remain", NULL))
	    settings.show_remain = g_key_file_get_boolean (kf, "General", "show_remain", NULL);
	  if (g_key_file_has_key (kf, "General", "rules_hint", NULL))
	    settings.rules_hint = g_key_file_get_boolean (kf, "General", "rules_hint", NULL);
	  if (g_key_file_has_key (kf, "General", "always_selected", NULL))
	    settings.always_selected = g_key_file_get_boolean (kf, "General", "always_selected", NULL);
	  if (g_key_file_has_key (kf, "General", "combo_always_editable", NULL))
	    settings.combo_always_editable = g_key_file_get_boolean (kf, "General", "combo_always_editable", NULL);
	  if (g_key_file_has_key (kf, "General", "show_gtk_palette", NULL))
	    settings.show_gtk_palette = g_key_file_get_boolean (kf, "General", "show_gtk_palette", NULL);
	  if (g_key_file_has_key (kf, "General", "expand_palette", NULL))
	    settings.expand_palette = g_key_file_get_boolean (kf, "General", "expand_palette", NULL);
	  if (g_key_file_has_key (kf, "General", "terminal", NULL))
	    settings.term = g_key_file_get_string (kf, "General", "terminal", NULL);
	  if (g_key_file_has_key (kf, "General", "ignore_unknown_options", NULL))
	    settings.ignore_unknown = g_key_file_get_boolean (kf, "General", "ignore_unknown_options", NULL);
	}

      g_key_file_free (kf);
    }
  else
    write_settings ();

  g_free (filename);
}

void
write_settings (void)
{
  GKeyFile *kf;
  gchar *context;

  kf = g_key_file_new ();

#if !GTK_CHECK_VERSION(2,22,0)
  g_key_file_set_boolean (kf, "General", "dialog_separator", settings.dlg_sep);
  g_key_file_set_comment (kf, "General", "dialog_separator", "Enable separator between dialog and buttons", NULL);
#endif
  g_key_file_set_integer (kf, "General", "width", settings.width);
  g_key_file_set_comment (kf, "General", "width", "Default dialog width", NULL);
  g_key_file_set_integer (kf, "General", "height", settings.height);
  g_key_file_set_comment (kf, "General", "height", "Default dialog height", NULL);
  g_key_file_set_integer (kf, "General", "timeout", settings.timeout);
  g_key_file_set_comment (kf, "General", "timeout", "Default timeout (0 for no timeout)", NULL);
  g_key_file_set_integer (kf, "General", "timeout_indicator", settings.timeout);
  g_key_file_set_comment (kf, "General", "timeout_indicator", 
			  "Position of timeout indicator (top, bottom, left, right, none)", NULL);
  g_key_file_set_boolean (kf, "General", "show_remain", settings.show_remain);
  g_key_file_set_comment (kf, "General", "show_remain", "Show remain seconds in timeout indicator", NULL);
  g_key_file_set_boolean (kf, "General", "rules_hint", settings.rules_hint);
  g_key_file_set_comment (kf, "General", "rules_hint", "Enable rules hints in list widget", NULL);
  g_key_file_set_boolean (kf, "General", "always_selected", settings.always_selected);
  g_key_file_set_comment (kf, "General", "always_selected", "List widget always have a selection", NULL);
  g_key_file_set_boolean (kf, "General", "combo_always_editable", settings.combo_always_editable);
  g_key_file_set_comment (kf, "General", "combo_always_editable", "Combo-box in entry dialog is always editable", NULL);
  g_key_file_set_boolean (kf, "General", "show_gtk_palette", settings.show_gtk_palette);
  g_key_file_set_comment (kf, "General", "show_gtk_palette", "Show GtkColorSelection palette", NULL);
  g_key_file_set_boolean (kf, "General", "expand_palette", settings.expand_palette);
  g_key_file_set_comment (kf, "General", "expand_palette", "Expand list of predefined colors in color dialog", NULL);
  g_key_file_set_string (kf, "General", "terminal", settings.term);
  g_key_file_set_comment (kf, "General", "terminal", "Default terminal command (use %s for command template)", NULL);
  g_key_file_set_boolean (kf, "General", "ignore_unknown_options", settings.ignore_unknown);
  g_key_file_set_comment (kf, "General", "ignore_unknown_options", "Ingnore unknown command-line options", NULL);

  context = g_key_file_to_data (kf, NULL, NULL);

  g_key_file_free (kf);

  if (g_mkdir_with_parents (g_get_user_config_dir (), 0644) != -1)
    {
      gchar *filename = g_build_filename (g_get_user_config_dir (), SETTINGS_FILE, NULL);
      g_file_set_contents (filename, context, -1, NULL);
      g_free (filename);
    }
  else
    g_printerr ("yad: cannot write settings file: %s\n", strerror (errno));

  g_free (context);
}

GdkPixbuf *
get_pixbuf (gchar *name, YadIconSize size)
{
  gint w, h;
  GdkPixbuf *pb = NULL;
  GError *err = NULL;

  if (g_file_test (name, G_FILE_TEST_EXISTS))
    {
      pb = gdk_pixbuf_new_from_file (name, &err);
      if (!pb)
	{
	  g_printerr ("yad_get_pixbuf(): %s\n", err->message);
	  g_error_free (err);
	}
    }
  else
    {
      if (size == YAD_BIG_ICON)
	gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &w, &h);
      else
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);

      pb = gtk_icon_theme_load_icon (settings.icon_theme, name, MIN (w, h),
				     GTK_ICON_LOOKUP_GENERIC_FALLBACK, NULL);
    }

  if (!pb)
    {
      if (size == YAD_BIG_ICON)
	pb = settings.big_fallback_image;
      else
	pb = settings.small_fallback_image;
    }

  return pb;
}

inline void
strip_new_line (gchar *str)
{
  gint nl = strlen (str) - 1;

  if (str[nl] == '\n')
    str[nl] = '\0';
}

gchar **
split_arg (const gchar *str)
{
  gchar **res;
  gchar *p_col;

  res = g_new0 (gchar*, 3);

  p_col =  g_strrstr (str, ":");
  if (p_col && p_col[1])
    {
      res[0] = g_strndup (str, p_col - str);
      res[1] = g_strdup (p_col + 1);
    }
  else
    res[0] = g_strdup (str);

  return res;
}

char *
escape_markup (char *str)
{
  char *res, *buf = str;
  unsigned i = 0, len;

  if (!str)
    return NULL;

  len = strlen (str);
  res = (char *) calloc (len + 1, sizeof (char));

  while (*buf)
    {
      switch (*buf)
        {
        case '&':
          len += 4;
          res = (char *) realloc (res, len + 1);
          strcpy (res + i, "&amp;");
          i += 5;
          break;
        case '<':
          len += 3;
          res = (char *) realloc (res, len + 1);
          strcpy (res + i, "&lt;");
          i += 4;
          break;
        case '>':
          len += 3;
          res = (char *) realloc (res, len + 1);
          strcpy (res + i, "&gt;");
          i += 4;
          break;
        case '"':
          len += 5;
          res = (char *) realloc (res, len + 1);
          strcpy (res + i, "&quot;");
          i += 6;
          break;
        case '\'':
          len += 5;
          res = (char *) realloc (res, len + 1);
          strcpy (res + i, "&apos;");
          i += 6;
          break;
        default:
          *(res + i) = *buf;
          i++;
          break;
        }
      buf++;
    }
  res[i] = '\0';

  return res;
}

char *
unescape_markup (char *str)
{
  char *res, *buf = str;
  int i = 0;

  if (!str)
    return NULL;

  res = calloc (strlen (str) + 1, sizeof (char));

  while (*buf)
    {
      if (*buf == '&')
        {
          buf++;
          if (strncmp (buf, "lt;", 3) == 0)
            {
              res[i] = '<';
              buf += 3;
            }
          else if (strncmp (buf, "gt;", 3) == 0)
            {
              res[i] = '>';
              buf += 3;
            }
          else if (strncmp (buf, "amp;", 4) == 0)
            {
              res[i] = '&';
              buf += 4;
            }
          else if (strncmp (buf, "quot;", 5) == 0)
            {
              res[i] = '"';
              buf += 5;
            }
          else if (strncmp (buf, "apos;", 5) == 0)
            {
              res[i] = '\'';
              buf += 5;
            }
        }
      else
        res[i] = *buf++;
      i++;
    }
  res[i] = '\0';

  return res;
}

char *
escape_str (char *str)
{
  char *res, *buf = str;
  unsigned i = 0, len;

  if (!str)
    return NULL;

  len = strlen (str);
  res = (char *) calloc (len + 1, sizeof (char));

  while (*buf)
    {
      switch (*buf)
        {
        case '\n':
          len += 1;
          res = (char *) realloc (res, len + 1);
          strcpy (res + i, "\\n");
          i += 2;
          break;
        case '\t':
          len += 1;
          res = (char *) realloc (res, len + 1);
          strcpy (res + i, "\\t");
          i += 2;
          break;
        default:
          *(res + i) = *buf;
          i++;
          break;
        }
      buf++;
    }
  res[i] = '\0';

  return res;
}
