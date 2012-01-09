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

typedef struct rule_t rule_t;
typedef struct action_t action_t;

typedef struct {
  /** when converting to camel case, always upcase the first character */
  gboolean  capitalize;

  /** the registered conversion rules */
  rule_t    *rules;
} config_t;

typedef struct {
  enum {
    has_prefix = 0,
    has_suffix,
    always_true
  } type;

  gchar *value;
} condition_t;

struct action_t {
  enum {
    add_prefix = 0,
    rem_prefix,
    add_suffix,
    rem_suffix
  } type;

  gchar     *value;
  action_t  *next;
};

struct rule_t {
  enum {
    s2c,
    c2s
  } domain;

  condition_t *condition;
  action_t    *actions;
  rule_t      *next;
  gchar       *label;
  gint        id;
};

enum {
  KB_CONVERT_SELECTION,
  KB_CONVERT_MORE,
  KB_CONVERT_ALL,
  KB_TEST,
  KB_COUNT
};

void cc_add_rule(rule_t *);
void cc_rem_rule(gint id);

void cc_convert_selection();
void cc_convert_range(int r_begin, int r_end, gchar const *txt, gint txtsz);
void cc_convert_all();

rule_t* cc_alloc_rule();
void    cc_free_rule(rule_t**);

/**
 * Helper for returning the selected text in the editor if the selection
 * is a single-line.
 *
 * @return
 * The value of the selection, or NULL if the selection is invalid. The returned
 * string (if any) must be freed by the caller.
 */
gchar* cc_get_selected_text(gint* sz);

#endif
