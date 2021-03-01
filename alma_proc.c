#include <string.h>
#include <ctype.h>
#include "alma_proc.h"
#include "tommy.h"

typedef enum introspect_kind {POS_INT_SPEC, POS_INT, POS_INT_GEN, NEG_INT_SPEC, NEG_INT, NEG_INT_GEN, ACQUIRED} introspect_kind;

// Returns a boolean value for if proc matches the procedure schema (binary function proc, first arg function)
int proc_valid(alma_function *proc) {
  return strcmp(proc->name, "proc") == 0 && proc->term_count == 2 && proc->terms[0].type == FUNCTION;
}

// Returns a boolean value for all variables being bound (needed prior to executing procedure)
int proc_bound_check(alma_function *proc, binding_list *bindings) {
  alma_term *bound_list = proc->terms+1;
  if (bound_list->type == FUNCTION) {
    alma_function *func = bound_list->function;
    for (int i = 0; i < func->term_count; i++) {
      // A variable not in argument bindings causes failure
      if (func->terms[i].type == VARIABLE && bindings_contain(bindings, func->terms[i].variable) == NULL)
        return 0;
    }
    return 1;
  }
  return 0;
}

static int structure_match(clause *original, clause *query, introspect_kind kind, int quote_level);

static int quotes_match(alma_quote *original, alma_quote *query, introspect_kind kind, int quote_level) {
  if (original->type == query->type && original->type == CLAUSE)
    return structure_match(original->clause_quote, query->clause_quote, kind, quote_level);
  return 0;
}

static int functions_match(alma_function *original, alma_function *query, introspect_kind kind, int quote_level) {
  if (original->term_count == query->term_count && strcmp(original->name, query->name) == 0) {
    for (int i = 0; i < original->term_count; i++) {
      if (original->terms[i].type == query->terms[i].type) {
        // Pair of variables / quasi-quotes don't need examination
        if ((original->terms[i].type == FUNCTION && !functions_match(original->terms[i].function, query->terms[i].function, kind, quote_level))
            || (original->terms[i].type == QUOTE && !quotes_match(original->terms[i].quote, query->terms[i].quote, kind, quote_level+1)))
          return 0;
      }
      // Non-matching cases fail when original isn't variable, query isn't fully-escaped var (i.e. non-unifying cases)
      else if (!(query->terms[i].type == QUASIQUOTE || query->terms[i].quasiquote->backtick_count == quote_level
            && (kind == POS_INT_GEN || kind == NEG_INT_GEN)) && original->terms[i].type != VARIABLE) {
          return 0;
      }
    }
    return 1;
  }
  return 0;
}

static int structure_match(clause *original, clause *query, introspect_kind kind, int quote_level) {
  if (original->pos_count == query->pos_count && original->neg_count == query->neg_count) {
    for (int i = 0; i < original->pos_count; i++) {
      if (!function_compare(original->pos_lits+i, query->pos_lits+i) || !functions_match(original->pos_lits[i], query->pos_lits[i], kind, quote_level))
        return 0;
    }
    for (int i = 0; i < original->neg_count; i++) {
      if (function_compare(original->neg_lits+i, query->neg_lits+i) || !functions_match(original->neg_lits[i], query->neg_lits[i], kind, quote_level))
        return 0;
    }
  }
  return 1;
}

// Duplicate original into copy as long as the structure of original matches query
// Subject to constaints:
// - If this is a GEN kind of introspection, query cannot have an escaped variable where original does not
// - Original must have predicate and function structure conducive to unifying with query
static int copy_and_quasiquote_clause(clause *original, clause *copy, clause *query, introspect_kind kind) {
  // Scan for sufficient matching structure of original vs query
  if (structure_match(original, query, kind, 0)) {
    copy_clause_structure(original, query);
    // scan and adjust quasiquote in copy
    return 1;
  }
  return 0;
}

