#ifndef alma_backsearch_h
#define alma_backsearch_h

#include "tommy.h"
#include "alma_formula.h"
#include "alma_clause.h"
#include "alma_kb.h"

// Used to keep track of binding lists for given clause of backsearch
typedef struct binding_mapping {
  long key;
  binding_list *bindings;
  tommy_node node;
} binding_mapping;

typedef struct backsearch_task {
  clause *target;
  // Model empty binding list to be copied for others
  binding_list *target_vars;

  int clause_count;

  tommy_array clauses;
  tommy_hashlin clause_bindings;

  tommy_array new_clauses;
  tommy_array new_clause_bindings;

  tommy_array to_resolve;
  tommy_node node;
} backsearch_task;

void backsearch_from_clause(tommy_list *backsearch_tasks, clause *c);
void generate_backsearch_tasks(kb *collection, long time, backsearch_task *bt);
void process_backsearch_tasks(kb *collection, long time, tommy_list *backsearch_tasks, kb_logger *logger);
void backsearch_halt(backsearch_task *t);

int idling_backsearch(tommy_list *backsearch_tasks);
int bm_compare(const void *arg, const void *obj);

#endif
