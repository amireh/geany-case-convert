/*
 *  caseconvert-ui.h
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

#ifndef H_GEANY_CASE_CONVERT_UI_H
#define H_GEANY_CASE_CONVERT_UI_H

#include <geany/geany.h>
#include <geany/support.h>
#include <memory.h>
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

void cc_ui_init();
void cc_ui_cleanup();

void cc_ui_show_add_rule_dialog();
void cc_ui_hide_add_rule_dialog();

void cc_ui_show_edit_rules_dialog();
void cc_ui_hide_edit_rules_dialog();

void cc_ui_show_convert_more_dialog();
void cc_ui_hide_convert_more_dialog();

#endif
