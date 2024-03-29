/*
 *  caseconvert.c
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

#include "caseconvert.h"
#include "caseconvert_ui.h"
#include "Scintilla.h"
#include <geany/search.h>
#include <stdlib.h>

GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

PLUGIN_VERSION_CHECK(147)

PLUGIN_SET_INFO("Case Converter",
                "Converts text between CamelCase and snake_case",
                "1.0",
                "Ahmad Amireh <ahmad@amireh.net>")

/* helper for freeing a list */
#define CC_LIST_FREE(list, item_t)        \
  item_t *__item, *__tmp_item = NULL;     \
  for (__item = list; __item != NULL;)    \
  {                                       \
    __tmp_item = __item;                  \
    __item = __item->next;                \
    g_free(__tmp_item);                   \
    __tmp_item = NULL;                    \
  }

static config_t config;

/* tokens are generated by splitting a string using a delimiter */
typedef struct token_t token_t;
struct token_t {
  gchar   *value;
  size_t  valsz;
  token_t *next;
};

/* returns a set of tokens by splitting "str" using "delim" */
static token_t* tokenize(gchar *str, gchar delim, int *nr_tokens)
{
  token_t *first = NULL, *tok = NULL, *tmptok = NULL;
  gint    bufsz = strlen(str);
  gint    i;
  gint    toksz = 0;
  *nr_tokens    = 0;

  for (i = 0; i < bufsz; ++i)
  {
    if (str[i] == delim || (i + 1 == bufsz && ++i && ++toksz))
    {
      /* define token */
      tok = g_malloc(sizeof(token_t));
      tok->next = NULL;
      tok->valsz = toksz;
      tok->value = g_strndup(&str[i-toksz], toksz);

      if (tmptok)
      {
        tmptok->next = tok;
      }

      if (!first) first = tok;

      tmptok = tok;
      toksz = 0;

      ++(*nr_tokens);

      continue;
    }

    ++toksz;
  }

  return first;
}

#ifdef VERBOSE
  void cc_log(const char *fmt, ...)
  {
    va_list args;
    char *msg = NULL;
    va_start(args, fmt);
    g_vasprintf(&msg, fmt, args);
    va_end(args);

    g_printf(msg);
    free(msg);
  }
#else
  void cc_log(G_GNUC_UNUSED const char *fmt, ...)
  {
  }
#endif

void cc_add_rule(rule_t *r)
{
  action_t *act = NULL;
  rule_t   *tmprule  = NULL;
  gint i = 0;

  cc_log("adding rule: \n");
  cc_log("\tlabel: '%s'\n", r->label ? r->label : "Unlabelled");
  cc_log("\tdomain: '%s'\n", r->domain == CC_RULE_C2S ? "camelToSnake" : "snake_to_camel");
  cc_log("\tcondition: '%s' (%d)\n", r->condition->value, r->condition->type);

  cc_log("\tactions: \n");
  for (act = r->actions; act != NULL; act = act->next, ++i)
  {
    cc_log("\t\t(%d). '%s' (%d)\n", i, act->value, act->type);
  }

  if (!config.rules) {
    /* first rule */
    config.rules = r;
  }
  else {
    /* append to the end of the rule list */
    for (tmprule = config.rules; tmprule != NULL; tmprule = tmprule->next) {
      if (tmprule->next == NULL) {
        tmprule->next = r;
        break;
      }
    }
  }
}

void cc_rem_rule(G_GNUC_UNUSED gint id)
{
}

void plugin_init(G_GNUC_UNUSED GeanyData *data)
{
  config.capitalize = FALSE;
  config.rules = NULL;

  cc_ui_init();
  cc_load_settings();
}

void plugin_cleanup(void)
{
  rule_t *rule, *tmprule = NULL;

  /* save rules */
  cc_save_settings();

  /* free rules, their actions and conditions */
  for (rule = config.rules; rule != NULL;) {
    tmprule = rule;
    rule = rule->next;
    cc_free_rule(&tmprule);
  }

  rule = tmprule = NULL;

  /* free up the UI resources */
  cc_ui_cleanup();
}

