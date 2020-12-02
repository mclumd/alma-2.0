#include <stdlib.h>
#include <string.h>
#include "alma_unify.h"

#include "alma_print.h"
#include "alma_kb.h"

void var_match_init(var_match_set *v) {
  v->levels = 1;
  v->match_levels = malloc(sizeof(*v->match_levels));
  v->match_levels[0].count = 0;
  v->match_levels[0].x = v->match_levels[0].y = NULL;
}

// Returns true if x is matched with y, or no match of x or y exists (which makes new match of them)
int var_match_check(var_match_set *v, int depth, alma_variable *x, alma_variable *y) {
  // Search existing level of depth
  if (depth < v->levels) {
    for (int i = 0; i < v->match_levels[depth].count; i++) {
      if (x->id == v->match_levels[depth].x[i] && y->id == v->match_levels[depth].y[i])
        return 1;
      else if ((x->id == v->match_levels[depth].x[i] && y->id != v->match_levels[depth].y[i]) || (x->id != v->match_levels[depth].x[i] && y->id == v->match_levels[depth].y[i]))
        return 0;
    }
    var_match_add(v, depth, x, y);
    return 1;
  }
  // New level of depth, increase match levels
  else {
    while (depth >= v->levels)
      var_match_new_level(v);
    var_match_add(v, depth, x, y);
    return 1;
  }
  return 0;
}

void var_match_new_level(var_match_set *v) {
  v->levels++;
  v->match_levels = realloc(v->match_levels, sizeof(*v->match_levels) * v->levels);
  v->match_levels[v->levels-1].count = 0;
  v->match_levels[v->levels-1].x = v->match_levels[v->levels-1].y = NULL;
}

void var_match_add(var_match_set *v, int depth, alma_variable *x, alma_variable *y) {
  v->match_levels[depth].count++;
  v->match_levels[depth].x = realloc(v->match_levels[depth].x, sizeof(*v->match_levels[depth].x) * v->match_levels[depth].count);
  v->match_levels[depth].x[v->match_levels[depth].count - 1] = x->id;
  v->match_levels[depth].y = realloc(v->match_levels[depth].y, sizeof(*v->match_levels[depth].y) * v->match_levels[depth].count);
  v->match_levels[depth].y[v->match_levels[depth].count - 1] = y->id;
}

/*static void print_matches(var_match_set *v) {
  for (int i = 0; i < v->levels; i++) {
    printf("Level %d:\n", i);
    if ( v->match_levels[i].count == 0)
      printf("None\n");
    for (int j = 0; j < v->match_levels[i].count; j++) {
      printf("%lld, %lld\n", v->match_levels[i].x[j], v->match_levels[i].y[j]);
    }
  }
}*/

// Function to call when short-circuiting function using them, to properly free var_matching instance
int release_matches(var_match_set *v, int retval) {
  //print_matches(v);
  for (int i = 0; i < v->levels; i++) {
    free(v->match_levels[i].x);
    free(v->match_levels[i].y);
  }
  free(v->match_levels);
  return retval;
}


// Returns term variable is bound to if it's in the bindings, null otherwise
alma_term* bindings_contain(binding_list *theta, alma_variable *var) {
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
  else if (x->type == FUNCTION) {
    for (int i = 0; i < x->function->term_count; i++) {
      if (occurs_check(theta, var, x->function->terms + i))
        return 1;
    }
  }
  else {
    // Stubbed quote case; need to eventually check when quasiquotation is added, TODO
    return 0;
  }
  return 0;
}

// For a given term, replace variables in the binding list, replace with what they're bound to
void subst(binding_list *theta, alma_term *term) {
  if (term->type == VARIABLE) {
    alma_term *contained = bindings_contain(theta, term->variable);
    if (contained != NULL) {
      free(term->variable->name);
      free(term->variable);
      copy_alma_term(contained, term);
    }
  }
  else if (term->type == FUNCTION) {
    for (int i = 0; i < term->function->term_count; i++)
      subst(theta, term->function->terms+i);
  }
  else {
    // Stubbed quote case; need to eventually subst when quasiquotation is added, TODO
  }
}

static void cascade_substitution(binding_list *theta) {
  for (int i = 0; i < theta->num_bindings; i++)
    subst(theta, theta->list[i].term);

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
    add_binding(theta, var, x, 1);
    return 1;
  }
}

