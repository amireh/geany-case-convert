#ifndef H_GEANY_CASE_CONVERT_H
#define H_GEANY_CASE_CONVERT_H

typedef struct rule_t rule_t;
typedef struct action_t action_t;

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

void cc_add_rule(rule_t *);
void cc_rem_rule(rule_t *);

rule_t* cc_alloc_rule();
void    cc_free_rule(rule_t**);
#endif
