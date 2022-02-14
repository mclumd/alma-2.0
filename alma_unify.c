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

static void adjust_quasiquote_level(alma_term *term, int quote_level, int new_level);

// Calls adjust_quasiquote_level on each term of a clause
static void adjust_clause_quote_level(clause *c, int quote_level, int new_level) {
  for (int i = 0; i < c->pos_count; i++)
    for (int j = 0; j < c->pos_lits[i]->term_count; j++)
      adjust_quasiquote_level(c->pos_lits[i]->terms+j, quote_level, new_level);
  for (int i = 0; i < c->neg_count; i++)
    for (int j = 0; j < c->neg_lits[i]->term_count; j++)
      adjust_quasiquote_level(c->neg_lits[i]->terms+j, quote_level, new_level);
  if (c->tag == FIF) {
    for (int i = 0; i < c->fif->num_conclusions; i++) {
      adjust_clause_quote_level(c->fif->conclusions[i], quote_level, new_level);
    }
  }
}

void increment_quote_level(clause *c, int quote_level) {
  adjust_clause_quote_level(c, quote_level, quote_level+1);
}

void decrement_quote_level(clause *c, int quote_level) {
  adjust_clause_quote_level(c, quote_level, quote_level-1);
}

// Does not recurse; assigns new amount of quasi-quotation to a plain variable/quasi-quote
static void adjust_var_quasiquote_level(alma_term *var, int backticks) {
  if (var->type == VARIABLE && backticks > 0) {
    var->type = QUASIQUOTE;
    alma_variable *temp = var->variable;
    var->quasiquote = malloc(sizeof(*var->quasiquote));
    var->quasiquote->backtick_count = backticks;
    var->quasiquote->variable = temp;
  }
  else if (var->type == QUASIQUOTE && var->quasiquote->backtick_count != backticks) {
    // Override amount of quasi-quotation marks with new
    if (backticks != 0)
      var->quasiquote->backtick_count = backticks;
    // If all quasiquotation would be lost, convert to regular variable
    else {
      var->type = VARIABLE;
      alma_quasiquote *temp = var->quasiquote;
      var->variable = temp->variable;
      free(temp);
    }
  }
}

// Function used for substitution and cases like truth predicate
// Only fully-escaping variables are adjusted to begin inside quotation level new_level
// Thus deeper levels of quotation increment new_level as they recursively descend
static void adjust_quasiquote_level(alma_term *term, int quote_level, int new_level) {
  if (term->type == VARIABLE && quote_level == 0 && new_level != 0) {
    adjust_var_quasiquote_level(term, new_level);
  }
  else if (term->type == FUNCTION) {
    for (int i = 0; i < term->function->term_count; i++)
      adjust_quasiquote_level(term->function->terms+i, quote_level, new_level);
  }
  else if (term->type == QUOTE && term->quote->type == CLAUSE) {
    adjust_clause_quote_level(term->quote->clause_quote, quote_level+1, new_level+1);
  }
  else if (term->type == QUASIQUOTE && term->quasiquote->backtick_count == quote_level) {
    adjust_var_quasiquote_level(term, new_level);
  }
}

// If binding is from a different quote level, adjusts to the arg quote level and backtick amount
static void adjust_context(binding *b, int quote_level, int backticks) {
  if (b->quote_level != quote_level) {
    if (quote_level == backticks) {
      // Fully-escaping quasi-quotation or non-quoted variable
      adjust_quasiquote_level(b->term, b->quote_level, quote_level);
    }
    else {
      // Otherwise, non-escaping case, so binding term must be just var/quasi-quote
      adjust_var_quasiquote_level(b->term, backticks);
    }
    b->quote_level = quote_level;
  }
}

