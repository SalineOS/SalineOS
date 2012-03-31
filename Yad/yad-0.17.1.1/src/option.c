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

#include "yad.h"

static gboolean add_button (const gchar *, const gchar *, gpointer, GError **);
static gboolean set_text_align (const gchar *, const gchar *, gpointer, GError **);
static gboolean add_column (const gchar *, const gchar *, gpointer, GError **);
static gboolean add_field (const gchar *, const gchar *, gpointer, GError **);
static gboolean add_bar (const gchar *, const gchar *, gpointer, GError **);
static gboolean add_scale_mark (const gchar *, const gchar *, gpointer, GError **);
static gboolean add_palette (const gchar *, const gchar *, gpointer, GError **);
static gboolean add_confirm_overwrite (const gchar *, const gchar *, gpointer, GError **);
static gboolean set_align (const gchar *, const gchar *, gpointer, GError **);
static gboolean set_justify (const gchar *, const gchar *, gpointer, GError **);
static gboolean set_scale_value (const gchar *, const gchar *, gpointer, GError **);
static gboolean set_ellipsize (const gchar *, const gchar *, gpointer, GError **);
static gboolean set_expander (const gchar *, const gchar *, gpointer, GError **);
static gboolean set_print_type (const gchar *, const gchar *, gpointer, GError **);

static gboolean about_mode = FALSE;
static gboolean version_mode = FALSE;
static gboolean calendar_mode = FALSE;
static gboolean color_mode = FALSE;
static gboolean dnd_mode = FALSE;
static gboolean entry_mode = FALSE;
static gboolean file_mode = FALSE;
static gboolean font_mode = FALSE;
static gboolean form_mode = FALSE;
static gboolean icons_mode = FALSE;
static gboolean list_mode = FALSE;
static gboolean multi_progress_mode = FALSE;
static gboolean notification_mode = FALSE;
static gboolean print_mode = FALSE;
static gboolean progress_mode = FALSE;
static gboolean scale_mode = FALSE;
static gboolean text_mode = FALSE;

static GOptionEntry general_options[] = {
  { "title", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.data.dialog_title,
    N_("Set the dialog title"),
    N_("TITLE") },
  { "window-icon", 0,
    0,
    G_OPTION_ARG_FILENAME,
    &options.data.window_icon,
    N_("Set the window icon"),
    N_("ICONPATH") },
  { "width", 0,
    0,
    G_OPTION_ARG_INT,
    &options.data.width,
    N_("Set the width"),
    N_("WIDTH") },
  { "height", 0,
    0,
    G_OPTION_ARG_INT,
    &options.data.height,
    N_("Set the height"),
    N_("HEIGHT") },
  { "geometry", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.data.geometry,
    N_("Set the window geometry"),
    N_("WxH+X+Y") },
  { "timeout", 0,
    0,
    G_OPTION_ARG_INT,
    &options.data.timeout,
    N_("Set dialog timeout in seconds"),
    N_("TIMEOUT") },
  { "timeout-indicator", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.data.to_indicator,
    N_("Show remaining time indicator (top, bottom, left, right)"),
    N_("POS") },
  { "text", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.data.dialog_text,
    N_("Set the dialog text"),
    N_("TEXT") },
  { "text-align", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_CALLBACK,
    set_text_align,
    N_("Set the dialog text alignment (left, center, right)"),
    N_("TYPE") },
  { "image", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_FILENAME,
    &options.data.dialog_image,
    N_("Set the dialog image"),
    N_("IMAGE") },
  { "image-on-top", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_NONE,
    &options.data.image_on_top,
    N_("Show image above main widget"),
    NULL },
  { "icon-theme", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.data.icon_theme,
    N_("Use specified icon theme instead of default"),
    N_("THEME") },
  { "expander", 0,
    G_OPTION_FLAG_OPTIONAL_ARG,
    G_OPTION_ARG_CALLBACK,
    set_expander,
    N_("Hide main widget with expander"),
    N_("TEXT") },
  { "button", 0,
    0,
    G_OPTION_ARG_CALLBACK,
    add_button,
    N_("Add dialog button (may be used multiple times)"),
    N_("NAME:ID") },
  { "no-buttons", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.data.no_buttons,
    N_("Don't show buttons"),
    NULL },
  { "no-markup", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.data.no_markup,
    N_("Don't use pango markup language in dialog's text"),
    NULL },
#if !GTK_CHECK_VERSION(2,22,0)
  { "dialog-sep", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.data.dialog_sep,
    N_("Add separator between dialog and buttons"),
    NULL },
#endif
  { "borders", 0,
    0,
    G_OPTION_ARG_INT,
    &options.data.borders,
    N_("Set window borders"),
    N_("NUMBER") },
  { "always-print-result", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.data.always_print,
    N_("Always print result"),
    NULL },
  { "selectable-labels", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.data.selectable_labels,
    N_("Dialog text can be selected"),
    NULL },
  /* window settings */
  { "sticky", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.data.sticky,
    N_("Set window sticky"),
    NULL },
  { "fixed", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.data.fixed,
    N_("Set window unresizable"),
    NULL },
  { "on-top", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.data.ontop,
    N_("Place window on top"),
    NULL },
  { "center", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.data.center,
    N_("Place window on center of screen"),
    NULL },
  { "mouse", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.data.mouse,
    N_("Place window at the mouse position"),
    NULL },
  { "undecorated", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.data.undecorated,
    N_("Set window undecorated"),
    NULL },
  { "skip-taskbar", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.data.skip_taskbar,
    N_("Don't show window in taskbar"),
    NULL },
#ifndef G_OS_WIN32
  { "kill-parent", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.kill_parent,
    N_("Send TERM to parent"),
    NULL },
  { "print-xid", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.print_xid,
    N_("Print X Window Id to the stderr"),
    NULL },
#endif
  { NULL }
};

