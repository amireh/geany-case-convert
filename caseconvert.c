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

#include "caseconvert.h"
#include "caseconvert-ui.h"

GeanyPlugin         *geany_plugin;
GeanyData           *geany_data;
GeanyFunctions      *geany_functions;

PLUGIN_VERSION_CHECK(211);

PLUGIN_SET_INFO("Case Converter", 
                "Converts selected text between CamelCase and snake_case",
                "1.0",
                "Ahmad Amireh <ahmad@amireh.net>");

static GtkWidget *menu_item_convert_case = NULL;
static GtkWidget *main_menu_item = NULL;
static GtkCheckMenuItem *column_mode_item;
static GtkWidget *anchor_rect_select_item;

#define FREE_LIST(item_t, list)           \
do {                                      \
  item_t *__item, *__tmpitem = NULL;      \
  for (__item = list; __item != NULL;) {  \
    __tmpitem = __item;                   \
    __item = __item->next;                \
    g_free(__tmpitem);                    \
    __tmpitem = NULL;                     \
  }                                       \
} while (0);
  

typedef struct {
  gboolean  capitalize;
  gboolean  add_prefix;
  gboolean  rem_prefix;
  gboolean  add_suffix;
  gboolean  rem_suffix;
  
  gchar     *suffix;
  gchar     *strip_suffix;
  gchar     *prefix;
  gchar     *strip_prefix;
  
  rule_t    *rules;
} config_t;

static config_t config;
static gint     rule_id;

/* Keybinding(s) */
enum {
  KB_CONVERT_CASE,
  KB_CONVERT_SELECTION,
  KB_COUNT
};
PLUGIN_KEY_GROUP(convert_case, KB_COUNT)

static void do_convert_case(void);
static gchar* to_camel(const gchar* input, size_t inputsz);
static gchar* to_snake(const gchar* input, size_t inputsz);

static void kb_convert_case(G_GNUC_UNUSED guint key_id) {
   
  /* sanity check */
  if (document_get_current() == NULL) {
    return;
  }
   
  do_convert_case();
}

static const char *pref = "";
static const char *strip_pref = "m";
static const char *suff = "_";
static const char *strip_suff = "";

void cc_add_rule(rule_t *r)
{
  action_t *act = NULL;
  rule_t   *tmprule  = NULL;
  gint i = 0;
  
  g_printf("adding rule: \n");
  g_printf("\tdomain: '%s'\n", r->domain == c2s ? "camelToSnake" : "snake_to_camel");
  g_printf("\tcondition: '%s' (%d)\n", r->condition->value, r->condition->type);
  
  g_printf("\tactions: \n");
  for (act = r->actions; act != NULL; act = act->next, ++i)
  {
    g_printf("\t\t(%d). '%s' (%d)\n", i, act->value, act->type);
  }
  
  if (!config.rules) config.rules = r;
  else {
    for (tmprule = config.rules; tmprule != NULL; tmprule = tmprule->next) {
      if (tmprule->next == NULL) {
        tmprule->next = r;
        break;
      }
    }
  }
  
}

void cc_rem_rule(rule_t *r)
{
}

static void on_convert_selection_activated(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED
	gpointer gdata)
{
  g_printf("in convert_selection_activated()\n");
}

static void on_convert_selection_key(G_GNUC_UNUSED guint key_id)
{
	g_printf("in convert_selection_key()\n");
}

static void on_add_rule_activated(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED
	gpointer gdata)
{
  g_printf("in add_rule_activated()\n");
  cc_show_add_rule_dialog();

}

static void on_edit_rules_activated(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED
	gpointer gdata)
{
  g_printf("in edit_rules_activated()\n");
}


