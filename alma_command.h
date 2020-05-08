#ifndef alma_command_h
#define alma_command_h

#include "alma_kb.h"
#include "alma_print.h"

void kb_init(kb **collection, char *file, char *agent, int verbose, kb_str *buf);
void kb_step(kb *collection, kb_str *buf);
void kb_print(kb *collection, kb_str *buf);
void kb_halt(kb *collection);
void kb_assert(kb *collection, char *string, kb_str *buf);
void kb_remove(kb *collection, char *string, kb_str *buf);
void kb_update(kb *collection, char *string, kb_str *buf);
void kb_observe(kb *collection, char *string, kb_str *buf);
void kb_backsearch(kb *collection, char *string, kb_str *buf);

#endif