static GOptionEntry calendar_options[] = {
  { "calendar", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &calendar_mode,
    N_("Display calendar dialog"),
    NULL },
  { "day", 0,
    0,
    G_OPTION_ARG_INT,
    &options.calendar_data.day,
    N_("Set the calendar day"),
    N_("DAY") },
  { "month", 0,
    0,
    G_OPTION_ARG_INT,
    &options.calendar_data.month,
    N_("Set the calendar month"),
    N_("MONTH") },
  { "year", 0,
    0,
    G_OPTION_ARG_INT,
    &options.calendar_data.year,
    N_("Set the calendar year"),
    N_("YEAR") },
  { "date-format", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.date_format,
    N_("Set the format for the returned date"),
    N_("PATTERN") },
  { "details", 0,
    0,
    G_OPTION_ARG_FILENAME,
    &options.calendar_data.details,
    N_("Set the filename with dates details"),
    N_("FILENAME") },
  { NULL }
};

static GOptionEntry color_options[] = {
  { "color", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &color_mode,
    N_("Display color selection dialog"),
    NULL },
  { "color-selection", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &color_mode,
    N_("Alias for --color"),
    NULL },
  { "init-color", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.color_data.init_color,
    N_("Set initial color value"),
    N_("COLOR") },
  { "palette", 0,
    G_OPTION_FLAG_OPTIONAL_ARG,
    G_OPTION_ARG_CALLBACK,
    add_palette,
    N_("Set path to palette file. Default - " RGB_FILE),
    N_("FILENAME") },
  { "extra", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.color_data.extra,
    N_("Use #rrrrggggbbbb format instead of #rrggbb"),
    NULL },
  { NULL }
};

static GOptionEntry dnd_options[] = {
  { "dnd", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &dnd_mode,
    N_("Display drag-n-drop box"),
    NULL },
  { "tooltip", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.dnd_data.tooltip,
    N_("Use dialog text as tooltip"),
    NULL },
  { "command", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.command,
    N_("Set command for process d-n-d data"),
    N_("CMD") },
  { NULL }
};

static GOptionEntry entry_options[] = {
  { "entry", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &entry_mode,
    N_("Display text entry or combo-box dialog"),
    NULL },
  { "entry-label", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.entry_data.entry_label,
    N_("Set the entry label"),
    N_("TEXT") },
  { "entry-text", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.entry_data.entry_text,
    N_("Set the entry text"),
    N_("TEXT") },
  { "hide-text", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.entry_data.hide_text,
    N_("Hide the entry text"),
    N_("TEXT") },
  { "completion", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.entry_data.completion,
    N_("Use completion instead of combo-box"),
    NULL },
  { "numeric", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.entry_data.numeric,
    N_("Use spin button for text entry"),
    NULL },
  { "editable", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_NONE,
    &options.common_data.editable,
    N_("Allow changes to text in combo-box"),
    NULL },
  { "licon", 0,
    0,
    G_OPTION_ARG_FILENAME,
    &options.entry_data.licon,
    N_("Set the left entry icon"),
    N_("IMAGE") },
  { "licon-action", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.entry_data.licon_action,
    N_("Set the left entry icon action"),
    N_("CMD") },
  { "ricon", 0,
    0,
    G_OPTION_ARG_FILENAME,
    &options.entry_data.ricon,
    N_("Set the right entry icon"),
    N_("IMAGE") },
  { "ricon-action", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.entry_data.ricon_action,
    N_("Set the right entry icon action"),
    N_("CMD") },
  { NULL }
};

static GOptionEntry file_options[] = {
  { "file", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &file_mode,
    N_("Display file selection dialog"),
    NULL },
  { "file-selection", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &file_mode,
    N_("Alias for --file"),
    NULL },
  { "filename", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_FILENAME,
    &options.common_data.uri,
    N_("Set the filename"),
    N_("FILENAME") },
  { "multiple", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_NONE,
    &options.common_data.multi,
    N_("Allow multiple files to be selected"),
    NULL },
  { "directory", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.file_data.directory,
    N_("Activate directory-only selection"),
    NULL },
  { "save", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.file_data.save,
    N_("Activate save mode"),
    NULL },
  { "separator", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.separator,
    N_("Set output separator character"),
    N_("SEPARATOR") },
  { "confirm-overwrite", 0,
    G_OPTION_FLAG_OPTIONAL_ARG,
    G_OPTION_ARG_CALLBACK,
    add_confirm_overwrite,
    N_("Confirm file selection if filename already exists"),
    N_("[TEXT]") },
  { "file-filter", 0,
    0,
    G_OPTION_ARG_STRING_ARRAY,
    &options.file_data.filter,
    N_("Sets a filename filter"),
    N_("NAME | PATTERN1 PATTERN2 ...") },
  { NULL }
};

static GOptionEntry font_options[] = {
  { "font", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &font_mode,
    N_("Display font selection dialog"),
    NULL },
  { "font-selection", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &font_mode,
    N_("Alias for --font"),
    NULL },
  { "fontname", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.font,
    N_("Set initial font"),
    N_("FONTNAME") },
  { "preview", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.font_data.preview,
    N_("Set preview text"),
    N_("TEXT") },
  { NULL }
};

