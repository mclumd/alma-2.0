#include <string.h>
#include <ctype.h>
#include "alma_proc.h"
#include "alma_print.h"
#include "tommy.h"
#include "alma_fif.h"

typedef enum introspect_kind {POS_INT_SPEC, POS_INT, POS_INT_GEN, NEG_INT_SPEC, NEG_INT, NEG_INT_GEN, ACQUIRED} introspect_kind;

// Returns a boolean value for if proc matches a procedure name
int is_proc(alma_function *proc, kb *alma) {
  for (int i = 0; i < sizeof(alma->procs)/sizeof(alma->procs[0]); i++)
    if (strcmp(alma->procs[i].name, proc->name) == 0)
      return 1;
  return 0;
}

// Returns a boolean value for all variables being bound (needed prior to executing procedure)
int proc_bound_check(alma_function *proc, binding_list *bindings, kb *alma) {
  int arity = 0;
  for (int i = 0; i < sizeof(alma->procs)/sizeof(alma->procs[0]); i++) {
    if (strcmp(alma->procs[i].name, proc->name) == 0) {
      arity = alma->procs[i].arity;
      break;
    }
  }
  // When an additional argument is given, the final argument indicates provided binding constraints
  if (proc->term_count == arity +1) {
    alma_term *bound_list = proc->terms+(proc->term_count-1);
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
  return 1;
}

// Structure-checking code

static int structure_match(clause *original, clause *query, introspect_kind kind, int quote_level);

static int quote_structure_match(alma_quote *original, alma_quote *query, introspect_kind kind, int quote_level) {
  if (original->type == query->type && original->type == CLAUSE)
    return structure_match(original->clause_quote, query->clause_quote, kind, quote_level);
  return 0;
}

static int function_structure_match(alma_function *original, alma_function *query, introspect_kind kind, int quote_level) {
  if (original->term_count == query->term_count && strcmp(original->name, query->name) == 0) {
    for (int i = 0; i < original->term_count; i++) {
      // Pair of variables / quasi-quotes doesn't need examination
      if (original->terms[i].type == query->terms[i].type) {
        if ((original->terms[i].type == FUNCTION && !function_structure_match(original->terms[i].function, query->terms[i].function, kind, quote_level))
            || (original->terms[i].type == QUOTE && !quote_structure_match(original->terms[i].quote, query->terms[i].quote, kind, quote_level+1)))
          return 0;
      }
      // Non-matching cases fail when original isn't fully-escaped variable and query isn't fully-escaped var (i.e. non-unifying cases)
      // Doing pos_int_gen/neg_int_gen prevents the case of query as fully-escaped var here
      else if (!(query->terms[i].type == QUASIQUOTE && query->terms[i].quasiquote->backtick_count == quote_level
               && kind != POS_INT_GEN && kind != NEG_INT_GEN) && original->terms[i].type != VARIABLE
               && !(original->terms[i].type == QUASIQUOTE && original->terms[i].quasiquote->backtick_count == quote_level-1)) {
        return 0;
      }
    }
    return 1;
  }
  return 0;
}

static int structure_match(clause *original, clause *query, introspect_kind kind, int quote_level) {
  if (original->tag == query->tag) {
    if (original->tag == FIF) {
      if (original->fif->premise_count == query->fif->premise_count && original->fif->num_conclusions == query->fif->num_conclusions) {
        for (int i = 0; i < original->fif->premise_count; i++) {
          if (!function_structure_match(fif_access(original, i), fif_access(query, i), kind, quote_level))
            return 0;
        }

        for (int i = 0; i < original->fif->num_conclusions; i++) {
          if (!structure_match(original->fif->conclusions[i], query->fif->conclusions[i], kind, quote_level))
            return 0;
        }
        return 1;
      }
    }
    else if (original->pos_count == query->pos_count && original->neg_count == query->neg_count) {
      for (int i = 0; i < original->pos_count; i++) {
        if (!function_structure_match(original->pos_lits[i], query->pos_lits[i], kind, quote_level))
          return 0;
      }
      for (int i = 0; i < original->neg_count; i++) {
        if (!function_structure_match(original->neg_lits[i], query->neg_lits[i], kind, quote_level))
          return 0;
      }
      return 1;
    }
  }
  return 0;
}

// Duplicate original into copy as long as the structure of original matches query
// Subject to constaints:
// - If this is a GEN kind of introspection, query cannot have an escaped variable where original does not
// - Original must have predicate and function structure conducive to unifying with query
static int copy_and_quasiquote_clause(clause *original, clause *copy, clause *query, introspect_kind kind) {
  // Scan for sufficient matching structure of original vs query
  if (structure_match(original, query, kind, 1)) {
    copy_clause_structure(original, copy);
    increment_clause_quote_level_paired(copy, query, 1);
    return 1;
  }
  return 0;
}

// Procedure functions and minor helpers

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

// For now, introspect fails on flagged formulas
// It seems intuitive to reject them, since introspection is based on current knowledge
static int introspect(alma_function *arg, binding_list *bindings, kb *alma, introspect_kind kind, fif_task *task) {
  alma_term *query = arg->terms+0;
  binding *res;
  // Argument must be a clause quote or variable bound to one
  if ((query->type != QUOTE || query->quote->type != CLAUSE) && (query->type != VARIABLE || !(res = bindings_contain(bindings, query->variable))
      || res->term->type != QUOTE || res->term->quote->type != CLAUSE)) {
    return 0;
  }

  // When second acquired arg is a bound variable, it must have a numeric value; obtain long from it
  long acquired_time_bound = 0;
  binding *time_binding = NULL;
  if (kind == ACQUIRED && (time_binding = bindings_contain(bindings, arg->terms[1].variable))) {
    if (time_binding->term->type != FUNCTION || time_binding->term->function->term_count != 0)
      return 0;
    else
      str_to_long(time_binding->term->function->name, &acquired_time_bound);
  }

  // Create copy and substitute based on bindings available
  alma_term *search_term = malloc(sizeof(*search_term));
  copy_alma_term(query, search_term);
  subst_term(bindings, search_term, 0);

  if (alma->verbose) {
    if (kind == POS_INT_SPEC)
      tee_alt("Performing pos_int_spec on \"", alma, NULL);
    else if (kind == POS_INT)
      tee_alt("Performing pos_int on \"", alma, NULL);
    else if (kind == POS_INT_GEN)
      tee_alt("Performing pos_int_gen on \"", alma, NULL);
    else if (kind == NEG_INT_SPEC)
      tee_alt("Performing neg_int_spec on \"", alma, NULL);
    else if (kind == NEG_INT)
      tee_alt("Performing neg_int on \"", alma, NULL);
    else if (kind == NEG_INT_GEN)
      tee_alt("Performing neg_int_gen on \"", alma, NULL);
    else if (kind == ACQUIRED)
      tee_alt("Performing acquired on \"", alma, NULL);
    clause_print(alma, search_term->quote->clause_quote, NULL);
    tee_alt("\"\n", alma, NULL);
  }

  void *mapping = clause_lookup(alma, search_term->quote->clause_quote);
  if (mapping != NULL) {
    alma_quote *q = malloc(sizeof(*q));
    q->type = CLAUSE;
    q->clause_quote = NULL;

    int non_specific = (kind == POS_INT || kind == POS_INT_GEN || kind == NEG_INT || kind == NEG_INT_GEN);
    for (int i = mapping_num_clauses(mapping, search_term->quote->clause_quote->tag)-1; i >= 0; i--) {
      clause *ith = mapping_access(mapping, search_term->quote->clause_quote->tag, i);
      // Must be a non-flagged result with pos / neg literal count matching query
      // If doing acquired proc, and the time argument was bound, it must match the clause
      if (flags_negative(ith) && counts_match(ith, search_term->quote->clause_quote)
          && (kind != ACQUIRED || !acquired_time_bound || (ith->acquired == acquired_time_bound))) {
        // Convert clause in question to quotation term
        if (non_specific) {
          q->clause_quote = malloc(sizeof(*q->clause_quote));
          // copy_and_quasiquote_clause may reject copying, for which would free and continue
          if (!copy_and_quasiquote_clause(ith, q->clause_quote, search_term->quote->clause_quote, kind)) {
            if (alma->verbose) {
              tee_alt("Structure failure of \"", alma, NULL);
              clause_print(alma, ith, NULL);
              tee_alt("\"\n", alma, NULL);
            }

            free(q->clause_quote);
            q->clause_quote = NULL;
            continue;
          }
        }
        else {
          q->clause_quote = ith;
        }

        // Create bindings copy as either empty list or copy of arg
        binding_list *copy = malloc(sizeof(*copy));
        copy_bindings(copy, bindings);

        if (alma->verbose) {
          tee_alt("Attempting to unify with \"", alma, NULL);
          clause_print(alma, q->clause_quote, NULL);
          tee_alt("\"\n", alma, NULL);
        }

        // Returning first match based at the moment
        if (quote_term_unify(search_term->quote, q, copy)) {
          if (alma->verbose)
            tee_alt("Unification succeeded\n", alma, NULL);

          // If an introspect call which passes in task info, add unified clause to the task
          if (task != NULL) {
            task->num_unified++;
            task->unified_clauses = realloc(task->unified_clauses, sizeof(*task->unified_clauses)*task->num_unified);
            task->unified_clauses[task->num_unified-1] = ith;
          }

          if (kind != NEG_INT_SPEC && kind != NEG_INT && kind != NEG_INT_GEN)
            swap_bindings(bindings, copy);
          cleanup_bindings(copy);

          if (kind == ACQUIRED) {
            // If they unify, create term out of acquired answer
            alma_term *time_term = malloc(sizeof(*time_term));
            func_from_long(time_term, ith->acquired);
            add_binding(bindings, arg->terms[1].variable, time_term, arg, 0);
          }

          // Don't free clause unless gen case
          if (!non_specific)
            q->clause_quote = NULL;
          free_quote(q);

          if (alma->verbose)
            tee_alt("\n", alma, NULL);

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

  if (alma->verbose)
    tee_alt("\n", alma, NULL);

  free_term(search_term);
  free(search_term);
  return kind == NEG_INT_SPEC || kind == NEG_INT || kind == NEG_INT_GEN;
}

// ancestor(A, B) returns true if a A appears as an ancestor in any derivation of B
static int ancestor(alma_term *ancestor, alma_term *descendant, alma_term *time, binding_list *bindings, kb *alma, int neg, int parent) {
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
  subst_term(bindings, ancestor_copy, 0);

  alma_term *descendant_copy = malloc(sizeof(*descendant_copy));
  copy_alma_term(descendant, descendant_copy);
  subst_term(bindings, descendant_copy, 0);

  int has_ancestor = 0;
  if (ancestor_copy->type == QUOTE && ancestor_copy->quote->type == CLAUSE &&
      descendant_copy->type == QUOTE && descendant_copy->quote->type == CLAUSE) {
    if (alma->verbose) {
      tee_alt("Performing ancestor proc on descendant \"", alma, NULL);
      clause_print(alma, descendant_copy->quote->clause_quote, NULL);
      tee_alt("\" for ancestor \"", alma, NULL);
      clause_print(alma, ancestor_copy->quote->clause_quote, NULL);
      tee_alt("\"\n", alma, NULL);
    }

    void *mapping = clause_lookup(alma, descendant_copy->quote->clause_quote);
    if (mapping != NULL) {
      alma_quote *quote_holder = malloc(sizeof(*quote_holder));
      quote_holder->type = CLAUSE;

      for (int i = mapping_num_clauses(mapping, descendant_copy->quote->clause_quote->tag)-1; i >= 0; i--) {
        clause *ith = mapping_access(mapping, descendant_copy->quote->clause_quote->tag, i);

        if (alma->verbose) {
          tee_alt("Processing \"", alma, NULL);
          clause_print(alma, ith, NULL);
          tee_alt("\"\n", alma, NULL);
        }

        if (counts_match(ith, descendant_copy->quote->clause_quote) && ith->acquired <= query_time
            && (flags_negative(ith) || flag_active_at_least(ith, query_time))) {
          // Create copy as either empty list or copy of arg
          binding_list *desc_bindings = malloc(sizeof(*desc_bindings));
          copy_bindings(desc_bindings, bindings);
          quote_holder->clause_quote = ith;

          if (quote_term_unify(descendant_copy->quote, quote_holder, desc_bindings)) {
            // Frontier of parents to expand
            tommy_array queue;
            tommy_array_init(&queue);
            // Checked items to avoid cycles
            tommy_array checked;
            tommy_array_init(&checked);

            // Parents as starting items for queue
            for (int j = 0; j < ith->parent_set_count; j++)
              for (int k = 0; k < ith->parents[j].count; k++)
                tommy_array_insert(&queue, ith->parents[j].clauses[k]);

            // Process queue in breadth-first manner
            for (int curr = 0; curr < tommy_array_size(&queue); curr++) {
              clause *c = tommy_array_get(&queue, curr);

              int present = 0;
              for (int j = 0; j < tommy_array_size(&checked); j++) {
                if (tommy_array_get(&checked, j) == c) {
                  present = 1;
                  break;
                }
              }

              // Clause wasn't in the checked items
              if (!present) {
                tommy_array_insert(&checked, c);

                binding_list *anc_bindings = malloc(sizeof(*anc_bindings));
                copy_bindings(anc_bindings, desc_bindings);

                if (counts_match(c, ancestor_copy->quote->clause_quote)
                    && (flags_negative(c) || flag_active_at_least(c, query_time))) {
                  if (alma->verbose) {
                    tee_alt("Attempting to unify with \"", alma, NULL);
                    clause_print(alma, c, NULL);
                    tee_alt("\"\n", alma, NULL);
                  }
                  quote_holder->clause_quote = c;

                  // Try unifying pair of quotes
                  if (quote_term_unify(ancestor_copy->quote, quote_holder, anc_bindings)) {
                    swap_bindings(anc_bindings, bindings);
                    cleanup_bindings(anc_bindings);
                    tommy_array_done(&queue);
                    tommy_array_done(&checked);
                    free(quote_holder);
                    cleanup_bindings(desc_bindings);
                    has_ancestor = 1;
                    goto cleanup;
                  }
                }
                else if (alma->verbose) {
                  tee_alt("Processing from queue \"", alma, NULL);
                  clause_print(alma, c, NULL);
                  tee_alt("\"\n", alma, NULL);
                }
                cleanup_bindings(anc_bindings);

                // Queue parents for expansion only when procedure is ancestor/non_ancestor, not parent
                if (!parent) {
                  for (int j = 0; j < c->parent_set_count; j++)
                    for (int k = 0; k < c->parents[j].count; k++)
                      tommy_array_insert(&queue, c->parents[j].clauses[k]);
                }
              }
            }

            tommy_array_done(&queue);
            tommy_array_done(&checked);
          }
          cleanup_bindings(desc_bindings);
        }
      }
      free(quote_holder);
    }
  }

  cleanup:
  free_term(ancestor_copy);
  free(ancestor_copy);
  free_term(descendant_copy);
  free(descendant_copy);

  if (neg)
    has_ancestor = !has_ancestor;

  if (alma->verbose) {
    if (has_ancestor)
      tee_alt("Ancestor succeeded!\n", alma, NULL);
    else
      tee_alt("Ancestor failure\n\n", alma, NULL);
  }

  return has_ancestor;
}

// Helper function to check a specific clause
// Returns 1 if each derivation has default parent out of the parent set
// Parent set must all be current beliefs to count toward result
static int check_default_parents(clause *c, kb *alma) {
  for (int i = 0; i < c->parent_set_count; i++) {
    if (alma->verbose)
      tee_alt("Processing parent set %d\n", alma, NULL, i);
    // Determine flags are negative for parent set first
    int flags_neg = 1;
    for (int j = 0; j < c->parents[i].count; j++) {
      if (!flags_negative(c->parents[i].clauses[j])) {
        flags_neg = 0;
        break;
      }
    }
    if (flags_neg) {
      if (alma->verbose)
        tee_alt("Flags negative\n", alma, NULL);
      // Then check for presence of default
      int default_fif = 0;
      for (int j = 0; j < c->parents[i].count; j++) {
        if (alma->verbose)
          tee_alt(" Processing parent index %ld\n", alma, NULL, c->parents[i].clauses[j]->index);
        if (c->parents[i].clauses[j]->tag == FIF) {
          for (int k = 0; k < c->parents[i].clauses[j]->fif->premise_count; k++) {
            alma_function *premise = fif_access(c->parents[i].clauses[j], k);
            // If premise is neg_int proc, with "abnormal(...)" arg, it's a default
            if (strcmp(premise->name, "neg_int") == 0 && (premise->term_count == 1 || premise->term_count == 2)
                && premise->terms->type == QUOTE && premise->terms->quote->type == CLAUSE
                && premise->terms->quote->clause_quote->neg_count == 0
                && premise->terms->quote->clause_quote->pos_count == 1
                && strcmp(premise->terms->quote->clause_quote->pos_lits[0]->name, "abnormal") == 0
                && premise->terms->quote->clause_quote->pos_lits[0]->term_count == 3) {
              default_fif = 1;
              if (alma->verbose)
                tee_alt(" Default parent found\n", alma, NULL);
              break;
            }
          }
        }
      }
      if (!default_fif) {
        return 0;
        if (alma->verbose)
          tee_alt(" No default parent found; failing\n", alma, NULL);
      }
    }
  }
  return 1;
}

// parents_defaults(F, T) returns true if each derivation of F has a default fif as an immediate parent
// parent_non_default(F, T) returns true if there exists a derivation of F without default fif as an immediate parent
static int parents_defaults(alma_term *arg, alma_term *time, binding_list *bindings, kb *alma, int invert) {
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

  // Create copy and substitute based on bindings available
  alma_term *arg_copy = malloc(sizeof(*arg_copy));
  copy_alma_term(arg, arg_copy);
  subst_term(bindings, arg_copy, 0);

  int result = 0;
  if (arg_copy->type == QUOTE && arg_copy->quote->type == CLAUSE) {
    if (alma->verbose) {
      if (invert)
        tee_alt("Performing parent_non_default proc on \"", alma, NULL);
      else
        tee_alt("Performing parents_default proc on \"", alma, NULL);
      clause_print(alma, arg_copy->quote->clause_quote, NULL);
      tee_alt("\"\n", alma, NULL);
    }

    void *mapping = clause_lookup(alma, arg_copy->quote->clause_quote);
    if (mapping != NULL) {
      alma_quote *quote_holder = malloc(sizeof(*quote_holder));
      quote_holder->type = CLAUSE;

      for (int i = mapping_num_clauses(mapping, arg_copy->quote->clause_quote->tag)-1; i >= 0; i--) {
        clause *ith = mapping_access(mapping, arg_copy->quote->clause_quote->tag, i);
        if (counts_match(ith, arg_copy->quote->clause_quote) && ith->acquired <= query_time
            && (flags_negative(ith) || flag_active_at_least(ith, query_time))) {
          // Create copy as either empty list or copy of arg
          binding_list *arg_bindings = malloc(sizeof(*arg_bindings));
          copy_bindings(arg_bindings, bindings);
          quote_holder->clause_quote = ith;

          // If unification followed by parents checking succeeds, cleans up and returns true
          if (quote_term_unify(arg_copy->quote, quote_holder, arg_bindings)) {
            if (alma->verbose) {
              tee_alt("Processing \"", alma, NULL);
              clause_print(alma, ith, NULL);
              tee_alt("\"\n", alma, NULL);
            }

            int check = check_default_parents(ith, alma);
            if ((check && !invert) || (!check && invert)) {
              swap_bindings(arg_bindings, bindings);
              cleanup_bindings(arg_bindings);
              result = 1;
              break;
            }
          }
          cleanup_bindings(arg_bindings);
        }
      }
      free(quote_holder);
    }
  }

  free_term(arg_copy);
  free(arg_copy);

  if (alma->verbose) {
    if (result) {
      if (invert)
        tee_alt("Has non-default parents\n", alma, NULL);
      else
        tee_alt("All parents default\n", alma, NULL);
    }
    else {
      if (invert)
        tee_alt("Failed to find non-default parent\n\n", alma, NULL);
      else
        tee_alt("Failed to find all parents defaults\n\n", alma, NULL);
    }
  }

  return result;
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

// not_equal(A, B) returns true if A and B with bindings substituted are not identical clause quotes
static int not_equal(alma_term *arg1, alma_term *arg2, binding_list *bindings, kb *alma) {
  // Create copies and substitute based on bindings available
  alma_term *arg1_copy = malloc(sizeof(*arg1_copy));
  copy_alma_term(arg1, arg1_copy);
  subst_term(bindings, arg1_copy, 0);

  alma_term *arg2_copy = malloc(sizeof(*arg2_copy));
  copy_alma_term(arg2, arg2_copy);
  subst_term(bindings, arg2_copy, 0);

  int not_equal = 1;
  if (arg1_copy->type == QUOTE && arg1_copy->quote->type == CLAUSE &&
      arg2_copy->type == QUOTE && arg2_copy->quote->type == CLAUSE) {
    if (alma->verbose) {
      tee_alt("Testing not-equal on \"", alma, NULL);
      clause_print(alma, arg1_copy->quote->clause_quote, NULL);
      tee_alt("\" and \"", alma, NULL);
      clause_print(alma, arg2_copy->quote->clause_quote, NULL);
      tee_alt("\"\n", alma, NULL);
    }

    not_equal = clauses_differ(arg1_copy->quote->clause_quote, arg2_copy->quote->clause_quote);
  }

  free_term(arg1_copy);
  free(arg1_copy);
  free_term(arg2_copy);
  free(arg2_copy);

  return not_equal;
}


// If proc is a valid procedure, runs and returns truth value
int proc_run(alma_function *proc, binding_list *bindings, fif_task *task, kb *alma) {
  // Each procedure has optional extra argument for bound constraint
  if (strstr(proc->name, "neg_int") == proc->name) {
    // Must match (given bindings) the schema neg_int("...") / neg_int_gen("...") / neg_int_spec("...")
    if (proc->term_count == 1 || proc->term_count == 2) {
      introspect_kind type = NEG_INT;
      if (strcmp(proc->name, "neg_int_spec") == 0)
        type = NEG_INT_SPEC;
      else if (strcmp(proc->name, "neg_int_gen") == 0)
        type = NEG_INT_GEN;
      return introspect(proc, bindings, alma, type, NULL);
    }
  }
  else if (strstr(proc->name, "pos_int") == proc->name) {
    // Must match (given bindings) the schema pos_int("...") / pos_int_gen("...") / pos_int_spec("...")
    if (proc->term_count == 1 || proc->term_count == 2) {
      introspect_kind type = POS_INT;
      if (strcmp(proc->name, "pos_int_spec") == 0)
        type = POS_INT_SPEC;
      else if (strcmp(proc->name, "pos_int_gen") == 0)
        type = POS_INT_GEN;
      return introspect(proc, bindings, alma, type, task);
    }
  }
  else if (strcmp(proc->name, "acquired") == 0) {
    // Must match (given bindings) the schema acquired("...", Var)
    if ((proc->term_count == 2 || proc->term_count == 3) && proc->terms[1].type == VARIABLE)
      return introspect(proc, bindings, alma, ACQUIRED, NULL);
  }
  else if (strcmp(proc->name, "ancestor") == 0) {
    // Must match (given bindings) the schema ancestor("...", "...", Time)
    if (proc->term_count == 3 || proc->term_count == 4)
      return ancestor(proc->terms+0, proc->terms+1, proc->terms+2, bindings, alma, 0, 0);
  }
  else if (strcmp(proc->name, "non_ancestor") == 0) {
    // Must match (given bindings) the schema non_ancestor("...", "...", Time)
    if (proc->term_count == 3 || proc->term_count == 4)
      return ancestor(proc->terms+0, proc->terms+1, proc->terms+2, bindings, alma, 1, 0);
  }
  else if (strcmp(proc->name, "parent") == 0) {
    // Must match (given bindings) the schema parent("...", "...", Time)
    if (proc->term_count == 3 || proc->term_count == 4)
      return ancestor(proc->terms+0, proc->terms+1, proc->terms+2, bindings, alma, 0, 1);
  }
  else if (strcmp(proc->name, "parents_defaults") == 0) {
    // Must match (given_bindings) the schema parents_defaults("...", Time)
    if (proc->term_count == 2 || proc->term_count == 3)
      return parents_defaults(proc->terms+0, proc->terms+1,bindings, alma, 0);
  }
  else if (strcmp(proc->name, "parent_non_default") == 0) {
    // Must match (given_bindings) the schema parent_non_default("...", Time)
    if (proc->term_count == 2 || proc->term_count == 3)
      return parents_defaults(proc->terms+0, proc->terms+1,bindings, alma, 1);
  }
  else if (strcmp(proc->name, "less_than") == 0) {
    if (proc->term_count == 2 || proc->term_count == 3)
      return less_than(proc->terms+0, proc->terms+1, bindings, alma);
  }
  else if (strcmp(proc->name, "quote_cons") == 0) {
    if ((proc->term_count == 2 || proc->term_count == 3)
        && proc->terms[0].type == VARIABLE && proc->terms[1].type == VARIABLE
        && !bindings_contain(bindings, proc->terms[1].variable))
      return quote_cons(proc->terms+0, (proc->terms+1)->variable, bindings, alma);
  }
  else if (strcmp(proc->name, "not_equal") == 0) {
    if (proc->term_count == 2)
      return not_equal(proc->terms+0, proc->terms+1, bindings, alma);
  }
  return 0;
}