static gboolean is_snake(gchar const* in, size_t insz)
{
  guint i;
  gboolean has_lc = FALSE; /* any lowercase letters ? */

  /* a string is considered snake_cased if there's any single underscore in the middle
   * of the string and it contains lowercase letter(s) */
  for (i = 0; i < insz; ++i) {
    if (in[i] == '_') {
      if (i+1 < insz && in[i+1] == '_') {
        /* skip consecutive underscores */
        while (i+1 < insz && in[i+1] == '_') ++i;
        continue;
      }

      if (has_lc)
        return TRUE;
    }
    else if (islower(in[i])) has_lc = TRUE;
  }
  return FALSE;
}

static gboolean is_camel(gchar const* in, size_t insz)
{
  guint i;

  /* a string is considered camelCased if any upper case letter is preceded
   * or followed by a lower one */
  for (i = 0; i < insz; ++i) {
    if (isupper(in[i])
    && ((i+1 < insz && isalpha(in[i+1]) && !isupper(in[i+1])) || (i > 0 && isalpha(in[i-1]) && !isupper(in[i-1]))))
    {
      return TRUE;
    }
  }
  return FALSE;
}

static gchar* do_convert(gchar const* in, size_t insz, gint *outsz)
{
  typedef struct {
    token_t* tokens;
    size_t   nr_tokens;
  } expr_t;

  /* iterators */
  guint   i = 0;       /* used to iterate tmp buffers */
  guint   cursor = 0;  /* used to iterate tmp buffers */
  guint   icursor = 0; /* used to iterate the input buffer */
  guint   ocursor = 0; /* used to iterate the out buffer */
  size_t  begin = 0;
  size_t  end = insz;

  /* flags */
  gboolean to_camel = FALSE; /* converting to CamelCase? (CC_RULE_S2C) */
  gboolean to_snake = FALSE; /* converting to snake_case? (CC_RULE_C2S) */
  gboolean upcasing = FALSE; /* used in CC_RULE_S2C conversion */

  /* buffers and sizes */
  gchar *input    = NULL;   /* the transformed input buffer */
  gchar *out      = NULL;   /* the final result buffer */
  gchar *tmp      = NULL;   /* a temp buffer */
  gchar c         = 0;      /* a character iterator */
  size_t inputsz  = 0;      /* the size of the transformed input */
  /*size_t outsz    = 0;*/      /* the size of the result string */

  /* used for snake2camel conversion */
  expr_t  *expr = NULL;
  token_t *tok = NULL;
  token_t *tmptok = NULL;
  gboolean tCase, testCase, testCASETwo; /* see below */

  rule_t      *rule = NULL;
  action_t    *act = NULL;
  gboolean    rule_met = FALSE;
  const gchar *prefix = NULL;
  const gchar *suffix = NULL;

  /* are we converting to CamelCase? */
  if (is_snake(in, insz)) {
    to_camel = TRUE;

    cc_log("input '%s' is snake_cased\n", in);
  }
  /* to snake_case? */
  else if (is_camel(in, insz)) {
    to_snake = TRUE;

    cc_log("input '%s' is camelCased\n", in);
  }
  /* unable to identify the string case, abort */
  else {
    return NULL;
  }

  /* find any matching rule and perform its transformation actions */
  for (rule = config.rules; rule != NULL; rule = rule->next)
  {
    if (!rule->enabled) continue;

    /* verify that the condition applies to this domain */
    if (rule->domain == CC_RULE_S2C && !to_camel) continue;
    else if (rule->domain == CC_RULE_C2S && !to_snake) continue;
    else if (rule->domain == CC_RULE_NULL) {
      cc_log("WARN: invalid rule domain (%d => '%s')!\n", rule->id, rule->label);
      continue;
    }

    switch (rule->condition->type)
    {
      case CC_CND_HAS_PREFIX:
        if (g_str_has_prefix(in, rule->condition->value)) { rule_met = TRUE; break; }
      break;
      case CC_CND_HAS_SUFFIX:
        if (g_str_has_suffix(in, rule->condition->value)) { rule_met = TRUE; break; }
      break;
      case CC_CND_ALWAYS_TRUE: rule_met = TRUE; break;
      default: rule_met = FALSE;
    }

    if (rule_met)
    {
      cc_log("rule has been met! (%d) => (%s)\n", rule->condition->type, rule->condition->value);

      /* perform the rule actions */
      for (act = rule->actions; act != NULL; act = act->next)
      {
        switch (act->type)
        {
          /* converting to snake_case will cost an additional '_' for prefixes and suffixes */
          case CC_ACT_ADD_PREFIX:
            prefix = act->value;
            inputsz += strlen(prefix) + (to_snake ? 1 : 0);
            cc_log("adding prefix '%s'\n", prefix);
          break;
          case CC_ACT_ADD_SUFFIX:
            suffix = act->value;
            inputsz += strlen(suffix) + (to_snake ? 1 : 0);
          break;

          case CC_ACT_REM_PREFIX:
            begin += strlen(act->value);
          break;
          case CC_ACT_REM_SUFFIX:
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
  g_snprintf(input, inputsz, "%s%s%s",
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

    *outsz = ocursor;
    out = g_malloc(sizeof(gchar) * (*outsz + 1));
    g_snprintf(out, *outsz + 1, "%s", tmp);
    g_free(tmp);
    tmp = NULL;

    cc_log("converted from snake_case '%s' into camelCase '%s'\n", input, out);
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

    c = input[icursor];
    for (icursor = 0; icursor < inputsz; ++icursor)
    {
      c = input[icursor];

      tCase = (icursor == 0 && icursor + 1 < inputsz && islower(c) && isupper(input[icursor+1]));

      /* case 2: "fooBar" => foo_bar
       *
       * if the cursor points to an uppercase, we peek 1 char behind, if it's
       * lower, then a new token is defined
       */
      testCase = (icursor > 1 && !isupper(input[icursor-1]) && isupper(c) && tok->valsz > 0);

      /* case 3: "mCURLObj" => m_curl_obj
       *
       * if the cursor points to an uppercase, the last char is an upper as well,
       * and the next is a lower case char
       */
      testCASETwo =
        (icursor > 1 && icursor +1 < inputsz && isupper(c) && isupper(input[icursor-1]) && islower(input[icursor+1]));

      if (tCase || testCase || testCASETwo)
      {
        if (tCase) {
          tmp[0] = c;
          ++tok->valsz;
        }

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

        if (tCase) continue;
      }

      /* parse token */
      tmp[tok->valsz] = isupper(c) ? tolower(c) : c;
      ++tok->valsz;
    }

    /* assign the trailing data to the last token */
    if (!tok->value && tok->valsz > 0) {
      tok->value = g_malloc(sizeof(char) * (tok->valsz + 1));
      memset(tok->value, 0, sizeof(char) * (tok->valsz + 1));
      g_snprintf(tok->value, tok->valsz, "%s", tmp);
      memset(tmp, 0, inputsz + 1);
    }

    cc_log("extracted (%d) tokens from expr '%s'\n", (int)expr->nr_tokens, input);

    g_free(tmp);

    /* now join the tokens and delimit them by '_' except for the last */
    *outsz = inputsz + expr->nr_tokens - 1;
    out = g_malloc(sizeof(gchar) * (*outsz + 1));
    memset(out, 0, sizeof(gchar) * (*outsz + 1));

    ocursor = 0;
    tok = NULL;
    for (tok = expr->tokens; tok != NULL; ) {
      for (i = 0; i < tok->valsz; ++i)
      {
        out[ocursor+i] = tok->value[i];
      }
      ocursor += tok->valsz;

      if (tok->next)
        out[ocursor++] = '_';

      cc_log("out buf now has '%s' (%d), wrote '%s' (%d)\n", out, ocursor, tok->value, (int)tok->valsz);

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

/* cc_get_selected_text():
 * helper for returning the selected text in the editor if the selection
 * is a single-line
 *
 * @note
 * if there is a selection, the returned string must be freed by the caller
 */
gchar* cc_get_selected_text(gint* sz)
{
  gchar *txt = NULL;
  gint  txtsz = *sz = 0;

  gint i = 0;

  /* selection cursors */
  gint startpos = 0;
  gint endpos = 0;

  /* used to check whether the selection is single or multi-line */
  gint startline = 0;
  gint endline = 0;

  /* get a pointer to the scintilla object */
  ScintillaObject *sci = document_get_current()->editor->sci;

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
    if (startline == endline) {

      /* get the text in question */
      txtsz = endpos - startpos;
      txt = g_malloc(txtsz + 1);
      sci_get_selected_text(sci, txt);

      *sz = txtsz + 1;
      return txt;
    }
  }

  return NULL;
}

/* captures the selected block of text in the Scintilla object and converts its case */
void cc_convert_selection(void)
{
  gint  txtsz = 0;
  gchar *repl = NULL;
  gint  replsz = 0;
  gchar *txt = cc_get_selected_text(&txtsz);
  /* get a pointer to the scintilla object */
  ScintillaObject *sci = document_get_current()->editor->sci;

  if (txt) {
    /* put the new text in */
    repl = do_convert(txt, txtsz, &replsz);
    if (repl) {
      sci_replace_sel(sci, repl);
      g_free(repl);
    }

    g_free(txt);
  }

  txt = repl = NULL;
}

#ifndef SSM
# define SSM(m, w, l) scintilla_send_message(sci, m, w, l)
#endif

/* converts the case of all occurences of the given text in the specified range */
void cc_convert_range(int r_begin, int r_end, gchar const *txt, gint txtsz, int flags)
{
  gchar *repl = NULL;
  gint  replsz = 0;
  /* get a pointer to the scintilla object */
  ScintillaObject *sci = document_get_current()->editor->sci;

  /* if no text was given, try to see if there's a selection */
  if (!txt) {
    txt = cc_get_selected_text(&txtsz);

    if (!txt) /* there isn't, abort */
      return;
  }

  /* get the converted version */
  repl = do_convert(txt, txtsz, &replsz);

  if (!repl) {
    return;
  }

  cc_log("converting '%s'(%d) to '%s'(%d) in [%d..%d]\n", txt, txtsz, repl, replsz, r_begin, r_end);

  sci_start_undo_action(sci);

  SSM(SCI_SETSEARCHFLAGS, flags, 0);
  SSM(SCI_SETTARGETSTART, r_begin, 0);
  SSM(SCI_SETTARGETEND,   r_end + 1, 0);

  while (SSM(SCI_SEARCHINTARGET, txtsz, (uptr_t)txt) != -1) {
    SSM(SCI_REPLACETARGET, -1, (uptr_t)repl);
    SSM(SCI_SETTARGETSTART, r_begin, 0);
    SSM(SCI_SETTARGETEND,   r_end + 1, 0);
  }

  SSM(SCI_SETSEARCHFLAGS, 0, 0);

  sci_end_undo_action(sci);

  g_free(repl);

  repl = NULL;
}

void cc_convert_all()
{
  ScintillaObject *sci = document_get_current()->editor->sci;

  /* sanity check */
  if (document_get_current() == NULL) {
    return;
  }

  cc_convert_range(0, sci_get_length(sci)+1, NULL, 0, 0);
}


rule_t* cc_get_rule(gint id)
{
  rule_t *iter = NULL;
  for (iter = config.rules; iter != NULL; iter = iter->next)
    if (iter->id == id) return iter;

  return NULL;
}

rule_t* cc_get_rules()
{
  return config.rules;
}

static gchar *cfg_file;

void cc_load_settings(void)
{
	GKeyFile *cfg = g_key_file_new();

  /* the following variables are used for parsing rule definitions */
  gchar     *rules = NULL;  /* rule definitions stored in cfg file */
  gint      bufsz = 0;      /* length of rule definitions buffer */
  gint      i = 0, x = 0;
  gchar     c = 0;

	cfg_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"caseconvert", G_DIR_SEPARATOR_S, "caseconvert.conf", NULL);
	g_key_file_load_from_file(cfg, cfg_file, G_KEY_FILE_NONE, NULL);

  config.capitalize = g_key_file_get_boolean(cfg, "caseconvert", "capitalize", NULL);
	rules = utils_get_setting_string(cfg, "caseconvert", "rules", "");
  bufsz = strlen(rules);

  /* parse rules */
  for (i = 0; i < bufsz; ++i) {
    rule_t    *r = NULL;
    gboolean  bracket_found = FALSE; /* used for validating a rule definition */
    token_t   *tokens = NULL;
    token_t   *tok = NULL;
    gint      nr_tokens = 0;
    gchar     *rbuf = NULL;
    gint      rbufsz = 0;  /* length of a single rule definition */

    c = rules[i];

    /* rule format:
     * [id,label,enabled,domain,cnd_type,cnd_val,act1_type,act1_val,...,actN_type,actN_val]
     */
    if (c == '[') {
      /* begin new rule definition */
      bracket_found = FALSE;
      rbufsz = 0;

      for (x = i+1; x < bufsz; ++x)
      {
        if (rules[x] == ']') {
          rbuf = g_strndup(&rules[i+1], x - i - 1);
          rbufsz = strlen(rbuf);
          cc_log("tokenizing %s\n", rbuf);
          tokens = tokenize(rbuf, ',', &nr_tokens);
          g_free(rbuf);
          rbuf = NULL;

          bracket_found = TRUE;
          break;
        }
      }

      if (!bracket_found || !tokens)
      {
        /* abort */
        break;
      }

      i += rbufsz;
    }

    if (!tokens) continue;

    /* validate the number of tokens, can't be less than 8 */
    if (nr_tokens < 8 || nr_tokens % 2 != 0) {
      cc_log("warn: number of tokens in rule defintion is invalid: %d, expected an even number GE than 8\n", nr_tokens);
      continue;
    }

    tok = tokens;
    r = cc_alloc_rule();
    if (!r) {
      break;
    }

    /* parse ID */
    r->id = atoi(tok->value);
    cc_log("rule id: %d\n", r->id);
    tok = tok->next;

    /* parse label */
    r->label = g_strdup(tok->value);
    cc_log("rule label: %s\n", r->label);
    tok = tok->next;

    /* parse "enabled" flag */
    r->enabled = atoi(tok->value);
    cc_log("rule enabled? %s(%d)\n", r->enabled ? "yes" : "no", r->enabled);
    tok = tok->next;

    /* parse "enabled" flag */
    r->domain = atoi(tok->value);
    cc_log("rule domain? %s(%d)\n", r->domain == 0 ? "CC_RULE_C2S" : "CC_RULE_S2C", r->domain);
    tok = tok->next;

    /* rule condition now: */
    {
      condition_t *cnd = cc_alloc_cnd();
      if (!cnd)
      {
        cc_free_rule(&r);
        break;
      }

      r->condition = cnd;

      /* parse the type */
      cnd->type = atoi(tok->value);
      cc_log("\tcondition type => %d\n", cnd->type);
      tok = tok->next;

      cnd->value = g_strdup(tok->value);
      cc_log("\tcondition value => %s\n", cnd->value);
      tok = tok->next;
      cnd = NULL;
    }

    /* rule actions now */
    {
      gint offset = 6;
      gint nr_actions = (nr_tokens - offset) / 2;
      action_t  *act = NULL, *tmpact = NULL;

      for (x = 0; x < nr_actions; ++x)
      {
        act = cc_alloc_act();
        if (!act)
        {
          /* abort */
          cc_free_rule(&r);
          g_free(rules);
          g_key_file_free(cfg);
          return;
        }

        /* parse the type */
        act->type = atoi(tok->value);
        cc_log("\taction type => %d\n", act->type);
        tok = tok->next;

        act->value = g_strdup(tok->value);
        cc_log("\tact value => %s\n", act->value);
        tok = tok->next;

        /* connect the actions */
        if (!tmpact) {
          r->actions = act;
        } else {
          tmpact->next = act;
        }

        tmpact = act;
      }
      act = tmpact = NULL;
    }

    /* register the rule and clean up */
    {
      CC_LIST_FREE(tokens, token_t);
      cc_add_rule(r);
      rbufsz = 0;
      tok = NULL;
      r = NULL;
    }
  }

  g_free(rules);
	g_key_file_free(cfg);
}

void cc_save_settings(void)
{
	GKeyFile  *cfg = NULL;
	gchar     *cfg_data = NULL;
	gchar     *cfg_dir = NULL;
  gchar     *rules = NULL;
  gint      bufsz = 0;
  rule_t    *rule = NULL;

  cfg = g_key_file_new();
  cfg_dir = g_path_get_dirname(cfg_file);
	g_key_file_load_from_file(cfg, cfg_file, G_KEY_FILE_NONE, NULL);

  /* serialize rules */
  for (rule = config.rules; rule != NULL; rule = rule->next)
  {
    gchar     *rbuf;  /* buffer for the current rule's definition */
    gint      rbufsz = 0; /* its size */
    action_t  *act;

    /* rule format:
     * [id,label,enabled,domain,cnd_type,cnd_val,act1_type,act1_val,...,actN_type,actN_val]
     */
    rbufsz += sizeof(gint);
    if (rule->label)
      rbufsz += strlen(rule->label);
    else
      rbufsz += 11; /* "Unlabelled" */

    rbufsz += strlen(rule->condition->value);
    rbufsz += sizeof(gchar) * 3 /* enabled, doman, and cnd_type all have a size of 1 */;
    rbufsz += 8; /* commas and brackets */

    rbuf = g_malloc(sizeof(gchar) * (rbufsz+1));
    g_snprintf(rbuf, rbufsz, "[%d,%s,%d,%d,%d,%s",
      rule->id,
      rule->label ? rule->label : "Unlabelled",
      rule->enabled,
      rule->domain,
      rule->condition->type,
      rule->condition->value);

    /* rule actions */
    for (act = rule->actions; act != NULL; act = act->next)
    {
      gchar     *tmp = NULL;
      size_t    tmpsz = 1 + (strlen(act->value)) + 2;

      rbufsz += tmpsz;

      tmp = g_strdup(rbuf);
      g_free(rbuf);
      rbuf = g_malloc(sizeof(gchar) * (rbufsz + 1));
      g_snprintf(rbuf, rbufsz, "%s,%d,%s", tmp, act->type, act->value);

      g_free(tmp);
    }

    strcat(rbuf, "]");

    cc_log("size of rule (%d) = %dbytes, serialized: \n%s\n", rule->id, rbufsz, rbuf);

    /* append the rule definition to the master rules buffer */
    {
      /* increment the master buffer size */
      bufsz += rbufsz;

      if (rules)
      {
        /* this is not the first rule, we have to recreate the string */
        gchar *tmp = g_strdup(rules);
        g_free(rules);
        rules = g_malloc(sizeof(gchar) + (bufsz + 1));
        g_snprintf(rules, bufsz, "%s%s", tmp, rbuf);
        g_free(tmp);
      } else
      {
        rules = g_malloc(sizeof(gchar) + (bufsz + 1));
        g_snprintf(rules, bufsz, "%s", rbuf);
      }
    }

    g_free(rbuf);
  }

	g_key_file_set_boolean(cfg, "caseconvert", "capitalize", config.capitalize);
	g_key_file_set_string(cfg, "caseconvert", "rules", rules);

	if (! g_file_test(cfg_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(cfg_dir, TRUE) != 0)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
			_("Plugin cfguration directory could not be created."));
	}
	else
	{
		/* write cfg to file */
		cfg_data = g_key_file_to_data(cfg, NULL, NULL);
		utils_write_file(cfg_file, cfg_data);
		g_free(cfg_data);
	}
	g_free(rules);
	g_free(cfg_dir);
	g_key_file_free(cfg);
}
