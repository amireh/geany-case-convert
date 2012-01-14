/*
 *  caseconvert-ui.c
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

#include "caseconvert_ui.h"
#include "caseconvert.h"

typedef struct {
  GtkDialog *dlg;

  GtkButton *btn_cancel;
  GtkButton *btn_create;

  /* domain radio buttons */
  GtkRadioButton *opt_domain_s2c;
  GtkRadioButton *opt_domain_c2s;

  /* condition radio buttons */
  GtkRadioButton *opt_cnd_has_prefix;
  GtkRadioButton *opt_cnd_has_suffix;
  GtkRadioButton *opt_cnd_always_true;

  /* actions radio buttons */
  GtkCheckButton *opt_act_rem_prefix;
  GtkCheckButton *opt_act_rem_suffix;
  GtkCheckButton *opt_act_add_prefix;
  GtkCheckButton *opt_act_add_suffix;

  GtkEntry *txt_cnd;
  GtkEntry *txt_act_rem_prefix;
  GtkEntry *txt_act_rem_suffix;
  GtkEntry *txt_act_add_prefix;
  GtkEntry *txt_act_add_suffix;

} add_rule_dlg_t;

enum ER_COLS {
    ER_COL_ID,
    ER_COL_ENABLED,
    ER_COL_LABEL,
    ER_COL_CND,
    ER_COL_ACT
  };

typedef struct {

  GtkDialog *dlg;

  GtkButton *btn_cancel;
  GtkButton *btn_save;

  GtkCellRendererText* cell_label;
  GtkCellRendererToggle* cell_enabled;

} edit_rules_dlg_t;

typedef struct {
  GtkDialog *dlg;

  GtkButton *btn_cancel;
  GtkButton *btn_selection;
  GtkButton *btn_document;

  /* search flags radio buttons */
  GtkCheckButton *opt_case_sensitive;
  GtkCheckButton *opt_whole_word;
  GtkCheckButton *opt_start_word;

  GtkEntry *txt_search;
} convert_more_dlg_t;

typedef struct {
  GtkWidget     *main_menu;
  GtkWidget     *convert_selection;
  GtkWidget     *convert_all;
  GtkWidget     *convert_more;
  GtkWidget     *add_rule;
  GtkWidget     *edit_rules;
} menu_items_t;

/* defined in caseconvert.c */
extern GeanyPlugin    *geany_plugin;
extern GeanyData      *geany_data;
extern GeanyFunctions *geany_functions;

static GtkBuilder         *builder = NULL;
static GtkWidget          *main_menu_item = NULL;
static menu_items_t       *menu_items = NULL;
static add_rule_dlg_t     *add_rule_dlg = NULL;
static edit_rules_dlg_t   *edit_rules_dlg = NULL;
static convert_more_dlg_t *convert_more_dlg = NULL;
static const char         *ui_file_path = "/home/kandie/Workspace/Projects/geany/caseconvert/caseconvert-ui.xml";

static void populate_rule_list();

/* UI event handlers */
static void on_add_rule_btn_create(GtkWidget*);

static void on_convert_more_btn_selection(GtkWidget*);
static void on_convert_more_btn_document(GtkWidget*);

static void on_er_enabled_toggled(GtkCellRendererToggle *cell,
                                   gchar                 *path_string,
                                   gpointer               user_data);
static void on_er_label_edited (GtkCellRendererText *cell,
                                gchar               *path_string,
                                gchar               *new_text,
                                gpointer             user_data);
static void on_er_btn_save();

/* keybindings */
PLUGIN_KEY_GROUP(convert_case, KB_COUNT)

