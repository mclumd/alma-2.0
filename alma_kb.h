#ifndef alma_kb_h
#define alma_kb_h

#include "tommy.h"
#include "alma_formula.h"
#include "alma_unify.h"

// TODO: Further consider style of using **, esp. for pos_lits/neg_lits in clause

struct parent_set;
struct fif_info;

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

typedef struct kb {
  long time;
  char *now_str; // String representation of now(time).
  char *prev_str; // String representation of now(time-1).

  int idling; // Boolean for idle KB state, from lack of tasks to advance
  tommy_array new_clauses; // Clauses to be permanently added when next step

  tommy_list clauses; // Linked list storing index_mappings, keeps track of all clauses
  tommy_hashlin index_map; // Maps index value to a clause

  // Hashsets and lists used together for multi-indexing http://www.tommyds.it/doc/multiindex.html
  tommy_hashlin pos_map; // Maps each predicate name to the set of clauses where it appears as positive literal
  tommy_list pos_list; // Linked list for iterating pos_map
  tommy_hashlin neg_map; // Maps each predicate name to the set of clauses where it appears as negative literal
  tommy_list neg_list; // Linked list for iterating neg_map

  tommy_hashlin fif_map; // Tracks fif formulas in clauses

  tommy_array res_tasks; // Stores tasks for resolution (non-tagged clauses) in next step

  // If grow to have many fifs, having pos and neg versions may help
  tommy_hashlin fif_tasks; // Stores tasks for fif rules

  tommy_list backsearch_tasks;

  tommy_hashlin distrusted; // Stores distrusted items by clause index
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

int clauses_differ(clause *x, clause *y);
clause* duplicate_check(kb *collection, clause *c);
void add_clause(kb *collection, clause *curr);
void remove_clause(kb *collection, clause *c);
struct backsearch_task;
void process_res_tasks(kb *collection, tommy_array *tasks, tommy_array *new_arr, struct backsearch_task *bs);
void make_single_task(kb *collection, clause *c, alma_function *c_lit, clause *other, tommy_array *tasks, int use_bif, int pos);
void make_res_tasks(kb *collection, clause *c, int count, alma_function **c_lits, tommy_hashlin *map, tommy_array *tasks, int use_bif, int pos);
void res_tasks_from_clause(kb *collection, clause *c, int process_negatives);
int assert_formula(kb *collection, char *string, int print);
int delete_formula(kb *collection, char *string, int print);
int update_formula(kb *collection, char *string);
void resolve(res_task *t, binding_list *mgu, clause *result);

// Functions used in alma_command
char* now(long t);
void free_clause(clause *c);
void set_variable_ids(clause *c, int id_from_name, binding_list *bs_bindings);
void flatten_node(alma_node *node, tommy_array *clauses, int print);
void nodes_to_clauses(alma_node *trees, int num_trees, tommy_array *clauses, int print);
void free_predname_mapping(void *arg);
int is_distrusted(kb *collection, long index);
char* long_to_str(long x);
void add_child(clause *parent, clause *child);
void transfer_parent(kb *collection, clause *target, clause *source, int add_children);
void distrust_recursive(kb *collection, clause *c, char *time);

int im_compare(const void *arg, const void *obj);
int pm_compare(const void *arg, const void *obj);
char* name_with_arity(char *name, int arity);

#endif