void plugin_init(GeanyData *data)
{
  /* init gettext and friends */
  //~ main_locale_init(LOCALEDIR, "geany-plugins");
   
  /* setup keybindings */
  keybindings_set_item(plugin_key_group, KB_CONVERT_CASE, kb_convert_case,
     GDK_9, GDK_CONTROL_MASK, "convert_case", _("Convert Case"), menu_item_convert_case);  
     
  config.capitalize = FALSE;
  
  config.add_prefix = FALSE;
  config.add_suffix = TRUE;
  config.rem_prefix = FALSE;
  config.rem_suffix = FALSE;
  
  config.prefix = (char*)pref;
  config.strip_prefix = (char*)strip_pref;
  config.suffix = (char*)suff;
  config.strip_suffix = (char*)strip_suff;
  
  /*rule_t* rule = g_malloc(sizeof(rule_t));
  rule->domain = c2s;
  
  rule->condition = g_malloc(sizeof(condition_t));
  rule->condition->type = always_true;
  rule->condition->value = " ";
  
  action_t *act = g_malloc(sizeof(action_t));
  act->type = add_prefix;
  act->value = "get";
  act->next = NULL;
  rule->actions = act;

  
  rule->condition->type = has_prefix;
  rule->condition->value = "m";
  
  action_t *act = g_malloc(sizeof(action_t));
  act->type = add_suffix;
  act->value = "_";
  rule->actions = act;
  
  act = g_malloc(sizeof(action_t));
  act->type = rem_prefix;
  act->value = "m";
  act->next = NULL;
  
  rule->actions->next = act;
    
  rule->next = NULL;
  
  config.rules = rule;*/

    
  GtkContainer *menu = NULL;
  GtkWidget *item = NULL;

	item = gtk_menu_item_new_with_mnemonic(_("Con_vert Case"));
	main_menu_item = item;
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), item);
  menu = GTK_CONTAINER(gtk_menu_new());
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), GTK_WIDGET(menu));
  
	item = gtk_menu_item_new_with_mnemonic(_("_Convert Selection"));
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(on_convert_selection_activated), NULL);
	keybindings_set_item(plugin_key_group, KB_CONVERT_SELECTION, on_convert_selection_key, 0, 0,
		"convert_selection", _("Convert Selection"), item);

	item = gtk_menu_item_new_with_mnemonic(_("_Add Rule"));
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(on_add_rule_activated), NULL);

	item = gtk_menu_item_new_with_mnemonic(_("_Edit Rules"));
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(on_edit_rules_activated), NULL);
      
  gtk_widget_show_all(main_menu_item);
  
  cc_ui_init();
  
  g_printf("convert_case: adding prefix '%s' to camelCase strings\n", config.prefix);
}

void plugin_cleanup(void)
{
  /* free rules, their actions and conditions */
  rule_t      *rule, *tmprule = NULL;
  action_t    *act, *tmpact = NULL;
  condition_t *cond = NULL;
  
  for (rule = config.rules; rule != NULL;) {
    tmprule = rule;
    rule = rule->next;
    cc_free_rule(&tmprule);
  }
  
  gtk_widget_destroy(main_menu_item);
  
  cc_ui_cleanup();
}

typedef struct token_t token_t;

struct token_t {
  gchar           *value;
  size_t          valsz;
  token_t         *next;
};

typedef struct {
  token_t*  tokens;
  size_t    nr_tokens;
} expr_t;

static gboolean is_snake(gchar const* in, size_t insz)
{
  gint i;
  /* a string is considered snake_cased if there's any single underscore in the middle
   * of the string */
  for (i = 0; i < insz; ++i) {
    if (in[i] == '_') {
      if (i+1 < insz && in[i+1] == '_') {
        /* skip consecutive underscores */
        while (i+1 < insz && in[i+1] == '_') ++i;
        continue;
      }
      return TRUE;
    }
  }
  return FALSE;
}

static gboolean is_camel(gchar const* in, size_t insz)
{
  gint i;
  
  /* a string is considered camelCased if any upper case letter is preceded
   * or followed by a lower one */
  for (i = 0; i < insz; ++i) {
    if (isupper(in[i]) && 
        ((i+1 < insz && !isupper(in[i+1]))
         ||
         (i > 0 && !isupper(in[i-1])))
       )
    {
      return TRUE;
    }
  }
  return FALSE;
}