void cc_ui_init()
{
  GtkContainer *menu = NULL;
  GtkWidget *item = NULL;
  GError *err = NULL;
  menu_items = g_malloc(sizeof(menu_items_t));

	item = gtk_menu_item_new_with_mnemonic(_("Con_vert Case"));
	main_menu_item = item;
  menu_items->main_menu = item;

  /* make the menu item sensitive only when documents are open */
	ui_add_document_sensitive(item);

	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), item);
  menu = GTK_CONTAINER(gtk_menu_new());
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), GTK_WIDGET(menu));

	item = gtk_menu_item_new_with_mnemonic(_("Convert _Selection"));
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(cc_convert_selection), NULL);
  menu_items->convert_selection = item;

	item = gtk_menu_item_new_with_mnemonic(_("Convert _All"));
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(cc_convert_all), NULL);
  menu_items->convert_all = item;

	item = gtk_menu_item_new_with_mnemonic(_("Convert _More"));
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(cc_ui_show_convert_more_dialog), NULL);
  menu_items->convert_more = item;

  item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_menu_item_new_with_mnemonic(_("A_dd Rule"));
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(cc_ui_show_add_rule_dialog), NULL);
  menu_items->add_rule = item;

	item = gtk_menu_item_new_with_mnemonic(_("_Edit Rules"));
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(cc_ui_show_edit_rules_dialog), NULL);
  menu_items->edit_rules = item;

  gtk_widget_show_all(main_menu_item);

  builder = gtk_builder_new();
  err = NULL;
  if (gtk_builder_add_from_file(builder, ui_file_path, &err) == 0)
  {
    cc_log("unable to read GTK UI definition\n");
    if (err->domain == GTK_BUILDER_ERROR)
      cc_log("\tcause: builder error '%s'\n", err->message);
    else if (err->domain == G_MARKUP_ERROR)
      cc_log("\tcause: markup error '%s'\n", err->message);
    else if (err->domain == G_FILE_ERROR)
      cc_log("\tcause: file error '%s'\n", err->message);
    else
      cc_log("\tcause: unknown error\n");

    return;
  }

  cc_log("read GTK UI definition successfully, binding widgets\n");

  /* set up the Add Rule dialog */
  add_rule_dlg = g_malloc(sizeof(add_rule_dlg_t));

  add_rule_dlg->dlg = (GtkDialog*)(gtk_builder_get_object(builder, "cc_dlg_add_rule"));
  add_rule_dlg->btn_cancel = (GtkButton*)(gtk_builder_get_object(builder, "btn_cancel"));
  add_rule_dlg->btn_create = (GtkButton*)(gtk_builder_get_object(builder, "btn_create"));

  add_rule_dlg->opt_domain_s2c = (GtkRadioButton*)(gtk_builder_get_object(builder, "opt_domain_s2c"));
  add_rule_dlg->opt_domain_c2s = (GtkRadioButton*)(gtk_builder_get_object(builder, "opt_domain_c2s"));

  add_rule_dlg->opt_cnd_has_prefix   = (GtkRadioButton*)(gtk_builder_get_object(builder, "opt_cnd_has_prefix"));
  add_rule_dlg->opt_cnd_has_suffix   = (GtkRadioButton*)(gtk_builder_get_object(builder, "opt_cnd_has_suffix"));
  add_rule_dlg->opt_cnd_always_true  = (GtkRadioButton*)(gtk_builder_get_object(builder, "opt_cnd_always_true"));

  add_rule_dlg->opt_act_rem_prefix  = (GtkCheckButton*)(gtk_builder_get_object(builder, "opt_act_rem_prefix"));
  add_rule_dlg->opt_act_rem_suffix  = (GtkCheckButton*)(gtk_builder_get_object(builder, "opt_act_rem_suffix"));
  add_rule_dlg->opt_act_add_prefix  = (GtkCheckButton*)(gtk_builder_get_object(builder, "opt_act_add_prefix"));
  add_rule_dlg->opt_act_add_suffix  = (GtkCheckButton*)(gtk_builder_get_object(builder, "opt_act_add_suffix"));

  add_rule_dlg->txt_cnd  =            (GtkEntry*)(gtk_builder_get_object(builder, "txt_cnd_value"));
  add_rule_dlg->txt_act_rem_prefix  = (GtkEntry*)(gtk_builder_get_object(builder, "txt_remove_prefix"));
  add_rule_dlg->txt_act_rem_suffix  = (GtkEntry*)(gtk_builder_get_object(builder, "txt_remove_suffix"));
  add_rule_dlg->txt_act_add_prefix  = (GtkEntry*)(gtk_builder_get_object(builder, "txt_add_prefix"));
  add_rule_dlg->txt_act_add_suffix  = (GtkEntry*)(gtk_builder_get_object(builder, "txt_add_suffix"));

  g_signal_connect(add_rule_dlg->dlg, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
  g_signal_connect(add_rule_dlg->btn_create, "clicked", G_CALLBACK(on_add_rule_btn_create), NULL);
  g_signal_connect(add_rule_dlg->btn_cancel, "clicked", G_CALLBACK(cc_ui_hide_add_rule_dialog), NULL);

  /* set up the Convert dialog */
  convert_more_dlg = g_malloc(sizeof(convert_more_dlg_t));

  convert_more_dlg->dlg = (GtkDialog*)(gtk_builder_get_object(builder, "cc_dlg_convert"));

  convert_more_dlg->btn_cancel =     (GtkButton*)(gtk_builder_get_object(builder, "ca_btn_cancel"));
  convert_more_dlg->btn_selection =  (GtkButton*)(gtk_builder_get_object(builder, "ca_btn_selection"));
  convert_more_dlg->btn_document =   (GtkButton*)(gtk_builder_get_object(builder, "ca_btn_document"));

  convert_more_dlg->opt_case_sensitive  =  (GtkCheckButton*)(gtk_builder_get_object(builder, "ca_opt_case_sensitive"));
  convert_more_dlg->opt_whole_word  =      (GtkCheckButton*)(gtk_builder_get_object(builder, "ca_opt_whole_word"));
  convert_more_dlg->opt_start_word  =      (GtkCheckButton*)(gtk_builder_get_object(builder, "ca_opt_start_word"));

  convert_more_dlg->txt_search  = (GtkEntry*)(gtk_builder_get_object(builder, "ca_txt_search"));

  g_signal_connect(convert_more_dlg->dlg, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
  g_signal_connect(convert_more_dlg->btn_cancel, "clicked", G_CALLBACK(cc_ui_hide_convert_more_dialog), NULL);
  g_signal_connect(convert_more_dlg->btn_selection, "clicked", G_CALLBACK(on_convert_more_btn_selection), NULL);
  g_signal_connect(convert_more_dlg->btn_document, "clicked", G_CALLBACK(on_convert_more_btn_document), NULL);

  /* set up the Edit Rules dialog */
  edit_rules_dlg = g_malloc(sizeof(edit_rules_dlg_t));

  edit_rules_dlg->dlg = (GtkDialog*)(gtk_builder_get_object(builder, "cc_dlg_edit_rules"));
  g_signal_connect(edit_rules_dlg->dlg, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

  edit_rules_dlg->btn_cancel  = (GtkButton*)(gtk_builder_get_object(builder, "cc_er_btn_cancel"));
  edit_rules_dlg->btn_save    = (GtkButton*)(gtk_builder_get_object(builder, "cc_er_btn_save"));

  edit_rules_dlg->cell_label = (GtkCellRendererText*)(gtk_builder_get_object(builder, "cc_er_list_label_cell"));
  g_object_set(edit_rules_dlg->cell_label, "editable", TRUE, NULL);
  g_signal_connect(edit_rules_dlg->cell_label, "edited", G_CALLBACK(on_er_label_edited), NULL);

  edit_rules_dlg->cell_enabled = (GtkCellRendererToggle*)(gtk_builder_get_object(builder, "cc_er_list_enabled_cell"));
  g_signal_connect(edit_rules_dlg->cell_enabled, "toggled", G_CALLBACK(on_er_enabled_toggled), NULL);

  g_signal_connect(edit_rules_dlg->btn_save, "clicked", G_CALLBACK(on_er_btn_save), NULL);
  g_signal_connect(edit_rules_dlg->btn_cancel, "clicked", G_CALLBACK(cc_ui_hide_edit_rules_dialog), NULL);

  /* setup keybindings */
  keybindings_set_item(plugin_key_group, KB_CONVERT_SELECTION, cc_convert_selection,
     GDK_9, GDK_CONTROL_MASK, "cc_convert_selection", _("Convert Selection"), menu_items->convert_selection);
  keybindings_set_item(plugin_key_group, KB_CONVERT_ALL, cc_convert_all,
     GDK_9, GDK_CONTROL_MASK, "cc_convert_all", _("Convert All"), menu_items->convert_all);
  keybindings_set_item(plugin_key_group, KB_CONVERT_MORE, cc_ui_show_convert_more_dialog,
     GDK_9, GDK_CONTROL_MASK, "cc_convert_more", _("Convert More"), menu_items->convert_more);

}

void cc_ui_cleanup()
{

  if (add_rule_dlg) {
    gtk_widget_destroy((GtkWidget*)add_rule_dlg->dlg);
    g_free(add_rule_dlg);
    add_rule_dlg = NULL;
  }

  if (edit_rules_dlg) {
    gtk_widget_destroy((GtkWidget*)edit_rules_dlg->dlg);
    g_free(edit_rules_dlg);
    edit_rules_dlg = NULL;
  }

  if (convert_more_dlg) {
    gtk_widget_destroy((GtkWidget*)convert_more_dlg->dlg);
    g_free(convert_more_dlg);
    convert_more_dlg = NULL;
  }

  if (menu_items) {
    g_free(menu_items);
    menu_items = NULL;
  }

  builder = NULL;

  if (main_menu_item) {
    gtk_widget_destroy(main_menu_item);
    main_menu_item = NULL;
  }
}

void cc_ui_show_add_rule_dialog()
{
  if (add_rule_dlg == NULL)
    return;

  gtk_widget_show_all((GtkWidget*)add_rule_dlg->dlg);
}

void cc_ui_hide_add_rule_dialog()
{
  if (add_rule_dlg == NULL)
    return;

  gtk_widget_hide((GtkWidget*)add_rule_dlg->dlg);
}

void cc_ui_show_edit_rules_dialog()
{
  if (edit_rules_dlg == NULL)
    return;

  gtk_widget_show_all((GtkWidget*)edit_rules_dlg->dlg);
  populate_rule_list();
}

void cc_ui_hide_edit_rules_dialog()
{
  if (edit_rules_dlg == NULL)
    return;

  gtk_widget_hide((GtkWidget*)edit_rules_dlg->dlg);
}

void cc_ui_show_convert_more_dialog()
{
  gchar   *txt;
  gint    txtsz;

  if (convert_more_dlg == NULL)
    return;

  gtk_widget_show_all((GtkWidget*)convert_more_dlg->dlg);

  /* set search value to the selected item, if any */
  txt = cc_get_selected_text(&txtsz);
  if (txt)
  {
    gtk_entry_set_text(convert_more_dlg->txt_search, txt);
  }
}

void cc_ui_hide_convert_more_dialog()
{
  if (convert_more_dlg == NULL)
    return;

  gtk_widget_hide((GtkWidget*)convert_more_dlg->dlg);
}


void on_add_rule_btn_create(G_GNUC_UNUSED GtkWidget* dlg)
{
  rule_t      *rule = cc_alloc_rule();
  condition_t *cnd = NULL;
  action_t    *act = NULL, *tmpact = NULL;

  /* TODO: validate entries before creating the rule */

  if (!rule) return;

  /* get domain */
  {
    gboolean is_c2s = gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_domain_c2s);
    rule->domain = is_c2s ? CC_RULE_C2S : CC_RULE_S2C;
  }

  /* parse the condition */
  {
    cnd = cc_alloc_cnd();
    if (!cnd)
    {
      cc_free_rule(&rule);
      return;
    }

    cnd->value = g_strdup(gtk_entry_get_text(add_rule_dlg->txt_cnd));
    rule->condition = cnd;

    cc_log("cond value: %s => %s\n", gtk_entry_get_text(add_rule_dlg->txt_cnd), cnd->value);

    if (gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_cnd_has_prefix))
      cnd->type = CC_CND_HAS_PREFIX;
    else if (gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_cnd_has_suffix))
      cnd->type = CC_CND_HAS_SUFFIX;
    else
      cnd->type = CC_CND_ALWAYS_TRUE;
  }

  /* parse the actions */
  {
    if (gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_act_rem_prefix)) {
      act = cc_alloc_act();
      act->type = CC_ACT_REM_PREFIX;
      act->value = g_strdup(gtk_entry_get_text(add_rule_dlg->txt_act_rem_prefix));
      act->next = NULL;

      rule->actions = act;
    }

    if (gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_act_rem_suffix)) {
      tmpact = act;

      act = cc_alloc_act();
      act->type = CC_ACT_REM_SUFFIX;
      act->value = g_strdup(gtk_entry_get_text(add_rule_dlg->txt_act_rem_suffix));
      act->next = NULL;

      if (tmpact) {
        tmpact->next = act;
        tmpact = act;
      }
      else {
        rule->actions = act;
      }
    }

    if (gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_act_add_prefix)) {
      tmpact = act;

      act = cc_alloc_act();
      act->type = CC_ACT_ADD_PREFIX;
      act->value = g_strdup(gtk_entry_get_text(add_rule_dlg->txt_act_add_prefix));
      act->next = NULL;

      if (tmpact) {
        tmpact->next = act;
        tmpact = act;
      }
      else {
        rule->actions = act;
      }
    }

    if (gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_act_add_suffix)) {
      tmpact = act;

      act = cc_alloc_act();
      act->type = CC_ACT_ADD_SUFFIX;
      act->value = g_strdup(gtk_entry_get_text(add_rule_dlg->txt_act_add_suffix));
      act->next = NULL;

      if (tmpact) {
        tmpact->next = act;
        tmpact = act;
      }
      else {
        rule->actions = act;
      }
    }
  } /* end of parsing actions */

  cc_add_rule(rule);
  rule = NULL;
  act = tmpact = NULL;
  cnd = NULL;

  cc_ui_hide_add_rule_dialog();
}

