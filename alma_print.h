#ifndef alma_print_h
#define alma_print_h

#include "alma_kb.h"
#include "alma_unify.h"

void enable_logs(void);
void enable_python_mode(void);
void disable_python_mode(void);
void tee_alt(char const *content, ...);
//kb_str * tee_fake(char const *content, ...);
void alma_fol_print(kb *collection, alma_node *node, kb_str *buf);
void clause_print(kb *collection, clause *c, kb_str *buf);
void print_unify(kb *collection, alma_function *pos, long pos_idx, alma_function *neg, long neg_idx, kb_str *buf);
void print_matches(kb *collection, var_match_set *v, kb_str *buf);
void print_bindings(kb *collection, binding_list *theta, int print_all, int last_newline, kb_str *buf);


#endif
