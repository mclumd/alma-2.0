#include <stdlib.h>
#include <string.h>
#include "alma_unify.h"
#include "alma_fif.h"
#include "alma_print.h"
#include "alma_clause.h"

// Var matching functions

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

// Function used for substitution, recursive unification, and cases like truth predicate
// Fully-escaping variables at quote_level+x are adjusted to quotation level new_level+x
static void adjust_context(alma_term *term, int quote_level, int new_level) {
  if (term->type == VARIABLE && term->variable->quasiquotes == quote_level) {
    term->variable->quasiquotes = new_level;
  }
  else if (term->type == FUNCTION) {
    for (int i = 0; i < term->function->term_count; i++)
      adjust_context(term->function->terms+i, quote_level, new_level);
  }
  else if (term->type == QUOTE && term->quote->type == CLAUSE) {
    adjust_clause_context(term->quote->clause_quote, quote_level+1, new_level+1);
  }
}

// Calls adjust_context on each term of a clause
void adjust_clause_context(clause *c, int quote_level, int new_level) {
  for (int i = 0; i < c->pos_count; i++)
    for (int j = 0; j < c->pos_lits[i]->term_count; j++)
      adjust_context(c->pos_lits[i]->terms+j, quote_level, new_level);
  for (int i = 0; i < c->neg_count; i++)
    for (int j = 0; j < c->neg_lits[i]->term_count; j++)
      adjust_context(c->neg_lits[i]->terms+j, quote_level, new_level);
  if (c->tag == FIF) {
    for (int i = 0; i < c->fif->num_conclusions; i++) {
      adjust_clause_context(c->fif->conclusions[i], quote_level, new_level);
    }
  }
}

// Only fully-escaping variables are adjusted to have an additional quasiquote
// And, only if corresponding place in the query is something other than a non-escaping variable
// Since term has yet to be placed in its outermost quote context, changes are made when effective quote_level is 1
// Query doesn't have this issue
static void increment_quasiquote_level_paired(alma_term *term, alma_term *query, int quote_level) {
  if (term->type == VARIABLE) {
    if (term->variable->quasiquotes+1 == quote_level && query != NULL
        && !(query->type == VARIABLE && query->variable->quasiquotes < quote_level)) {
      term->variable->quasiquotes++;
    }
  }
  else if (term->type == FUNCTION) {
    for (int i = 0; i < term->function->term_count; i++) {
      if (query != NULL && query->type == VARIABLE)
        query = NULL;
      alma_term *query_term = (query == NULL) ? NULL : query->function->terms+i;
      increment_quasiquote_level_paired(term->function->terms+i, query_term, quote_level);
    }
  }
  else if (term->type == QUOTE && term->quote->type == CLAUSE) {
    if (query != NULL && query->type == VARIABLE)
      query = NULL;
    clause *query_clause = (query == NULL) ? NULL : query->quote->clause_quote;
    increment_clause_quote_level_paired(term->quote->clause_quote, query_clause, quote_level+1);
  }
}

// Calls increment_clause_quote_level_paired on each term of a clause
void increment_clause_quote_level_paired(clause *c, clause *query, int quote_level) {
  for (int i = 0; i < c->pos_count; i++)
    for (int j = 0; j < c->pos_lits[i]->term_count; j++) {
      alma_term *query_term = (query == NULL) ? NULL : query->pos_lits[i]->terms+j;
      increment_quasiquote_level_paired(c->pos_lits[i]->terms+j, query_term, quote_level);
    }
  for (int i = 0; i < c->neg_count; i++)
    for (int j = 0; j < c->neg_lits[i]->term_count; j++) {
      alma_term *query_term = (query == NULL) ? NULL : query->neg_lits[i]->terms+j;
      increment_quasiquote_level_paired(c->neg_lits[i]->terms+j, query_term, quote_level);
    }
  if (c->tag == FIF) {
    for (int i = 0; i < c->fif->num_conclusions; i++) {
      clause *conc = (query == NULL) ? NULL : query->fif->conclusions[i];
      increment_clause_quote_level_paired(c->fif->conclusions[i], conc, quote_level);
    }
  }
}


