#ifndef alma_unify_h
#define alma_unify_h

#include "alma_formula.h"

typedef struct binding {
  alma_variable *var;
  alma_term *term;
} binding;

int unify(alma_term *x, alma_term *y, binding **theta, int *num_bindings);
int pred_unify(alma_function *x, alma_function *y, binding **theta, int *num_bindings);

#endif