void on_convert_more_btn_selection(G_GNUC_UNUSED GtkWidget* dlg)
{
  ScintillaObject *sci = NULL;
  gint begin = 0;
  gint end = 0;
  const gchar *txt = NULL;
  guint txtsz = 0;
  int flags = 0;

  /* sanity check */
  if (document_get_current() == NULL) {
    return;
  }

  sci = document_get_current()->editor->sci;

  /* verify that the search text field isn't empty */
  txt = gtk_entry_get_text(convert_more_dlg->txt_search);
  txtsz = gtk_entry_get_text_length(convert_more_dlg->txt_search);

  if (!txt || txtsz == 1)
    return;

  begin = sci_get_selection_start(sci);
  end = sci_get_selection_end(sci);

  if (gtk_toggle_button_get_active((GtkToggleButton*)convert_more_dlg->opt_case_sensitive))
    flags |= SCFIND_MATCHCASE;
  if (gtk_toggle_button_get_active((GtkToggleButton*)convert_more_dlg->opt_whole_word))
    flags |= SCFIND_WHOLEWORD;
  if (gtk_toggle_button_get_active((GtkToggleButton*)convert_more_dlg->opt_start_word))
    flags |= SCFIND_WORDSTART;

  cc_convert_range(begin, end, txt, txtsz + 1, flags);
}

