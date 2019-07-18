#include <string.h>
#include "alma_proc.h"

// Returns a boolean value for if proc matches the procedure schema (binary function proc, first arg function)
int proc_valid(alma_function *proc) {
  return strcmp(proc->name, "proc") == 0 && proc->term_count == 2 && proc->terms[0].type == FUNCTION;
}

// Returns a boolean value for all variables being bound (needed prior to executing procedure)
int proc_bound_check(alma_term *bound_list) {
  switch(bound_list->type) {
    case VARIABLE:
      return 0;
    case CONSTANT:
      return 1;
    case FUNCTION: {
      alma_function *func = bound_list->function;
      for (int i = 0; i < func->term_count; i++) {
        if (func->terms[i].type == VARIABLE)
          return 0;
      }
      return 1;
    }
  }
  return 0;
}

// Negative introspection procedure
static binding_list* neg_int(alma_function *introspect, alma_term *bound, kb* alma) {
  return NULL;
}

// Must match the schema learned(literal(...), Var) OR learned(not(literal(...)), Var)
// Stub currently returns the first match, even if KB and unification give multiple options
static binding_list* learned(alma_function *learned, alma_term *bound, kb *alma) {
  if (learned->term_count == 2 && learned->terms[0].type == FUNCTION && learned->terms[1].type == VARIABLE) {
    alma_function *search = learned->terms[0].function;
    tommy_hashlin *map = &alma->pos_map;
    int pos = 1;
    if (strcmp(search->name, "not") == 0) {
      pos = 0;
      map = &alma->neg_map;
      if (search->term_count != 1 || search->terms[0].type != FUNCTION)
        return NULL;
      search = search->terms[0].function;
    }

    char *name = name_with_arity(search->name, search->term_count);
    predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    free(name);
    if (result != NULL) {
      for (int i = 0; i < result->num_clauses; i++) {
        alma_function *lit = (pos ? result->clauses[i]->pos_lits[0] : result->clauses[i]->neg_lits[0]);
        binding_list *theta = malloc(sizeof(*theta));
        theta->list = NULL;
        theta->num_bindings = 0;

        // Returning first match based at the moment
        if (pred_unify(search, lit, theta)) {
          free_predname_mapping(result);
          cleanup_bindings(theta);

          // If they unify, create term out of learned answer
          alma_term *time_term = malloc(sizeof(*time_term));
          time_term->type = CONSTANT;
          time_term->constant = malloc(sizeof(*time_term->constant));
          int len = snprintf(NULL, 0, "%ld", result->clauses[i]->learned);
          time_term->constant->name = malloc(len+1);
          snprintf(time_term->constant->name, len+1, "%ld", result->clauses[i]->learned);

          // Create Var/time_term binding, substitute on
          theta = malloc(sizeof(*theta));
          theta->list = malloc(sizeof(*theta->list));
          theta->list[0].var = learned->terms[1].variable;
          theta->list[0].term = time_term;
          theta->num_bindings = 1;
          return theta;
        }
        cleanup_bindings(theta);
      }
      free_predname_mapping(result);
    }
  }
  return NULL;
}

// If proc is a valid procedure, runs and returns truth value
binding_list* proc_run(alma_function *proc, kb *alma) {
  if (strcmp(proc->terms[0].function->name, "neg_int") == 0) {
    return neg_int(proc->terms[0].function, proc->terms+1, alma);
  }
  else if (strcmp(proc->terms[0].function->name, "found") == 0) {
    return learned(proc->terms[0].function, proc->terms+1, alma);
  }
  return NULL;
}
