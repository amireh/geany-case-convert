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

#ifndef H_GEANY_CASE_CONVERT_TYPES_H
#define H_GEANY_CASE_CONVERT_TYPES_H

#include <glib.h>

typedef struct rule_t rule_t;
typedef struct action_t action_t;

typedef struct {
  enum {
    CC_CND_NULL = 0,
    CC_CND_HAS_PREFIX,
    CC_CND_HAS_SUFFIX,
    CC_CND_ALWAYS_TRUE
  } type;

  gchar *value; /* text to look for, if eligible */
} condition_t;

struct action_t {
  enum {
    CC_ACT_NULL = 0,
    CC_ACT_ADD_PREFIX,
    CC_ACT_REM_PREFIX,
    CC_ACT_ADD_SUFFIX,
    CC_ACT_REM_SUFFIX
  } type;

  gchar     *value; /* text to be removed or added */
  action_t  *next;
};

struct rule_t {
  enum {
    CC_RULE_NULL = 0,
    CC_RULE_S2C, /* snake_case to camelCase */
    CC_RULE_C2S  /* camelCase to snake_case */
  } domain;

  condition_t *condition;
  action_t    *actions;
  rule_t      *next;
  gchar       *label; /* optional identifier */
  gint        id;     /* unique identifier, automatically generated */
  gboolean    enabled;
};

/* helpers for allocating and freeing objects */
rule_t*       cc_alloc_rule();
void          cc_free_rule(rule_t**);

condition_t*  cc_alloc_cnd();
void          cc_free_cnd(condition_t**);

action_t*     cc_alloc_act();
void          cc_free_act(action_t**);

#endif
