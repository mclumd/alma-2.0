#ifndef alma_print_h
#define alma_print_h

#include "alma_kb.h"
#include "alma_unify.h"

void alma_fol_print(alma_node *node);
void clause_print(clause *c);
void print_bindings(binding_list *theta);

#endif