// For now, introspect fails on distrusted formulas
// It seems intuitive to reject them, since introspection is based on current knowledge
static int introspect(alma_function *arg, binding_list *bindings, kb *collection, introspect_kind kind) {
  alma_term *query = arg->terms+0;
  binding *res;
  // Argument must be a clause quote or variable bound to one
  if ((query->type != QUOTE || query->quote->type != CLAUSE) && (query->type != VARIABLE || !(res = bindings_contain(bindings, query->variable))
      || res->term->type != QUOTE || res->term->quote->type != CLAUSE)) {
    return 0;
  }

  // Create copy and substitute based on bindings available
  alma_term *search_term = malloc(sizeof(*search_term));
  copy_alma_term(query, search_term);
  subst(bindings, search_term, 0);

  predname_mapping *result = clause_lookup(collection, search_term->quote->clause_quote);
  if (result != NULL) {
    alma_quote *q = malloc(sizeof(*q));
    q->type = CLAUSE;
    q->clause_quote = NULL;

    int non_specific = (kind == POS_INT || kind == POS_INT_GEN || kind == NEG_INT || kind == NEG_INT_GEN);
    for (int i = result->num_clauses-1; i >= 0; i--) {
      // Must be a non-distrusted result with pos / neg literal count matching query
      if (!result->clauses[i]->distrusted
          && result->clauses[i]->pos_count == search_term->quote->clause_quote->pos_count
          && result->clauses[i]->neg_count == search_term->quote->clause_quote->neg_count) {
        // Convert clause in question to quotation term
        if (non_specific) {
          q->clause_quote = malloc(sizeof(*q->clause_quote));
          // copy_and_quasiquote_clause may reject copying, for which would free and continue
          if (!copy_and_quasiquote_clause(result->clauses[i], q->clause_quote, search_term->quote->clause_quote, kind)) {
            free_clause(q->clause_quote);
            q->clause_quote = NULL;
            continue;
          }
        }
        else {
          q->clause_quote = result->clauses[i];
        }

        // Create bindings copy as either empty list or copy of arg
        binding_list *copy = malloc(sizeof(*copy));
        copy_bindings(copy, bindings);

        // Returning first match based at the moment
        if (quote_term_unify(search_term->quote, q, copy)) {
          if (kind != NEG_INT_SPEC && kind != NEG_INT && kind != NEG_INT_GEN)
            swap_bindings(bindings, copy);
          cleanup_bindings(copy);

          if (kind == ACQUIRED) {
            // If they unify, create term out of acquired answer
            alma_term *time_term = malloc(sizeof(*time_term));
            time_term->type = FUNCTION;
            time_term->function = malloc(sizeof(*time_term->function));
            time_term->function->name = long_to_str(result->clauses[i]->acquired);
            time_term->function->term_count = 0;
            time_term->function->terms = NULL;
            add_binding(bindings, arg->terms[1].variable, time_term, arg, 0);
          }

          // Don't free clause unless gen case
          if (!non_specific)
            q->clause_quote = NULL;
          free_quote(q);

          free_term(search_term);
          free(search_term);
          return kind != NEG_INT_SPEC && kind != NEG_INT && kind != NEG_INT_GEN;
        }

        if (non_specific)
          free_clause(q->clause_quote);
        q->clause_quote = NULL;
        cleanup_bindings(copy);
      }
    }
    free_quote(q);
  }

  free_term(search_term);
  free(search_term);
  return kind == NEG_INT_SPEC || kind == NEG_INT || kind == NEG_INT_GEN;
}