// Only fully-escaping variables are adjusted to have an additional quasiquote backtick
// And, only if corresponding place in the query is something other than a non-escaping variable
// Since term has yet to be placed in its outermost quote context, changes are made when effective quote_level is 1
// Query doesn't have this issue
static void increment_quasiquote_level_paired(alma_term *term, alma_term *query, int quote_level) {
  if (term->type == VARIABLE) {
    if (quote_level == 1 && query != NULL && query->type != VARIABLE && !(query->type == QUASIQUOTE && query->quasiquote->backtick_count < quote_level)) {
      term->type = QUASIQUOTE;
      alma_variable *temp = term->variable;
      term->quasiquote = malloc(sizeof(*term->quasiquote));
      term->quasiquote->backtick_count = 1;
      term->quasiquote->variable = temp;
    }
  }
  else if (term->type == FUNCTION) {
    for (int i = 0; i < term->function->term_count; i++) {
      if (query != NULL && (query->type == VARIABLE || query->type == QUASIQUOTE))
        query = NULL;
      alma_term *query_term = (query == NULL) ? NULL : query->function->terms+i;
      increment_quasiquote_level_paired(term->function->terms+i, query_term, quote_level);
    }
  }
  else if (term->type == QUOTE && term->quote->type == CLAUSE) {
    if (query != NULL && (query->type == VARIABLE || query->type == QUASIQUOTE))
      query = NULL;
    clause *query_clause = (query == NULL) ? NULL : query->quote->clause_quote;
    increment_clause_quote_level_paired(term->quote->clause_quote, query_clause, quote_level+1);
  }
  else if (term->type == QUASIQUOTE && term->quasiquote->backtick_count+1 == quote_level) {
    if (query != NULL && query->type != VARIABLE && !(query->type == QUASIQUOTE && query->quasiquote->backtick_count < quote_level)) {
      term->quasiquote->backtick_count++;
    }
  }
}

// Calls adjust_quasiquote_level on each term of a clause
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

// For a given term, replace its variables present in the binding list with their bindings
void subst_term(binding_list *theta, alma_term *term, int quote_level) {
  if (term->type == VARIABLE) {
    binding *contained = bindings_contain(theta, term->variable);
    // If a variable is bound, replace it with whatever it is bound to
    if (contained != NULL) {
      free(term->variable->name);
      free(term->variable);
      copy_alma_term(contained->term, term);

      // If a variable is being substituted for a binding, and that binding comes from quotation,
      // then must decrease quasi-quotation to match unquoted substitution context
      if (quote_level == 0 && contained->quote_level != quote_level)
        adjust_quasiquote_level(term, contained->quote_level, quote_level);
    }
  }
  else if (term->type == FUNCTION) {
    subst_func(theta, term->function, quote_level);
  }
  else if (term->type == QUOTE && term->quote->type == CLAUSE) {
    subst_clause(theta, term->quote->clause_quote, quote_level+1);
  }
  else if (term->type == QUASIQUOTE) {
    binding *contained = bindings_contain(theta, term->quasiquote->variable);
    // If a variable is bound, replace it with whatever it is bound to
    if (contained != NULL) {
      int quasiquote_amount = term->quasiquote->backtick_count;
      free(term->quasiquote->variable->name);
      free(term->quasiquote->variable);
      free(term->quasiquote);
      copy_alma_term(contained->term, term);

      // If quote levels aren't equal, must adjust when substitute into new quote context
      if (quasiquote_amount == quote_level && contained->quote_level != quote_level)
        adjust_quasiquote_level(term, contained->quote_level, quote_level);
    }
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

static int occurs_check_var(binding_list *theta, alma_variable *var, alma_variable *x) {
  if (x->id == var->id)
    return 1;
  binding *res = bindings_contain(theta, x);
  // If x is a bound variable, occurs-check what it's bound to
  return res != NULL ? occurs_check(theta, var, res->term) : 0;
}

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
    return occurs_check_clause(theta, var, x->quote->clause_quote);
  }
  else if (x->type == QUASIQUOTE) {
    return occurs_check_var(theta, var, x->quasiquote->variable);
  }
  return 0;
}

static int var_insuff_quoted(alma_term *term, int must_exceed_level, int quote_level);

static int func_insuff_quoted(alma_function *func, int must_exceed_level, int quote_level) {
  for (int i = 0; i < func->term_count; i++)
    if (var_insuff_quoted(func->terms+i, must_exceed_level, quote_level))
      return 1;
  return 0;
}

