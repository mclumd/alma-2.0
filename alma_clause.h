#ifndef alma_clause_h
#define alma_clause_h

#include "tommy.h"
#include "alma_formula.h"
#include "alma_unify.h"
#include "alma_print.h"

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
  struct clause *equiv_belief; // Indicates a belief linked to another clause (i.e. p(X) to bel(A, "p(X)"))
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

void set_variable_ids(clause *c, int id_from_name, int non_escaping_only, binding_list *bs_bindings, long long *id_count);
int clauses_from_source(char *source, int file_src, tommy_array *clauses, long long *id_count, kb_logger *logger);
void make_clause(alma_node *node, clause *c);
void free_clause(clause *c);
void copy_clause_structure(clause *orignal, clause *copy);
void copy_parents(clause *original, clause *copy);
void transfer_parent(clause *target, clause *source, int add_children);
void add_child(clause *parent, clause *child);
void remove_child(clause *p, clause *c);

void nodes_to_clauses(alma_node *trees, int num_trees, tommy_array *clauses, long long *id_count);
int clauses_differ(clause *x, clause *y, int quote_level);

int flags_negative(clause *c);
long flag_min(clause *c);
char* name_with_arity(char *name, int arity);
int counts_match(clause *x, clause *y);
int function_compare(const void *p1, const void *p2);

// Following structs used internally to clause

// Struct for a tracking name/ID record; for purposes of managing variable names/IDs
typedef struct name_record {
  long long new_id;
  union {
    long long old_id;
    char *name;
  };
} name_record;

// Tree for recursively tracking name_records across different quotations and levels of quotation
// Next_level list represents each quotation nested further inside of the clause
typedef struct record_tree {
  int quote_level;

  int next_level_count;
  struct record_tree **next_level;

  int variable_count;
  name_record **variables;

  // Parent pointer to trace back for quasi-quote
  struct record_tree *parent;
} record_tree;

#endif
