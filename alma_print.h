#ifndef alma_print_h
#define alma_print_h

#include "alma_kb.h"
#include "alma_unify.h"
#include "resolution.h"

void tee(char const *content, ...);
void alma_fol_print(alma_node *node);
void clause_print(clause *c);
void res_task_print(res_task *t);
void print_bindings(binding_list *theta);

#endif