static int clause_var_insuff_quoted(clause *c, int must_exceed_level, int quote_level) {
  for (int i = 0; i < c->pos_count; i++)
    if (func_insuff_quoted(c->pos_lits[i], must_exceed_level, quote_level))
      return 1;
  for (int i = 0; i < c->neg_count; i++)
    if (func_insuff_quoted(c->neg_lits[i], must_exceed_level, quote_level))
      return 1;
  if (c->tag == FIF)
    for (int i = 0; i < c->fif->num_conclusions; i++)
      if (clause_var_insuff_quoted(c->fif->conclusions[i], must_exceed_level, quote_level))
        return 1;
  return 0;
}

// Non-escaping variables must occur in *effective* quotation level above must_exceed_level
// Thus, checks for location of quantifiers inside
// Non-escaping variables violating above condition have quantifiers outside of the arg term; in such cases return 1
// Returning 1 indicates insufficient quotation of a variable, which should then not unify
static int var_insuff_quoted(alma_term *term, int must_exceed_level, int quote_level) {
  if (term->type == VARIABLE) {
    return quote_level <= must_exceed_level;
  }
  else if (term->type == FUNCTION) {
    return func_insuff_quoted(term->function, must_exceed_level, quote_level);
  }
  else if (term->type == QUASIQUOTE) {
    return term->quasiquote->backtick_count != quote_level && quote_level - term->quasiquote->backtick_count <= must_exceed_level;
  }
  else if (term->type == QUOTE && term->quote->type == CLAUSE) {
    return clause_var_insuff_quoted(term->quote->clause_quote, must_exceed_level, quote_level+1);
  }
  return 0;
}

// Cases to check for variables within quotes that don't escape
static int non_escaping_var_failure(alma_variable *x, alma_variable *y, void *x_parent, void *y_parent, int level, binding_list *theta) {
  if (!var_match_consistent(theta->quoted_var_matches, level, x, y)) {
    // Debug
    printf("Var matching not consistent if matching %lld with %lld\n", x->id, y->id);
    return 1;
  }
  // Variables which are from the same (non-null) parent cannot be unified
  if (x_parent == y_parent && x_parent != NULL) {
    // Debug
    printf("Same parent for quoted vars %lld and %lld; unification failure\n", x->id, y->id);
    return 1;
  }
  return 0;
}


// Unification functions

static int unify(alma_term *x, alma_term *y, void *x_parent, void *y_parent, int quote_level, binding_list *theta);
static void add_binding_detailed(binding_list *theta, alma_variable *var, int quote_level, int var_qq_level, alma_term *term, void *parent, int copy_term);

static int unify_quasiquote(alma_term *qqterm, alma_term *x, void *qq_parent, void *x_parent, int quote_level, binding_list *theta) {
  alma_quasiquote *qq = qqterm->quasiquote;

  // Non-escaping quasi-quotes can ONLY unify with another quasi-quote of same backticks
  if (qq->backtick_count < quote_level && (x->type != QUASIQUOTE || qq->backtick_count != x->quasiquote->backtick_count))
    return 0;

  // If qq is a bound variable, unify what it's bound to with x
  binding *res = bindings_contain(theta, qq->variable);
  if (res != NULL) {
    adjust_context(res, quote_level, qq->backtick_count);
    return unify(res->term, x, res->term_parent, x_parent, res->quote_level, theta);
  }
  // If x is a bound quasi-quote, unify qq with what x is bound to
  else if (x->type == QUASIQUOTE && (res = bindings_contain(theta, x->quasiquote->variable)) != NULL) {
    adjust_context(res, quote_level, x->quasiquote->backtick_count);
    return unify(qqterm, res->term, qq_parent, res->term_parent, res->quote_level, theta);
  }
  // Occurs check to avoid infinite regress in bindings
  else if (occurs_check(theta, qq->variable, x)) {
    // Debug
    printf("Occurs check failure for %lld\n", qq->variable->id);
    return 0;
  }
  // Rejecting for unification: terms with non-escaping variables that have their quantifiers outside the quasi-quoted variable
  // A case specific to fully-escaping quasi-quote, given above filtering
  else if (x->type != QUASIQUOTE && var_insuff_quoted(x, quote_level, quote_level)) {
    // Debug
    printf("Cannot unify %lld with term having insufficiently quoted variables\n", qq->variable->id);
    return 0;
  }
  // Failure cases for quasi-quotation
  else if (x->type == QUASIQUOTE) {
    // Check for failures with non-escaping variables
    if (qq->backtick_count < quote_level && non_escaping_var_failure(qq->variable, x->quasiquote->variable, qq_parent, x_parent, quote_level - qq->backtick_count, theta)) {
      return 0;
    }
    // Fully-escaping quasi-quotation case; reject cases with other non-escaping quasi-quote
    else if (qq->backtick_count == quote_level && x->quasiquote->backtick_count < quote_level) {
        // Debug
        printf("Cannot unify %lld with fully-escaping %lld\n", x->quasiquote->variable->id, qq->variable->id);
        return 0;
    }
  }

  add_binding_detailed(theta, qq->variable, quote_level, qq->backtick_count, x, x_parent, 1);
  return 1;
}

