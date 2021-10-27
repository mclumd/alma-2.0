#ifndef alma_kb_h
#define alma_kb_h

#include <stdio.h>
#include "tommy.h"
#include "alma_formula.h"
#include "alma_clause.h"
#include "alma_unify.h"
#include "alma_print.h"

typedef struct kb {
  long long variable_id_count;
  long next_index;
  char *prefix; // String prefix for indices when printing
  int verbose; // Boolean flag for printing extra output

  tommy_list clauses; // Linked list storing index_mappings, keeps track of all clauses
  tommy_hashlin index_map; // Maps index value to a clause
  tommy_hashlin fif_map; // Map of fif_mapping, tracks fif formulas

  // Hashsets and lists used together for multi-indexing http://www.tommyds.it/doc/multiindex.html
  tommy_hashlin pos_map; // Maps each predicate name to the set of clauses where it appears as positive literal
  tommy_list pos_list; // Linked list for iterating pos_map
  tommy_hashlin neg_map; // Maps each predicate name to the set of clauses where it appears as negative literal
  tommy_list neg_list; // Linked list for iterating neg_map

  tommy_array new_clauses; // Clauses to be permanently added when next stepping
  tommy_array timestep_delay_clauses; // Clauses to be added a timestep after new_clauses

  tommy_array distrust_set; // Root clauses distrusted when stepping in last step
  tommy_array distrust_parents; // Formulas as parents to use for distrusted() instances created from corresponding clauses
  tommy_array handle_set; // Contra clauses handled when stepping in last step
  tommy_array handle_parents; // Formulas as parents to use for handled() instances created from corresponding clauses
  tommy_array retire_set; // Root clauses retired when stepping in last step
  tommy_array retire_parents; // Formulas as parents to use for retired() instance created from corresponding clauses

  tommy_array pos_lit_reinstates; // Holds positive literal reinstatements, for contradiction checking
  tommy_array neg_lit_reinstates; // Holds negative literal reinstatements, for contradiction checking
  tommy_array trues; // True() literals to have their arguments unwrapped

  tommy_array res_tasks; // Stores tasks for resolution (non-tagged clauses) in next step
  // If grow to have many fifs, having pos and neg versions may help
  tommy_hashlin fif_tasks; // Stores tasks for fif rules
} kb;

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

typedef struct res_task {
  clause *x;
  clause *y;
  alma_function *pos; // Positive literal from x
  alma_function *neg; // Negative literal from y
} res_task;

void kb_init(kb* collection, int verbose);
void kb_print(kb *collection, kb_logger *logger);

clause* duplicate_check(kb *collection, long time, clause *c, int check_distrusted);
void* clause_lookup(kb *collection, clause *c);
clause* mapping_access(void *mapping, if_tag tag, int index);
int mapping_num_clauses(void *mapping, if_tag tag);

struct backsearch_task;
void process_res_tasks(kb *collection, long time, tommy_array *tasks, tommy_array *new_arr, struct backsearch_task *bs, kb_logger *logger);
struct alma_proc;
void process_new_clauses(kb *collection, struct alma_proc *procs, long time, kb_logger *logger, int make_tasks);
void make_single_task(clause *c, alma_function *c_lit, clause *other, tommy_array *tasks, int use_bif, int pos);
void make_res_tasks(clause *c, int count, alma_function **c_lits, tommy_hashlin *map, tommy_array *tasks, int use_bif, int pos);
void res_tasks_from_clause(kb *collection, clause *c, int process_negatives);

clause* assert_formula(kb *collection, char *string, int print, kb_logger *logger);
int delete_formula(kb *collection, long time, char *string, int print, kb_logger *logger);
int update_formula(kb *collection, long time, char *string, kb_logger *logger);

void free_predname_mapping(void *arg);
int pm_compare(const void *arg, const void *obj);

void func_from_long(alma_term *t, long l);

#endif
