#ifndef alma_proc_h
#define alma_proc_h

#include "alma_formula.h"
#include "alma_kb.h"

int proc_valid(alma_function *proc);
int proc_bound_check(alma_term *bound_list);
int proc_run(alma_function *proc, kb *alma);

#endif