// ancestor(A, B) returns true if a A appears as an ancestor in any derivation of B
static int ancestor(alma_term *ancestor, alma_term *descendant, alma_term *time, binding_list *bindings, kb *alma) {
  // Ensure time argument is correctly constructed: either a numeric constant, or a variable bound to one
  binding *res;
  if (time->type == VARIABLE && (res = bindings_contain(bindings, time->variable))) {
    time = res->term;
  }
  if (time->type != FUNCTION || time->function->term_count != 0)
    return 0;
  for (int j = 0; j < strlen(time->function->name); j++) {
    if (!isdigit(time->function->name[j]))
      return 0;
  }
  long query_time = atol(time->function->name);

  // Create copies and substitute based on bindings available
  alma_term *ancestor_copy = malloc(sizeof(*ancestor_copy));
  copy_alma_term(ancestor, ancestor_copy);
  subst(bindings, ancestor_copy, 0);

  alma_term *descendant_copy = malloc(sizeof(*descendant_copy));
  copy_alma_term(descendant, descendant_copy);
  subst(bindings, descendant_copy, 0);

  int has_ancestor = 0;
  if (ancestor_copy->type == QUOTE && ancestor_copy->quote->type == CLAUSE
      && descendant_copy->type == QUOTE && descendant_copy->quote->type == CLAUSE) {
    // Frontier of parents to expand
    tommy_array queue;
    tommy_array_init(&queue);

    // Note: for now, will just use first unifying case for ancestor search
    // If necessary, can make more general at later point, by trying all possibilities
    predname_mapping *result = clause_lookup(alma, descendant_copy->quote->clause_quote);
    binding_list *desc_bindings = NULL;
    if (result != NULL) {
      alma_quote *q = malloc(sizeof(*q));
      q->type = CLAUSE;

      for (int i = result->num_clauses-1; i >= 0; i--) {
        if (result->clauses[i]->pos_count == descendant_copy->quote->clause_quote->pos_count &&
            result->clauses[i]->neg_count == descendant_copy->quote->clause_quote->neg_count &&
            (!result->clauses[i]->distrusted || result->clauses[i]->distrusted >= query_time) &&
            result->clauses[i]->acquired <= query_time) {
          // Create copy as either empty list or copy of arg
          desc_bindings = malloc(sizeof(*desc_bindings));
          copy_bindings(desc_bindings, bindings);
          q->clause_quote = result->clauses[i];

          if (quote_term_unify(descendant_copy->quote, q, desc_bindings)) {
            // Starting item for queue
            tommy_array_insert(&queue, result->clauses[i]);
            break;
          }
          cleanup_bindings(desc_bindings);
          desc_bindings = NULL;
        }
      }

      free(q);
    }

    // Checked items to avoid cycles
    tommy_array checked;
    tommy_array_init(&checked);

    alma_quote *quote_holder = malloc(sizeof(*quote_holder));
    quote_holder->type = CLAUSE;
    // Continue processing queue in breadth-first manner
    for (int curr = 0; curr < tommy_array_size(&queue); curr++) {
      clause *c = tommy_array_get(&queue, curr);

      int present = 0;
      for (int i = 0; i < tommy_array_size(&checked); i++)
        if (tommy_array_get(&checked, i) == c) {
          present = 1;
          break;
        }

      // Clause wasn't in the checked items
      if (!present) {
        tommy_array_insert(&checked, c);

        binding_list *anc_bindings = malloc(sizeof(*anc_bindings));
        copy_bindings(anc_bindings, desc_bindings);

        if (c->pos_count == ancestor_copy->quote->clause_quote->pos_count && c->neg_count == ancestor_copy->quote->clause_quote->neg_count) {
          quote_holder->clause_quote = c;
          // Try unifying pair of quotes
          if (quote_term_unify(ancestor_copy->quote, quote_holder, anc_bindings)) {
            swap_bindings(anc_bindings, bindings);
            cleanup_bindings(anc_bindings);
            has_ancestor = 1;
            break;
          }
        }
        cleanup_bindings(anc_bindings);

        // Queue parents for expansion
        for (int i = 0; i < c->parent_set_count; i++)
          for (int j = 0; j < c->parents[i].count; j++)
            tommy_array_insert(&queue, c->parents[i].clauses[j]);
      }
    }

    if (desc_bindings != NULL)
      cleanup_bindings(desc_bindings);
    free(quote_holder);
    tommy_array_done(&queue);
    tommy_array_done(&checked);
  }

  free_term(ancestor_copy);
  free(ancestor_copy);
  free_term(descendant_copy);
  free(descendant_copy);
  return has_ancestor;
}

// Convert string to long while checking for only digits
static int str_to_long(char *str, long *res) {
  *res = 0;
  for (int i = strlen(str)-1, j = 1; i >= 0; i--, j*=10) {
    if (isdigit(str[i]))
      *res += (str[i] - '0') * j;
    else
      return 0;
  }
  return 1;
}