// X cannot be quasi-quotation due to ordering of cases in unify()
static int unify_var(alma_term *varterm, alma_term *x, void *var_parent, void *x_parent, int quote_level, binding_list *theta) {
  alma_variable *var = varterm->variable;

  // If the variable is quoted, can only unify with another variable
  if (quote_level > 0 && x->type != VARIABLE)
    return 0;
  // If var is a bound variable, unify what it's bound to with x
  binding *res = bindings_contain(theta, var);
  if (res != NULL) {
    adjust_context(res, quote_level, 0);
    return unify(res->term, x, res->term_parent, x_parent, res->quote_level, theta);
  }
  // If x is a bound variable, unify var with what x is bound to
  else if (x->type == VARIABLE && (res = bindings_contain(theta, x->variable)) != NULL) {
    adjust_context(res, quote_level, 0);
    return unify(varterm, res->term, var_parent, res->term_parent, res->quote_level, theta);
  }
  // Occurs check to avoid infinite regress in bindings
  else if (occurs_check(theta, var, x)) {
    return 0;
  }
  // Check for failures with non-escaping variables
  else if (quote_level > 0 && x->type == VARIABLE && non_escaping_var_failure(var, x->variable, var_parent, x_parent, quote_level, theta)) {
    return 0;
  }

  // X must be a quote, function, or suitable variable
  add_binding_detailed(theta, var, quote_level, 0, x, x_parent, 1);
  return 1;
}

static int unify_function(alma_function *x, alma_function *y, void *x_parent, void *y_parent, int quote_level, binding_list *theta) {
  if (x->term_count != y->term_count || strcmp(x->name, y->name) != 0)
    return 0;
  for (int i = 0; i < x->term_count; i++)
    if (!unify(x->terms+i, y->terms+i, x_parent, y_parent, quote_level, theta))
      return 0;
  return 1;
}

static int unify_clause(clause *x, clause *y, void *x_parent, void *y_parent, int quote_level, binding_list *theta) {
  if (x->tag != y->tag || x->pos_count != y->pos_count || x->neg_count != y->neg_count)
    return 0;

  if (x->tag == FIF) {
    if (x->fif->num_conclusions != y->fif->num_conclusions)
      return 0;
    for (int i = 0; i < x->fif->premise_count; i++)
      if (!unify_function(fif_access(x, i), fif_access(y, i), x_parent, y_parent, quote_level, theta))
        return 0;
    for (int i = 0; i < x->fif->num_conclusions; i++)
      if (!unify_clause(x->fif->conclusions[i], y->fif->conclusions[i], x_parent, y_parent, quote_level, theta))
        return 0;
  }
  else {
    for (int i = 0; i < x->pos_count; i++)
      if (!unify_function(x->pos_lits[i], y->pos_lits[i], x_parent, y_parent, quote_level, theta))
        return 0;
    for (int i = 0; i < x->neg_count; i++)
      if (!unify_function(x->neg_lits[i], y->neg_lits[i], x_parent, y_parent, quote_level, theta))
        return 0;
  }
  return 1;
}

