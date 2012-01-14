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

#include "caseconvert_types.h"
#include "caseconvert.h"

static gint rule_id; /* generates IDs for rule objects */
static gint get_rule_id();

/* helper for allocating a rule object */
rule_t* cc_alloc_rule()
{
  rule_t* r = g_malloc(sizeof(rule_t));
  r->condition = NULL;
  r->actions = NULL;
  r->next = NULL;
  r->id = get_rule_id();
  r->label = NULL;
  r->enabled = TRUE;

  return r;
}

/* helper for freeing a rule_t object, along with its condition and list of actions */
void cc_free_rule(rule_t** in)
{
  rule_t* r = *in;
  action_t *act = NULL;

  for (act = r->actions; act != NULL;) {
    action_t *tmp = act;
    act = act->next;
    cc_free_act(&tmp);
  }
  r->actions = NULL;

  if (r->condition) {
    cc_free_cnd(&r->condition);
    r->condition = NULL;
  }

  if (r->label) {
    g_free(r->label);
    r->label = NULL;
  }

  g_free(r);

  (*in) = NULL;
  r = NULL;
}

condition_t* cc_alloc_cnd()
{
  condition_t* c = NULL;

  c = g_malloc(sizeof(condition_t));
  if (!c)
    return NULL;

  c->type = CC_CND_NULL;
  c->value = NULL;

  return c;
}

void cc_free_cnd(condition_t** in_c)
{
  condition_t* c = *in_c;

  if (c->value) {
    g_free(c->value);
    c->value = NULL;
  }

  g_free(c);

  (*in_c) = NULL;
}

action_t* cc_alloc_act()
{
  action_t* a = NULL;

  a = g_malloc(sizeof(action_t));
  if (!a)
    return NULL;

  a->type = CC_ACT_NULL;
  a->value = NULL;
  a->next = NULL;

  return a;
}

void cc_free_act(action_t** in_a)
{
  action_t* a = *in_a;

  if (a->value) {
    g_free(a->value);
    a->value = NULL;
  }

  a->next = NULL;

  g_free(a);

  (*in_a) = NULL;
}

static gint get_rule_id()
{
  rule_t  *rules = cc_get_rules();
  rule_t  *r = NULL;
  gint    max_id = rule_id;

  for (r = rules; r != NULL; r = r->next)
    if (r->id > max_id) max_id = r->id;

  rule_id = max_id + 1;

  return rule_id;
}
