#ifndef alma_command_h
#define alma_command_h

#include "alma_kb.h"
void kb_init(kb **collection, char *file, char *agent, int verbose, int differential_priorities);
void kb_step(kb *collection, int singleton);
void kb_print(kb *collection);
void kb_halt(kb *collection);
void kb_assert(kb *collection, char *string);
void kb_remove(kb *collection, char *string);
void kb_update(kb *collection, char *string);
void kb_observe(kb *collection, char *string);
void kb_backsearch(kb *collection, char *string);

#endif