static int unify_quote(alma_quote *x, alma_quote *y, void *x_parent, void *y_parent, int quote_level, binding_list *theta) {
  if (x->type != y->type)
    return 0;

  if (x->type == SENTENCE) {
    // TODO, find way to deal with raw sentence quote unification
    // or otherwise, noted here as a gap
    return 0;
  }
  else
    return unify_clause(x->clause_quote, y->clause_quote, x_parent, y_parent, quote_level, theta);
}

// Unification function based on algorithm in AIMA book
// Modified for quotation and quasi-quootation
static int unify(alma_term *x, alma_term *y, void *x_parent, void *y_parent, int quote_level, binding_list *theta) {
  // Cases where unification succeeds without changing theta:
  //  1. If trying to unify variable with itself
  //  2. If trying to unify quasi-quoted variable with itself
  //  3. If trying to unify variable with quasi-quoted variable of same ID (i.e. also with itself)
  if ((x->type == VARIABLE && y->type == VARIABLE && x->variable->id == y->variable->id)
      || (x->type == QUASIQUOTE && y->type == QUASIQUOTE
          && x->quasiquote->variable->id == y->quasiquote->variable->id)
      || (x->type == VARIABLE && y->type == QUASIQUOTE && x->variable->id == y->quasiquote->variable->id)
      || (x->type == QUASIQUOTE && y->type == VARIABLE && x->quasiquote->variable->id == y->variable->id)) {
    return 1;
  }
  else if (x->type == QUASIQUOTE) {
    return unify_quasiquote(x, y, x_parent, y_parent, quote_level, theta);
  }
  else if (y->type == QUASIQUOTE) {
    return unify_quasiquote(y, x, y_parent, x_parent, quote_level, theta);
  }
  else if (x->type == VARIABLE) {
    return unify_var(x, y, x_parent, y_parent, quote_level, theta);
  }
  else if (y->type == VARIABLE) {
    return unify_var(y, x, y_parent, x_parent, quote_level, theta);
  }
  else if (x->type == FUNCTION && y->type == FUNCTION) {
    return unify_function(x->function, y->function, x_parent, y_parent, quote_level, theta);
  }
  else if (x->type == QUOTE && y->type == QUOTE) {
    return unify_quote(x->quote, y->quote, x_parent, y_parent, quote_level+1, theta);
  }
  else
    return 0;
}


// Functions used outside to initiate unification

int quote_term_unify(alma_quote *x, alma_quote *y, binding_list *theta) {
  return unify_quote(x, y, x, y, 1, theta);
}

int term_unify(alma_term *x, alma_term *y, binding_list *theta) {
  return unify(x, y, x, y, 0, theta);
}

// Non-static function called externally by alma
// Bindings need to COPY variables/terms in formulas, not alias -- handle carefully
// All bindings must be cleaned up by caller
int pred_unify(alma_function *x, alma_function *y, binding_list *theta, int verbose) {
  int ret = unify_function(x, y, x, y, 0, theta);
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

// Append new binding of var/term with additional fields used by quotaton as well
static void add_binding_detailed(binding_list *theta, alma_variable *var, int quote_level, int qq_level, alma_term *term, void *parent, int copy_term) {
  theta->num_bindings++;
  theta->list = realloc(theta->list, sizeof(*theta->list) * theta->num_bindings);
  theta->list[theta->num_bindings-1].var = malloc(sizeof(*var));
  copy_alma_var(var, theta->list[theta->num_bindings-1].var);
  theta->list[theta->num_bindings-1].quote_level = quote_level;
  if (copy_term) {
    theta->list[theta->num_bindings-1].term = malloc(sizeof(*term));
    copy_alma_term(term, theta->list[theta->num_bindings-1].term);
  }
  else {
    theta->list[theta->num_bindings-1].term = term;
  }
  theta->list[theta->num_bindings-1].term_parent = parent;

  // Cascade substitution call to deal with bound variables inside other bindings
  cascade_substitution(theta);
}

// Append new binding of var/term
void add_binding(binding_list *theta, alma_variable *var, alma_term *term, void *parent, int copy_term) {
  add_binding_detailed(theta, var, 0, 0, term, parent, copy_term);
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
