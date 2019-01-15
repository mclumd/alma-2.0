#include <string.h>
#include "alma_unify.h"

// Returns term variable is bound to if it's in the bindings, null otherwise
static alma_term* bindings_contain(binding_list *theta, alma_variable *var) {
  if (theta != NULL) {
    for (int i = 0; i < theta->num_bindings; i++) {
      if (theta->list[i].var->id == var->id)
        return theta->list[i].term;
    }
  }
  return NULL;
}

static int occurs_check(binding_list *theta, alma_variable *var, alma_term *x) {
  // If x is a bound variable, occurs-check what it's bound to
  if (x->type == VARIABLE) {
    if (x->variable->id == var->id)
      return 1;
    alma_term *res;
    if ((res = bindings_contain(theta, x->variable)) != NULL)
      return occurs_check(theta, var, res);
  }
  // In function case occurs-check each argument
  else if (x->type == FUNCTION){
    for (int i = 0; i < x->function->term_count; i++) {
      if (occurs_check(theta, var, x->function->terms + i))
        return 1;
    }
  }
  return 0;
}

// For a given term, replace variables in the binding list, replace with what they're bound to
void subst(binding_list *theta, alma_term *term) {
  switch (term->type) {
    case VARIABLE: {
      alma_term *contained = bindings_contain(theta, term->variable);
      if (contained != NULL) {
        free(term->variable->name);
        free(term->variable);
        copy_alma_term(contained, term);
      }
      break;
    }
    case CONSTANT:
      return;
    case FUNCTION: {
      for (int i = 0; i < term->function->term_count; i++) {
        subst(theta, term->function->terms+i);
      }
      break;
    }
  }
}

static void cascade_substitution(binding_list *theta) {
  for (int i = 0; i < theta->num_bindings; i++) {
    subst(theta, theta->list[i].term);
  }
  // AIMA code examples process functions for a second time; may need to do here as well
}

static int unify_var(alma_term *varterm, alma_term *x, binding_list *theta) {
  alma_variable *var = varterm->variable;
  alma_term *res = bindings_contain(theta, var);
  // If var is a bound variable, unify what it's bound to with x
  if (res != NULL) {
    return unify(res, x, theta);
  }
  // If x is a bound variable, unify var with x is bound to
  else if (x->type == VARIABLE && (res = bindings_contain(theta, x->variable)) != NULL) {
    return unify(varterm, res, theta);
  }
  else if (occurs_check(theta, var, x)) {
    return 0;
  }
  else {
    // Append new binding of var/term
    theta->num_bindings++;
    theta->list = realloc(theta->list, sizeof(*theta->list) * theta->num_bindings);
    theta->list[theta->num_bindings-1].var = malloc(sizeof(alma_variable));
    copy_alma_var(var, theta->list[theta->num_bindings-1].var);
    theta->list[theta->num_bindings-1].term = malloc(sizeof(alma_term));
    copy_alma_term(x, theta->list[theta->num_bindings-1].term);
    // Cascade substitution call to deal with bound variables inside other bindings
    cascade_substitution(theta);
    return 1;
  }
}

// Unification function based on algorithm in AIMA book
int unify(alma_term *x, alma_term *y, binding_list *theta) {
  // Unification succeeds without changing theta if trying to unify variable with itself
  if (x->type == VARIABLE && y->type == VARIABLE
      && x->variable->id == y->variable->id) {
    return 1;
  }
  else if (x->type == VARIABLE) {
    return unify_var(x, y, theta);
  }
  else if (y->type == VARIABLE) {
    return unify_var(y, x, theta);
  }
  else if (x->type == CONSTANT && y->type == CONSTANT
          && strcmp(x->constant->name, y->constant->name) == 0) {
    return 1;
  }
  else if (x->type == FUNCTION && y->type == FUNCTION
          && strcmp(x->function->name, y->function->name) == 0
          && x->function->term_count == y->function->term_count) {
    for (int i = 0; i < x->function->term_count; i++) {
      if (!unify(x->function->terms+i, y->function->terms+i, theta))
        return 0;
    }
    return 1;
  }
  else
    return 0;
}

// Bindings need to COPY variables/terms in formulas, not alias -- handle carefully
int pred_unify(alma_function *x, alma_function *y, binding_list *theta) {
  // May eventually move these initializations outside of function
  theta->list = NULL;
  theta->num_bindings = 0;
  if (x->term_count != y->term_count || strcmp(x->name, y->name) != 0)
    return 0;

  for (int i = 0; i < x->term_count; i++) {
    // Bindings build up over repeated calls
    if (!unify(x->terms+i, y->terms+i, theta)) {
      return 0;
    }
  }

  // Unification suceeded; bindings must be cleaned up by caller
  return 1;
}


void print_bindings(binding_list *theta) {
  for (int i = 0; i < theta->num_bindings; i++) {
    printf("%s / ", theta->list[i].var->name);
    alma_term_print(theta->list[i].term);
    printf("\n");
  }
}

// Function to free binding block, after failure or success
void cleanup_bindings(binding_list *theta) {
  for (int i = 0; i < theta->num_bindings; i++) {
    free(theta->list[i].var->name);
    free(theta->list[i].var);
    free_term(theta->list[i].term);
    free(theta->list[i].term);
  }
  free(theta->list);
  free(theta);
}
