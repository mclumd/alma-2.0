#ifndef alma_print_h
#define alma_print_h

#include "alma_kb.h"
#include "alma_unify.h"

void enable_logs(void);
void enable_python_mode(void);
void disable_python_mode(void);
void tee_alt(char const *content, ...);
//kb_str * tee_fake(char const *content, ...);
void alma_fol_print(alma_node *node, kb_str *buf);
void clause_print(clause *c, kb_str *buf);
void print_bindings(binding_list *theta, kb_str *buf);


#endif
