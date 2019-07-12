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
    case FUNCTION:
      alma_function *func = bound_list->function;
      for (int i = 0; i < func->term_count; i++) {
        if (func->terms[i].type == VARIABLE)
          return 0;
      }
      return 1;
  }
}

// If proc is a valid procedure, runs and returns truth value
int proc_run(alma_function *proc, kb *alma) {
  if (strcmp(proc->name, "neg_int") == 0) {
    return neg_int(proc->terms[0]->function, proc->terms[1], alma);
  }
}

// Negative introspection procedure,
static int neg_int(alma_function *introspect, alma_term *bound, kb* alma) {

}