static GOptionEntry form_options[] = {
  { "form", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &form_mode,
    N_("Display form dialog"),
    NULL },
  { "field", 0,
    0,
    G_OPTION_ARG_CALLBACK,
    add_field,
    N_("Add field to form (TYPE - H, RO, NUM, CHK, CB, CBE, FL, SFL, MFL, DIR, CDIR, MDIR, FN, DT, CLR, BTN, LBL or TXT)"),
    N_("LABEL[:TYPE]") },
  { "align", 0,
    0,
    G_OPTION_ARG_CALLBACK,
    set_align,
    N_("Set alignment of fileds labels (left, center or right)"),
    N_("TYPE") },
  { "columns", 0,
    0,
    G_OPTION_ARG_INT,
    &options.form_data.columns,
    N_("Set number of columns in form"),
    N_("NUMBER") },
  { "separator", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.separator,
    N_("Set output separator character"),
    N_("SEPARATOR") },
  { "item-separator", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.item_separator,
    N_("Set separator character for combobox or scale data"),
    N_("SEPARATOR") },
  { "date-format", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.date_format,
    N_("Set the format for the returned date"),
    N_("PATTERN") },
  { NULL }
};

static GOptionEntry icons_options[] = {
  { "icons", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &icons_mode,
    N_("Display icons box dialog"),
    NULL },
  { "read-dir", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.icons_data.directory,
    N_("Read data from .desktop files in specified directory"),
    N_("DIRECTORY") },
  { "compact", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.icons_data.compact,
    N_("Use compact (list) view"),
    NULL },
  { "generic", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.icons_data.generic,
    N_("Use GenericName field instead of Name for icon label"),
    NULL },
  { "stdin", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.icons_data.stdinput,
    N_("Read data from stdin"),
    NULL },
  { "item-width", 0,
    0,
    G_OPTION_ARG_INT,
    &options.icons_data.width,
    N_("Set the width of dialog items"),
    NULL },
  { "term", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.icons_data.term,
    /* xgettext: no-c-format */
    N_("Use specified pattern for launch command in terminal (default: xterm -e %s)"),
    N_("PATTERN") },
  { "sort-by-name", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.icons_data.sort_by_name,
    N_("Sort items by name instead of filename"),
    NULL },
  { "descend", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.icons_data.descend,
    N_("Sort items in descending order"),
    NULL },
  { NULL }
};

static GOptionEntry list_options[] = {
  { "list", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &list_mode,
    N_("Display list dialog"),
    NULL },
  { "no-headers", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.list_data.no_headers,
    N_("Don't show column headers"),
    NULL },
  { "column", 0,
    0,
    G_OPTION_ARG_CALLBACK,
    add_column,
    N_("Set the column header (TYPE - TEXT, NUM, FLT, CHK, IMG or TIP)"),
    N_("COLUMN[:TYPE]") },
  { "checklist", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.list_data.checkbox,
    N_("Use check boxes for first column"),
    NULL },
  { "radiolist", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.list_data.checkbox,
    N_("Alias to checklist (deprecated)"),
    NULL },
  { "separator",
    0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.separator,
    N_("Set output separator character"),
    N_("SEPARATOR") },
  { "multiple", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_NONE,
    &options.common_data.multi,
    N_("Allow multiple rows to be selected"),
    NULL },
  { "editable", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_NONE,
    &options.common_data.editable,
    N_("Allow changes to text"),
    NULL },
  { "print-all", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.list_data.print_all,
    N_("Print all data from list"),
    NULL },
  { "ellipsize", 0,
    0,
    G_OPTION_ARG_CALLBACK,
    set_ellipsize,
    N_("Set ellipsize mode for text columns (TYPE - NONE, START, MIDDLE or END)"),
    N_("TYPE") },
  { "print-column", 0,
    0,
    G_OPTION_ARG_INT,
    &options.list_data.print_column,
    N_("Print a specific column. By default or if 0 is specified will be printed all columns"),
    N_("NUMBER") },
  { "hide-column", 0,
    0,
    G_OPTION_ARG_INT,
    &options.list_data.hide_column,
    N_("Hide a specific column"),
    N_("NUMBER") },
  { "expand-column", 0,
    0,
    G_OPTION_ARG_INT,
    &options.list_data.expand_column,
    N_("Set the column expandable by default. 0 sets all columns expandable"),
    N_("NUMBER") },
  { "search-column", 0,
    0,
    G_OPTION_ARG_INT,
    &options.list_data.search_column,
    N_("Set the quick search column. Default is first column. Set it to 0 for disable searching"),
    N_("NUMBER") },
  { "limit", 0,
    0,
    G_OPTION_ARG_INT,
    &options.list_data.limit,
    N_("Set the limit of rows in list"),
    N_("NUMBER") },
  { "dclick-action", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.list_data.dclick_action,
    N_("Set double-click action"),
    N_("CMD") },
  { "regex-search", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.list_data.regex_search,
    N_("Use regex in search"),
    NULL },
  { NULL }
};

static GOptionEntry multi_progress_options[] = {
  { "multi-progress", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &multi_progress_mode,
    N_("Display multi progress bars dialog"),
    NULL },
  { "bar", 0,
    0,
    G_OPTION_ARG_CALLBACK,
    add_bar,
    N_("Add the progress bar (TYPE - NORM, RTL or PULSE)"),
    N_("LABEL[:TYPE]") },
  { "vertical", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_NONE,
    &options.common_data.vertical,
    N_("Show vertical bars"),
    NULL },
  { NULL }
};

static GOptionEntry notification_options[] = {
  { "notification", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &notification_mode,
    N_("Display notification"),
    NULL},
  { "command", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.command,
    N_("Set left-click action"),
    N_("CMD") },
  { "listen", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.notification_data.listen,
    N_("Listen for commands on stdin"),
    NULL },
  { "separator", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.item_separator,
    N_("Set separator character for menu values"),
    N_("SEPARATOR") },
  { "item-separator", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.item_separator,
    N_("Set separator character for menu items"),
    N_("SEPARATOR") },
  { NULL }
};

