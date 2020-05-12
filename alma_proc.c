#include <string.h>
#include <ctype.h>
#include "alma_proc.h"
#include "tommy.h"

#define run_pos_int 0
#define run_neg_int 1
#define run_learned 2

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

static int introspect(alma_function *arg, binding_list *bindings, kb *collection, int kind) {
  // Create copy and substitute based on bindings available
  alma_term *search_term = malloc(sizeof(*search_term));
  copy_alma_term(arg->terms, search_term);
  subst(bindings, search_term);

  alma_function *search = NULL;
  tommy_hashlin *map = &collection->pos_map;

  if (search_term->type == VARIABLE) {
    free_term(search_term);
    free(search_term);
    return 0;
  }
  else {
    search = search_term->function;

    // Extract from not, if present
    if (strcmp(search->name, "not") == 0) {
      map = &collection->neg_map;
      if (search->term_count != 1) {
        free_term(search_term);
        free(search_term);
        return 0;
      }

      alma_term *temp = search_term;
      search_term = search->terms;
      free(temp->function->name);
      free(temp->function);
      free(temp);

      if (search_term->type == FUNCTION)
        search = search_term->function;
      else {
        free_term(search_term);
        free(search_term);
        return 0;
      }
    }
  }

  char *name = name_with_arity(search->name, search->term_count);
  predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
  free(name);
  if (result != NULL) {
    for (int i = result->num_clauses-1; i >= 0; i--) {
      // Must be a non-distrusted singleton result
      if (!is_distrusted(collection, result->clauses[i]->index) && result->clauses[i]->pos_count + result->clauses[i]->neg_count == 1) {
        alma_function *lit = (map == &collection->pos_map ? result->clauses[i]->pos_lits[0] : result->clauses[i]->neg_lits[0]);

        // Create copy as either empty list or copy of arg
        binding_list *copy = malloc(sizeof(*copy));
        copy_bindings(copy, bindings);

        // Returning first match based at the moment
        if (pred_unify(search, lit, copy)) {
          if (kind != run_neg_int)
            swap_bindings(bindings, copy);
          cleanup_bindings(copy);

          if (kind == run_learned) {
            // If they unify, create term out of learned answer
            alma_term *time_term = malloc(sizeof(*time_term));
            time_term->type = FUNCTION;
            time_term->function = malloc(sizeof(*time_term->function));
            time_term->function->name = long_to_str(result->clauses[i]->learned);
            time_term->function->term_count = 0;
            time_term->function->terms = NULL;

            // Insert answer of learned query into binding set
            bindings->num_bindings++;
            bindings->list = realloc(bindings->list, sizeof(*bindings->list) * bindings->num_bindings);
            bindings->list[bindings->num_bindings-1].var = malloc(sizeof(alma_variable));
            copy_alma_var(arg->terms[1].variable, bindings->list[bindings->num_bindings-1].var);
            bindings->list[bindings->num_bindings-1].term = time_term;
          }

          free_term(search_term);
          free(search_term);
          return kind != run_neg_int;
        }
        cleanup_bindings(copy);
      }
    }
  }
  free_term(search_term);
  free(search_term);
  return kind == run_neg_int;
}

