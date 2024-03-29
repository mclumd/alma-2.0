#ifndef alma_h
#define alma_h

#include "tommy.h"
#include "alma_kb.h"
#include "alma_proc.h"

typedef struct alma {
  long time;
  char *now; // String representation of now(time).
  char *prev; // String representation of now(time-1).

  int idling; // Boolean for idle reasoner state, from each KB idling
  kb *core_kb; // Core KB contains all beliefs except backsearches and agent models
  tommy_list backsearch_tasks; // Stores tasks for backsearch
  alma_proc procs[17]; // Procedure info array

  FILE *almalog;
} alma;

#endif
