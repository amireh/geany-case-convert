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
#include "caseconvert-ui.h"
#include "Scintilla.h"
#include <geany/search.h>

GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

PLUGIN_VERSION_CHECK(211)

PLUGIN_SET_INFO("Case Converter",
                "Converts selected text between CamelCase and snake_case",
                "1.0",
                "Ahmad Amireh <ahmad@amireh.net>")

/*
PluginCallback plugin_callbacks[] =
{
  { "document-activate", (GCallback) &on_document_open, FALSE, NULL },
  { NULL, NULL, FALSE, NULL }
};
*/
/* token_t and expr_t are used in camelCase to snake_case conversion */
typedef struct token_t token_t;
struct token_t {
  gchar   *value;
  size_t  valsz;
  token_t *next;
};

typedef struct {
  token_t* tokens;
  size_t   nr_tokens;
} expr_t;

static config_t config;
static gint     rule_id; /* generates IDs for rule objects */

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

  cc_ui_init();
}

void plugin_cleanup(void)
{
  /* free rules, their actions and conditions */
  rule_t      *rule, *tmprule = NULL;

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
  guint i;

  /* a string is considered camelCased if any upper case letter is preceded
   * or followed by a lower one */
  for (i = 0; i < insz; ++i) {
    if (isupper(in[i])
    && ((i+1 < insz && !isupper(in[i+1])) || (i > 0 && !isupper(in[i-1]))))
    {
      return TRUE;
    }
  }
  return FALSE;
}

static gchar* do_convert(gchar const* in, size_t insz, gint *outsz)
{
  /* iterators */
  guint   i = 0;       /* used to iterate tmp buffers */
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

    g_printf("extracted (%d) tokens from expr '%s'\n", (int)expr->nr_tokens, input);

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

      g_printf("out buf now has '%s' (%d), wrote '%s' (%d)\n", out, ocursor, tok->value, (int)tok->valsz);

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

/* helper for allocating a rule object */
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

/* helper for freeing a rule_t object, along with its condition and list of actions */
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
#define SSM(m, w, l) scintilla_send_message(sci, m, w, l)
#endif

/* converts the case of all occurences of the given text in the specified range */
void cc_convert_range(int r_begin, int r_end, gchar const *txt, gint txtsz)
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

  g_printf("converting '%s'(%d) to '%s'(%d) in [%d..%d]\n", txt, txtsz, repl, replsz, r_begin, r_end);

  sci_start_undo_action(sci);

  SSM(SCI_SETTARGETSTART, r_begin, 0);
  SSM(SCI_SETTARGETEND,   r_end + 1, 0);

  while (SSM(SCI_SEARCHINTARGET, txtsz, (uptr_t)txt) != -1) {
    SSM(SCI_REPLACETARGET, -1, (uptr_t)repl);
    SSM(SCI_SETTARGETSTART, r_begin, 0);
    SSM(SCI_SETTARGETEND,   r_end + 1, 0);
  }

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

  cc_convert_range(0, sci_get_length(sci)+1, NULL, 0);
}
