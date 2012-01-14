/*
 *  caseconvert.h
 *
 *  Copyright 2012 Ahmad Amireh <ahmad@amireh.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef H_GEANY_CASE_CONVERT_H
#define H_GEANY_CASE_CONVERT_H

#include <geany/geany.h>
#include <geany/support.h>
#include <memory.h>
#include <ctype.h>
#include <stdarg.h>
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include <geany/geanyplugin.h>
#include <geany/plugindata.h>
#include <geany/geanyfunctions.h>
#include <geany/document.h>
#include <geany/keybindings.h>
#include <geany/ui_utils.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gdk/gdkkeysyms.h>

#include "caseconvert_types.h"

typedef struct {
  /* when converting to camel case, always upcase the first character, ie:
   *  CamelCase instead of camelCase
   */
  gboolean  capitalize;

  rule_t    *rules; /* the registered conversion rules */
} config_t;

/* must compile with -DVERBOSE to log */
void cc_log(const char *fmt, ...);

void cc_add_rule(rule_t *);
void cc_rem_rule(gint id);
rule_t* cc_get_rule(gint id);
rule_t* cc_get_rules();

/** converts case found within the editor's cursor selection */
void cc_convert_selection();

/** converts all occurences of txt found in the specified range */
void cc_convert_range(int r_begin, int r_end, gchar const *txt, gint txtsz, int flags);

/** converts all occurences of the selected text found in the document */
void cc_convert_all();

/**
 * Helper for returning the selected text in the editor if the selection
 * is a single-line.
 *
 * @return
 * The value of the selection, or NULL if the selection is invalid. The returned
 * string (if any) must be freed by the caller.
 */
gchar* cc_get_selected_text(gint* sz);

/** reads plugin preferences and registers previously defined rules */
void cc_load_settings();

/** saves plugin preferences and all defined rules */
void cc_save_settings();

#endif