void on_convert_more_btn_document(G_GNUC_UNUSED GtkWidget* dlg)
{
  ScintillaObject *sci = document_get_current()->editor->sci;
  const gchar *txt = NULL;
  guint txtsz;
  int flags = 0;

  /* sanity check */
  if (document_get_current() == NULL) {
    return;
  }

  /* verify that the search text field isn't empty */
  txt = gtk_entry_get_text(convert_more_dlg->txt_search);
  txtsz = gtk_entry_get_text_length(convert_more_dlg->txt_search);

  if (!txt || txtsz == 1)
    return;

  if (gtk_toggle_button_get_active((GtkToggleButton*)convert_more_dlg->opt_case_sensitive))
    flags |= SCFIND_MATCHCASE;
  if (gtk_toggle_button_get_active((GtkToggleButton*)convert_more_dlg->opt_whole_word))
    flags |= SCFIND_WHOLEWORD;
  if (gtk_toggle_button_get_active((GtkToggleButton*)convert_more_dlg->opt_start_word))
    flags |= SCFIND_WORDSTART;

  cc_convert_range(0, sci_get_length(sci)+1, txt, txtsz + 1, flags);
}

static void on_er_enabled_toggled(G_GNUC_UNUSED GtkCellRendererToggle *cell,
                                  gchar *path_string,
                                  G_GNUC_UNUSED gpointer user_data)
{
  GtkListStore *liststore = (GtkListStore*)gtk_builder_get_object(builder, "cc_er_rules_list");
  GtkTreeIter iter;
  gboolean res = gtk_tree_model_get_iter_from_string((GtkTreeModel*)liststore, &iter, path_string);

  if (res == TRUE) {
    gboolean flag = FALSE;
    gtk_tree_model_get((GtkTreeModel*)liststore, &iter, ER_COL_ENABLED, &flag, -1);

    gtk_list_store_set(liststore, &iter, ER_COL_ENABLED, (gboolean) !flag, -1);
    cc_log("rule has been %s!\n", flag ? "disabled" : "enabled");
  }

  return;
}


