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

void cc_show_add_rule_dialog();
void cc_hide_add_rule_dialog();

#endif
