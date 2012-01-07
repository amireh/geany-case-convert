#include "caseconvert-ui.h"
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

extern GeanyPlugin    *geany_plugin;
extern GeanyData      *geany_data;
extern GeanyFunctions *geany_functions;

static GtkBuilder     *builder = NULL;
static add_rule_dlg_t *add_rule_dlg = NULL;
static const char     *ui_file_path = "/home/kandie/Workspace/Projects/geany/caseconvert/caseconvert-ui.xml";

/* UI event handlers */
static void cc_on_btn_create(GtkWidget* dlg);
static void cc_on_btn_cancel(GtkWidget* dlg);
static void cc_on_domain_changed(GtkWidget*);
static void cc_on_cnd_changed(GtkWidget*);

void cc_ui_init()
{
  add_rule_dlg = g_malloc(sizeof(add_rule_dlg_t));

  builder = gtk_builder_new();
  GError *err = NULL;
  if (gtk_builder_add_from_file(builder, ui_file_path, &err) == 0)
  {
    g_printf("unable to read GTK UI definition\n");
    if (err->domain == GTK_BUILDER_ERROR)
      g_printf("\tcause: builder error '%s'\n", err->message);
    else if (err->domain == G_MARKUP_ERROR)
      g_printf("\tcause: markup error '%s'\n", err->message);
    else if (err->domain == G_FILE_ERROR)
      g_printf("\tcause: file error '%s'\n", err->message);
    else
      g_printf("\tcause: unknown error\n");
    
    return;
  }
  
  g_printf("read GTK UI definition successfully, binding widgets\n");
  
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
  
  g_signal_connect(add_rule_dlg->btn_create, "clicked", G_CALLBACK(cc_on_btn_create), NULL);
  g_signal_connect(add_rule_dlg->btn_cancel, "clicked", G_CALLBACK(cc_on_btn_cancel), NULL);
  
  g_signal_connect(add_rule_dlg->opt_domain_s2c, "toggled", G_CALLBACK(cc_on_domain_changed), NULL);
  g_signal_connect(add_rule_dlg->opt_domain_c2s, "toggled", G_CALLBACK(cc_on_domain_changed), NULL);
  
  g_signal_connect(add_rule_dlg->opt_cnd_has_prefix,  "toggled", G_CALLBACK(cc_on_cnd_changed), NULL);
  g_signal_connect(add_rule_dlg->opt_cnd_has_suffix,  "toggled", G_CALLBACK(cc_on_cnd_changed), NULL);
  g_signal_connect(add_rule_dlg->opt_cnd_always_true, "toggled", G_CALLBACK(cc_on_cnd_changed), NULL);
}

void cc_ui_cleanup()
{
  if (add_rule_dlg) {
    gtk_widget_destroy((GtkWidget*)add_rule_dlg->dlg);
    g_free(add_rule_dlg);
  }
  
  builder = NULL;
}

void cc_show_add_rule_dialog()
{
  gtk_widget_show((GtkWidget*)add_rule_dlg->dlg);
}

void cc_hide_add_rule_dialog()
{
  gtk_widget_hide((GtkWidget*)add_rule_dlg->dlg);
}

void cc_on_btn_cancel(GtkWidget* dlg)
{
  cc_hide_add_rule_dialog();
}

void cc_on_btn_create(GtkWidget* dlg)
{
  rule_t      *rule = cc_alloc_rule();
  condition_t *cnd = NULL;
  action_t    *act = NULL, *tmpact = NULL;
  
  
  /* get domain */
  {
    gboolean is_c2s = gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_domain_c2s);
    rule->domain = is_c2s ? c2s : s2c;
  }
  
  /* parse the condition */
  {
    cnd = g_malloc(sizeof(condition_t));
    cnd->value = g_strdup(gtk_entry_get_text(add_rule_dlg->txt_cnd));
    rule->condition = cnd;
    
    g_printf("cond value: %s => %s\n", gtk_entry_get_text(add_rule_dlg->txt_cnd), cnd->value);
    
    if (gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_cnd_has_prefix))
      cnd->type = has_prefix;
    else if (gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_cnd_has_suffix))
      cnd->type = has_suffix;
    else
      cnd->type = always_true;    
  }
  
  /* parse the actions */
  {
    if (gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_act_rem_prefix)) {
      act = g_malloc(sizeof(action_t));
      act->type = rem_prefix;
      act->value = g_strdup(gtk_entry_get_text(add_rule_dlg->txt_act_rem_prefix));
      act->next = NULL;
      
      rule->actions = act;
    }
    
    if (gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_act_rem_suffix)) {
      tmpact = act;
      
      act = g_malloc(sizeof(action_t));
      act->type = rem_suffix;
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
      
      act = g_malloc(sizeof(action_t));
      act->type = add_prefix;
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
      
      act = g_malloc(sizeof(action_t));
      act->type = add_suffix;
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
  
  cc_hide_add_rule_dialog();
}

void cc_on_domain_changed(GtkWidget* n)
{
  gboolean c2s = gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_domain_c2s);
  g_printf("domain is now set to '%s'\n", c2s ? "camel2Snake" : "snake_to_camel");
   
}

void cc_on_cnd_changed(GtkWidget* n)
{
  char type = 0;
  if (gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_cnd_has_prefix))
    type = has_prefix;
  else if (gtk_toggle_button_get_active((GtkToggleButton*)add_rule_dlg->opt_cnd_has_suffix))
    type = has_suffix;
  else
    type = always_true;
    
  g_printf("condition type is now set to '%d'\n", type);
}
