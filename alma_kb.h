#ifndef alma_kb_h
#define alma_kb_h

#include <stdio.h>
#include "tommy.h"
#include "alma_formula.h"
#include "alma_unify.h"
#include "clause.h"
#include "resolution.h"
#include "res_task_heap.h"

extern FILE *almalog;

// TODO: Further consider style of using **, esp. for pos_lits/neg_lits in clause

struct parent_set;
struct fif_info;

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



typedef struct kb {
  int verbose; // Boolean flag for printing extra output
  int diff_prior;  // Boolean flag for using differential priorities
  double (*calc_priority)(struct kb*, res_task *);

  long time;
  char *now; // String representation of now(time).
  char *prev; // String representation of now(time-1).
  char *wallnow;
  char *wallprev;

  int tracking_resolutions;
  int **resolution_choices;
  tommy_array *subject_list;
  int num_subjects;

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

  //tommy_array res_tasks; // Stores tasks for resolution (non-tagged clauses) in next step
  res_task_heap res_tasks;
  int res_heap_size;

  // If grow to have many fifs, having pos and neg versions may help
  tommy_hashlin fif_tasks; // Stores tasks for fif rules

  tommy_list backsearch_tasks;

  tommy_hashlin distrusted; // Stores distrusted items by clause index
} kb;

int clauses_differ(clause *x, clause *y);
clause* distrusted_dupe_check(kb *collection, clause *c);
clause* duplicate_check(kb *collection, clause *c);
void add_clause(kb *collection, clause *curr);
void remove_clause(kb *collection, clause *c);
struct backsearch_task;
void process_res_tasks(kb *collection, res_task_heap *tasks, tommy_array *new_arr, struct backsearch_task *bs);
void process_single_res_task(kb *collection, res_task_heap *tasks, tommy_array *new_arr, struct backsearch_task *bs);
void make_single_task(kb *collection, clause *c, alma_function *c_lit, clause *other, res_task_heap *tasks, int use_bif, int pos);
void make_res_tasks(kb *collection, clause *c, int count, alma_function **c_lits, tommy_hashlin *map, res_task_heap *tasks, int use_bif, int pos);
void process_new_clauses(kb *collection);
void res_tasks_from_clause(kb *collection, clause *c, int process_negatives);
clause* assert_formula(kb *collection, char *string, int print);
int delete_formula(kb *collection, char *string, int print);
int update_formula(kb *collection, char *string);
void resolve(res_task *t, binding_list *mgu, clause *result);

// Functions used in alma_command
char* now(long t);
char* walltime();
void free_clause(clause *c);
void copy_clause_structure(clause *orignal, clause *copy);
void set_variable_ids(clause *c, int id_from_name, binding_list *bs_bindings);
void flatten_node(alma_node *node, tommy_array *clauses, int print);
void nodes_to_clauses(alma_node *trees, int num_trees, tommy_array *clauses, int print);
void free_predname_mapping(void *arg);
int is_distrusted(kb *collection, long index);
char* long_to_str(long x);
void add_child(clause *parent, clause *child);
void transfer_parent(kb *collection, clause *target, clause *source, int add_children);
void distrust_recursive(kb *collection, clause *c, clause *parent);

int im_compare(const void *arg, const void *obj);
int pm_compare(const void *arg, const void *obj);
char* name_with_arity(char *name, int arity);
void init_resolution_choices(int ***resolution_choices, int num_subjects, int num_timesteps);
#endif
