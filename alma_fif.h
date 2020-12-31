#ifndef alma_fif_h
#define alma_fif_h

#include "tommy.h"
#include "alma_formula.h"
#include "alma_kb.h"

typedef struct fif_info {
  int premise_count;
  int *ordering; // Records the interleaving order of positive and negative literals
  alma_function *conclusion; // Pointer to track conclusion of fif
  int neg_conc; /// Boolean indicating whether conclusion predicate is negated
} fif_info;

// Used to map set of fif clauses
typedef struct fif_mapping {
  char *conclude_name; // Key for hashing
  int num_clauses;
  clause **clauses;
  tommy_node node;
} fif_mapping;

// Used to hold partial fif tasks in fif_tasks
typedef struct fif_task_mapping {
  char *predname; // Key for hashing -- name/arity of next literal to unify
  tommy_list tasks; // List of specific tasks per predname
  tommy_node node;
} fif_task_mapping;

// Tasks contained in an fif_mapping's list
typedef struct fif_task {
  clause *fif;
  binding_list *bindings;
  int premises_done;
  int num_unified;
  long *unified_clauses; // Indices of clauses task has unified with
  int num_to_unify;
  clause **to_unify;
  int proc_next; // Boolean indicating next as a proc instead of unifiable
  tommy_node node; // For storage in fif_mapping's list
} fif_task;

void fif_task_map_init(kb *collection, clause *c);
void fif_tasks_from_clause(kb *collection, clause *c);
void process_fif_tasks(kb *collection);

void fif_to_front(tommy_array *clauses);
void free_fif_mapping(void *arg);
void free_fif_task(fif_task *task);
void free_fif_task_mapping(void *arg);
alma_function* fif_access(clause *c, int i);
void remove_fif_singleton_tasks(kb *collection, clause *c);

int fifm_compare(const void *arg, const void *obj);
int fif_taskm_compare(const void *arg, const void *obj);

#endif