static int unify_quote_func(alma_function *x, alma_function *y, binding_list *theta) {
  if (strcmp(x->name, y->name) == 0 && x->term_count == y->term_count) {
    for (int i = 0; i < x->term_count; i++) {
      alma_term *x_t = x->terms+i;
      alma_term *y_t = y->terms+i;
      if (x_t->type != y_t->type
          || (x_t->type == VARIABLE && strcmp(x_t->variable->name, y_t->variable->name) != 0)
          || (x_t->type == FUNCTION && !unify_quote_func(x_t->function, y_t->function, theta))
          || (x_t->type == QUOTE && !unify_quotes(x_t->quote, y_t->quote, theta)))
        return 0;
    }
    return 1;
  }
  return 0;
}

// TODO -- generalize to unify quasiquoted variables when this is added
// Will involve tracking quote_levels
int unify_quotes(alma_quote *x, alma_quote *y, binding_list *theta) {
  if (x->type == y->type) {
    if (x->type == SENTENCE) {
      // TODO, find way to deal with raw sentence quote unification
    }
    else {
      clause *c_x = x->clause_quote;
      clause *c_y = y->clause_quote;
      for (int i = 0; i < c_x->pos_count; i++)
        if (!unify_quote_func(c_x->pos_lits[i], c_y->pos_lits[i], theta))
          return 0;
      for (int i = 0; i < c_x->neg_count; i++)
        if (!unify_quote_func(c_x->neg_lits[i], c_y->neg_lits[i], theta))
          return 0;
      return 1;
    }
  }
  return 0;
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
  else if (x->type == FUNCTION && y->type == FUNCTION
          && strcmp(x->function->name, y->function->name) == 0
          && x->function->term_count == y->function->term_count) {
    for (int i = 0; i < x->function->term_count; i++) {
      if (!unify(x->function->terms+i, y->function->terms+i, theta))
        return 0;
    }
    return 1;
  }
  else if (x->type == QUOTE && y->type == QUOTE) {
    return unify_quotes(x->quote, y->quote, theta);
  }
  else
    return 0;
}

// Bindings need to COPY variables/terms in formulas, not alias -- handle carefully
int pred_unify(alma_function *x, alma_function *y, binding_list *theta) {
  if (x->term_count != y->term_count || strcmp(x->name, y->name) != 0)
    return 0;

  for (int i = 0; i < x->term_count; i++) {
    // Bindings build up over repeated calls
    if (!unify(x->terms+i, y->terms+i, theta))
      return 0;
  }

  // Unification suceeded; bindings must be cleaned up by caller
  return 1;
}


// Operations on bindings

// Append new binding of var/term
void add_binding(binding_list *theta, alma_variable *var, alma_term *term, int copy_term) {
  theta->num_bindings++;
  theta->list = realloc(theta->list, sizeof(*theta->list) * theta->num_bindings);
  theta->list[theta->num_bindings-1].var = malloc(sizeof(*var));
  copy_alma_var(var, theta->list[theta->num_bindings-1].var);
  if (copy_term) {
    theta->list[theta->num_bindings-1].term = malloc(sizeof(*term));
    copy_alma_term(term, theta->list[theta->num_bindings-1].term);
  }
  else {
    theta->list[theta->num_bindings-1].term = term;
  }

  // Cascade substitution call to deal with bound variables inside other bindings
  cascade_substitution(theta);
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

// Assumes dest allocated
void copy_bindings(binding_list *dest, binding_list *src) {
  dest->num_bindings = src->num_bindings;
  if (dest->num_bindings > 0) {
    dest->list = malloc(sizeof(*dest->list) * src->num_bindings);
    for (int i = 0; i < src->num_bindings; i++) {
      dest->list[i].var = malloc(sizeof(alma_variable));
      copy_alma_var(src->list[i].var, dest->list[i].var);
      dest->list[i].term = malloc(sizeof(alma_term));
      copy_alma_term(src->list[i].term, dest->list[i].term);
    }
  }
  else
    dest->list = NULL;
}

void swap_bindings(binding_list *a, binding_list *b) {
  binding *temp = a->list;
  a->list = b->list;
  b->list = temp;
  int num_temp = a->num_bindings;
  a->num_bindings = b->num_bindings;
  b->num_bindings = num_temp;
}