// Returns true if digit value of x is less than digit value of y
// Else, incuding cases where types differ, false
static int less_than(alma_term *x, alma_term *y, binding_list *bindings, kb *alma) {
  char *x_str;
  char *y_str;
  if (x->type == VARIABLE) {
    binding *res = bindings_contain(bindings, x->variable);
    if (res == NULL || res->term->type != FUNCTION || res->term->function->term_count != 0)
      return 0;
    x_str = res->term->function->name;
  }
  else if (x->type == FUNCTION && x->function->term_count == 0)
    x_str = x->function->name;
  else
    return 0;
  if (y->type == VARIABLE) {
    binding *res = bindings_contain(bindings, y->variable);
    if (res == NULL || res->term->type != FUNCTION || res->term->function->term_count != 0)
      return 0;
    y_str = res->term->function->name;
  }
  else if (y->type == FUNCTION && y->function->term_count == 0)
    y_str = y->function->name;
  else
    return 0;

  long xval, yval;
  if (!str_to_long(x_str, &xval) || !str_to_long(y_str, &yval))
    return 0;
  else
    return xval < yval;
}

// If first argument is a digit that's valid index of formula, binds second arg to quote of its formula and returns true
static int idx_to_form(alma_term *index_term, alma_term *result, binding_list *bindings, kb *alma) {
  char *idx_str;
  if (index_term->type == VARIABLE) {
    binding *res = bindings_contain(bindings, index_term->variable);
    if (res == NULL || res->term->type != FUNCTION || res->term->function->term_count != 0)
      return 0;
    idx_str = res->term->function->name;
  }
  else if (index_term->type == FUNCTION && index_term->function->term_count == 0)
    idx_str = index_term->function->name;
  else
    return 0;

  if (result->type != VARIABLE || bindings_contain(bindings, result->variable))
    return 0;

  long index;
  if (!str_to_long(idx_str, &index))
    return 0;

  index_mapping *map_res = tommy_hashlin_search(&alma->index_map, im_compare, &index, tommy_hash_u64(0, &index, sizeof(index)));
  if (map_res != NULL) {
    clause *formula = map_res->value;
    clause *quote_form = malloc(sizeof(*quote_form));
    copy_clause_structure(formula, quote_form);
    set_variable_ids(quote_form, 1, 1, NULL, alma);

    alma_term *quoted = malloc(sizeof(*quoted));
    quoted->type = QUOTE;
    quoted->quote = malloc(sizeof(*quoted->quote));
    quoted->quote->type = CLAUSE;
    quoted->quote->clause_quote = quote_form;
    // Within idx_to_form proc literal, index_term also can uniquely identify source of resulting binding
    // Used as a proxy for the parent pointer to proc, instead of passing around further info
    add_binding(bindings, result->variable, quoted, index_term, 0);
    return 1;
  }
  else
    return 0;
}