static void on_er_label_edited (G_GNUC_UNUSED GtkCellRendererText *cell,
                                gchar *path_string,
                                gchar *new_text,
                                G_GNUC_UNUSED gpointer user_data)
{
  GtkListStore *liststore = (GtkListStore*)gtk_builder_get_object(builder, "cc_er_rules_list");
  GtkTreeIter iter;
  gboolean res = gtk_tree_model_get_iter_from_string((GtkTreeModel*)liststore, &iter, path_string);

  if (res == TRUE) {
    gtk_list_store_set(liststore, &iter, ER_COL_LABEL, new_text, -1);
    cc_log("rule label has been modified to '%s'!\n", new_text);
  }
}

static gboolean
traverse_rules(GtkTreeModel *model,
               G_GNUC_UNUSED GtkTreePath  *path,
               GtkTreeIter  *iter,
               G_GNUC_UNUSED gpointer      user_data)
{
  gchar *in_label;
  gint in_id;
  gboolean in_enabled;
  rule_t *rule = NULL;

  gtk_tree_model_get (model, iter,
                      ER_COL_ID, &in_id,
                      ER_COL_LABEL, &in_label,
                      ER_COL_ENABLED, &in_enabled,
                      -1);

  rule = cc_get_rule(in_id);

  if (rule) {
    cc_log("Rule (%d) '%s' => %s\n", in_id, in_label, in_enabled ? "enabled" : "disabled");
    rule->label = g_strdup(in_label);
    rule->enabled = in_enabled;
  }

  g_free(in_label);

  return FALSE; /* get next row */
}

