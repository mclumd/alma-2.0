#ifndef alma_proc_h
#define alma_proc_h

#include "alma_formula.h"
#include "alma_unify.h"

struct kb;
struct fif_task;
struct kb_logger;

typedef struct alma_proc {
  char *name;
  int arity;
} alma_proc;

int is_proc(alma_function *proc, alma_proc *procs);
int proc_bound_check(alma_function *proc, binding_list *bindings, alma_proc *procs);
int proc_run(alma_function *proc, binding_list *bindings, struct fif_task *task, struct kb *alma, struct kb_logger *logger);

#endif
