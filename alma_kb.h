#ifndef alma_kb_h
#define alma_kb_h

#include <stdio.h>
#include "tommy.h"
#include "alma_formula.h"
#include "alma_unify.h"
#include "alma_proc.h"
// TODO: Further consider style of using **, esp. for pos_lits/neg_lits in clause

struct parent_set;
struct fif_info;

typedef struct kb_str {
  long size;
  long limit;
  char *buffer;
  char *curr;
} kb_str;

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
  long acquired; // Time asserted to KB
  long distrusted; // Distrust status; if nonzero the time it was distrusted
  long retired; // Retired status; if nonzero the time it was retired
  long handled; // Handled status; if nonzero the time that it was handled
  int dirty_bit;
  char pyobject_bit;
} clause;

typedef struct parent_set {
  int count;
  clause **clauses;
} parent_set;

typedef struct kb {
  int verbose; // Boolean flag for printing extra output

  long time;
  char *now; // String representation of now(time).
  char *prev; // String representation of now(time-1).

  int idling; // Boolean for idle KB state, from lack of tasks to advance
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

  tommy_list clauses; // Linked list storing index_mappings, keeps track of all clauses
  tommy_hashlin index_map; // Maps index value to a clause
  tommy_hashlin fif_map; // Map of fif_mapping, tracks fif formulas

  // Hashsets and lists used together for multi-indexing http://www.tommyds.it/doc/multiindex.html
  tommy_hashlin pos_map; // Maps each predicate name to the set of clauses where it appears as positive literal
  tommy_list pos_list; // Linked list for iterating pos_map
  tommy_hashlin neg_map; // Maps each predicate name to the set of clauses where it appears as negative literal
  tommy_list neg_list; // Linked list for iterating neg_map

  tommy_array res_tasks; // Stores tasks for resolution (non-tagged clauses) in next step
  // If grow to have many fifs, having pos and neg versions may help
  tommy_hashlin fif_tasks; // Stores tasks for fif rules
  tommy_list backsearch_tasks;

  long size;
  FILE *almalog;

  long long variable_id_count;
  long next_index;

  alma_proc procs[17];
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

int formulas_from_source(char *source, int file_src, int *formula_count, alma_node **formulas, kb *collection, kb_str *buf);
void make_clause(alma_node *node, clause *c);
int clauses_differ(clause *x, clause *y);
clause* duplicate_check(kb *collection, clause *c, int check_distrusted);
void free_clause(clause *c);
void copy_clause_structure(clause *orignal, clause *copy);
void set_variable_ids(clause *c, int id_from_name, int non_escaping_only, binding_list *bs_bindings, kb *collection);
void flatten_node(kb *collection, alma_node *node, tommy_array *clauses, int print, kb_str *buf);
void nodes_to_clauses(kb *collection, alma_node *trees, int num_trees, tommy_array *clauses, int print, kb_str *buf);
void* clause_lookup(kb *collection, clause *c);
clause* mapping_access(void *mapping, if_tag tag, int index);
int mapping_num_clauses(void *mapping, if_tag tag);
int counts_match(clause *x, clause *y);

struct backsearch_task;
void process_res_tasks(kb *collection, tommy_array *tasks, tommy_array *new_arr, struct backsearch_task *bs, kb_str *buf);
void process_new_clauses(kb *collection, kb_str *buf, int make_tasks);
void make_single_task(kb *collection, clause *c, alma_function *c_lit, clause *other, tommy_array *tasks, int use_bif, int pos);
void make_res_tasks(kb *collection, clause *c, int count, alma_function **c_lits, tommy_hashlin *map, tommy_array *tasks, int use_bif, int pos);
void res_tasks_from_clause(kb *collection, clause *c, int process_negatives);
clause* assert_formula(kb *collection, char *string, int print, kb_str *buf);
int delete_formula(kb *collection, char *string, int print, kb_str *buf);
int update_formula(kb *collection, char *string, kb_str *buf);
void transfer_parent(kb *collection, clause *target, clause *source, int add_children, kb_str *buf);

void free_predname_mapping(void *arg);
int pm_compare(const void *arg, const void *obj);
int flags_negative(clause *c);
long flag_min(clause *c);
void func_from_long(alma_term *t, long l);
char* name_with_arity(char *name, int arity);

#endif
