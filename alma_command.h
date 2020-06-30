#ifndef alma_command_h
#define alma_command_h

#include "alma_kb.h"


typedef struct rl_init_params {
  int tracking_resolutions;
  int resolutions_horizon;
  char *subjects_file;
  char *resolutions_file;
  char *rl_priority_file;
  tommy_array *tracking_subjects;
  double *tracking_priorities;
  int use_lists;   // Use lists instead of files
} rl_init_params;


void kb_init(kb **collection, char *file, char *agent, int verbose, int differential_priorities, int res_heap_size, rl_init_params rip, kb_str *buf);
void kb_step(kb *collection, int singleton, kb_str *buf);
void kb_print(kb *collection, kb_str *buf);
void kb_halt(kb *collection);
void kb_assert(kb *collection, char *string, kb_str *buf);
void kb_remove(kb *collection, char *string, kb_str *buf);
void kb_update(kb *collection, char *string, kb_str *buf);
void kb_observe(kb *collection, char *string, kb_str *buf);
void kb_backsearch(kb *collection, char *string, kb_str *buf);
int load_file(kb *collec, char *filename, kb_str *buf);
#endif