static GOptionEntry print_options[] = {
  { "print", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &print_mode,
    N_("Display printing dialog"),
    NULL },
  { "filename", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_FILENAME,
    &options.common_data.uri,
    N_("Source filename"),
    N_("FILENAME") },
  { "type", 0,
    0,
    G_OPTION_ARG_CALLBACK,
    set_print_type,
    N_("Set source type (TYPE - TEXT, IMAGE or RAW)"),
    N_("TYPE") },
  { "headers", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.print_data.headers,
    N_("Add headers to page"),
    NULL },
  { "add-preview", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.print_data.preview,
    N_("Enable preview in print dialog"),
    NULL },
  { "fontname", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.font,
    N_("Use specified font"),
    N_("FONTNAME") },
  { NULL }
};

static GOptionEntry progress_options[] = {
  { "progress", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &progress_mode,
    N_("Display progress indication dialog"),
    NULL },
  { "progress-text", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.progress_data.progress_text,
    N_("Set progress text"),
    N_("TEXT") },
  { "percentage", 0,
    0,
    G_OPTION_ARG_INT,
    &options.progress_data.percentage,
    N_("Set initial percentage"),
    N_("PERCENTAGE") },
  { "pulsate", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.progress_data.pulsate,
    N_("Pulsate progress bar"),
    NULL },
  { "auto-close", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.progress_data.autoclose,
    /* xgettext: no-c-format */
    N_("Dismiss the dialog when 100% has been reached"),
    NULL },
#ifndef G_OS_WIN32
  { "auto-kill", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.progress_data.autokill,
    N_("Kill parent process if cancel button is pressed"),
    NULL },
#endif
  { "rtl", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.progress_data.rtl,
    N_("Right-To-Left progress bar direction"),
    NULL },
  { NULL }
};

static GOptionEntry scale_options[] = {
  { "scale", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &scale_mode,
    N_("Display scale dialog"),
    NULL },
  { "value", 0,
    0,
    G_OPTION_ARG_CALLBACK,
    set_scale_value,
    N_("Set initial value"),
    N_("VALUE") },
  { "min-value", 0,
    0,
    G_OPTION_ARG_INT,
    &options.scale_data.min_value,
    N_("Set minimum value"),
    N_("VALUE") },
  { "max-value", 0,
    0,
    G_OPTION_ARG_INT,
    &options.scale_data.max_value,
    N_("Set maximum value"),
    N_("VALUE") },
  { "step", 0,
    0,
    G_OPTION_ARG_INT,
    &options.scale_data.step,
    N_("Set step size"),
    N_("VALUE") },
  { "page", 0,
    0,
    G_OPTION_ARG_INT,
    &options.scale_data.page,
    N_("Set paging size"),
    N_("VALUE") },
  { "print-partial", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.scale_data.print_partial,
    N_("Print partial values"),
    NULL },
  { "hide-value", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.scale_data.hide_value,
    N_("Hide value"),
    NULL },
  { "vertical", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_NONE,
    &options.common_data.vertical,
    N_("Show vertical scale"),
    NULL },
  { "invert", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.scale_data.invert,
    N_("Invert direction"),
    NULL },
  { "mark", 0,
    0,
    G_OPTION_ARG_CALLBACK,
    add_scale_mark,
    N_("Add mark to scale (may be used multiple times)"),
    N_("NAME:VALUE") },
  { NULL }
};

static GOptionEntry text_options[] = {
  { "text-info", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_NONE,
    &text_mode,
    N_("Display text information dialog"),
    NULL },
  { "fore", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.text_data.fore,
    N_("Use specified color for text"),
    N_("COLOR") },
  { "back", 0,
    0,
    G_OPTION_ARG_STRING,
    &options.text_data.back,
    N_("Use specified color for background"),
    N_("COLOR") },
  { "fontname", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_STRING,
    &options.common_data.font,
    N_("Use specified font"),
    N_("FONTNAME") },
  { "wrap", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.text_data.wrap,
    N_("Enable text wrapping"),
    NULL },
  { "justify", 0,
    0,
    G_OPTION_ARG_CALLBACK,
    set_justify,
    N_("Set justification (TYPE - left, right, center or fill)"),
    N_("TYPE") },
  { "margins", 0,
    0,
    G_OPTION_ARG_INT,
    &options.text_data.margins,
    N_("Set text margins"),
    N_("SIZE") },
  { "tail", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.text_data.tail,
    N_("Autoscroll to end of text"),
    NULL },
  { "filename", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_FILENAME,
    &options.common_data.uri,
    N_("Open file"),
    N_("FILENAME") },
  { "editable", 0,
    G_OPTION_FLAG_NOALIAS,
    G_OPTION_ARG_NONE,
    &options.common_data.editable,
    N_("Allow changes to text"),
    NULL },
  { "show-uri", 0,
    0,
    G_OPTION_ARG_NONE,
    &options.text_data.uri,
    N_("Make URI clickable"),
    NULL },
  { NULL }
};

static GOptionEntry misc_options[] = {
  { "about", 0,
    0,
    G_OPTION_ARG_NONE,
    &about_mode,
    N_("Show about dialog"),
    NULL },
  { "version", 0,
    0,
    G_OPTION_ARG_NONE,
    &version_mode,
    N_("Print version"),
    NULL },
  { NULL }
};

