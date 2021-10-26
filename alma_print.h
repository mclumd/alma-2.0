#ifndef alma_print_h
#define alma_print_h

#include "alma_kb.h"
#include "alma_unify.h"

void enable_logs(void);
void enable_python_mode(void);
void disable_python_mode(void);
void tee_alt(char const *content, ...);
//kb_str * tee_fake(char const *content, ...);
void alma_fol_print(alma_node *node, kb_logger *logger);
void clause_print(clause *c, kb_logger *logger);
void print_unify(alma_function *pos, long pos_idx, alma_function *neg, long neg_idx, kb_logger *logger);
void print_matches(var_match_set *v, kb_logger *logger);
void print_bindings(binding_list *theta, int print_all, int last_newline, kb_logger *logger);


#endif
