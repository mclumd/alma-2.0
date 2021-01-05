#ifndef alma_unify_h
#define alma_unify_h

#include "alma_formula.h"

// For a particular level, stores count-length arrays of x and y indices
typedef struct var_matching {
  long long x;
  long long y;
} var_matching;

typedef struct var_match_set {
  int levels;
  int *level_counts;
  var_matching **matches;
} var_match_set;

void var_match_init(var_match_set *v);
int var_match_consistent(var_match_set *v, int depth, alma_variable *x, alma_variable *y);
void var_match_new_level(var_match_set *v);
void var_match_add(var_match_set *v, int depth, alma_variable *x, alma_variable *y);
int release_matches(var_match_set *v, int retval);

typedef struct binding {
  alma_variable *var;
  int var_quote_lvl;
  int var_quasi_quote_lvl;
  alma_term *term;
  int term_quote_lvl;
  int term_quasi_quote_lvl;
  // Parent literal/term pointer to track overall source of term in binding
  void *term_parent;
} binding;

typedef struct binding_list {
  int num_bindings;
  binding *list;
  var_match_set *quoted_var_matches;
} binding_list;

binding* bindings_contain(binding_list *theta, alma_variable *var);
void subst(binding_list *theta, alma_term *term, int level);

int quote_term_unify(alma_quote *x, alma_quote *y, binding_list *theta);
int term_unify(alma_term *x, alma_term *y, binding_list *theta);
int pred_unify(alma_function *x, alma_function *y, binding_list *theta);

void init_bindings(binding_list *theta);
void add_binding(binding_list *theta, alma_variable *var, alma_term *term, void *parent, int copy_term);
void cleanup_bindings(binding_list *theta);
void copy_bindings(binding_list *dest, binding_list *src);
void swap_bindings(binding_list *a, binding_list *b);

#endif