static GOptionEntry rest_options[] = {
  { "rest", 0,
    0,
    G_OPTION_ARG_FILENAME,
    &options.rest_file,
    N_("Load extra arguments from file"),
    N_("FILENAME") },
  { G_OPTION_REMAINING, 0,
    0,
    G_OPTION_ARG_STRING_ARRAY,
    &options.extra_data,
    NULL, NULL },
  { NULL }
};

static gboolean
add_button (const gchar *option_name, const gchar *value,
	    gpointer data, GError **err)
{
  YadButton *btn;
  gchar **bstr = split_arg (value);

  btn = g_new0 (YadButton, 1);
  btn->name = g_strdup (bstr[0]);
  if (bstr[1])
    {
      if (bstr[1][0] >= '0' && bstr[1][0] <= '9')
	btn->response = g_ascii_strtoll (bstr[1], NULL, 10);
      else
	btn->cmd = g_strdup (bstr[1]);
    }
  else
    btn->response = g_slist_length (options.data.buttons);
  options.data.buttons = g_slist_append (options.data.buttons, btn);

  g_strfreev (bstr);
  return TRUE;
}

static gboolean
set_text_align (const gchar *option_name, const gchar *value,
	    gpointer data, GError **err)
{
  if (g_ascii_strcasecmp (value, "left") == 0)
    options.data.text_align = 0.0;
  else if (g_ascii_strcasecmp (value, "right") == 0)
    options.data.text_align = 1.0;
  else if (g_ascii_strcasecmp (value, "center") == 0)
    options.data.text_align = 0.5;
  else
    g_printerr (_("Unknown align type: %s\n"), value);

  return TRUE;
}

static gboolean
add_column (const gchar *option_name, const gchar *value,
	    gpointer data, GError **err)
{
  YadColumn *col;
  gchar **cstr = split_arg (value);

  col = g_new0 (YadColumn, 1);
  col->name = g_strdup (cstr[0]);
  if (g_ascii_strcasecmp (cstr[0], "@fore@") == 0)
    col->type = YAD_COLUMN_ATTR_FORE;
  else if (g_ascii_strcasecmp (cstr[0], "@back@") == 0)
    col->type = YAD_COLUMN_ATTR_BACK;
  else if (g_ascii_strcasecmp (cstr[0], "@font@") == 0)
    col->type = YAD_COLUMN_ATTR_FONT;
  else
    {
      if (cstr[1])
	{
	  if (g_ascii_strcasecmp (cstr[1], "NUM") == 0)
	    col->type = YAD_COLUMN_NUM ;
	  else if (g_ascii_strcasecmp (cstr[1], "CHK") == 0)
	    col->type = YAD_COLUMN_CHECK;
	  else if (g_ascii_strcasecmp (cstr[1], "FLT") == 0)
	    col->type = YAD_COLUMN_FLOAT;
	  else if (g_ascii_strcasecmp (cstr[1], "IMG") == 0)
	    col->type = YAD_COLUMN_IMAGE;
	  else if (g_ascii_strcasecmp (cstr[1], "TIP") == 0)
	    col->type = YAD_COLUMN_TOOLTIP;
	  else
	    col->type = YAD_COLUMN_TEXT;
	}
      else
	col->type = YAD_COLUMN_TEXT;
    }
  options.list_data.columns =
    g_slist_append (options.list_data.columns, col);

  g_strfreev (cstr);
  return TRUE;
}

static gboolean
add_field (const gchar *option_name, const gchar *value,
	   gpointer data, GError **err)
{
  YadField *fld;
  gchar **fstr = split_arg (value);

  fld = g_new0 (YadField, 1);
  fld->name = g_strdup (fstr[0]);
  if (fstr[1])
    {
      if (g_ascii_strcasecmp (fstr[1], "H") == 0)
	fld->type = YAD_FIELD_HIDDEN;
      else if (g_ascii_strcasecmp (fstr[1], "RO") == 0)
	fld->type = YAD_FIELD_READ_ONLY;
      else if (g_ascii_strcasecmp (fstr[1], "NUM") == 0)
	fld->type = YAD_FIELD_NUM;
      else if (g_ascii_strcasecmp (fstr[1], "CHK") == 0)
	fld->type = YAD_FIELD_CHECK;
      else if (g_ascii_strcasecmp (fstr[1], "CB") == 0)
	fld->type = YAD_FIELD_COMBO;
      else if (g_ascii_strcasecmp (fstr[1], "CBE") == 0)
	fld->type = YAD_FIELD_COMBO_ENTRY;
      else if (g_ascii_strcasecmp (fstr[1], "FL") == 0)
	fld->type = YAD_FIELD_FILE;
      else if (g_ascii_strcasecmp (fstr[1], "SFL") == 0)
	fld->type = YAD_FIELD_FILE_SAVE;
      else if (g_ascii_strcasecmp (fstr[1], "DIR") == 0)
	fld->type = YAD_FIELD_DIR;
      else if (g_ascii_strcasecmp (fstr[1], "CDIR") == 0)
	fld->type = YAD_FIELD_DIR_CREATE;
      else if (g_ascii_strcasecmp (fstr[1], "FN") == 0)
	fld->type = YAD_FIELD_FONT;
      else if (g_ascii_strcasecmp (fstr[1], "CLR") == 0)
	fld->type = YAD_FIELD_COLOR;
      else if (g_ascii_strcasecmp (fstr[1], "MFL") == 0)
	fld->type = YAD_FIELD_MFILE;
      else if (g_ascii_strcasecmp (fstr[1], "MDIR") == 0)
	fld->type = YAD_FIELD_MDIR;
      else if (g_ascii_strcasecmp (fstr[1], "DT") == 0)
	fld->type = YAD_FIELD_DATE;
      else if (g_ascii_strcasecmp (fstr[1], "BTN") == 0)
	fld->type = YAD_FIELD_BUTTON;
      else if (g_ascii_strcasecmp (fstr[1], "LBL") == 0)
	fld->type = YAD_FIELD_LABEL;
      else if (g_ascii_strcasecmp (fstr[1], "TXT") == 0)
	fld->type = YAD_FIELD_TEXT;
      else
	fld->type = YAD_FIELD_SIMPLE;
    }
  else
    fld->type = YAD_FIELD_SIMPLE;
  options.form_data.fields =
    g_slist_append (options.form_data.fields, fld);

  g_strfreev (fstr);
  return TRUE;
}

