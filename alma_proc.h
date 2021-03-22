#ifndef alma_proc_h
#define alma_proc_h

#include "alma_formula.h"
#include "alma_unify.h"

struct kb;

typedef struct alma_proc {
  char *name;
  int arity;
} alma_proc;

int is_proc(alma_function *proc, struct kb *alma);
int proc_bound_check(alma_function *proc, binding_list *bindings, struct kb *alma);
int proc_run(alma_function *proc, binding_list *bindings, struct kb *alma);

#endif
