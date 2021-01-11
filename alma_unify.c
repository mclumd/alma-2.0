#include <stdlib.h>
#include <string.h>
#include "alma_unify.h"

#include "alma_print.h"
#include "alma_kb.h"

void var_match_init(var_match_set *v) {
  v->levels = 1;
  v->level_counts = malloc(sizeof(*v->level_counts));
  v->level_counts[0] = 0;
  v->matches = malloc(sizeof(*v->matches));
  v->matches[0] = NULL;
}

// Returns true if x is matched with y, or no match of x or y exists (which makes new match of them)
int var_match_consistent(var_match_set *v, int depth, alma_variable *x, alma_variable *y) {
  // Search existing level of depth
  if (depth < v->levels) {
    for (int i = 0; i < v->level_counts[depth]; i++) {
      if ((x->id == v->matches[depth][i].x && y->id == v->matches[depth][i].y) || (x->id == v->matches[depth][i].y && y->id == v->matches[depth][i].x))
        return 1;
      else if ((x->id == v->matches[depth][i].x && y->id != v->matches[depth][i].y) || (x->id != v->matches[depth][i].x && y->id == v->matches[depth][i].y)
               || (x->id == v->matches[depth][i].y && y->id != v->matches[depth][i].x) || (x->id != v->matches[depth][i].y && y->id == v->matches[depth][i].x))
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
  v->matches = realloc(v->matches, sizeof(*v->matches) * v->levels);
  v->matches[v->levels-1] = NULL;
  v->level_counts = realloc(v->level_counts, sizeof(*v->level_counts) * v->levels);
  v->level_counts[v->levels-1] = 0;
}

void var_match_add(var_match_set *v, int depth, alma_variable *x, alma_variable *y) {
  v->level_counts[depth]++;
  v->matches[depth] = realloc(v->matches[depth], sizeof(*v->matches[depth]) * v->level_counts[depth]);
  var_matching latest;
  latest.x = x->id;
  latest.y = y->id;
  v->matches[depth][v->level_counts[depth]-1] = latest;
}

// Function to call when short-circuiting function using them, to properly free var_matching instance
int release_matches(var_match_set *v, int retval) {
  //print_matches(v);
  for (int i = 0; i < v->levels; i++)
    free(v->matches[i]);
  free(v->matches);
  free(v->level_counts);
  return retval;
}


// Substitution functions

static void subst_func(binding_list *theta, alma_function *func, int level) {
  for (int i = 0; i < func->term_count; i++)
    subst(theta, func->terms+i, level);
}

// For a given term, replace variables in the binding list, replace with what they're bound to
void subst(binding_list *theta, alma_term *term, int level) {
  if (term->type == VARIABLE) {
    // TODO: must be generalized significantly based on quotation and quasi-quotation amounts

    binding *contained = bindings_contain(theta, term->variable);
    if (contained != NULL) {
      free(term->variable->name);
      free(term->variable);
      copy_alma_term(contained->term, term);
    }
  }
  else if (term->type == FUNCTION) {
    subst_func(theta, term->function, level);
  }
  else if (term->type == QUOTE && term->quote->type == CLAUSE) {
    clause *c = term->quote->clause_quote;
    for (int i = 0; i < c->pos_count; i++)
      subst_func(theta, c->pos_lits[i], level+1);
    for (int i = 0; i < c->neg_count; i++)
      subst_func(theta, c->neg_lits[i], level+1);
  }
  else if (term->type == QUASIQUOTE) {
    // TODO
    // complications of context-dependent substitution to take care with
  }
}

static void cascade_substitution(binding_list *theta) {
  for (int i = 0; i < theta->num_bindings; i++)
    subst(theta, theta->list[i].term, 0);

  // AIMA code examples process functions for a second time; may need to do here as well
}


// Unification helper functions

// Returns term variable is bound to if it's in the bindings, null otherwise
binding* bindings_contain(binding_list *theta, alma_variable *var) {
  if (theta != NULL) {
    for (int i = 0; i < theta->num_bindings; i++) {
      if (theta->list[i].var->id == var->id)
        return theta->list+i;
    }
  }
  return NULL;
}

static int occurs_check(binding_list *theta, alma_variable *var, alma_term *x);

static int occurs_check_func(binding_list *theta, alma_variable *var, alma_function *func) {
  for (int i = 0; i < func->term_count; i++) {
    if (occurs_check(theta, var, func->terms + i))
      return 1;
  }
  return 0;
}

static int occurs_check_var(binding_list *theta, alma_variable *var, alma_variable *x) {
  if (x->id == var->id)
    return 1;
  binding *res;
  // If x is a bound variable, occurs-check what it's bound to
  if ((res = bindings_contain(theta, x)) != NULL)
    return occurs_check(theta, var, res->term);
  return 0;
}

static int occurs_check(binding_list *theta, alma_variable *var, alma_term *x) {
  if (x->type == VARIABLE) {
    return occurs_check_var(theta, var, x->variable);
  }
  // In function case occurs-check each argument
  else if (x->type == FUNCTION) {
    return occurs_check_func(theta, var, x->function);
  }
  // For a quote, occurs-check every literal recursively
  // Argument variable may appear at arbitrary depth in the quoted formula
  else if (x->type == QUOTE && x->quote->type == CLAUSE) {
    clause *c = x->quote->clause_quote;
    for (int i = 0; i < c->pos_count; i++)
      if (occurs_check_func(theta, var, c->pos_lits[i]))
        return 1;
    for (int i = 0; i < c->neg_count; i++)
      if (occurs_check_func(theta, var, c->neg_lits[i]))
        return 1;
  }
  else if (x->type == QUASIQUOTE) {
    return occurs_check_var(theta, var, x->quasiquote->variable);
  }
  return 0;
}


// Unification functions

static int unify(alma_term *x, alma_term *y, void *x_parent, void *y_parent, int x_quote_lvl, int y_quote_lvl, binding_list *theta);
static void add_binding_detailed(binding_list *theta, alma_variable *var, int var_quote_lvl, int var_qq_lvl, alma_term *term, int term_quote_lvl, void *parent, int copy_term);

static int unify_var(alma_term *varterm, alma_term *x, void *var_parent, void *x_parent, int var_quote_lvl, int x_quote_lvl, binding_list *theta) {
  alma_variable *var = varterm->variable;
  binding *res = bindings_contain(theta, var);
  // If var is a bound variable, unify what it's bound to with x
  if (res != NULL) {
    return unify(res->term, x, res->term_parent, x_parent, res->term_quote_lvl, x_quote_lvl, theta);
  }
  // If x is a bound variable, unify var with what x is bound to
  else if (x->type == VARIABLE && (res = bindings_contain(theta, x->variable)) != NULL) {
    return unify(varterm, res->term, var_parent, res->term_parent, var_quote_lvl, res->term_quote_lvl, theta);
  }
  // Occurs check to avoid infinite regress in bindings
  else if (occurs_check(theta, var, x)) {
    return 0;
  }
  // No recursive calls left, check conditions and potentially make binding
  else {
    // Cases to check for variables within quotes that don't escape
    // TODO: note this for now assumes no quasi-quotation
    if (var_quote_lvl > 0 && x_quote_lvl > 0 && x->type == VARIABLE) {
      // Check for match of var and x being consistent with existing matching
      int check = var_match_consistent(theta->quoted_var_matches, x_quote_lvl, x->variable, var);
      if (var_quote_lvl != x_quote_lvl)
        check = check && var_match_consistent(theta->quoted_var_matches, var_quote_lvl, x->variable, var);
      if (!check) {
        // Debug
        printf("Var matching not consistent if matching %lld with %lld\n", var->id, x->variable->id);
        return 0;
      }

      // Variables which are from the same (non-null) parent cannot be unified
      if (var_parent == x_parent && var_parent != NULL) {
        // Debug
        printf("Same parent for quoted vars %lld and %lld; unification failure\n", var->id, x->variable->id);
        return 0;
      }
    }

    add_binding_detailed(theta, var, var_quote_lvl, 0, x, x_quote_lvl, x_parent, 1);
    return 1;
  }
}

static int unify_quotes(alma_quote *x, alma_quote *y, void *x_parent, void *y_parent, int x_quote_lvl, int y_quote_lvl, binding_list *theta);

static int unify_quote_func(alma_function *x, alma_function *y, void *x_parent, void *y_parent, int x_quote_lvl, int y_quote_lvl, binding_list *theta) {
  if (strcmp(x->name, y->name) == 0 && x->term_count == y->term_count) {
    for (int i = 0; i < x->term_count; i++) {
      alma_term *x_t = x->terms+i;
      alma_term *y_t = y->terms+i;
      if (x_t->type == y_t->type) {
        if (x_t->type == VARIABLE && !unify_var(x_t, y_t, x_parent, y_parent, x_quote_lvl, y_quote_lvl, theta))
          return 0;
        else if (x_t->type == FUNCTION && !unify_quote_func(x_t->function, y_t->function, x_parent, y_parent, x_quote_lvl, y_quote_lvl, theta))
          return 0;
        else if (x_t->type == QUOTE && !unify_quotes(x_t->quote, y_t->quote, x_parent, y_parent, x_quote_lvl+1, y_quote_lvl+1, theta))
          return 0;
      }
      else
        return 0;
    }
    return 1;
  }
  return 0;
}

// TODO -- generalize to unify quasiquoted variables when this is added
static int unify_quotes(alma_quote *x, alma_quote *y, void *x_parent, void *y_parent, int x_quote_lvl, int y_quote_lvl, binding_list *theta) {
  if (x->type == y->type) {
    if (x->type == SENTENCE) {
      // TODO, find way to deal with raw sentence quote unification
      // or otherwise, noted here as a gap
    }
    else {
      clause *c_x = x->clause_quote;
      clause *c_y = y->clause_quote;
      for (int i = 0; i < c_x->pos_count; i++)
        if (!unify_quote_func(c_x->pos_lits[i], c_y->pos_lits[i], x_parent, y_parent, x_quote_lvl, y_quote_lvl, theta))
          return 0;
      for (int i = 0; i < c_x->neg_count; i++)
        if (!unify_quote_func(c_x->neg_lits[i], c_y->neg_lits[i], x_parent, y_parent, x_quote_lvl, y_quote_lvl, theta))
          return 0;
      return 1;
    }
  }
  return 0;
}

// Non-escaping variables must occur in *effective* quotation level above must_exceed_lvl
// Thus, checks for location of quantifiers inside
// Non-escaping variables violating above condition have quantifiers outside of the arg term; in such cases return 1
// Returning 1 indicates insufficient quotation of a variable, which should then not unify
static int var_insuff_quoted(alma_term *term, int must_exceed_lvl, int quote_level) {
  if (term->type == VARIABLE) {
    return quote_level <= must_exceed_lvl;
  }
  else if (term->type == FUNCTION) {
    for (int i = 0; i < term->function->term_count; i++)
      if (var_insuff_quoted(term->function->terms+i, must_exceed_lvl, quote_level))
        return 1;
  }
  else if (term->type == QUOTE) {
    if (term->type == CLAUSE) {
      clause *c = term->quote->clause_quote;
      for (int i = 0; i < c->pos_count; i++)
        for (int j = 0; j < c->pos_lits[i]->term_count; j++)
          if (var_insuff_quoted(c->pos_lits[i]->terms+j, must_exceed_lvl, quote_level+1))
            return 1;
      for (int i = 0; i < c->neg_count; i++)
        for (int j = 0; j < c->neg_lits[i]->term_count; j++)
          if (var_insuff_quoted(c->neg_lits[i]->terms+j, must_exceed_lvl, quote_level+1))
            return 1;
    }
  }
  else if (term->type == QUASIQUOTE) {
    return quote_level - term->quasiquote->backtick_count <= must_exceed_lvl;
  }
  return 0;
}

static int unify_quasiquote(alma_term *qqterm, alma_term *x, void *qq_parent, void *x_parent, int qq_quote_lvl, int x_quote_lvl, binding_list *theta) {
  alma_quasiquote *qq = qqterm->quasiquote;

  // Case of a quasi-quoted variable not escaping quotation
  if (qq->backtick_count < qq_quote_lvl) {
    if (x->type == QUASIQUOTE && x->quasiquote->backtick_count < x_quote_lvl
        && qq->backtick_count == x->quasiquote->backtick_count) {
      // For now, same kinds of binding and occurs checks as for regular unification
      binding *res = bindings_contain(theta, qq->variable);
      if (res != NULL) {
        if (res->var_quasi_quote_lvl != qq->backtick_count || res->term->type != QUASIQUOTE || res->term->quasiquote->backtick_count != x->quasiquote->backtick_count) {
          // Debug
          printf("Surprising mismatch of quasi-quote type or mark amounts to unify %lld with %lld\n", res->var->id, x->quasiquote->variable->id);
          return 0;
        }
        return unify_quasiquote(res->term, x, res->term_parent, x_parent, res->term_quote_lvl, x_quote_lvl, theta);
      }
      else if ((res = bindings_contain(theta, x->quasiquote->variable)) != NULL) {
        if (res->var_quasi_quote_lvl != x->quasiquote->backtick_count || res->term->type != QUASIQUOTE || res->term->quasiquote->backtick_count != qq->backtick_count) {
          // Debug
          printf("Surprising mismatch of quasi-quote type or mark amounts to unify %lld with %lld\n", res->var->id, qq->variable->id);
          return 0;
        }
        return unify_quasiquote(qqterm, res->term, qq_parent, res->term_parent, qq_quote_lvl, res->term_quote_lvl, theta);
      }
      else if (occurs_check(theta, qq->variable, x)) {
        // Debug
        printf("Occurs check failure for %lld\n", qq->variable->id);
        return 0;
      }
      else {
        int check = var_match_consistent(theta->quoted_var_matches, qq_quote_lvl, qq->variable, x->quasiquote->variable);
        if (qq_quote_lvl != x_quote_lvl)
          check = check && var_match_consistent(theta->quoted_var_matches, x_quote_lvl, qq->variable, x->quasiquote->variable);
        if (!check) {
          // Debug
          printf("Var matching not consistent if matching %lld with %lld\n", qq->variable->id, x->quasiquote->variable->id);
          return 0;
        }

        // Variables which are from the same (non-null) parent cannot be unified
        if (qq_parent == x_parent && qq_parent != NULL) {
          // Debug
          printf("Same parent for quoted vars %lld and %lld; unification failure\n", qq->variable->id, x->quasiquote->variable->id);
          return 0;
        }

        add_binding_detailed(theta, qq->variable, qq_quote_lvl, qq->backtick_count, x, x_quote_lvl, x_parent, 1);
      }
    }
    // Non-escaping quasi-quote can ONLY unify with another quasi-quote
    else
      return 0;
  }
  // Quasi-quoted variable fully escaping quotation has wider unification options
  else {
    // For now, same kinds of binding and occurs checks as for regular unification
    binding *res = bindings_contain(theta, qq->variable);
    if (res != NULL) {
      return unify(res->term, x, res->term_parent, x_parent, res->term_quote_lvl, x_quote_lvl, theta);
    }
    else if (x->type == QUASIQUOTE && x->quasiquote->backtick_count == x_quote_lvl
             && (res = bindings_contain(theta, x->quasiquote->variable)) != NULL) {
      unify(qqterm, res->term, qq_parent, res->term_parent, qq_quote_lvl, res->term_quote_lvl, theta);
    }
    else if (occurs_check(theta, qq->variable, x)) {
      // Debug
      printf("Occurs check failure for %lld\n", qq->variable->id);
      return 0;
    }
    else if (x->type == FUNCTION || x->type == QUOTE || x->type == VARIABLE) {
      // Rejecting for unification: non-escaping variables that have their quantifiers outside the quasi-quoted variable
      // Includes all regular variables
      if (var_insuff_quoted(x, qq_quote_lvl, qq_quote_lvl)) {
        // Debug
        printf("Cannot unify %lld with term having insufficientl quoted variables\n", qq->variable->id);
        return 0;
      }
      else {
        add_binding_detailed(theta, qq->variable, qq_quote_lvl, qq->backtick_count, x, x_quote_lvl, x_parent, 1);
        return 1;
      }
    }
    // Two quasi-quotes, without bindings, remain
    else {
      // Reject cases with other quasi-quote not fully escaping
      if (x->quasiquote->backtick_count != x_quote_lvl) {
        // Debug
        printf("Cannot unify %lld with fully-escaping %lld\n", qq->variable->id, x->quasiquote->variable->id);
        return 0;
      }
      // For a pair of fully-escaped quasi-quoted variables,
      else {
        add_binding_detailed(theta, qq->variable, qq_quote_lvl, qq->backtick_count, x, x_quote_lvl, x_parent, 1);
        return 1;
      }
    }
  }
  return 0;
}

// Unification function based on algorithm in AIMA book
// Modified for quotation and quasi-quootation
static int unify(alma_term *x, alma_term *y, void *x_parent, void *y_parent, int x_quote_lvl, int y_quote_lvl, binding_list *theta) {
  // Unification succeeds without changing theta if trying to unify variable with itself
  if (x->type == VARIABLE && y->type == VARIABLE
      && x->variable->id == y->variable->id) {
    return 1;
  }
  else if (x->type == VARIABLE) {
    return unify_var(x, y, x_parent, y_parent, x_quote_lvl, y_quote_lvl, theta);
  }
  else if (y->type == VARIABLE) {
    return unify_var(y, x, y_parent, x_parent, x_quote_lvl, y_quote_lvl, theta);
  }
  else if (x->type == QUASIQUOTE) {
    return unify_quasiquote(x, y, x_parent, y_parent, x_quote_lvl, y_quote_lvl, theta);
  }
  else if (y->type == QUASIQUOTE) {
    return unify_quasiquote(y, x, y_parent, x_parent, x_quote_lvl, y_quote_lvl, theta);
  }
  else if (x->type == FUNCTION && y->type == FUNCTION && strcmp(x->function->name, y->function->name) == 0
          && x->function->term_count == y->function->term_count) {
    for (int i = 0; i < x->function->term_count; i++) {
      if (!unify(x->function->terms+i, y->function->terms+i, x_parent, y_parent, x_quote_lvl, y_quote_lvl, theta))
        return 0;
    }
    return 1;
  }
  else if (x->type == QUOTE && y->type == QUOTE) {
    return unify_quotes(x->quote, y->quote, x_parent, y_parent, x_quote_lvl+1, y_quote_lvl+1, theta);
  }
  else
    return 0;
}


// Functions used outside to initiate unification

int quote_term_unify(alma_quote *x, alma_quote *y, binding_list *theta) {
  return unify_quotes(x, y, x, y, 0, 0, theta);
}

int term_unify(alma_term *x, alma_term *y, binding_list *theta) {
  return unify(x, y, x, y, 0, 0, theta);
}

// Bindings need to COPY variables/terms in formulas, not alias -- handle carefully
int pred_unify(alma_function *x, alma_function *y, binding_list *theta) {
  if (x->term_count != y->term_count || strcmp(x->name, y->name) != 0)
    return 0;

  for (int i = 0; i < x->term_count; i++) {
    // Bindings build up over repeated calls
    if (!unify(x->terms+i, y->terms+i, x, y, 0, 0, theta))
      return 0;
  }

  // Unification suceeded; bindings must be cleaned up by caller
  return 1;
}


// Operations on bindings

void init_bindings(binding_list *theta) {
  theta->num_bindings = 0;
  theta->list = NULL;
  theta->quoted_var_matches = malloc(sizeof*(theta->quoted_var_matches));
  var_match_init(theta->quoted_var_matches);
}

// Append new binding of var/term with additional fields used by quotaton as well
static void add_binding_detailed(binding_list *theta, alma_variable *var, int var_quote_lvl, int var_qq_lvl, alma_term *term, int term_quote_lvl, void *parent, int copy_term) {
  theta->num_bindings++;
  theta->list = realloc(theta->list, sizeof(*theta->list) * theta->num_bindings);
  theta->list[theta->num_bindings-1].var = malloc(sizeof(*var));
  copy_alma_var(var, theta->list[theta->num_bindings-1].var);
  theta->list[theta->num_bindings-1].var_quote_lvl = var_quote_lvl;
  theta->list[theta->num_bindings-1].var_quasi_quote_lvl = var_qq_lvl;
  if (copy_term) {
    theta->list[theta->num_bindings-1].term = malloc(sizeof(*term));
    copy_alma_term(term, theta->list[theta->num_bindings-1].term);
  }
  else {
    theta->list[theta->num_bindings-1].term = term;
  }
  theta->list[theta->num_bindings-1].term_quote_lvl = term_quote_lvl;
  theta->list[theta->num_bindings-1].term_parent = parent;


  // Cascade substitution call to deal with bound variables inside other bindings
  cascade_substitution(theta);
}

// Append new binding of var/term
void add_binding(binding_list *theta, alma_variable *var, alma_term *term, void *parent, int copy_term) {
  add_binding_detailed(theta, var, 0, 0, term, 0, parent, copy_term);
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

  release_matches(theta->quoted_var_matches, 0);
  free(theta->quoted_var_matches);

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
      dest->list[i].var_quote_lvl = src->list[i].var_quote_lvl;
      dest->list[i].var_quasi_quote_lvl = src->list[i].var_quasi_quote_lvl;
      dest->list[i].term = malloc(sizeof(alma_term));
      copy_alma_term(src->list[i].term, dest->list[i].term);
      dest->list[i].term_quote_lvl = src->list[i].term_quote_lvl;
      dest->list[i].term_parent = src->list[i].term_parent;
    }
  }
  else
    dest->list = NULL;
  if (src->quoted_var_matches != NULL) {
    dest->quoted_var_matches = malloc(sizeof(*dest->quoted_var_matches));
    dest->quoted_var_matches->levels = src->quoted_var_matches->levels;
    dest->quoted_var_matches->level_counts = malloc(sizeof(*dest->quoted_var_matches->level_counts) * dest->quoted_var_matches->levels);
    memcpy(dest->quoted_var_matches->level_counts, src->quoted_var_matches->level_counts, sizeof(*dest->quoted_var_matches->level_counts) * dest->quoted_var_matches->levels);
    dest->quoted_var_matches->matches = malloc(sizeof(*dest->quoted_var_matches->matches) * dest->quoted_var_matches->levels);
    for (int i = 0; i < dest->quoted_var_matches->levels; i++) {
      if (dest->quoted_var_matches->level_counts[i] > 0) {
        dest->quoted_var_matches->matches[i] = malloc(sizeof(*dest->quoted_var_matches->matches[i]) * dest->quoted_var_matches->level_counts[i]);
        memcpy(dest->quoted_var_matches->matches[i], src->quoted_var_matches->matches[i], sizeof(*dest->quoted_var_matches->matches[i]) * dest->quoted_var_matches->level_counts[i]);
      }
      else
        dest->quoted_var_matches->matches[i] = NULL;
    }
  }
  else
    dest->quoted_var_matches = NULL;
}

void swap_bindings(binding_list *a, binding_list *b) {
  binding_list temp = *a;
  *a = *b;
  *b = temp;
}