static gboolean
add_bar (const gchar *option_name, const gchar *value,
	 gpointer data, GError **err)
{
  YadProgressBar *bar;
  gchar **bstr = split_arg (value);

  bar = g_new0 (YadProgressBar, 1);
  bar->name = g_strdup (bstr[0]);
  if (bstr[1])
    {
      if (g_ascii_strcasecmp (bstr[1], "RTL") == 0)
	bar->type = YAD_PROGRESS_RTL;
      else if (g_ascii_strcasecmp (bstr[1], "PULSE") == 0)
	bar->type = YAD_PROGRESS_PULSE;
      else
	bar->type = YAD_PROGRESS_NORMAL;
    }
  else
    bar->type = YAD_PROGRESS_NORMAL;
  options.multi_progress_data.bars =
    g_slist_append (options.multi_progress_data.bars, bar);

  g_strfreev (bstr);
  return TRUE;
}

static gboolean
add_scale_mark (const gchar *option_name, const gchar *value,
		gpointer data, GError **err)
{
  YadScaleMark *mark;
  gchar **mstr = split_arg (value);

  mark = g_new0 (YadScaleMark, 1);
  mark->name = g_strdup (mstr[0]);
  if (mstr[1])
    {
      mark->value = g_ascii_strtoll (mstr[1], NULL, 10);
      options.scale_data.marks = g_slist_append (options.scale_data.marks, mark);
    }
  else
    {
      g_printerr (_("Mark %s doesn't have a value\n"), mark->name);
      g_free (mark->name);
      g_free (mark);
    }

  g_strfreev (mstr);
  return TRUE;
}

static gboolean
add_palette (const gchar *option_name, const gchar *value,
	     gpointer data, GError **err)
{
  options.color_data.use_palette = TRUE;
  if (value)
    options.color_data.palette = g_strdup (value);

  return TRUE;
}

static gboolean
add_confirm_overwrite (const gchar *option_name, const gchar *value,
		       gpointer data, GError **err)
{
  options.file_data.confirm_overwrite = TRUE;
  if (value)
    options.file_data.confirm_text = g_strdup (value);

  return TRUE;
}

static gboolean
set_align (const gchar *option_name, const gchar *value,
	   gpointer data, GError **err)
{
  if (g_ascii_strcasecmp (value, "left") == 0)
    options.form_data.align = 0.0;
  else if (g_ascii_strcasecmp (value, "right") == 0)
    options.form_data.align = 1.0;
  else if (g_ascii_strcasecmp (value, "center") == 0)
    options.form_data.align = 0.5;
  else
    g_printerr (_("Unknown align type: %s\n"), value);

  return TRUE;
}

static gboolean
set_justify (const gchar *option_name, const gchar *value,
	     gpointer data, GError **err)
{
  if (g_ascii_strcasecmp (value, "left") == 0)
    options.text_data.justify = GTK_JUSTIFY_LEFT;
  else if (g_ascii_strcasecmp (value, "right") == 0)
    options.text_data.justify = GTK_JUSTIFY_RIGHT;
  else if (g_ascii_strcasecmp (value, "center") == 0)
    options.text_data.justify = GTK_JUSTIFY_CENTER;
  else if (g_ascii_strcasecmp (value, "fill") == 0)
    options.text_data.justify = GTK_JUSTIFY_FILL;
  else
    g_printerr (_("Unknown justification type: %s\n"), value);

  return TRUE;
}

static gboolean
set_expander (const gchar *option_name, const gchar *value,
	      gpointer data, GError **err)
{
  if (value)
    options.data.expander = g_strdup (value);
  else
    options.data.expander = "";
  return TRUE;
}

static gboolean
set_scale_value (const gchar *option_name, const gchar *value,
		 gpointer data, GError **err)
{
  options.scale_data.value = atoi (value);
  options.scale_data.have_value = TRUE;

  return TRUE;
}

static gboolean
set_ellipsize (const gchar *option_name, const gchar *value,
	       gpointer data, GError **err)
{
  if (g_ascii_strcasecmp (value, "none") == 0)
    options.list_data.ellipsize = PANGO_ELLIPSIZE_NONE;
  else if (g_ascii_strcasecmp (value, "start") == 0)
    options.list_data.ellipsize = PANGO_ELLIPSIZE_START;
  else if (g_ascii_strcasecmp (value, "middle") == 0)
    options.list_data.ellipsize = PANGO_ELLIPSIZE_MIDDLE;
  else if (g_ascii_strcasecmp (value, "end") == 0)
    options.list_data.ellipsize = PANGO_ELLIPSIZE_END;
  else
    g_printerr (_("Unknown ellipsize type: %s\n"), value);

  return TRUE;
}

static gboolean
set_print_type (const gchar *option_name, const gchar *value,
		gpointer data, GError **err)
{
  if (g_ascii_strcasecmp (value, "text") == 0)
    options.print_data.type = YAD_PRINT_TEXT;
  else if (g_ascii_strcasecmp (value, "image") == 0)
    options.print_data.type = YAD_PRINT_IMAGE;
  else if (g_ascii_strcasecmp (value, "raw") == 0)
    options.print_data.type = YAD_PRINT_RAW;
  else
    g_printerr (_("Unknown source type: %s\n"), value);

  return TRUE;
}