// If first argument is a bound variable, constructs a quotation term out of term it's bound to, if able
// If variable is bound to a function, this is treated as standing for a singleton clause
// If variable is bound to a quote, this can be used directly with no difference
static int quote_cons(alma_term *to_quote, alma_variable *result, binding_list *bindings, kb *alma) {
  binding *res = bindings_contain(bindings, to_quote->variable);
  if (res == NULL) {
    return 0;
  }
  else if (res->term->type == QUOTE) {
    alma_term *quoted = malloc(sizeof(*quoted));
    copy_alma_term(res->term, quoted);
    add_binding(bindings, result, quoted, to_quote, 0);
    return 1;
  }
  else if (res->term->type != FUNCTION) {
    return 0;
  }

  alma_function *func = res->term->function;
  int neg = 0;
  if (strcmp(func->name, "not") == 0) {
    if (func->term_count != 1 || func->terms->type != FUNCTION) {
      return 0;
    }
    func = func->terms->function;
    neg = 1;
  }

  // Copy is constructed into a clause
  clause *formula = malloc(sizeof(*formula));
  formula->pos_count = neg ? 0 : 1;
  formula->neg_count = neg ? 1 : 0;
  formula->pos_lits = neg ? NULL : malloc(sizeof(*formula->pos_lits));
  formula->neg_lits = neg ? malloc(sizeof(*formula->pos_lits)) : NULL;
  if (neg) {
    formula->neg_lits[0] = malloc(sizeof(*formula->neg_lits[0]));
    copy_alma_function(func, formula->neg_lits[0]);
  }
  else {
    formula->pos_lits[0] = malloc(sizeof(*formula->pos_lits[0]));
    copy_alma_function(func, formula->pos_lits[0]);
  }
  formula->parent_set_count = formula->children_count = 0;
  formula->parents = NULL;
  formula->children = NULL;
  formula->tag = NONE;
  formula->fif = NULL;

  // Replace non-escaping variable IDs
  set_variable_ids(formula, 0, 1, NULL, alma);
  // Place inside quotation
  increment_quote_level(formula, 0);

  alma_term *quoted = malloc(sizeof(*quoted));
  quoted->type = QUOTE;
  quoted->quote = malloc(sizeof(*quoted->quote));
  quoted->quote->type = CLAUSE;
  quoted->quote->clause_quote = formula;
  add_binding(bindings, result, quoted, to_quote, 0);
  return 1;
}

// If proc is a valid procedure, runs and returns truth value
int proc_run(alma_function *proc, binding_list *bindings, kb *alma) {
  alma_function *func = proc->terms[0].function;
  if (strstr(func->name, "neg_int") == func->name) {
    // Must match (given bindings) the schema neg_int(literal(...)) / neg_int_gen(literal(...))
    if (func->term_count == 1) {
      if (strlen(func->name) == strlen("neg_int")) {
        return introspect(func, bindings, alma, NEG_INT);
      }
      else if (strcmp(func->name, "neg_int_spec") == 0) {
        return introspect(func, bindings, alma, NEG_INT_SPEC);
      }
      else if (strcmp(func->name, "neg_int_gen") == 0) {
        return introspect(func, bindings, alma, NEG_INT_GEN);
      }
    }
  }
  else if (strstr(func->name, "pos_int") == func->name) {
    // Must match (given bindings) the schema pos_int(literal(...)) / pos_int_gen(literal(...))
    if (func->term_count == 1) {
      if (strlen(func->name) == strlen("pos_int")) {
        return introspect(func, bindings, alma, POS_INT);
      }
      else if (strcmp(func->name, "pos_int_spec") == 0) {
        return introspect(func, bindings, alma, POS_INT_SPEC);
      }
      else if (strcmp(func->name, "pos_int_gen") == 0) {
        return introspect(func, bindings, alma, POS_INT_GEN);
      }
    }
  }
  else if (strcmp(func->name, "acquired") == 0) {
    // Must match (given bindings) the schema acquired(literal(...), Var) OR acquired(not(literal(...)), Var); Var must be unbound
    if (func->term_count == 2 && func->terms[1].type == VARIABLE && !bindings_contain(bindings, func->terms[1].variable))
      return introspect(func, bindings, alma, ACQUIRED);
  }
  else if (strcmp(func->name, "ancestor") == 0) {
    if (func->term_count == 3)
      return ancestor(func->terms+0, func->terms+1, func->terms+2, bindings, alma);
  }
  else if (strcmp(func->name, "less_than") == 0) {
    if (func->term_count == 2)
      return less_than(func->terms+0, func->terms+1, bindings, alma);
  }
  else if (strcmp(func->name, "idx_to_form") == 0) {
    if (func->term_count == 2)
      return idx_to_form(func->terms+0, func->terms+1, bindings, alma);
  }
  else if (strcmp(func->name, "quote_cons") == 0) {
    if (func->term_count == 2 && func->terms[0].type == VARIABLE && func->terms[1].type == VARIABLE
        && !bindings_contain(bindings, func->terms[1].variable))
      return quote_cons(func->terms+0, (func->terms+1)->variable, bindings, alma);
  }
  return 0;
}