static void subst_func(binding_list *theta, alma_function *func, int level) {
  for (int i = 0; i < func->term_count; i++)
    subst_term(theta, func->terms+i, level);
}

// For a given term at quote_level, replace its variables present in the binding list with their bindings
void subst_term(binding_list *theta, alma_term *term, int quote_level) {
  if (term->type == VARIABLE) {
    binding *contained = bindings_contain(theta, term->variable);
    // If a variable is bound, replace it with whatever it is bound to
    if (contained != NULL) {
      int quasiquote_amount = term->variable->quasiquotes;
      free(term->variable->name);
      free(term->variable);
      copy_alma_term(contained->term, term);

      // Adjust context based on different quote level if necessary
      if (quasiquote_amount == quote_level && contained->quote_level != quote_level)
        adjust_context(term, contained->quote_level, quote_level);
    }
  }
  else if (term->type == FUNCTION) {
    subst_func(theta, term->function, quote_level);
  }
  else if (term->type == QUOTE && term->quote->type == CLAUSE) {
    subst_clause(theta, term->quote->clause_quote, quote_level+1);
  }
}

void subst_clause(binding_list *theta, clause *c, int quote_level) {
  for (int i = 0; i < c->pos_count; i++)
    subst_func(theta, c->pos_lits[i], quote_level);
  for (int i = 0; i < c->neg_count; i++)
    subst_func(theta, c->neg_lits[i], quote_level);
  if (c->tag == FIF)
    for (int i = 0; i < c->fif->num_conclusions; i++)
      subst_clause(theta, c->fif->conclusions[i], quote_level);
}

static void cascade_substitution(binding_list *theta) {
  for (int i = 0; i < theta->num_bindings; i++)
    subst_term(theta, theta->list[i].term, theta->list[i].quote_level);
}


// Unification helper functions

// Returns term variable is bound to if it's in the bindings, null otherwise
// Ignores quasiquotes field of var, due to bindings not tracking this context
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

static int occurs_check_clause(binding_list *theta, alma_variable *var, clause *c) {
  for (int i = 0; i < c->pos_count; i++)
    if (occurs_check_func(theta, var, c->pos_lits[i]))
      return 1;
  for (int i = 0; i < c->neg_count; i++)
    if (occurs_check_func(theta, var, c->neg_lits[i]))
      return 1;
  if (c->tag == FIF)
    for (int i = 0; i < c->fif->num_conclusions; i++)
      if (occurs_check_clause(theta, var, c->fif->conclusions[i]))
        return 1;
  return 0;
}

static int occurs_check(binding_list *theta, alma_variable *var, alma_term *x) {
  int res = 0;
  if (x->type == VARIABLE) {
    if (var->id == x->variable->id)
      res = 1;
    else {
      binding *b = bindings_contain(theta, x->variable);
      // If x is a bound variable, occurs-check what it's bound to
      res = (b == NULL) ? 0 : occurs_check(theta, var, b->term);
    }
  }
  // In function case occurs-check each argument
  else if (x->type == FUNCTION) {
    res = occurs_check_func(theta, var, x->function);
  }
  // For a quote, occurs-check every literal recursively
  // Argument variable may appear at arbitrary depth in the quoted formula
  else if (x->type == QUOTE && x->quote->type == CLAUSE) {
    res = occurs_check_clause(theta, var, x->quote->clause_quote);
  }

  // Debug
  if (res)
    printf("Occurs check failure for %lld\n", var->id);
  return res;
}

static int vars_suff_quoted(alma_term *term, int must_exceed_level, int quote_level);

static int func_vars_suff_quoted(alma_function *func, int must_exceed_level, int quote_level) {
  for (int i = 0; i < func->term_count; i++)
    if (!vars_suff_quoted(func->terms+i, must_exceed_level, quote_level))
      return 0;
  return 1;
}