static gchar* convert_text(gchar const* in, size_t insz)
{
  /* iterators */
  gint    i = 0;       /* used to iterate tmp buffers */
  guint   cursor = 0;  /* used to iterate tmp buffers */
  guint   icursor = 0; /* used to iterate the input buffer */
  guint   ocursor = 0; /* used to iterate the out buffer */
  size_t  begin = 0;
  size_t  end = insz;

  /* flags */
  gboolean to_camel = FALSE; /* converting to CamelCase? (s2c) */
  gboolean to_snake = FALSE; /* converting to snake_case? (c2s) */
  gboolean upcasing = FALSE; /* used in s2c conversion */
  
  /* buffers and sizes */
  gchar *input    = NULL;   /* the transformed input buffer */
  gchar *out      = NULL;   /* the final result buffer */
  gchar *tmp      = NULL;   /* a temp buffer */
  size_t inputsz  = 0;      /* the size of the transformed input */
  size_t outsz    = 0;      /* the size of the result string */
  
  /* used for snake2camel conversion */
  expr_t  *expr = NULL;
  token_t *tok = NULL;
  token_t *tmptok = NULL;
  
  rule_t      *rule = NULL;
  action_t    *act = NULL;
  gboolean    rule_met = FALSE;
  const gchar *prefix = NULL;
  const gchar *suffix = NULL;
  
  /* are we converting to CamelCase? */
  if (is_snake(in, insz)) {
    to_camel = TRUE;
    
    g_printf("input '%s' is snake_cased\n", in);
  } 
  /* to snake_case? */
  else if (is_camel(in, insz)) { 
    to_snake = TRUE;
    
    g_printf("input '%s' is camelCased\n", in);
  }
  /* unable to identify the string case, abort */
  else {
    return NULL;
  }
  
  /* find any matching rule and perform its transformation actions */
  for (rule = config.rules; rule != NULL; rule = rule->next)
  {
    /* first verify that the condition applies to this domain */
    if (rule->domain == s2c && !to_camel) continue;
    else if (rule->domain == c2s && !to_snake) continue;
    
    switch (rule->condition->type)
    {
      case has_prefix:
        if (g_str_has_prefix(in, rule->condition->value)) { rule_met = TRUE; break; }
      break;
      case has_suffix:
        if (g_str_has_suffix(in, rule->condition->value)) { rule_met = TRUE; break; }
      break;
      case always_true: rule_met = TRUE; break;
    }
    
    if (rule_met)
    {
      g_printf("rule has been met! (%d) => (%s)\n", rule->condition->type, rule->condition->value);
      
      /* perform the rule actions */
      for (act = rule->actions; act != NULL; act = act->next)
      {
        switch (act->type)
        {
          /* converting to snake_case will cost an additional '_' for prefixes and suffixes */
          case add_prefix: 
            prefix = act->value;
            inputsz += strlen(prefix) + (to_snake ? 1 : 0);
            g_printf("adding prefix '%s'\n", prefix);
          break;
          case add_suffix: 
            suffix = act->value;
            inputsz += strlen(suffix) + (to_snake ? 1 : 0);
          break;
          
          case rem_prefix: 
            begin += strlen(act->value);
          break;
          case rem_suffix: 
            end -= strlen(act->value);
          break;
          default: ;
        }
      }
      
      break;
    }
  }
  
  /* nothing left to convert? :D */
  if (end == 0) {
    return NULL;
  }
  
  /* clone the stripped input */
  tmp = g_malloc(sizeof(gchar) * (end + 1));
  memset(tmp, 0, sizeof(gchar) * (end + 1));
  i = 0;
  for (cursor = begin; cursor < end; ++cursor, ++i) {
    tmp[i] = in[cursor];
  }
  tmp[cursor] = '\0';
  inputsz += cursor;

  /* create the transformed input */
  input = g_malloc(sizeof(gchar) * inputsz);
  memset(input, 0, sizeof(gchar) * inputsz);
  snprintf(input, inputsz, "%s%s%s", 
    (prefix ? prefix : ""), 
    tmp,
    (suffix ? suffix : ""));

  g_free(tmp);
  tmp = NULL;
  
  if (to_camel)
  {
    /* convert snake_case => [prefix][c|C]amelCase[suffix]
     * 
     * case 1:    "foo_bar"      => "fooBar"
     * case 2:    "foo_bar"      => "FooBar"       (capitalize)
     * case 3:    "singleton"    => "getSingleton" (add prefix)
     * case 4:    "member_"      => "mMember"      (remove suffix, add prefix)
     * case 5:    "foo_BAR"      => "fooBAR"       (maintain consecutive case)
     * case 6.a:  "foo_BAR_zoo"  => "fooBARZoo"    (be smart about #5)
     * case 6.b   "foo_bAR_zoo"  => "fooBARZoo"    
     * case 7:    "foo_bar__"    => "fooBar__"     (consecutive '_' are untouched)
     */
    
    /* capitalize the word? */
    if (config.capitalize) {
      upcasing = 1;
    }
    
    /* if a prefix was added, we need to capitalize the first original letter */
    if (prefix)
    {
      size_t pos = strlen(prefix);
      input[pos] = toupper(input[pos]);
    }
    
    /* the resulting buffer will be at most the original buffer size */
    tmp = g_malloc(sizeof(char) * (inputsz + 1));
    memset(tmp, 0, sizeof(char) * (inputsz + 1));
    
    for (icursor = 0; icursor < inputsz; ++icursor)
    {
      /* case 7: write any consecutive underscores without modification */
      if (input[icursor] == '_' && ((icursor + 1 < inputsz) && (input[icursor+1] == '_'))) { 
        tmp[ocursor++] = input[icursor];
        while ((icursor + 1 < inputsz) && (input[icursor+1] == '_'))
          tmp[ocursor++] = input[++icursor];
        upcasing = 0;
        continue;
      }
      
      /* otherwise, if it's a single underscore, skip it and upcase next */
      if (input[icursor] == '_') {
        upcasing = 1;
        continue;
      }
      
      if (upcasing) {
        tmp[ocursor++] = toupper(input[icursor]);
        upcasing = 0;
        continue;
      }
            
      tmp[ocursor++] = input[icursor];
    }
    
    outsz = ocursor;
    out = g_malloc(sizeof(gchar) * (outsz + 1));
    snprintf(out, outsz + 1, "%s", tmp);
    g_free(tmp);
    tmp = NULL;
    
    g_printf("converted from snake_case '%s' into camelCase '%s'\n", input, out);
  }
  else
  {
    /* convert camelCase => snake_case */
    
    tmp = g_malloc(sizeof(char) * (inputsz + 1));
    memset(tmp, 0, sizeof(char) * (inputsz + 1));
    
    expr = g_malloc(sizeof(expr_t));
    expr->tokens = NULL;
    expr->nr_tokens = 0;
    
    tok = g_malloc(sizeof(token_t));
    tok->value = NULL;
    tok->valsz = 0;
    tok->next = NULL;
    
    expr->tokens = tok;
    ++expr->nr_tokens;
    
    char currc = input[icursor];
    char nextc = currc;
    char prevc;
    int isupper_next = 0;
    for (icursor = 0; icursor < inputsz; ++icursor)
    {
      prevc = currc;
      currc = input[icursor];
      if (icursor + 1 < inputsz)
        nextc = input[icursor+1];
      
      /* case 1: "fooBar" => foo_bar
       * 
       * if the cursor points to an uppercase, we peek 1 char behind, if it's
       * lower, then a new token is defined
       */
      gboolean testCase = (icursor > 1 && !isupper(input[icursor-1]) && isupper(currc) && tok->valsz > 0);
      
      /* case 2: "fooBARzoo" => foo_bar_zoo
       * 
       * if the cursor points to a _lowercase_, we check if it is preceded
       * by a number of consecutive CAPITALS, if it is, a new token is defined
       */
      gboolean testCASEtwo = (icursor > 2 && !isupper(currc) && isupper(input[icursor-1]) && isupper(input[icursor-2]));
      
      if (testCase || testCASEtwo)
      {
        /* extract the token value and reset the buffer */
        tok->value = g_malloc(sizeof(gchar) * (tok->valsz + 1));
        memset(tok->value, 0, sizeof(gchar) * (tok->valsz + 1));
        g_snprintf(tok->value, tok->valsz + 1, "%s", tmp);
        memset(tmp, 0, sizeof(gchar) * (inputsz + 1));
        
        /* define the next token */
        tmptok = g_malloc(sizeof(token_t));
        tmptok->value = NULL;
        tmptok->valsz = 0;
        tmptok->next = NULL;
        
        /* and link it */
        tok->next = tmptok;
        tok = tmptok;
        tmptok = NULL;
        ++expr->nr_tokens;        
      }
      
      /* parse token */
      tmp[tok->valsz] = isupper(currc) ? tolower(currc) : currc;
      ++tok->valsz;
    }
    
    /* assign the trailing data to the last token */
    if (!tok->value && tok->valsz > 0) {
      tok->value = g_malloc(sizeof(char) * (tok->valsz + 1));
      memset(tok->value, 0, sizeof(char) * (tok->valsz + 1));
      g_snprintf(tok->value, tok->valsz, "%s", tmp);
      memset(tmp, 0, inputsz + 1);      
    }
    
    g_printf("extracted (%d) tokens from expr '%s'\n", expr->nr_tokens, input);
    
    g_free(tmp);
    
    /* now join the tokens and delimit them by '_' except for the last */
    outsz = inputsz + expr->nr_tokens - 1;
    out = g_malloc(sizeof(gchar) * (outsz + 1));
    memset(out, 0, sizeof(gchar) * (outsz + 1));
    
    ocursor = 0;
    tok = NULL;
    for (tok = expr->tokens; tok != NULL;) {
      for (i = 0; i < tok->valsz; ++i)
      {
        out[ocursor+i] = tok->value[i];
      }
      ocursor += tok->valsz;
      
      if (tok->next)
        out[ocursor++] = '_';
        
      g_printf("out buf now has '%s' (%d), wrote '%s' (%d)\n", out, ocursor, tok->value, tok->valsz);

      tmptok = tok;      
      tok = tok->next;
      g_free(tmptok);
      tmptok = NULL;
    }
    
    out[ocursor] = '\0';
    expr->tokens = NULL;
    g_free(expr);
    expr = NULL;
  }
  
  /* ... */

  return out;
}