static void on_er_btn_save()
{
  GtkListStore *liststore = (GtkListStore*)gtk_builder_get_object(builder, "cc_er_rules_list");
  gtk_tree_model_foreach(GTK_TREE_MODEL(liststore), traverse_rules, NULL);

  cc_save_settings();
}

static void populate_rule_list()
{
  rule_t *rule;
  GtkListStore *liststore = (GtkListStore*)gtk_builder_get_object(builder, "cc_er_rules_list");
  /* first, remove all entries */
  gtk_list_store_clear(liststore);

  for (rule = cc_get_rules(); rule != NULL; rule = rule->next)
  {
    GtkTreeIter   iter;

    gchar *cnd_txt = NULL, *act_txt = NULL;

    /* condition to human-readable string */
    {
      if (rule->condition->type == CC_CND_HAS_PREFIX)
      {
        cnd_txt = g_malloc(sizeof(gchar) * (strlen("Begins with ") + strlen(rule->condition->value) + 1));
        g_sprintf(cnd_txt, "Begins with %s", rule->condition->value);
      }
      else if (rule->condition->type == CC_CND_HAS_SUFFIX)
      {
        cnd_txt = g_malloc(sizeof(gchar) * (strlen("Ends with ") + strlen(rule->condition->value) + 1));
        g_sprintf(cnd_txt, "Ends with %s", rule->condition->value);
      }
      else if (rule->condition->type == CC_CND_ALWAYS_TRUE)
      {
        cnd_txt = g_strdup("Always true");
      }
      else
      {
        cnd_txt = g_strdup("Invalid!");
      }
    }

    /* actions to human-readable string */
    {
      action_t *a = NULL;
      for (a = rule->actions; a != NULL; a = a->next) {
        gchar *msg = NULL;
        switch (a->type)
        {
          case CC_ACT_ADD_PREFIX: msg = g_strdup("Add Prefix "); break;
          case CC_ACT_ADD_SUFFIX: msg = g_strdup("Add Suffix "); break;
          case CC_ACT_REM_PREFIX: msg = g_strdup("Rem Prefix "); break;
          case CC_ACT_REM_SUFFIX: msg = g_strdup("Rem Suffix "); break;
          default:
          msg = g_strdup("Invalid");
        }

        if (act_txt) {
          gchar *tmp = g_strdup(act_txt);
          act_txt = g_malloc(sizeof(gchar) * (strlen(tmp) + strlen(msg) + strlen(a->value) + 4));
          sprintf(act_txt, "%s, %s %s", tmp, msg, a->value);
          g_free(tmp);
        } else {
          act_txt = g_malloc(sizeof(gchar) * (strlen(msg) + strlen(a->value) + 2));
          sprintf(act_txt, "%s %s", msg, a->value);
        }
      }
    }

    gtk_list_store_append(liststore, &iter);
    gtk_list_store_set (liststore, &iter,
                        ER_COL_ID,      rule->id,
                        ER_COL_ENABLED, rule->enabled,
                        ER_COL_LABEL,   rule->label ? rule->label : "Unlabelled",
                        ER_COL_CND,     cnd_txt,
                        ER_COL_ACT,     act_txt,
                        -1);

    g_free(cnd_txt);
    g_free(act_txt);
  }
}
