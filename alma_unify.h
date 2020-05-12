#ifndef alma_unify_h
#define alma_unify_h

#include "alma_formula.h"

typedef struct binding {
  alma_variable *var;
  alma_term *term;
} binding;

typedef struct binding_list {
  int num_bindings;
  binding *list;
} binding_list;

alma_term* bindings_contain(binding_list *theta, alma_variable *var);
void subst(binding_list *theta, alma_term *term);
int unify(alma_term *x, alma_term *y, binding_list *theta);
int pred_unify(alma_function *x, alma_function *y, binding_list *theta);

void add_binding(binding_list *theta, alma_variable *var, alma_term *term);
void print_bindings(binding_list *theta);
void cleanup_bindings(binding_list *theta);
void copy_bindings(binding_list *dest, binding_list *src);
void swap_bindings(binding_list *a, binding_list *b);

#endif
