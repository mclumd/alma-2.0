#include <string.h>
#include "alma_proc.h"

// Returns a boolean value for if proc matches the procedure schema (binary function proc, first arg function)
int proc_valid(alma_function *proc) {
  return strcmp(proc->name, "proc") == 0 && proc->term_count == 2 && proc->terms[0].type == FUNCTION;
}

// Returns a boolean value for all variables being bound (needed prior to executing procedure)
int proc_bound_check(alma_function *proc, binding_list *bindings) {
  alma_term *bound_list = proc->terms+1;
  switch(bound_list->type) {
    case VARIABLE:
      return 0;
    case CONSTANT:
      return 1;
    case FUNCTION: {
      alma_function *func = bound_list->function;
      for (int i = 0; i < func->term_count; i++) {
        // A variable not in argument bindings causes failure
        if (func->terms[i].type == VARIABLE && (bindings == NULL || bindings_contain(bindings, func->terms[i].variable) == NULL))
          return 0;
      }
      return 1;
    }
  }
  return 0;
}

// Negative introspection procedure
static int neg_int(alma_function *introspect, alma_term *bound, binding_list *bindings, kb* alma) {
  return 0;
}

// Must match (given bindings) the schema learned(literal(...), Var) OR learned(not(literal(...)), Var)
// Stub currently returns the first match, even if KB and unification give multiple options
static int learned(alma_function *learned, alma_term *bound, binding_list *bindings, kb *alma) {
  if (learned->term_count == 2 && learned->terms[1].type == VARIABLE
      && (bindings == NULL || !bindings_contain(bindings, learned->terms[1].variable))) {

    // Create copy and substitute based on bindings available
    alma_term *search_term = malloc(sizeof(*search_term));
    copy_alma_term(learned->terms, search_term);
    subst(bindings, search_term);

    alma_function *search;
    tommy_hashlin *map = &alma->pos_map;
    int pos = 1;

    if (search_term->type == VARIABLE) {
      free_term(search_term);
      free(search_term);
      return 0;
    }
    else if (search_term->type == FUNCTION) {
      search = search_term->function;

      // Extract from not, if present
      if (strcmp(search->name, "not") == 0) {
        pos = 0;
        map = &alma->neg_map;
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
        else if (search_term->type == VARIABLE) {
          free_term(search_term);
          free(search_term);
          return 0;
        }
      }
    }
    else {
      search = malloc(sizeof(*search));
      search->name = search_term->constant->name;
      search->term_count = 0;
      search->terms = NULL;
      search_term->constant->name = NULL;
      free(search_term->constant);
      search_term->function = search;
      search_term->type = FUNCTION;
    }

    char *name = name_with_arity(search->name, search->term_count);
    predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    free(name);
    if (result != NULL) {

      for (int i = 0; i < result->num_clauses; i++) {
        alma_function *lit = (pos ? result->clauses[i]->pos_lits[0] : result->clauses[i]->neg_lits[0]);

        // Create copy as either empty list or copy of arg
        binding_list *copy = malloc(sizeof(*copy));
        if (bindings == NULL) {
          copy->list = NULL;
          copy->num_bindings = 0;
        }
        else
          copy_bindings(copy, bindings);

        // Returning first match based at the moment
        if (pred_unify(search, lit, copy)) {
          binding *temp = bindings->list;
          bindings->list = copy->list;
          copy->list = temp;
          int num_temp = bindings->num_bindings;
          bindings->num_bindings = copy->num_bindings;
          copy->num_bindings = num_temp;
          cleanup_bindings(copy);

          // If they unify, create term out of learned answer
          alma_term *time_term = malloc(sizeof(*time_term));
          time_term->type = CONSTANT;
          time_term->constant = malloc(sizeof(*time_term->constant));
          int len = snprintf(NULL, 0, "%ld", result->clauses[i]->learned);
          time_term->constant->name = malloc(len+1);
          snprintf(time_term->constant->name, len+1, "%ld", result->clauses[i]->learned);

          // Insert answer of learned query into binding set
          bindings->num_bindings++;
          bindings->list = realloc(bindings->list, sizeof(*bindings->list) * bindings->num_bindings);
          bindings->list[bindings->num_bindings-1].var = malloc(sizeof(alma_variable));
          copy_alma_var(learned->terms[1].variable, bindings->list[bindings->num_bindings-1].var);
          bindings->list[bindings->num_bindings-1].term = time_term;

          free_term(search_term);
          free(search_term);
          return 1;
        }
        cleanup_bindings(copy);
      }
    }
    free_term(search_term);
    free(search_term);
  }
  return 0;
}

// If proc is a valid procedure, runs and returns truth value
int proc_run(alma_function *proc, binding_list *bindings, kb *alma) {
  if (strcmp(proc->terms[0].function->name, "neg_int") == 0) {
    return neg_int(proc->terms[0].function, proc->terms+1, bindings, alma);
  }
  else if (strcmp(proc->terms[0].function->name, "learned") == 0) {
    return learned(proc->terms[0].function, proc->terms+1, bindings, alma);
  }
  return 0;
}