// ancestor(A, B) returns true if a A appears as an ancestor in any derivation of B
static int ancestor(alma_term *ancestor, alma_term *descendant, binding_list *bindings, kb *alma) {
  // Create copies and substitute based on bindings available
  alma_term *ancestor_copy = malloc(sizeof(*ancestor_copy));
  copy_alma_term(ancestor, ancestor_copy);
  subst(bindings, ancestor_copy);

  alma_term *descendant_copy = malloc(sizeof(*descendant_copy));
  copy_alma_term(descendant, descendant_copy);
  subst(bindings, descendant_copy);

  int has_ancestor = 0;
  if (ancestor_copy->type == FUNCTION && descendant_copy->type == FUNCTION) {
    alma_function *ancestor_f = ancestor_copy->function;
    alma_function *descendant_f = descendant_copy->function;

    int a_pos = 1;
    // Extract from not, if present
    if (strcmp(ancestor_f->name, "not") == 0) {
      a_pos = 0;
      if (ancestor_f->term_count != 1)
        goto ancestor_ret;

      alma_term *temp = ancestor_copy;
      ancestor_copy = ancestor_f->terms;
      free(temp->function->name);
      free(temp->function);
      free(temp);
      if (ancestor_copy->type != FUNCTION)
        goto ancestor_ret;

      ancestor_f = ancestor_copy->function;
    }

    int d_pos = 1;
    tommy_hashlin *d_map = &alma->pos_map;
    if (strcmp(descendant_f->name, "not") == 0) {
      d_pos = 0;
      d_map = &alma->neg_map;
      if (descendant_f->term_count != 1)
        goto ancestor_ret;

      alma_term *temp = descendant_copy;
      descendant_copy = descendant_f->terms;
      free(temp->function->name);
      free(temp->function);
      free(temp);
      if (descendant_copy->type != FUNCTION)
        goto ancestor_ret;

      descendant_f = descendant_copy->function;
    }

    // Frontier of parents to expand
    tommy_array queue;
    tommy_array_init(&queue);

    binding_list *desc_bindings = NULL;
    // Locate clause unifying with descendant
    // Note: for now, will just use first unifying case for ancestor search
    // If necessary, can make more general at later point, by trying all possibilities
    char *name = name_with_arity(descendant_f->name, descendant_f->term_count);
    predname_mapping *result = tommy_hashlin_search(d_map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    free(name);
    if (result != NULL) {
      for (int i = result->num_clauses-1; i >= 0; i--) {
        if (result->clauses[i]->pos_count + result->clauses[i]->neg_count == 1) {
          alma_function *lit = (d_pos ? result->clauses[i]->pos_lits[0] : result->clauses[i]->neg_lits[0]);
          // Create copy as either empty list or copy of arg
          desc_bindings = malloc(sizeof(*desc_bindings));
          copy_bindings(desc_bindings, bindings);

          if (pred_unify(descendant_f, lit, desc_bindings)) {
            // Starting item for queue
            tommy_array_insert(&queue, result->clauses[i]);
            break;
          }
          cleanup_bindings(desc_bindings);
        }
      }
    }

    // Checked items to avoid cycles
    tommy_array checked;
    tommy_array_init(&checked);

    // Continue processing queue in breadth-first manner
    for (int curr = 0; curr < tommy_array_size(&queue); curr++) {
      clause *c = tommy_array_get(&queue, curr);

      int present = 0;
      for (int i = 0; i < tommy_array_size(&checked); i++)
        if (tommy_array_get(&checked, i) == c) {
          present = 1;
          break;
        }
      if (!present) {
        tommy_array_insert(&checked, c);

        // If singleton with literal sign matching anecstor's, try unifying with ancestor
        if (c->pos_count + c->neg_count == 1 && (a_pos ? c->pos_count : c->neg_count) == 1) {
          alma_function *lit = (a_pos ? c->pos_lits[0] : c->neg_lits[0]);

          binding_list *anc_bindings = malloc(sizeof(*anc_bindings));
          copy_bindings(anc_bindings, desc_bindings);

          if (pred_unify(ancestor_f, lit, anc_bindings)) {
            swap_bindings(anc_bindings, bindings);
            cleanup_bindings(anc_bindings);
            has_ancestor = 1;
            break;
          }
          cleanup_bindings(anc_bindings);
        }

        // Queue parents for expansion
        for (int i = 0; i < c->parent_set_count; i++)
          for (int j = 0; j < c->parents[i].count; j++)
            tommy_array_insert(&queue, c->parents[i].clauses[j]);
      }
    }

    if (desc_bindings != NULL)
      cleanup_bindings(desc_bindings);
    tommy_array_done(&queue);
    tommy_array_done(&checked);
  }

  ancestor_ret:
  free_term(ancestor_copy);
  free(ancestor_copy);
  free_term(descendant_copy);
  free(descendant_copy);
  return has_ancestor;
}

// Returns true if digit value of x is less than digit value of y
// Else, incuding cases where types differ, false
static int less_than(alma_term *x, alma_term *y, binding_list *bindings, kb *alma) {
  char *x_str;
  char *y_str;
  if (x->type == VARIABLE) {
    alma_term *bound = bindings_contain(bindings, x->variable);
    if (bound == NULL || bound->type != FUNCTION || bound->function->term_count != 0)
      return 0;
    x_str = bound->function->name;
  }
  else if (x->type == FUNCTION && x->function->term_count == 0)
    x_str = x->function->name;
  else
    return 0;
  if (y->type == VARIABLE) {
    alma_term *bound = bindings_contain(bindings, y->variable);
    if (bound == NULL || bound->type != FUNCTION || bound->function->term_count != 0)
      return 0;
    y_str = bound->function->name;
  }
  else if (y->type == FUNCTION && y->function->term_count == 0)
    y_str = y->function->name;
  else
    return 0;

  int xval = 0;
  int yval = 0;
  for (int i = strlen(x_str)-1, j = 1; i >= 0; i--, j*=10) {
    if (isdigit(x_str[i]))
      xval += (x_str[i] - '0') * j;
    else
      return 0;
  }
  for (int i = strlen(y_str)-1, j = 1; i >= 0; i--, j*=10) {
    if (isdigit(y_str[i]))
      yval += (y_str[i] - '0') * j;
    else
      return 0;
  }

  return xval < yval;
}

// If first argument is a digit that's valid index of formula, binds second arg to quote of it and returns true
static int idx_to_form(alma_term *index_term, alma_term *result, binding_list *bindings, kb *alma) {
  char *idx_str;
  if (index_term->type == VARIABLE) {
    alma_term *bound = bindings_contain(bindings, index_term->variable);
    if (bound == NULL || bound->type != FUNCTION || bound->function->term_count != 0)
      return 0;
    idx_str = bound->function->name;
  }
  else if (index_term->type == FUNCTION && index_term->function->term_count == 0)
    idx_str = index_term->function->name;
  else
    return 0;

  if (result->type != VARIABLE || bindings_contain(bindings, result->variable))
    return 0;

  long index = 0;
  for (int i = strlen(idx_str)-1, j = 1; i >= 0; i--, j*=10) {
    if (isdigit(idx_str[i]))
      index += (idx_str[i] - '0') * j;
    else
      return 0;
  }

  index_mapping *map_res = tommy_hashlin_search(&alma->index_map, im_compare, &index, tommy_hash_u64(0, &index, sizeof(index)));
  if (map_res != NULL) {
    clause *formula = map_res->value;
    clause *quote_form = malloc(sizeof(*quote_form));
    copy_clause_structure(formula, quote_form);
    set_variable_ids(quote_form, 1, NULL);

    alma_term *quoted = malloc(sizeof(*quoted));
    quoted->type = QUOTE;
    quoted->quote = malloc(sizeof(*quoted->quote));
    quoted->quote->type = CLAUSE;
    quoted->quote->clause_quote = quote_form;
    add_binding(bindings, result->variable, quoted);
    return 1;
  }
  else
    return 0;
}

// If proc is a valid procedure, runs and returns truth value
int proc_run(alma_function *proc, binding_list *bindings, kb *alma) {
  alma_function *func = proc->terms[0].function;
  if (strcmp(func->name, "neg_int") == 0) {
    // Must match (given bindings) the schema neg_int(literal(...))
    if (func->term_count == 1)
      return introspect(func, bindings, alma, run_neg_int);
  }
  else if (strcmp(func->name, "pos_int") == 0) {
    // Must match (given bindings) the schema pos_int(literal(...))
    if (func->term_count == 1)
      return introspect(func, bindings, alma, run_pos_int);
  }
  else if (strcmp(func->name, "learned") == 0) {
    // Must match (given bindings) the schema learned(literal(...), Var) OR learned(not(literal(...)), Var) and Var unbound
    if (func->term_count == 2 && func->terms[1].type == VARIABLE && !bindings_contain(bindings, func->terms[1].variable))
      return introspect(func, bindings, alma, run_learned);
  }
  else if (strcmp(func->name, "ancestor") == 0) {
    if (func->term_count == 2)
      return ancestor(func->terms+0, func->terms+1, bindings, alma);
  }
  else if (strcmp(func->name, "less_than") == 0) {
    if (func->term_count == 2)
      return less_than(func->terms+0, func->terms+1, bindings, alma);
  }
  else if (strcmp(func->name, "idx_to_form") == 0) {
    if (func->term_count == 2)
      return idx_to_form(func->terms+0, func->terms+1, bindings, alma);
  }
  return 0;
}