static int clause_vars_suff_quoted(clause *c, int must_exceed_level, int quote_level) {
  for (int i = 0; i < c->pos_count; i++)
    if (!func_vars_suff_quoted(c->pos_lits[i], must_exceed_level, quote_level))
      return 0;
  for (int i = 0; i < c->neg_count; i++)
    if (!func_vars_suff_quoted(c->neg_lits[i], must_exceed_level, quote_level))
      return 0;
  if (c->tag == FIF)
    for (int i = 0; i < c->fif->num_conclusions; i++)
      if (!clause_vars_suff_quoted(c->fif->conclusions[i], must_exceed_level, quote_level))
        return 0;
  return 1;
}

// Non-escaping variables must occur in *effective* quotation level above must_exceed_level
// Thus, checks for location of quantifiers inside
// Non-escaping variables violating above condition have quantifiers outside of the arg term; in such cases return 1
// Returning 1 indicates sufficient quotation of a variable; otherwise must not unify
static int vars_suff_quoted(alma_term *term, int must_exceed_level, int quote_level) {
  if (term->type == VARIABLE) {
    return term->variable->quasiquotes == quote_level || quote_level - term->variable->quasiquotes > must_exceed_level;
  }
  else if (term->type == FUNCTION) {
    return func_vars_suff_quoted(term->function, must_exceed_level, quote_level);
  }
  else if (term->type == QUOTE && term->quote->type == CLAUSE) {
    return clause_vars_suff_quoted(term->quote->clause_quote, must_exceed_level, quote_level+1);
  }
  return 0;
}


// Unification functions

static int unify(alma_term *x, alma_term *y, int quote_level, binding_list *theta);

static int unify_var(alma_term *varterm, alma_term *x, int quote_level, binding_list *theta) {
  alma_variable *var = varterm->variable;

  if (x->type == VARIABLE) {
    // A variable can directly unify with another only if of the same quasiquotes
    if (var->quasiquotes != x->variable->quasiquotes) {
      // Debug
      printf("Cannot unify %lld with different quasiquote amount %lld\n", var->id, x->variable->id);
      return 0;
    }
    // Check for inconsistent partially-escaping variable matching
    if (var->quasiquotes < quote_level && !var_match_consistent(theta->quoted_var_matches, quote_level - var->quasiquotes, var, x->variable)) {
     // Debug
     printf("Var matching not consistent if matching %lld with %lld\n", var->id, x->variable->id);
     return 0;
    }
  }
  // X is not a variable, so var can unify only if fully-escaping and x has inner variables sufficiently quoted
  else if (var->quasiquotes != quote_level || !vars_suff_quoted(x, quote_level, quote_level)) {
    // Debug
    printf("Cannot unify %lld with term having insufficiently quoted variables\n", var->id);
    return 0;
  }

  // If var is a bound variable, unify what it's bound to with x
  binding *res = bindings_contain(theta, var);
  if (res != NULL) {
    // If var is fully-escaping and from a different quote level, adjust res' context to quote level
    if (var->quasiquotes == quote_level && res->quote_level != quote_level) {
      adjust_context(res->term, res->quote_level, quote_level);
      res->quote_level = quote_level;
    }
    return unify(res->term, x, res->quote_level, theta);
  }
  // If x is a bound variable, unify var with what x is bound to
  else if (x->type == VARIABLE && (res = bindings_contain(theta, x->variable)) != NULL) {
    // If x is fully-escaping and from a different quote level, adjust res' context to quote level
    if (x->variable->quasiquotes == quote_level && res->quote_level != quote_level) {
      adjust_context(res->term, res->quote_level, quote_level);
      res->quote_level = quote_level;
    }
    return unify(varterm, res->term, res->quote_level, theta);
  }
  // Occurs check to avoid infinite regress in bindings
  else if (occurs_check(theta, var, x)) {
    return 0;
  }

  add_binding(theta, var, quote_level, x, 1);
  return 1;
}

static int unify_function(alma_function *x, alma_function *y, int quote_level, binding_list *theta) {
  if (x->term_count != y->term_count || strcmp(x->name, y->name) != 0)
    return 0;
  for (int i = 0; i < x->term_count; i++)
    if (!unify(x->terms+i, y->terms+i, quote_level, theta))
      return 0;
  return 1;
}