void
yad_set_mode (void)
{
  if (calendar_mode)
    options.mode = YAD_MODE_CALENDAR;
  else if (color_mode)
    options.mode = YAD_MODE_COLOR;
  else if (dnd_mode)
    options.mode = YAD_MODE_DND;
  else if (entry_mode)
    options.mode = YAD_MODE_ENTRY;
  else if (file_mode)
    options.mode = YAD_MODE_FILE;
  else if (font_mode)
    options.mode = YAD_MODE_FONT;
  else if (form_mode)
    options.mode = YAD_MODE_FORM;
  else if (icons_mode)
    options.mode = YAD_MODE_ICONS;
  else if (list_mode)
    options.mode = YAD_MODE_LIST;
  else if (multi_progress_mode)
    options.mode = YAD_MODE_MULTI_PROGRESS;
  else if (notification_mode)
    options.mode = YAD_MODE_NOTIFICATION;
  else if (print_mode)
    options.mode = YAD_MODE_PRINT;
  else if (progress_mode)
    options.mode = YAD_MODE_PROGRESS;
  else if (scale_mode)
    options.mode = YAD_MODE_SCALE;
  else if (text_mode)
    options.mode = YAD_MODE_TEXTINFO;
  else if (about_mode)
    options.mode = YAD_MODE_ABOUT;
  else if (version_mode)
    options.mode = YAD_MODE_VERSION;
}

void
yad_options_init (void)
{
  /* Set default mode */
  options.mode = YAD_MODE_MESSAGE;
  options.rest_file = NULL;
  options.extra_data = NULL;
#ifndef G_OS_WIN32
  options.kill_parent = FALSE;
  options.print_xid = FALSE;
#endif

  /* Initialize general data */
  options.data.dialog_title = NULL;
  options.data.window_icon = "yad";
  options.data.width = settings.width;
  options.data.height = settings.height;
  options.data.geometry = NULL;
  options.data.dialog_text = NULL;
  options.data.text_align = 0.0;
  options.data.dialog_image = NULL;
  options.data.image_on_top = FALSE;
  options.data.icon_theme = NULL;
  options.data.expander = NULL;
  options.data.timeout = settings.timeout;
  options.data.to_indicator = settings.to_indicator;
  options.data.buttons = NULL;
  options.data.no_buttons = FALSE;
#if !GTK_CHECK_VERSION(2,22,0)
  options.data.dialog_sep = settings.dlg_sep;
#endif
  options.data.borders = -1;
  options.data.no_markup = FALSE;
  options.data.always_print = FALSE;
  options.data.selectable_labels = FALSE;

  /* Initialize window options */
  options.data.sticky = FALSE;
  options.data.fixed = FALSE;
  options.data.ontop = FALSE;
  options.data.center = FALSE;
  options.data.mouse = FALSE;
  options.data.undecorated = FALSE;
  options.data.skip_taskbar = FALSE;

  /* Initialize common data */
  options.common_data.uri = NULL;
  options.common_data.font = NULL;
  options.common_data.separator = "|";
  options.common_data.item_separator = "!";
  options.common_data.multi = FALSE;
  options.common_data.editable = FALSE;
  options.common_data.command = NULL;
  options.common_data.date_format = "%x";
  options.common_data.vertical = FALSE;

  /* Initialize calendar data */
  options.calendar_data.day = -1;
  options.calendar_data.month = -1;
  options.calendar_data.year = -1;
  options.calendar_data.details = NULL;

  /* Initialize color data */
  options.color_data.init_color = NULL;
  options.color_data.use_palette = FALSE;
  options.color_data.palette = NULL;
  options.color_data.extra = FALSE;

  /* Initialize DND data */
  options.dnd_data.tooltip = FALSE;

  /* Initialize entry data */
  options.entry_data.entry_text = NULL;
  options.entry_data.entry_label = NULL;
  options.entry_data.hide_text = FALSE;
  options.entry_data.completion = FALSE;
  options.entry_data.numeric = FALSE;
  options.entry_data.licon = NULL;
  options.entry_data.licon_action = NULL;
  options.entry_data.ricon = NULL;
  options.entry_data.ricon_action = NULL;

  /* Initialize file data */
  options.file_data.directory = FALSE;
  options.file_data.save = FALSE;
  options.file_data.confirm_overwrite = FALSE;
  options.file_data.confirm_text = N_("File exist. Overwrite?");
  options.file_data.filter = NULL;

  /* Initialize font data */
  options.font_data.preview = NULL;

  /* Initialize form data */
  options.form_data.fields = NULL;
  options.form_data.align = 0.0;
  options.form_data.columns = 1;

  /* Initialize icons data */
  options.icons_data.directory = NULL;
  options.icons_data.compact = FALSE;
  options.icons_data.generic = FALSE;
  options.icons_data.stdinput = FALSE;
  options.icons_data.width = -1;
  options.icons_data.term = settings.term;
  options.icons_data.sort_by_name = FALSE;
  options.icons_data.descend = FALSE;

  /* Initialize list data */
  options.list_data.columns = NULL;
  options.list_data.no_headers = FALSE;
  options.list_data.checkbox = FALSE;
  options.list_data.print_all = FALSE;
  options.list_data.print_column = 0;
  options.list_data.hide_column = 0;
  options.list_data.expand_column = -1;    // must be -1 for disable expand by default (keep the original behavior)
  options.list_data.search_column = 0;
  options.list_data.limit = 0;
  options.list_data.ellipsize = PANGO_ELLIPSIZE_NONE;
  options.list_data.dclick_action = NULL;
  options.list_data.regex_search = FALSE;

  /* Initialize multiprogress data */
  options.multi_progress_data.bars = NULL;

  /* Initialize notification data */
  options.notification_data.listen = FALSE;

  /* Initialize print data */
  options.print_data.type = YAD_PRINT_TEXT;
  options.print_data.headers = FALSE;
  options.print_data.preview = FALSE;

  /* Initialize progress data */
  options.progress_data.progress_text = NULL;
  options.progress_data.percentage = 0;
  options.progress_data.pulsate = FALSE;
  options.progress_data.autoclose = FALSE;
#ifndef G_OS_WIN32
  options.progress_data.autokill = FALSE;
#endif
  options.progress_data.rtl = FALSE;

  /* Initialize scale data */
  options.scale_data.value = 0;
  options.scale_data.min_value = 0;
  options.scale_data.max_value = 100;
  options.scale_data.step = 1;
  options.scale_data.page = -1;
  options.scale_data.print_partial = FALSE;
  options.scale_data.hide_value = FALSE;
  options.scale_data.have_value = FALSE;
  options.scale_data.invert = FALSE;
  options.scale_data.marks = NULL;

  /* Initialize text data */
  options.text_data.fore = NULL;
  options.text_data.back = NULL;
  options.text_data.wrap = FALSE;
  options.text_data.justify = GTK_JUSTIFY_LEFT;
  options.text_data.tail = FALSE;
}

