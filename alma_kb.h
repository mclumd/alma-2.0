#ifndef alma_kb_h
#define alma_kb_h

#include "tommyds/tommyds/tommytypes.h"
#include "tommyds/tommyds/tommyarray.h"
#include "tommyds/tommyds/tommyhashlin.h"
#include "tommyds/tommyds/tommylist.h"
#include "alma_formula.h"
#include "alma_unify.h"

// TODO: Further consider style of using **, esp. for pos_lits/neg_lits in clause

typedef struct clause {
  int pos_count;
  int neg_count;
  alma_function **pos_lits;
  alma_function **neg_lits;
  if_tag tag;
} clause;

// Simple definition for now, likely to expand significantly in future
typedef struct kb {
  tommy_array clauses; // Array storing pointers to clauses

  // Hashset and list used together for multi-indexing http://www.tommyds.it/doc/multiindex.html
  tommy_hashlin pos_map; // Maps each predicate name to the set of clauses where it appears as positive literal
  tommy_list pos_list; // Linked list for iterating pos_map
  tommy_hashlin neg_map; // Maps each predicate name to the set of clauses where it appears as negative literal
  tommy_list neg_list; // Linked list for iterating neg_map

  tommy_array task_list; // Stores tasks to attempt resolution on next step
  long long task_count;
} kb;

// Struct to be held in the tommy_hashlin hash tables of a KB
// Will be hashed only based on the predname string, hance making a map of strings to array of clause pointers
typedef struct map_entry {
  char *predname; // Key, used as argument for tommy_inthash_u32 hashing function
  int num_clauses;
  clause **clauses; // Value
  tommy_node hash_node; // Node used for storage in tommy_hashlin
  tommy_node list_node; // Node used for storage in tommy_list
} map_entry;

typedef struct task {
  clause *x;
  clause *y;
  alma_function *pos; // Positive literal from x
  alma_function *neg; // Negative literal from y
} task;

void kb_init(alma_node *trees, int num_trees, kb **collection);
void free_kb(kb *collection);
void kb_print(kb *collection);

void maps_add(kb *collection, tommy_array *clauses);
void get_tasks(kb *collection, tommy_array *new_clauses, int process_negs);
void resolve(task *t, binding_list *mgu, clause *result);
void forward_chain(kb *collection);

#endif
