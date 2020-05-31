#ifndef alma_command_h
#define alma_command_h

#include "alma_kb.h"

typedef struct rl_init_params {
  int tracking_resolutions;
  int resolutions_horizon;
  char *subjects_file;
  char *resolutions_file;
  char *rl_priority_file;
} rl_init_params;

void kb_init(kb **collection, char *file, char *agent, int verbose, int differential_priorities, int res_heap_size, rl_init_params rip);
void kb_step(kb *collection, int singleton);
void kb_print(kb *collection);
void kb_halt(kb *collection);
void kb_assert(kb *collection, char *string);
void kb_remove(kb *collection, char *string);
void kb_update(kb *collection, char *string);
void kb_observe(kb *collection, char *string);
void kb_backsearch(kb *collection, char *string);
int load_file(kb *collec, char *filename);
#endif