GOptionContext *
yad_create_context (void)
{
  GOptionContext *tmp_ctx;
  GOptionGroup *a_group;

  tmp_ctx = g_option_context_new (_("Yet another dialoging program"));
  g_option_context_add_main_entries (tmp_ctx, rest_options, GETTEXT_PACKAGE);

  /* Adds general option entries */
  a_group = g_option_group_new ("general", _("General options"),
				_("Show general options"), NULL, NULL);
  g_option_group_add_entries (a_group, general_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds calendar option entries */
  a_group = g_option_group_new ("calendar", _("Calendar options"),
				_("Show calendar options"), NULL, NULL);
  g_option_group_add_entries (a_group, calendar_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds color option entries */
  a_group = g_option_group_new ("color", _("Color selection options"),
				_("Show color selection options"), NULL, NULL);
  g_option_group_add_entries (a_group, color_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds dnd option entries */
  a_group = g_option_group_new ("dnd", _("DND options"),
				_("Show drag-n-drop options"), NULL, NULL);
  g_option_group_add_entries (a_group, dnd_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds entry option entries */
  a_group = g_option_group_new ("entry", _("Text entry options"),
				_("Show text entry options"), NULL, NULL);
  g_option_group_add_entries (a_group, entry_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds file selection option entries */
  a_group = g_option_group_new ("file", _("File selection options"),
				_("Show file selection options"), NULL, NULL);
  g_option_group_add_entries (a_group, file_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Add font selection option entries */
  a_group = g_option_group_new ("font", _("Font selection options"),
				_("Show font selection options"), NULL, NULL);
  g_option_group_add_entries (a_group, font_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Add form option entries */
  a_group = g_option_group_new ("form", _("Form options"),
				_("Show form options"), NULL, NULL);
  g_option_group_add_entries (a_group, form_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Add icons option entries */
  a_group = g_option_group_new ("icons", _("Icons box options"),
				_("Show icons box options"), NULL, NULL);
  g_option_group_add_entries (a_group, icons_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds list option entries */
  a_group = g_option_group_new ("list", _("List options"),
				_("Show list options"), NULL, NULL);
  g_option_group_add_entries (a_group, list_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds multi progress option entries */
  a_group = g_option_group_new ("multi-progress", _("Multi progress bars options"),
				_("Show Multi progress bars options"), NULL, NULL);
  g_option_group_add_entries (a_group, multi_progress_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds notification option entries */
  a_group = g_option_group_new ("notification", _("Notification icon options"),
				_("Show notification icon options"), NULL, NULL);
  g_option_group_add_entries (a_group, notification_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds print option entries */
  a_group = g_option_group_new ("print", _("Print dialog options"),
				_("Show print dialog options"), NULL, NULL);
  g_option_group_add_entries (a_group, print_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds progress option entries */
  a_group = g_option_group_new ("progress", _("Progress options"),
				_("Show progress options"), NULL, NULL);
  g_option_group_add_entries (a_group, progress_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds scale option entries */
  a_group = g_option_group_new ("scale", _("Scale options"),
				_("Show scale options"), NULL, NULL);
  g_option_group_add_entries (a_group, scale_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds text option entries */
  a_group = g_option_group_new ("text", _("Text information options"),
				_("Show text information options"), NULL, NULL);
  g_option_group_add_entries (a_group, text_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds miscellaneous option entries */
  a_group = g_option_group_new ("misc", _("Miscellaneous options"),
				_("Show miscellaneous options"), NULL, NULL);
  g_option_group_add_entries (a_group, misc_options);
  g_option_group_set_translation_domain (a_group, GETTEXT_PACKAGE);
  g_option_context_add_group (tmp_ctx, a_group);

  /* Adds gtk option entries */
  a_group = gtk_get_option_group (TRUE);
  g_option_context_add_group (tmp_ctx, a_group);

  g_option_context_set_help_enabled (tmp_ctx, TRUE);
  g_option_context_set_ignore_unknown_options (tmp_ctx, settings.ignore_unknown);

  return tmp_ctx;
}
