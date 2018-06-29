#include <string.h>
#include "alma_unify.h"
#include "alma_formula.h"


// Function to free binding block, after failure or success
static void cleanup_bindings(binding *theta, int num_bindings) {
  // TODO
}

// Returns term variable is bound to if it's in the bindings, null otherwise
static alma_term* bindings_contain(binding *theta, int num_bindings, alma_variable *var) {
  if (theta != NULL) {
    for (int i = 0; i < num_bindings; i++) {
      if (strcmp(theta[i].var->name, var->name) == 0)
        return theta[i].term;
    }
  }
  return NULL;
}

static int occurs_check(binding *theta, int num_bindings, alma_variable *var, alma_term *x) {
  alma_term *res;
  // If x is a bound variable, occurs-check what it's bound to
  if (x->type == VARIABLE && (res = bindings_contain(theta, num_bindings, x->variable)) != NULL) {
    return occurs_check(theta, num_bindings, var, res);
  }
  // In function case occurs-check each argument
  else if (x->type == FUNCTION){
    for (int i = 0; i < x->function->term_count; i++) {
      if (occurs_check(theta, num_bindings, var, x->function->terms + i))
        return 1;
    }
  }
  return 0;
}

static void cascade_substitution(binding *theta, int num_bindings) {
  for (int i = 0; i < num_bindings; i++) {
    // TODO
  }
}

static int unify_var(alma_term *varterm, alma_term *x, binding **theta, int *num_bindings) {
  alma_variable *var = varterm->variable;
  alma_term *res = bindings_contain(*theta, *num_bindings, var);
  // If var is a bound variable, unify what it's bound to with x
  if (res != NULL) {
    return unify(res, x, theta, num_bindings);
  }
  // If x is a bound variable, unify var with x is bound to
  else if (x->type == VARIABLE && (res = bindings_contain(*theta, *num_bindings, x->variable)) != NULL) {
    return unify(varterm, res, theta, num_bindings);
  }
  else if (occurs_check(*theta, *num_bindings, var, x)) {
    return 0;
  }
  else {
    // Append new binding of var/term
    (*num_bindings)++;
    *theta = realloc(*theta, sizeof(binding) * *num_bindings);
    (*theta)[*num_bindings-1].var = malloc(sizeof(alma_variable));
    copy_alma_var(var, (*theta)[*num_bindings-1].var);
    (*theta)[*num_bindings-1].term = malloc(sizeof(alma_term));
    copy_alma_term(x, (*theta)[*num_bindings-1].term);
    // Cascade substitution call to deal with bound variables inside other bindings
    cascade_substitution(*theta, *num_bindings);
    return 1;
  }
}

// Unification function based on algorithm in AIMA book
int unify(alma_term *x, alma_term *y, binding **theta, int *num_bindings) {
  if (x->type == VARIABLE) {
    return unify_var(x, y, theta, num_bindings);
  }
  else if (y->type == VARIABLE) {
    return unify_var(y, x, theta, num_bindings);
  }
  else if (x->type == CONSTANT && y->type == CONSTANT
          && strcmp(x->constant->name, y->constant->name) == 0) {
    return 1;
  }
  else if (x->type == FUNCTION && y->type == FUNCTION
          && strcmp(x->function->name, y->function->name) == 0
          && x->function->term_count == y->function->term_count) {
    for (int i = 0; i < x->function->term_count; i++) {
      if (!unify(x->function->terms+i, y->function->terms+i, theta, num_bindings))
        return 0;
    }
    return 1;
  }
  else
    return 0;
}

// Returns nonzero for unification failure
// Bindings need to COPY variables/terms in formulas, not alias -- handle carefully
int pred_unify(alma_function *x, alma_function *y, binding **theta, int *num_bindings) {
  // May eventually move these initializations outside of function
  *theta = NULL;
  *num_bindings = 0;
  if (x->term_count != y->term_count || strcmp(x->name, y->name) != 0)
    return 0;

  for (int i = 0; i < x->term_count; i++) {
    // Bindings build up over repeated calls
    if (!unify(x->terms+i, y->terms+i, theta, num_bindings)) {
      cleanup_bindings(*theta, *num_bindings);
      return 0;
    }
  }

  // Unification suceeded; bindings must be cleaned up by caller
  return 1;
}