static void do_convert_case(void)
{
  printf("in convert_case()\n");
  
  gchar *txt;
  gint txt_len;

  gint startline;
  gint endline;
  gint line_iter;
  gint linepos;
  gint linelen;

  gint startcol;
  gint endcol;
  
  gint i;
     
  gint startpos;
  gint endpos;
    
  ScintillaObject *sci;

  /* get a pointer to the scintilla object */
  sci = document_get_current()->editor->sci;  
  
  if (sci_has_selection(sci)) {

    startpos = sci_get_selection_start(sci);
    endpos = sci_get_selection_end(sci);
  
    /* sanity check -- we dont care which way the block was selected */
    if(startpos > endpos) {
      i = endpos;
      endpos = startpos;
      startpos = i;
    }

    startline = sci_get_line_from_position(sci, startpos);
    endline = sci_get_line_from_position(sci, endpos);

    /* normal mode */
    // if(sci_get_selection_mode(sci) == 1){
    if(startline == endline) {

      /* get the text in question */
      txt_len = endpos - startpos;
      txt = g_malloc(txt_len + 1);
      sci_get_selected_text(sci, txt);

      /* put the new text in */
      gchar *repl = convert_text(txt, sci_get_selected_text_length(sci));
      if (repl) {
        sci_replace_sel(sci, repl);
        g_free(repl);
      }
      /*sci_set_selection_start(sci, startpos - 1);*/
      /*sci_set_selection_end(sci, endpos + 1);*/

      g_printf("got some selected text '%s'\n", txt);
      
      g_free(txt);
      
      }
   }
}

rule_t* cc_alloc_rule()
{
  rule_t* r = g_malloc(sizeof(rule_t));
  r->condition = NULL;
  r->actions = NULL;
  r->next = NULL;
  r->id = ++rule_id;
  r->label = NULL;
  
  return r;
}

void cc_free_rule(rule_t** in)
{
  rule_t* r = *in;
  action_t *act = NULL, *tmpact = NULL;
  
  for (act = r->actions; act != NULL;) {
    tmpact = act;
    act = act->next;
    g_free(tmpact->value);
    g_free(tmpact);
    tmpact = NULL;
  }
    
  if (r->condition) {
    g_free(r->condition->value);
    g_free(r->condition);
  }
  
  if (r->label) {
    g_free(r->label);
  }
  
  g_free(r);
  
  r = NULL;
  in = NULL;
}