static int unify_clause(clause *x, clause *y, int quote_level, binding_list *theta) {
  if (x->tag != y->tag || x->pos_count != y->pos_count || x->neg_count != y->neg_count)
    return 0;

  if (x->tag == FIF) {
    if (x->fif->num_conclusions != y->fif->num_conclusions)
      return 0;
    for (int i = 0; i < x->fif->premise_count; i++)
      if (!unify_function(fif_access(x, i), fif_access(y, i), quote_level, theta))
        return 0;
    for (int i = 0; i < x->fif->num_conclusions; i++)
      if (!unify_clause(x->fif->conclusions[i], y->fif->conclusions[i], quote_level, theta))
        return 0;
  }
  else {
    for (int i = 0; i < x->pos_count; i++)
      if (!unify_function(x->pos_lits[i], y->pos_lits[i], quote_level, theta))
        return 0;
    for (int i = 0; i < x->neg_count; i++)
      if (!unify_function(x->neg_lits[i], y->neg_lits[i], quote_level, theta))
        return 0;
  }
  return 1;
}

static int unify_quote(alma_quote *x, alma_quote *y, int quote_level, binding_list *theta) {
  if (x->type != y->type)
    return 0;

  if (x->type == SENTENCE) {
    // TODO, find way to deal with raw sentence quote unification
    // or otherwise, noted here as a gap
    return 0;
  }
  else
    return unify_clause(x->clause_quote, y->clause_quote, quote_level, theta);
}

// Unification function based on algorithm in AIMA book
// Modified for quotation and quasi-quotation
static int unify(alma_term *x, alma_term *y, int quote_level, binding_list *theta) {
  // Unification succeeds without changing theta if trying to unify variable with itself
  if (x->type == VARIABLE && y->type == VARIABLE && x->variable->id == y->variable->id) {
    return 1;
  }
  else if (x->type == VARIABLE) {
    return unify_var(x, y, quote_level, theta);
  }
  else if (y->type == VARIABLE) {
    return unify_var(y, x, quote_level, theta);
  }
  else if (x->type == FUNCTION && y->type == FUNCTION) {
    return unify_function(x->function, y->function, quote_level, theta);
  }
  else if (x->type == QUOTE && y->type == QUOTE) {
    return unify_quote(x->quote, y->quote, quote_level+1, theta);
  }
  else
    return 0;
}


// Functions used outside to initiate unification

int quote_term_unify(alma_quote *x, alma_quote *y, binding_list *theta) {
  return unify_quote(x, y, 1, theta);
}

int term_unify(alma_term *x, alma_term *y, binding_list *theta) {
  return unify(x, y, 0, theta);
}

// All bindings must be cleaned up by caller
int pred_unify(alma_function *x, alma_function *y, binding_list *theta, int verbose) {
  int ret = unify_function(x, y, 0, theta);
  /*if (verbose) {
    if (ret)
      printf("Unification success!\n");
    else
      printf("Unification failure\n");
  }*/
  return ret;
}


// Operations on bindings

void init_bindings(binding_list *theta) {
  theta->num_bindings = 0;
  theta->list = NULL;
  theta->quoted_var_matches = malloc(sizeof*(theta->quoted_var_matches));
  var_match_init(theta->quoted_var_matches);
}

// Append new binding of var/term
void add_binding(binding_list *theta, alma_variable *var, int quote_level, alma_term *term, int copy_term) {
  theta->num_bindings++;
  theta->list = realloc(theta->list, sizeof(*theta->list) * theta->num_bindings);
  theta->list[theta->num_bindings-1].var = malloc(sizeof(*var));
  copy_alma_var(var, theta->list[theta->num_bindings-1].var);
  theta->list[theta->num_bindings-1].var->quasiquotes = 0; // Binding ignores quasi-quote amounts
  theta->list[theta->num_bindings-1].quote_level = quote_level;
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
      dest->list[i].quote_level = src->list[i].quote_level;
      dest->list[i].term = malloc(sizeof(alma_term));
      copy_alma_term(src->list[i].term, dest->list[i].term);
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
