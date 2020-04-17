#ifndef RESOLUTION_H
#define RESOLUTION_H
#include "alma_formula.h"
#include "tommy.h"

typedef struct clause {
  int pos_count;
  int neg_count;
  alma_function **pos_lits;
  alma_function **neg_lits;
  int parent_set_count;
  int children_count;
  struct parent_set *parents; // Use more efficient structure for as needed
  struct clause **children; // Use more efficient structure for as needed
  if_tag tag;
  struct fif_info *fif; // Data used to store additional fif information; non-null only if FIF tagged
  long index; // Index of clause, used as key in index_map of KB
  long learned; // Time asserted to KB
} clause;

typedef struct parent_set {
  int count;
  clause **clauses;
} parent_set;



// Map used for entries in index_map
typedef struct index_mapping {
  long key;
  clause *value;
  tommy_node hash_node; // Used for storage in tommy_hashlin
  tommy_node list_node; // Used for storage in tommy_list
} index_mapping;

// Struct to be held in the positive/negative tommy_hashlin hash tables (pos_map/neg_map)
typedef struct predname_mapping {
  char *predname; // Key for hashing, name/arity combination (see name_with_arity())
  int num_clauses;
  clause **clauses; // Value
  tommy_node hash_node; // Node used for storage in tommy_hashlin
  tommy_node list_node; // Node used for storage in tommy_list
} predname_mapping;

// Used to track entries in distrusted map
typedef struct distrust_mapping {
  long key;
  clause *value;
  tommy_node node;
} distrust_mapping;

typedef struct res_task {
  clause *x;
  clause *y;
  alma_function *pos; // Positive literal from x
  alma_function *neg; // Negative literal from y
} res_task;

typedef struct res_task_pri {
    res_task *res_task;
    double priority;
} res_task_pri;

#endif
