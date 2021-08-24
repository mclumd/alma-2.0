#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "alma_kb.h"
#include "alma_backsearch.h"
#include "alma_fif.h"
#include "alma_proc.h"
#include "alma_print.h"
#include "limits.h"

static void add_lit(int *count, alma_function ***lits, alma_function *new_lit) {
  (*count)++;
  *lits = realloc(*lits, sizeof(**lits) * *count);
  (*lits)[*count-1] = new_lit;
  quote_convert_func((*lits)[*count-1]);
}

static void make_clause_rec(alma_node *node, clause *c, int invert) {
  // Note: predicate null assignment necessary for freeing of nodes without losing predicates
  if (node != NULL) {
    if (node->type == FOL) {
      // Neg lit case for NOT
      if (node->fol->op == NOT) {
        if (invert)
          add_lit(&c->pos_count, &c->pos_lits, node->fol->arg1->predicate);
        else
          add_lit(&c->neg_count, &c->neg_lits, node->fol->arg1->predicate);
        node->fol->arg1->predicate = NULL;
      }
      // Case if node is OR (or AND when processing FIF fragments)
      else {
        make_clause_rec(node->fol->arg1, c, invert);
        make_clause_rec(node->fol->arg2, c, invert);
      }
      if (c->tag == NONE)
        c->tag = node->fol->tag;
    }
    // Otherwise, only pos lit left
    else {
      if (invert)
        add_lit(&c->neg_count, &c->neg_lits, node->predicate);
      else
        add_lit(&c->pos_count, &c->pos_lits, node->predicate);
      node->predicate = NULL;
    }
  }
}

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

static void init_record_tree(record_tree *records, int quote_level, record_tree *parent) {
  records->quote_level = quote_level;
  records->next_level_count = 0;
  records->next_level = NULL;
  records->variable_count = 0;
  records->variables = NULL;
  records->parent = parent;
}

static void free_record_tree(record_tree *records) {
  for (int i = 0; i < records->next_level_count; i++) {
    free_record_tree(records->next_level[i]);
    free(records->next_level[i]);
  }
  free(records->next_level);

  for (int i = 0; i < records->variable_count; i++)
    free(records->variables[i]);
  free(records->variables);
}

static name_record* records_retrieve(record_tree *records, int quote_level, long long query_id) {
  if (quote_level == records->quote_level) {
    for (int i = 0; i < records->variable_count; i++)
      if (records->variables[i]->old_id == query_id)
        return records->variables[i];
  }
  // Otherwise, records->quote_level is less than query quote level
  else {
    for (int i = 0; i < records->next_level_count; i++) {
      name_record *rec = records_retrieve(records->next_level[i], quote_level, query_id);
      if (rec != NULL)
        return rec;
    }
  }
  return NULL;
}

/*static void records_print(record_tree *records) {
  printf("Level %d: ", records->quote_level);
  if (records->variable_count == 0)
    printf("None\n");
  for (int i = 0; i < records->variable_count; i++) {
    printf("%lld", records->variables[i]->new_id);
    if (i == records->variable_count-1)
      printf("\n");
    else
      printf(" ");
  }

  for (int i = 0; i < records->next_level_count; i++) {
    records_print(records->next_level[i]);
  }
}*/


// For a single ALMA variable, set fields in record_tree
static void set_var_name(record_tree *records, alma_variable *var, int id_from_name, int quote_level, kb *collection) {
  // If a match for the variable is found in the record tree, set ID using new field
  for (int i = 0; i < records->variable_count; i++) {
    name_record *rec = records->variables[i];
    if ((!id_from_name && var->id == rec->old_id) ||
        (id_from_name && strcmp(var->name, rec->name) == 0)) {
      var->id = rec->new_id;
      return;
    }
  }
  // If did not find match from records, create a new one
  records->variable_count++;
  records->variables = realloc(records->variables, sizeof(*records->variables) * records->variable_count);
  records->variables[records->variable_count-1] = malloc(sizeof(*records->variables[records->variable_count-1]));
  if (id_from_name)
    records->variables[records->variable_count-1]->name = var->name;
  else
    records->variables[records->variable_count-1]->old_id = var->id;
  var->id = records->variables[records->variable_count-1]->new_id = collection->variable_id_count;
  collection->variable_id_count++;
}

static void set_variable_ids_rec(record_tree *records, clause *c, int id_from_name, int non_escaping_only, int quote_level, kb *collection);

static void set_variable_names(record_tree *records, alma_term *term, int id_from_name, int non_escaping_only, int quote_level, kb *collection) {
  if (term->type == VARIABLE && (!non_escaping_only || quote_level > 0)) {
    set_var_name(records, term->variable, id_from_name, quote_level, collection);
  }
  else if (term->type == FUNCTION) {
    for (int i = 0; i < term->function->term_count; i++)
      set_variable_names(records, term->function->terms+i, id_from_name, non_escaping_only, quote_level, collection);
  }
  else if (term->type == QUOTE) {
    if (term->quote->type == CLAUSE) {
      // Create new child record_tree for further nested quotation
      records->next_level_count++;
      records->next_level = realloc(records->next_level, sizeof(*records->next_level) * records->next_level_count);
      records->next_level[records->next_level_count-1] = malloc(sizeof(*records->next_level[records->next_level_count-1]));
      init_record_tree(records->next_level[records->next_level_count-1], quote_level+1, records);

      set_variable_ids_rec(records->next_level[records->next_level_count-1], term->quote->clause_quote, id_from_name, non_escaping_only, quote_level+1, collection);
    }
  }
  else if (term->type == QUASIQUOTE && (!non_escaping_only || quote_level > term->quasiquote->backtick_count)) {
    // Trace back up along parents based on amount of quasi-quotation
    for (int i = 0; i < term->quasiquote->backtick_count; i++)
      records = records->parent;
    set_var_name(records, term->quasiquote->variable, id_from_name, quote_level - term->quasiquote->backtick_count, collection);
  }
}

static void set_variable_ids_rec(record_tree *records, clause *c, int id_from_name, int non_escaping_only, int quote_level, kb *collection) {
  for (int i = 0; i < c->pos_count; i++)
    for (int j = 0; j < c->pos_lits[i]->term_count; j++)
      set_variable_names(records, c->pos_lits[i]->terms+j, id_from_name, non_escaping_only, quote_level, collection);
  for (int i = 0; i < c->neg_count; i++)
    for (int j = 0; j < c->neg_lits[i]->term_count; j++)
      set_variable_names(records, c->neg_lits[i]->terms+j, id_from_name, non_escaping_only, quote_level, collection);

  if (c->tag == FIF)
    for (int i = 0; i < c->fif->num_conclusions; i++)
      set_variable_ids_rec(records, c->fif->conclusions[i], id_from_name, non_escaping_only, quote_level, collection);
}


static void set_clause_from_records(record_tree *records, clause *c, int quote_level, kb *collection);

// Use existing assembled record_tree to update ID values from old_id to new_id in the argument term
static void set_term_from_records(record_tree *records, alma_term *term, int quote_level, kb *collection) {
  if (term->type == VARIABLE) {
    name_record *rec = records_retrieve(records, quote_level, term->variable->id);
    if (rec != NULL)
      term->variable->id = rec->new_id;
  }
  else if (term->type == FUNCTION) {
    for (int i = 0; i < term->function->term_count; i++)
      set_term_from_records(records, term->function->terms+i, quote_level, collection);
  }
  else if (term->type == QUOTE) {
    if (term->quote->type == CLAUSE) {
      set_clause_from_records(records, term->quote->clause_quote, quote_level+1, collection);
    }
  }
  else {
    // Trace back up along parents based on amount of quasi-quotation
    for (int i = 0; i < term->quasiquote->backtick_count; i++)
      records = records->parent;
    name_record *rec = records_retrieve(records, quote_level - term->quasiquote->backtick_count, term->quasiquote->variable->id);
    if (rec != NULL)
      term->variable->id = rec->new_id;
  }
}

static void set_clause_from_records(record_tree *records, clause *c, int quote_level, kb *collection) {
  for (int i = 0; i < c->pos_count; i++)
    for (int j = 0; j < c->pos_lits[i]->term_count; j++)
      set_term_from_records(records, c->pos_lits[i]->terms+j, quote_level, collection);
  for (int i = 0; i < c->neg_count; i++)
    for (int j = 0; j < c->neg_lits[i]->term_count; j++)
      set_term_from_records(records, c->neg_lits[i]->terms+j, quote_level, collection);

  if (c->tag == FIF)
    for (int i = 0; i < c->fif->num_conclusions; i++)
      set_clause_from_records(records, c->fif->conclusions[i], quote_level, collection);
}

// Given a clause, assign the ID fields of each variable
// Two cases:
// 1) If clause is result of resolution, replace existing matching IDs with fresh values each
// id_from_name is false in this case; distinct variables are tracked using ID given names may not be unique
// 2) Otherwise, give variables with the same name matching IDs
// id_from_name is true in this case; distinct variables are tracked by name
// non_escaping_only sets variable IDs only in the case where they don't escape quotation
// Fresh ID values drawn from variable_id_count global variable
void set_variable_ids(clause *c, int id_from_name, int non_escaping_only, binding_list *bs_bindings, kb *collection) {
  record_tree *records;
  records = malloc(sizeof(*records));
  init_record_tree(records, 0, NULL);

  set_variable_ids_rec(records, c, id_from_name, non_escaping_only, 0, collection);

  // If bindings for a backsearch have been passed in, update variable names for them as well
  // Id_from_name always false in this case; omits that parameter
  if (bs_bindings)
    for (int i = 0; i < bs_bindings->num_bindings; i++)
      // TODO: eventually needs to get quote_level out based on binding?
      set_term_from_records(records, bs_bindings->list[i].term, 0, collection);

  //records_print(records);
  free_record_tree(records);
  free(records);
}

static void init_ordering_rec(fif_info *info, alma_node *node, int *next, int *pos, int *neg) {
  if (node->type == FOL) {
    // Pos lit case for NOT
    if (node->fol->op == NOT)
      info->ordering[(*next)++] = (*pos)++;
    // Case of node is AND
    else {
      init_ordering_rec(info, node->fol->arg1, next, pos, neg);
      init_ordering_rec(info, node->fol->arg2, next, pos, neg);
    }
  }
  // Otherwise, neg lit
  else
    info->ordering[(*next)++] = (*neg)--;
}

// Given a node for a fif formula, record inorder traversal ofpositive and negative literals
static void init_ordering(fif_info *info, alma_node *node) {
  int next = 0;
  int pos = 0;
  int neg = -1;
  // Fif node must necessarily at top level have an OR -- ignore second branch of with conclusion
  init_ordering_rec(info, node->fol->arg1, &next, &pos, &neg);
}

// Comparison used by qsort of clauses -- orders by increasing function name and increasing arity
static int function_compare(const void *p1, const void *p2) {
  alma_function **f1 = (alma_function **)p1;
  alma_function **f2 = (alma_function **)p2;
  int compare = strcmp((*f1)->name, (*f2)->name);
  if (compare == 0)
    return (*f1)->term_count - (*f2)->term_count;
  else
    return compare;
}


int formulas_from_source(char *source, int file_src, int *formula_count, alma_node **formulas, kb *collection, kb_str *buf) {
  alma_node *errors = NULL;
  int error_count;
  int ret = fol_from_source(source, file_src, formula_count, formulas, &error_count, &errors);
  if (ret) {
    // Print messages and free error cases
    for (int i = 0; i < error_count; i++) {
      tee_alt("Error: quasi-quotation marks exceed level of quotation in ", collection, buf);
      alma_fol_print(collection, errors + i, buf);
      tee_alt("\n", collection, buf);
      free_alma_tree(errors+i);
    }
    free(errors);

    // Convert ALMA FOL to CNF formulas, except for fif cases
    for (int i = 0; i < *formula_count; i++) {
      if ((*formulas)[i].type != FOL || (*formulas)[i].fol->tag != FIF)
        make_cnf(*formulas+i);
    }
    //tee_alt("Standardized equivalents:\n", collection, buf);
    /*for (int i = 0; i < *formula_count; i++) {
      alma_fol_print(collection, *formulas+i, buf);
      tee_alt("\n", collection, buf);
    }*/
    return 1;
  }
  else
    return ret;
}


static void make_fif_conclusions(alma_node *node, clause *c) {
  if (node != NULL) {
    if (node->type == FOL && node->fol->op == AND) {
      make_fif_conclusions(node->fol->arg1, c);
      make_fif_conclusions(node->fol->arg2, c);
    }
    else {
      c->fif->num_conclusions++;
      c->fif->conclusions = realloc(c->fif->conclusions, sizeof(*c->fif->conclusions) * c->fif->num_conclusions);
      clause *latest = malloc(sizeof(*latest));
      make_clause(node, latest);
      c->fif->conclusions[c->fif->num_conclusions-1] = latest;
    }
  }
}

void make_clause(alma_node *node, clause *c) {
  c->pos_count = c->neg_count = 0;
  c->pos_lits = c->neg_lits = NULL;
  c->parent_set_count = c->children_count = 0;
  c->parents = NULL;
  c->children = NULL;
  c->tag = NONE;
  c->fif = NULL;

  if (node->type == FOL && node->fol->tag == FIF) {
    // Clause pos and neg lits initialize from fif premises
    make_clause_rec(node->fol->arg1, c, 1);

    // Initialize fif info except for conclusions
    c->tag = FIF;
    c->fif = malloc(sizeof(*c->fif));
    c->fif->premise_count = c->pos_count + c->neg_count;
    c->fif->ordering = malloc(sizeof(*c->fif->ordering) * c->fif->premise_count);
    init_ordering(c->fif, node);

    // Initialize fif conclusion structures
    c->fif->num_conclusions = 0;
    c->fif->conclusions = NULL;
    make_fif_conclusions(node->fol->arg2, c);

    if (c->fif->conclusions[0]->pos_count > 0)
      c->fif->indexing_conc = c->fif->conclusions[0]->pos_lits[0];
    else
      c->fif->indexing_conc = c->fif->conclusions[0]->neg_lits[0];
  }
  else {
    make_clause_rec(node, c, 0);
    // Non-fif clauses can be sorted by literal for ease of resolution
    qsort(c->pos_lits, c->pos_count, sizeof(*c->pos_lits), function_compare);
    qsort(c->neg_lits, c->neg_count, sizeof(*c->neg_lits), function_compare);
  }
}

// Flattens a single alma node and adds its contents to collection
// Recursively calls when an AND is found to separate conjunctions
void flatten_node(kb *collection, alma_node *node, tommy_array *clauses, int print, kb_str *buf) {
  if (node->type == FOL && node->fol->tag != FIF && node->fol->op == AND) {
    if (node->fol->arg1->type == FOL)
      node->fol->arg1->fol->tag = node->fol->tag;
    flatten_node(collection, node->fol->arg1, clauses, print, buf);
    if (node->fol->arg2->type == FOL)
      node->fol->arg2->fol->tag = node->fol->tag;
    flatten_node(collection, node->fol->arg2, clauses, print, buf);
  }
  else {
    clause *c = malloc(sizeof(*c));
    c->dirty_bit = 1;
    c->pyobject_bit = (char) 1;
    make_clause(node, c);
    set_variable_ids(c, 1, 0, NULL, collection);

    if (print) {
      tee_alt("-a: ", collection, buf);
      clause_print(collection, c, buf);
      tee_alt(" added\n", collection, buf);
    }
    tommy_array_insert(clauses, c);
  }
}

void free_clause(clause *c) {
  if (c == NULL)
    return;

  for (int i = 0; i < c->pos_count; i++)
    free_function(c->pos_lits[i]);
  for (int i = 0; i < c->neg_count; i++)
    free_function(c->neg_lits[i]);
  free(c->pos_lits);
  free(c->neg_lits);
  for (int i = 0; i < c->parent_set_count; i++)
    free(c->parents[i].clauses);
  free(c->parents);
  free(c->children);

  if (c->tag == FIF) {
    free(c->fif->ordering);
    for (int i = 0; i < c->fif->num_conclusions; i++)
      free_clause(c->fif->conclusions[i]);
    free(c->fif->conclusions);
    free(c->fif);
  }

  free(c);
}

// Space for copy must be allocated before call
// Does not copy parents/children/index/acquired
void copy_clause_structure(clause *original, clause *copy) {
  copy->pos_count = original->pos_count;
  copy->neg_count = original->neg_count;
  copy->pos_lits = copy->pos_count > 0 ? malloc(sizeof(*copy->pos_lits)*copy->pos_count) : NULL;
  for (int i = 0; i < copy->pos_count; i++) {
    copy->pos_lits[i] = malloc(sizeof(*copy->pos_lits[i]));
    copy_alma_function(original->pos_lits[i], copy->pos_lits[i]);
  }
  copy->neg_lits = copy->neg_count > 0 ? malloc(sizeof(*copy->neg_lits)*copy->neg_count) : NULL;
  for (int i = 0; i < copy->neg_count; i++) {
    copy->neg_lits[i] = malloc(sizeof(*copy->neg_lits[i]));
    copy_alma_function(original->neg_lits[i], copy->neg_lits[i]);
  }
  copy->parent_set_count = copy->children_count = 0;
  copy->parents = NULL;
  copy->children = NULL;
  copy->tag = original->tag;
  if (copy->tag == FIF) {
    copy->fif = malloc(sizeof(*copy->fif));
    copy->fif->premise_count = original->fif->premise_count;
    copy->fif->ordering = malloc(sizeof(*copy->fif->ordering)*copy->fif->premise_count);
    memcpy(copy->fif->ordering, original->fif->ordering, sizeof(*copy->fif->ordering)*copy->fif->premise_count);
    copy->fif->num_conclusions = original->fif->num_conclusions;
    copy->fif->conclusions = malloc(sizeof(*copy->fif->conclusions) * copy->fif->num_conclusions);
    for (int i = 0; i < copy->fif->num_conclusions; i++) {
      copy->fif->conclusions[i] = malloc(sizeof(*copy->fif->conclusions[i]));
      copy_clause_structure(original->fif->conclusions[i], copy->fif->conclusions[i]);
    }
    if (copy->fif->conclusions[0]->pos_count > 0)
      copy->fif->indexing_conc = copy->fif->conclusions[0]->pos_lits[0];
    else
      copy->fif->indexing_conc = copy->fif->conclusions[0]->neg_lits[0];
  }
  else
    copy->fif = NULL;
}

void copy_parents(clause *original, clause *copy) {
  copy->parent_set_count = original->parent_set_count;
  if (copy->parent_set_count > 0) {
    copy->parents = malloc(sizeof(*copy->parents)*copy->parent_set_count);
    for (int i = 0; i < copy->parent_set_count; i++) {
      copy->parents[i].count = original->parents[i].count;
      copy->parents[i].clauses = malloc(sizeof(*copy->parents[i].clauses)*copy->parents[i].count);
      for (int j = 0; j < copy->parents[i].count; j++)
        copy->parents[i].clauses[j] = original->parents[i].clauses[j];
    }
  }
  else
    copy->parents = NULL;
}

// Flattens trees into set of clauses (tommy_array must be initialized prior)
// trees argument freed here
void nodes_to_clauses(kb *collection, alma_node *trees, int num_trees, tommy_array *clauses, int print, kb_str *buf) {
  //  printf("NODES 2 CLAUSES: %d\n",num_trees);
  for (int i = 0; i < num_trees; i++) {
    //    printf("FLATTEN NODE: %d/%d",i,num_trees);
    flatten_node(collection, trees+i, clauses, print, buf);
    free_alma_tree(trees+i);
  }
  free(trees);
}

void free_predname_mapping(void *arg) {
  predname_mapping *entry = arg;
  free(entry->predname);
  free(entry->clauses);
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in kb_halt
  free(entry);
}

// Compare function to be used by tommy_hashlin_search for index_mapping
// Compares long arg to key of index_mapping
static int im_compare(const void *arg, const void *obj) {
  return *(const long*)arg - ((const index_mapping*)obj)->key;
}

// Compare function to be used by tommy_hashlin_search for predname_mapping
// Compares string arg to predname of predname_mapping
int pm_compare(const void *arg, const void *obj) {
  return strcmp((const char*)arg, ((const predname_mapping*)obj)->predname);
}

// Returns true if clause is not distrusted, retired, or handled
int flags_negative(clause *c) {
  return c->distrusted < 0 && c->retired < 0 && c->handled < 0;
}

// Returns minimum among the flags of value >= 0
long flag_min(clause *c) {
  long min = LONG_MAX;
  if (c->distrusted >= 0)
    min = c->distrusted;
  if (c->retired >= 0 && c->retired < min)
    min = c->retired;
  if (c->handled >= 0 && c->handled < min)
    min = c->handled;
  return min;
}

// Given "name" and integer arity, returns "name/arity"
// Allocated cstring is later placed in predname_mapping, and eventually freed by kb_halt
char* name_with_arity(char *name, int arity) {
  int arity_len = snprintf(NULL, 0, "%d", arity);
  char *arity_str = malloc(arity_len+1);
  snprintf(arity_str, arity_len+1, "%d", arity);
  char *name_with_arity = malloc(strlen(name) + strlen(arity_str) + 2);
  memcpy(name_with_arity, name, strlen(name));
  name_with_arity[strlen(name)] = '/';
  strcpy(name_with_arity + strlen(name) + 1, arity_str);
  free(arity_str);
  return name_with_arity;
}

// Returns a mapping holding a set of clauses that contains those unifiable with clause c
// Return type is a fif_mapping or predname_mapping depending on c's tag
// For predname_mapping, looks up mapping for a predicate appearing as a literal in clause c;
//  currently, simply picks the first positive or negative literal
void* clause_lookup(kb *collection, clause *c) {
  if (c->tag == FIF) {
    char *name = c->fif->indexing_conc->name;
    return tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
  }
  else {
    tommy_hashlin *map = &collection->pos_map;
    alma_function *pred;
    if (c->pos_count != 0) {
      pred = c->pos_lits[0];
    }
    else {
      pred = c->neg_lits[0];
      map = &collection->neg_map;
    }
    char *name = name_with_arity(pred->name, pred->term_count);
    predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    free(name);
    return result;
  }
}

// Retrieves index-th item from mapping
// Tag determines interpreting it as fif_mapping or predname_mapping
clause* mapping_access(void *mapping, if_tag tag, int index) {
  if (tag == FIF)
    return ((fif_mapping*)mapping)->clauses[index];
  else
    return ((predname_mapping*)mapping)->clauses[index];
}

// Retrieves number of clauses held by mapping
// Tag determines interpreting it as fif_mapping or predname_mapping
int mapping_num_clauses(void *mapping, if_tag tag) {
  if (tag == FIF)
    return ((fif_mapping*)mapping)->num_clauses;
  else
    return ((predname_mapping*)mapping)->num_clauses;
}

// Determines of clauses have combatible tags and counts of arguments and conclusions
// Performs additional checks for fif conclusions
int counts_match(clause *x, clause *y) {
  if (x->tag == y->tag && x->pos_count == y->pos_count && x->neg_count == y->neg_count) {
    if (x->tag == FIF && x->fif->num_conclusions != y->fif->num_conclusions)
      return 0;
    return 1;
  }
  return 0;
}

// Inserts clause into the hashmap (with key based on lit), as well as the linked list
// If the map entry already exists, append; otherwise create a new one
static void map_add_clause(tommy_hashlin *map, tommy_list *list, alma_function *lit, clause *c) {
  char *name = name_with_arity(lit->name, lit->term_count);
  predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
  if (result != NULL) {
    result->num_clauses++;
    result->clauses = realloc(result->clauses, sizeof(*result->clauses) * result->num_clauses);
    result->clauses[result->num_clauses-1] = c; // Aliases with pointer assignment
    free(name); // Name not added into hashmap must be freed
  }
  else {
    predname_mapping *entry = malloc(sizeof(*entry));
    entry->predname = name;
    entry->num_clauses = 1;
    entry->clauses = malloc(sizeof(*entry->clauses));
    entry->clauses[0] = c; // Aliases with pointer assignment
    tommy_hashlin_insert(map, &entry->hash_node, entry, tommy_hash_u64(0, entry->predname, strlen(entry->predname)));
    tommy_list_insert_tail(list, &entry->list_node, entry);
  }
}

// Removes clause from hashmap and list if it exists
static void map_remove_clause(tommy_hashlin *map, tommy_list *list, alma_function *lit, clause *c) {
  char *name = name_with_arity(lit->name, lit->term_count);
  predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
  if (result != NULL) {
    for (int i = 0; i < result->num_clauses; i++) {
      if (result->clauses[i] == c) {
        if (i < result->num_clauses-1)
          result->clauses[i] = result->clauses[result->num_clauses-1];
        result->num_clauses--;

        // Shrink size of clause list of result
        if (result->num_clauses > 0) {
          result->clauses = realloc(result->clauses, sizeof(*result->clauses) * result->num_clauses);
        }
        // Remove predname_mapping from hashmap and linked list
        else {
          tommy_hashlin_remove_existing(map, &result->hash_node);
          tommy_list_remove_existing(list, &result->list_node);
          free_predname_mapping(result);
          free(name);
          return;
        }
      }
    }
  }
  free(name);
}

// Returns first alma_function pointer for literal with given name in clause
// Searching positive or negative literals is decided by pos boolean
static alma_function* literal_by_name(clause *c, char *name, int pos) {
  if (c != NULL) {
    int count = pos ? c->pos_count : c->neg_count;
    alma_function **lits = pos ? c->pos_lits : c->neg_lits;
    for (int i = 0; i < count; i++)
      if (strcmp(name, lits[i]->name) == 0)
        return lits[i];
  }
  return NULL;
}

// Returns 0 if variable pair respects x and y matchings in matches arg outside of quotes
static int variables_differ(alma_variable *x, alma_variable *y, var_match_set *matches, int quote_level) {
  return !var_match_consistent(matches, quote_level, x, y);
}

static int functions_differ(alma_function *x, alma_function *y, var_match_set *matches, int quote_level);

static int clauses_differ_rec(clause *x, clause *y, var_match_set *matches, int quote_level) {
  if (x->tag != y->tag || x->pos_count != y->pos_count || x->neg_count != y->neg_count)
    return 1;

  if (x->tag == FIF) {
    if (x->fif->num_conclusions != y->fif->num_conclusions)
      return 1;
    for (int i = 0; i < x->fif->premise_count; i++)
      if (functions_differ(fif_access(x, i), fif_access(y, i), matches, quote_level))
        return 1;
    for (int i = 0; i < x->fif->num_conclusions; i++)
      if (clauses_differ_rec(x->fif->conclusions[i], y->fif->conclusions[i], matches, quote_level))
        return 1;
  }
  else {
    // TODO: account for case in which may have several literals with name
    // Ignoring this case, sorted literal lists allows comparing ith literals of each clause
    for (int i = 0; i < x->pos_count; i++)
      if (functions_differ(x->pos_lits[i], y->pos_lits[i], matches, quote_level))
        return 1;
    // TODO: account for case in which may have several literals with name
    // Ignoring this case, sorted literal lists allows comparing ith literals of each clause
    for (int i = 0; i < x->neg_count; i++)
      if (functions_differ(x->neg_lits[i], y->neg_lits[i], matches, quote_level))
        return 1;
  }

  return 0;
}

// Returns 0 if quotation terms are equal while respecting x and y matchings based on matches arg; otherwise returns 1
// Pairs of variables according to var_match_set arg corresponding to their level of quotation
static int quotes_differ(alma_quote *x, alma_quote *y, var_match_set *matches, int quote_level) {
  if (x->type == y->type) {
    if (x->type == SENTENCE) {
      // TODO when handling case for quote of formula equivalent to more than one clause
      return 1;
    }
    else
      return clauses_differ_rec(x->clause_quote, y->clause_quote, matches, quote_level);
  }
  return 1;
}

static int quasiquotes_differ(alma_quasiquote *x, alma_quasiquote *y, var_match_set *matches, int quote_level) {
  return x->backtick_count != y->backtick_count || !var_match_consistent(matches, quote_level - x->backtick_count, x->variable, y->variable);
}

// Returns 0 if functions are equal while respecting x and y matchings based on matches arg; otherwise returns 1
// Further detail in clauses_differ
static int functions_differ(alma_function *x, alma_function *y, var_match_set *matches, int quote_level) {
  if (x->term_count == y->term_count && strcmp(x->name, y->name) == 0) {
    for (int i = 0; i < x->term_count; i++)
      if (x->terms[i].type != y->terms[i].type
          || (x->terms[i].type == VARIABLE && variables_differ(x->terms[i].variable, y->terms[i].variable, matches, quote_level))
          || (x->terms[i].type == FUNCTION && functions_differ(x->terms[i].function, y->terms[i].function, matches, quote_level))
          || (x->terms[i].type == QUOTE && quotes_differ(x->terms[i].quote, y->terms[i].quote, matches, quote_level+1))
          || (x->terms[i].type == QUASIQUOTE && quasiquotes_differ(x->terms[i].quasiquote, y->terms[i].quasiquote, matches, quote_level)))
        return 1;

    // All arguments compared as equal; return 0
    return 0;
  }
  return 1;
}

// Returns 0 if clauses have equal positive and negative literal sets; otherwise returns 1
// Equality of variables is that there is a one-to-one correspondence in the sets of variables x and y use,
// based on each location where a variable maps to another
// Thus a(X, X) and a(X, Y) are here considered different
int clauses_differ(clause *x, clause *y) {
    var_match_set matches;
    var_match_init(&matches);
    if (clauses_differ_rec(x, y, &matches, 0))
      return release_matches(&matches, 1);
    // All literals compared as equal; return 0
    return release_matches(&matches, 0);
}

// Checks if x and y are ground literals that are identical
// Currently does not check within quotes; any quotation term found returns 0
static int ground_duplicate_literals(alma_function *x, alma_function *y) {
  if (strcmp(x->name, y->name) == 0 && x->term_count == y->term_count) {
    for (int i = 0; i < x->term_count; i++)
      if (x->terms[i].type != y->terms[i].type || x->terms[i].type == VARIABLE || x->terms[i].type == QUASIQUOTE || x->terms[i].type == QUOTE
          || (x->terms[i].type == FUNCTION && !ground_duplicate_literals(x->terms[i].function, y->terms[i].function)))
        return 0;
    return 1;
  }
  return 0;
}

static void remove_dupe_literals(int *count, alma_function ***triple) {
  alma_function **lits = *triple;
  int deleted = 0;
  int new_count = *count;
  for (int i = 0; i < *count-1; i++) {
    if (ground_duplicate_literals(lits[i], lits[i+1])) {
      new_count--;
      deleted = 1;
      free_function(lits[i]);
      lits[i] = NULL;
    }
  }
  if (deleted) {
    // Collect non-null literals at start of set
    int loc = 1;
    for (int i = 0; i < *count-1; i++) {
      if (lits[i] == NULL) {
        loc = i+1;
        while (loc < *count && lits[loc] == NULL)
          loc++;
        if (loc >= *count)
          break;
        lits[i] = lits[loc];
        lits[loc] = NULL;
      }
    }
    *triple = realloc(lits, sizeof(*lits) * new_count);
    *count = new_count;
  }
}

// If c is found to be a clause's duplicate, returns a pointer to that clause; null otherwise
// See comments preceding clauses_differ function for further detail
// Check_distrusted flag determines whether distrusted clauses are used for dupe comparison as well
clause* duplicate_check(kb *collection, clause *c, int check_distrusted) {
  if (c->tag == FIF) {
    char *name = c->fif->indexing_conc->name;
    fif_mapping *result = tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (result != NULL) {
      for (int i = 0; i < result->num_clauses; i++) {
        if (c->tag == result->clauses[i]->tag && !clauses_differ(c, result->clauses[i]))
          return result->clauses[i];
      }
    }
  }
  else {
    // Check for ground literal duplicates, and alter clause to remove any found
    // Necessary for dupe checking to work in such cases, even if side effects are wasted for a discarded duplicate
    qsort(c->pos_lits, c->pos_count, sizeof(*c->pos_lits), function_compare);
    qsort(c->neg_lits, c->neg_count, sizeof(*c->neg_lits), function_compare);
    remove_dupe_literals(&c->pos_count, &c->pos_lits);
    remove_dupe_literals(&c->neg_count, &c->neg_lits);

    char *name;
    tommy_hashlin *map;
    // If clause has a positive literal, all duplicate candidates must have that same positive literal
    // Arbitrarily pick first positive literal as one to use; may be able to do smarter literal choice later
    if (c->pos_count > 0) {
      map = &collection->pos_map;
      name = name_with_arity(c->pos_lits[0]->name, c->pos_lits[0]->term_count);
    }
    else {
      map = &collection->neg_map;
      name = name_with_arity(c->neg_lits[0]->name, c->neg_lits[0]->term_count);
    }
    predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    free(name);
    if (result != NULL) {
      for (int i = 0; i < result->num_clauses; i++) {
        // A clause distrusted this timestep is still checked against
        if ((check_distrusted || flags_negative(result->clauses[i]) || flag_min(result->clauses[i]) == collection->time) &&
            c->tag == result->clauses[i]->tag && !clauses_differ(c, result->clauses[i]))
          return result->clauses[i];
      }
    }
  }
  return NULL;
}

// Remove res tasks using clause
static void remove_res_tasks(kb *collection, clause *c) {
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->res_tasks); i++) {
    res_task *current_task = tommy_array_get(&collection->res_tasks, i);
    if (current_task != NULL && (current_task->x == c || current_task->y == c)) {
      // Removed task set to null; null checked for when processing later
      tommy_array_set(&collection->res_tasks, i, NULL);
      free(current_task);
    }
  }
}

// Removes c from children list of p
static void remove_child(clause *p, clause *c) {
  if (p != NULL) {
    for (int j = 0; j < p->children_count; j++) {
      if (p->children[j] == c) {
        if (j < p->children_count-1)
          p->children[j] = p->children[p->children_count-1];
        p->children_count--;
        if (p->children_count > 0) {
          p->children = realloc(p->children, sizeof(*p->children) * p->children_count);
        }
        else {
          free(p->children);
          p->children = NULL;
        }
        break;
      }
    }
  }
}

// Removes fif from fif_map, and deletes all fif tasks using it
static void remove_fif_tasks(kb *collection, clause *c) {
  for (int i = 0; i < c->fif->premise_count; i++) {
    alma_function *f = fif_access(c, i);
    char *name = name_with_arity(f->name, f->term_count);
    fif_task_mapping *tm = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (tm != NULL) {
      tommy_node *curr = tommy_list_head(&tm->tasks);
      while (curr) {
        fif_task *currdata = curr->data;
        curr = curr->next;
        if (currdata->fif->index == c->index) {
          tommy_list_remove_existing(&tm->tasks, &currdata->node);
          free_fif_task(currdata);
        }
      }
    }
    free(name);
  }
}

void make_single_task(kb *collection, clause *c, alma_function *c_lit, clause *other, tommy_array *tasks, int use_bif, int pos) {
  if (c != other && (other->tag != BIF || use_bif) && other->tag != FIF) {
    alma_function *other_lit = literal_by_name(other, c_lit->name, pos);
    if (other_lit != NULL && other_lit != c_lit) {
      res_task *t = malloc(sizeof(*t));
      if (!pos) {
        t->x = c;
        t->pos = c_lit;
        t->y = other;
        t->neg = other_lit;
      }
      else {
        t->x = other;
        t->pos = other_lit;
        t->y = c;
        t->neg = c_lit;
      }

      tommy_array_insert(tasks, t);
    }
  }
}

// Helper of res_tasks_from_clause
void make_res_tasks(kb *collection, clause *c, int count, alma_function **c_lits, tommy_hashlin *map, tommy_array *tasks, int use_bif, int pos) {
  for (int i = 0; i < count; i++) {
    char *name = name_with_arity(c_lits[i]->name, c_lits[i]->term_count);
    predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));

    // New tasks are from Cartesian product of result's clauses with clauses' ith
    if (result != NULL)
      for (int j = 0; j < result->num_clauses; j++)
        if (flags_negative(result->clauses[j]))
          make_single_task(collection, c, c_lits[i], result->clauses[j], tasks, use_bif, pos);

    free(name);
  }
}

// Finds new res tasks based on matching pos/neg predicate pairs, where one is from the KB and the other from arg
// Tasks are added into the res_tasks of collection
// Used only for non-bif resolution tasks; hence checks tag of c
void res_tasks_from_clause(kb *collection, clause *c, int process_negatives) {
  if (c->tag != BIF && c->tag != FIF) {
    make_res_tasks(collection, c, c->pos_count, c->pos_lits, &collection->neg_map, &collection->res_tasks, 0, 0);
    // Only done if clauses differ from KB's clauses (i.e. after first task generation)
    if (process_negatives)
      make_res_tasks(collection, c, c->neg_count, c->neg_lits, &collection->pos_map, &collection->res_tasks, 0, 1);
  }
}

// Returns boolean based on success of string parse
// If parses successfully, adds to collection's new_clauses
// Returns pointer to first clause asserted, if any
clause* assert_formula(kb *collection, char *string, int print, kb_str *buf) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas, collection, buf)) {
    tommy_array temp;
    tommy_array_init(&temp);
    nodes_to_clauses(collection, formulas, formula_count, &temp, print, buf);
    clause *ret = tommy_array_size(&temp) > 0 ? tommy_array_get(&temp, 0) : NULL;
    for (tommy_size_t i = 0; i < tommy_array_size(&temp); i++)
      tommy_array_insert(&collection->new_clauses, tommy_array_get(&temp, i));
    tommy_array_done(&temp);
    return ret;
  }
  return NULL;
}

// Given a clause already existing in KB, remove from data structures
// Note that fif removal, or removal of a singleton clause used in a fif rule, may be very expensive
static void remove_clause(kb *collection, clause *c, kb_str *buf) {
  if (c->tag == FIF) {
    char *name = c->fif->indexing_conc->name;
    fif_mapping *fifm = tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (fifm != NULL) {
      for (int i = 0; i < fifm->num_clauses; i++) {
        if (fifm->clauses[i]->index == c->index) {
          if (i < fifm->num_clauses-1) {
            fifm->clauses[i] = fifm->clauses[fifm->num_clauses-1];
          }
          fifm->num_clauses--;
          if (fifm->num_clauses == 0) {
            tommy_hashlin_remove_existing(&collection->fif_map, &fifm->node);
            free_fif_mapping(fifm);
          }
          else {
            fifm->clauses = realloc(fifm->clauses, sizeof(*fifm->clauses)*fifm->num_clauses);
          }
          break;
        }
      }
    }

    remove_fif_tasks(collection, c);
  }
  else {
    // Non-fif must be unindexed from pos/neg maps, have resolution tasks and fif tasks (if in any) removed
    for (int i = 0; i < c->pos_count; i++)
      map_remove_clause(&collection->pos_map, &collection->pos_list, c->pos_lits[i], c);
    for (int i = 0; i < c->neg_count; i++)
      map_remove_clause(&collection->neg_map, &collection->neg_list, c->neg_lits[i], c);

    remove_res_tasks(collection, c);

    // May be used in fif tasks if it's a singleton clause
    if (c->pos_count + c->neg_count == 1)
      remove_fif_singleton_tasks(collection, c);
  }

  index_mapping *result = tommy_hashlin_search(&collection->index_map, im_compare, &c->index, tommy_hash_u64(0, &c->index, sizeof(c->index)));
  tommy_list_remove_existing(&collection->clauses, &result->list_node);
  tommy_hashlin_remove_existing(&collection->index_map, &result->hash_node);
  free(result);

  // Remove clause from the parents list of each of its children
  for (int i = 0; i < c->children_count; i++) {
    clause *child = c->children[i];
    if (child != NULL) {
      int new_count = child->parent_set_count;

      //  If a parent set has a clause matching, remove match
      for (int j = 0; j < child->parent_set_count; j++) {
        for (int k = 0; k < child->parents[j].count; k++) {
          if (child->parents[j].clauses[k] == c) {
            // If last parent in set, deallocate
            if (child->parents[j].count == 1) {
              new_count--;
              child->parents[j].count = 0;
              free(child->parents[j].clauses);
              child->parents[j].clauses = NULL;
            }
            // Otherwise, reallocate without parent
            else {
              child->parents[j].count--;
              child->parents[j].clauses[k] = child->parents[j].clauses[child->parents[j].count];
              child->parents[j].clauses = realloc(child->parents[j].clauses, sizeof(*child->parents[j].clauses)*child->parents[j].count);

              // After reallocating, check for parent set identical to this one and delete it if one exists
              for (int m = 0; m < child->parent_set_count; m++) {
                if (m != j && child->parents[m].clauses != NULL && child->parents[j].count == child->parents[m].count) {
                  int dupe = 1;
                  for (int n = 0; n < child->parents[m].count; n++) {
                    if (child->parents[m].clauses[n]->index != child->parents[j].clauses[n]->index) {
                      dupe = 0;
                      break;
                    }
                  }
                  if (dupe) {
                    new_count--;
                    child->parents[j].count = 0;
                    free(child->parents[j].clauses);
                    child->parents[j].clauses = NULL;
                    break;
                  }
                }
              }
            }
            break;
          }
        }
      }

      if (new_count > 0 && new_count != child->parent_set_count) {
        // Collect non-null parent sets at start of parents
        int loc = 1;
        for (int j = 0; j < child->parent_set_count-1; j++, loc++) {
          if (child->parents[j].clauses == NULL) {
            while (loc < child->parent_set_count && child->parents[loc].clauses == NULL)
              loc++;
            if (loc >= child->parent_set_count)
              break;
            child->parents[j].clauses = child->parents[loc].clauses;
            child->parents[j].count = child->parents[loc].count;
            child->parents[loc].clauses = NULL;
          }
        }
        child->parents = realloc(child->parents, sizeof(*child->parents)*new_count);
      }
      if (new_count == 0 && child->parents != NULL) {
        free(child->parents);
        child->parents = NULL;
      }
      child->parent_set_count = new_count;
    }
  }

  // Remove clause from the children list of each of its parents
  for (int i = 0; i < c->parent_set_count; i++)
    for (int j = 0; j < c->parents[i].count; j++)
      remove_child(c->parents[i].clauses[j], c);

  free_clause(c);
}

int delete_formula(kb *collection, char *string, int print, kb_str *buf) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas, collection, buf)) {
    // Convert formulas to clause array
    tommy_array clauses;
    tommy_array_init(&clauses);
    for (int i = 0; i < formula_count; i++) {
      flatten_node(collection, formulas+i, &clauses, 0, buf);
      free_alma_tree(formulas+i);
    }
    free(formulas);

    // Process and remove each clause
    for (tommy_size_t i = 0; i < tommy_array_size(&clauses); i++) {
      clause *curr = tommy_array_get(&clauses, i);
      clause *c = duplicate_check(collection, curr, 1);
      if (c != NULL) {
        if (print) {
          tee_alt("-a: ", collection, buf);
          clause_print(collection, c, buf);
          tee_alt(" removed\n", collection, buf);
        }
        remove_clause(collection, c, buf);
      }
      else if (print) {
        tee_alt("-a: ", collection, buf);
        clause_print(collection, curr, buf);
        tee_alt(" not found\n", collection, buf);
      }
      free_clause(curr);
    }

    tommy_array_done(&clauses);

    return 1;
  }
  return 0;
}

int update_formula(kb *collection, char *string, kb_str *buf) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas, collection, buf)) {
    // Must supply two formula arguments to update
    if (formula_count != 2) {
      for (int i = 0; i < formula_count; i++)
        free_alma_tree(formulas+i);
      free(formulas);
      tee_alt("-a: Incorrect number of arguments to update\n", collection, buf);
      return 0;
    }

    // Convert formulas to clause array
    tommy_array clauses;
    tommy_array_init(&clauses);
    for (int i = 0; i < formula_count; i++) {
      flatten_node(collection, formulas+i, &clauses, 0, buf);
      free_alma_tree(formulas+i);
    }
    free(formulas);

    int update_fail = 0;
    clause *target = tommy_array_get(&clauses, 0);
    clause *update = tommy_array_get(&clauses, 1);
    tommy_array_done(&clauses);
    clause *t_dupe;
    clause *u_dupe;
    if (target->tag == FIF || update->tag == FIF) {
      tee_alt("-a: Cannot update with fif clause\n", collection, buf);
      update_fail = 1;
    }
    else if ((t_dupe = duplicate_check(collection, target, 1)) == NULL) {
      tee_alt("-a: Clause ", collection, buf);
      clause_print(collection, target, buf);
      tee_alt(" to update not present\n", collection, buf);
      update_fail = 1;
    }
    else if ((u_dupe = duplicate_check(collection, update, 1)) != NULL) {
      tee_alt("-a: New version of clause already present\n", collection, buf);
      update_fail = 1;
    }

    if (update_fail) {
      free_clause(target);
      free_clause(update);
      return 0;
    }
    else {
      // Clear out res and fif tasks with old version of clause
      for (int i = 0; i < t_dupe->pos_count; i++)
        map_remove_clause(&collection->pos_map, &collection->pos_list, t_dupe->pos_lits[i], t_dupe);
      for (int i = 0; i < t_dupe->neg_count; i++)
        map_remove_clause(&collection->neg_map, &collection->neg_list, t_dupe->neg_lits[i], t_dupe);
      remove_res_tasks(collection, t_dupe);
      if (t_dupe->pos_count + t_dupe->neg_count == 1)
        remove_fif_singleton_tasks(collection, t_dupe);

      tee_alt("-a: ", collection, buf);
      clause_print(collection, target, buf);
      tee_alt(" updated to ", collection, buf);
      clause_print(collection, update, buf);
      tee_alt("\n", collection, buf);
      free_clause(target);

      // Swap clause contents from update
      int count_temp = t_dupe->pos_count;
      t_dupe->pos_count = update->pos_count;
      update->pos_count = count_temp;
      alma_function **lits_temp = t_dupe->pos_lits;
      t_dupe->pos_lits = update->pos_lits;
      update->pos_lits = lits_temp;
      count_temp = t_dupe->neg_count;
      t_dupe->neg_count = update->neg_count;
      update->neg_count = count_temp;
      lits_temp = t_dupe->neg_lits;
      t_dupe->neg_lits = update->neg_lits;
      update->neg_lits = lits_temp;
      t_dupe->dirty_bit = 1;
      t_dupe->pyobject_bit = 1;

      // Generate new tasks with updated clause
      res_tasks_from_clause(collection, t_dupe, 1);
      fif_tasks_from_clause(collection, t_dupe);
      for (int j = 0; j < t_dupe->pos_count; j++)
        map_add_clause(&collection->pos_map, &collection->pos_list, t_dupe->pos_lits[j], t_dupe);
      for (int j = 0; j < t_dupe->neg_count; j++)
        map_add_clause(&collection->neg_map, &collection->neg_list, t_dupe->neg_lits[j], t_dupe);

      free_clause(update);
      return 1;
    }
  }
  return 0;
}

// Resolve helper
static void lits_copy(int count, alma_function **lits, alma_function **cmp, binding_list *mgu, alma_function **res_lits, int *res_count) {
  for (int i = 0; i < count; i++) {
    // In calls cmp can be assigned to resolvent to not copy it
    if (cmp == NULL || lits[i] != *cmp) {
      res_lits[*res_count] = malloc(sizeof(*res_lits[*res_count]));
      copy_alma_function(lits[i], res_lits[*res_count]);
      (*res_count)++;
    }
  }
}

// Given an MGU, make a single resulting clause without unified literal pair, then substitute
static void resolve(res_task *t, binding_list *mgu, clause *result) {
  result->pos_count = 0;
  if (t->x->pos_count + t->y->pos_count - 1 > 0) {
    result->pos_lits = malloc(sizeof(*result->pos_lits) * (t->x->pos_count + t->y->pos_count - 1));
    // Copy positive literals from t->x and t->y
    lits_copy(t->x->pos_count, t->x->pos_lits, &t->pos, mgu, result->pos_lits, &result->pos_count);
    lits_copy(t->y->pos_count, t->y->pos_lits, NULL, mgu, result->pos_lits, &result->pos_count);
  }
  else
    result->pos_lits = NULL;

  result->neg_count = 0;
  if (t->x->neg_count + t->y->neg_count - 1 > 0) {
    result->neg_lits = malloc(sizeof(*result->neg_lits) * (t->x->neg_count + t->y->neg_count - 1));
    // Copy negative literals from t->x and t->y
    lits_copy(t->y->neg_count, t->y->neg_lits, &t->neg, mgu, result->neg_lits, &result->neg_count);
    lits_copy(t->x->neg_count, t->x->neg_lits, NULL, mgu, result->neg_lits, &result->neg_count);
  }
  else
    result->neg_lits = NULL;

  result->tag = NONE;
  result->fif = NULL;
  subst_clause(mgu, result, 0);
}

static void add_child(clause *parent, clause *child) {
  parent->children_count++;
  parent->children = realloc(parent->children, sizeof(*parent->children) * parent->children_count);
  parent->children[parent->children_count-1] = child;
}

// Transfers first parent of source into parent collection of target if it's not a repeat
void transfer_parent(kb *collection, clause *target, clause *source, int add_children, kb_str *buf) {
  // Check if the duplicate clause is a repeat from the same parents
  int repeat_parents = 0;
  for (int j = 0; j < target->parent_set_count; j++) {
    int k = 0;
    for (; k < source->parents[0].count && k < target->parents[j].count; k++) {
      if (source->parents[0].clauses[k] != target->parents[j].clauses[k])
        break;
    }
    if (k == target->parents[j].count && k == source->parents[0].count) {
      repeat_parents = 1;
      break;
    }
  }

  // A duplicate clause derivation should be acknowledged by adding extra parents to the clause it duplicates
  // Copy parents of source to target
  // As a new clause target has only one parent set
  if (!repeat_parents) {
    target->parents = realloc(target->parents, sizeof(*target->parents) * (target->parent_set_count + 1));
    int last = target->parent_set_count;
    target->parents[last].count = source->parents[0].count;
    target->parents[last].clauses = malloc(sizeof(*target->parents[last].clauses) * target->parents[last].count);
    memcpy(target->parents[last].clauses, source->parents[0].clauses, sizeof(*target->parents[last].clauses) * target->parents[last].count);
    target->parent_set_count++;

    if (add_children) {
      // Parents of source also gain new child in target
      for (int j = 0; j < source->parents[0].count; j++) {
        int insert = 1;
        for (int k = 0; k < source->parents[0].clauses[j]->children_count; k++) {
          if (source->parents[0].clauses[j]->children[k] == target) {
            insert = 0;
            break;
          }
        }
        if (insert)
          add_child(source->parents[0].clauses[j], target);
      }
    }
  }
}


// Places clause into term as clause_quote type of quote, with copy of structure
static void quote_from_clause(alma_term *t, clause *c) {
  t->type = QUOTE;
  t->quote = malloc(sizeof(*t->quote));
  t->quote->type = CLAUSE;
  t->quote->clause_quote = malloc(sizeof(*t->quote->clause_quote));
  copy_clause_structure(c, t->quote->clause_quote);
}

void func_from_long(alma_term *t, long l) {
  t->type = FUNCTION;
  t->function = malloc(sizeof(*t->function));
  int length = snprintf(NULL, 0, "%ld", l);
  t->function->name = malloc(length+1);
  snprintf(t->function->name, length+1, "%ld", l);
  t->function->term_count = 0;
  t->function->terms = NULL;
}

static int derivations_distrusted(clause *c) {
  for (int i = 0; i < c->parent_set_count; i++) {
    int parent_distrusted = 0;
    for (int j = 0; j < c->parents[i].count; j++) {
      if (c->parents[i].clauses[j]->distrusted >= 0) {
        parent_distrusted = 1;
        break;
      }
    }
    if (!parent_distrusted)
      return 0;
  }
  return 1;
}

// Commonly used for special clauses like true, distrust, contra, etc.
static void init_single_parent(clause *child, clause *parent) {
  if (parent != NULL) {
    child->parent_set_count = 1;
    child->parents = malloc(sizeof(*child->parents));
    child->parents[0].count = 1;
    child->parents[0].clauses = malloc(sizeof(*child->parents[0].clauses));
    child->parents[0].clauses[0] = parent;
  }
}

// Creates a meta-formula literal with binary arguments of quotation term for c and the time
static clause* make_meta_literal(kb *collection, char *predname, clause *c, long time) {
  clause *res = malloc(sizeof(*res));
  res->pos_count = 1;
  res->neg_count = 0;
  res->pos_lits = malloc(sizeof(*res->pos_lits));
  res->pos_lits[0] = malloc(sizeof(*res->pos_lits[0]));
  res->pos_lits[0]->name = malloc(strlen(predname)+1);
  strcpy(res->pos_lits[0]->name, predname);
  res->pos_lits[0]->term_count = 2;
  res->pos_lits[0]->terms = malloc(sizeof(*res->pos_lits[0]->terms) * 2);
  quote_from_clause(res->pos_lits[0]->terms+0, c);
  func_from_long(res->pos_lits[0]->terms+1, collection->time);
  res->neg_lits = NULL;
  res->parent_set_count = 0;
  res->children_count = 0;
  res->parents = NULL;
  res->children = NULL;
  res->tag = NONE;
  res->fif = NULL;
  set_variable_ids(res, 1, 0, NULL, collection);
  return res;
}

static void distrust_recursive(kb *collection, clause *c, clause *contra, kb_str *buf) {
  // Formula becomes distrusted at current time
  c->distrusted = collection->time;

  if (c->tag == FIF)
    remove_fif_tasks(collection, c);

  // Assert atomic distrusted() formula
  clause *d = make_meta_literal(collection, "distrusted", c, collection->time);
  init_single_parent(d, contra);
  tommy_array_insert(&collection->new_clauses, d);

  // Recursively distrust children
  if (c->children != NULL) {
    for (int i = 0; i < c->children_count; i++) {
      if (c->children[i]->distrusted < 0 && derivations_distrusted(c->children[i])) {
        distrust_recursive(collection, c->children[i], contra, buf);
      }
    }
  }
}

static void retire_recursive(kb *collection, clause *c, clause *contra, kb_str *buf) {
  // TODO: appropriate retired-handling code to recurse through clause and descendants as needed
}

static void binding_subst(binding_list *target, binding_list *theta) {
  for (int i = 0; i < target->num_bindings; i++)
    subst_term(theta, target->list[i].term, 0);
}

static binding_list* parent_binding_prepare(backsearch_task *bs, long parent_index, binding_list *theta) {
  // Retrieve parent existing bindings, if any
  binding_mapping *mapping = NULL;
  if (parent_index < 0)
    mapping = tommy_hashlin_search(&bs->clause_bindings, bm_compare, &parent_index, tommy_hash_u64(0, &parent_index, sizeof(parent_index)));

  // Substitute based on MGU obtained when unifying
  if (mapping != NULL) {
    binding_list *copy = malloc(sizeof(*copy));
    copy_bindings(copy, mapping->bindings);
    binding_subst(copy, theta);
    return copy;
  }
  else
    return NULL;
}

static void make_contra(kb *collection, clause *contradictand_pos, clause *contradictand_neg) {
  clause *contra = malloc(sizeof(*contra));
  contra->pos_count = 1;
  contra->neg_count = 0;
  contra->pos_lits = malloc(sizeof(*contra->pos_lits));
  contra->pos_lits[0] = malloc(sizeof(*contra->pos_lits[0]));
  contra->pos_lits[0]->name = malloc(strlen("contra_event")+1);
  strcpy(contra->pos_lits[0]->name, "contra_event");
  contra->pos_lits[0]->term_count = 3;
  contra->pos_lits[0]->terms = malloc(sizeof(*contra->pos_lits[0]->terms) * 3);
  quote_from_clause(contra->pos_lits[0]->terms+0, contradictand_pos);
  quote_from_clause(contra->pos_lits[0]->terms+1, contradictand_neg);
  func_from_long(contra->pos_lits[0]->terms+2, collection->time);
  contra->neg_lits = NULL;
  contra->parent_set_count = contra->children_count = 0;
  contra->parents = NULL;
  contra->children = NULL;
  contra->tag = NONE;
  contra->fif = NULL;
  set_variable_ids(contra, 1, 0, NULL, collection);
  tommy_array_insert(&collection->new_clauses, contra);

  clause *contradicting = malloc(sizeof(*contradicting));
  copy_clause_structure(contra, contradicting);
  contradicting->pos_lits[0]->name = realloc(contradicting->pos_lits[0]->name, strlen("contradicting")+1);
  strcpy(contradicting->pos_lits[0]->name, "contradicting");
  init_single_parent(contradicting, contra);
  set_variable_ids(contradicting, 1, 0, NULL, collection);
  tommy_array_insert(&collection->new_clauses, contradicting);

  tommy_array_insert(&collection->distrust_set, contradictand_pos);
  tommy_array_insert(&collection->distrust_parents, contra);
  tommy_array_insert(&collection->distrust_set, contradictand_neg);
  tommy_array_insert(&collection->distrust_parents, contra);
}

// Process resolution tasks from argument and place results in new_arr
void process_res_tasks(kb *collection, tommy_array *tasks, tommy_array *new_arr, backsearch_task *bs, kb_str *buf) {
  for (tommy_size_t i = 0; i < tommy_array_size(tasks); i++) {
    res_task *current_task = tommy_array_get(tasks, i);
    if (current_task != NULL) {
      // Does not do resolution with a distrusted clause
      if (flags_negative(current_task->x) && flags_negative(current_task->y)) {
        binding_list *theta = malloc(sizeof(*theta));
        init_bindings(theta);
        if (collection->verbose)
          print_unify(collection, current_task->pos, current_task->x->index, current_task->neg, current_task->y->index, buf);

        // Given a res_task, attempt unification
        if (pred_unify(current_task->pos, current_task->neg, theta, collection->verbose)) {
          // If successful, create clause for result of resolution and add to new_clauses
          clause *res_result = malloc(sizeof(*res_result));
          resolve(current_task, theta, res_result);

          binding_list *x_bindings = NULL;
          if (bs) {
            x_bindings = parent_binding_prepare(bs, current_task->x->index, theta);
            binding_list *y_bindings = parent_binding_prepare(bs, current_task->y->index, theta);
            if (x_bindings != NULL && y_bindings != NULL) {
              // Check that tracked bindings for unified pair are compatible/unifiable
              binding_list *parent_theta = malloc(sizeof(*parent_theta));
              init_bindings(parent_theta);
              int unify_fail = 0;
              for (int j = 0; j < x_bindings->num_bindings; j++) {
                if (!term_unify(x_bindings->list[j].term, y_bindings->list[j].term, parent_theta)) {
                  unify_fail = 1;
                  break;
                }
              }
              cleanup_bindings(y_bindings);
              if (unify_fail) {
                cleanup_bindings(parent_theta);
                cleanup_bindings(x_bindings);

                if (res_result->pos_count > 0) {
                  for (int j = 0; j < res_result->pos_count; j++)
                    free_function(res_result->pos_lits[j]);
                  free(res_result->pos_lits);
                }
                if (res_result->neg_count > 0) {
                  for (int j = 0; j < res_result->neg_count; j++)
                    free_function(res_result->neg_lits[j]);
                  free(res_result->neg_lits);
                }
                free(res_result);
                goto cleanup;
              }
              else {
                // Use x_bindings with collected info as binding stuff
                binding_subst(x_bindings, parent_theta);

                // Apply parent_theta to resolution result as well
                subst_clause(parent_theta, res_result, 0);

                cleanup_bindings(parent_theta);
              }
            }
            else if (x_bindings == NULL) {
              // Consolidate to only x_bindings
              x_bindings = y_bindings;
            }
          }

          // A resolution result must be empty to be a valid clause to add to KB
          if (res_result->pos_count > 0 || res_result->neg_count > 0) {
            // Initialize parents of result
            res_result->parent_set_count = 1;
            res_result->parents = malloc(sizeof(*res_result->parents));
            res_result->parents[0].count = 2;
            res_result->parents[0].clauses = malloc(sizeof(*res_result->parents[0].clauses) * 2);
            res_result->parents[0].clauses[0] = current_task->x;
            res_result->parents[0].clauses[1] = current_task->y;
            res_result->children_count = 0;
            res_result->children = NULL;

            set_variable_ids(res_result, 0, 0, x_bindings, collection);

            tommy_array_insert(new_arr, res_result);
            if (bs)
              tommy_array_insert(&bs->new_clause_bindings, x_bindings);
          }
          else {
            free(res_result);

            if (bs) {
              clause *answer = malloc(sizeof(*answer));
              memcpy(answer, bs->target, sizeof(*answer));
              if (bs->target->pos_count == 1) {
                answer->pos_lits = malloc(sizeof(*answer->pos_lits));
                answer->pos_lits[0] = malloc(sizeof(*answer->pos_lits[0]));
                copy_alma_function(bs->target->pos_lits[0], answer->pos_lits[0]);
                for (int j = 0; j < answer->pos_lits[0]->term_count; j++)
                  subst_term(x_bindings, answer->pos_lits[0]->terms+j, 0);
              }
              else {
                answer->neg_lits = malloc(sizeof(*answer->neg_lits));
                answer->neg_lits[0] = malloc(sizeof(*answer->neg_lits[0]));
                copy_alma_function(bs->target->neg_lits[0], answer->neg_lits[0]);
                for (int j = 0; j < answer->neg_lits[0]->term_count; j++)
                  subst_term(x_bindings, answer->neg_lits[0]->terms+j, 0);
              }
              set_variable_ids(answer, 0, 0, NULL, collection);

              // TODO: parent setup for backsearch answer?
              tommy_array_insert(&collection->new_clauses, answer);
              cleanup_bindings(x_bindings);
            }
            // If not a backward search, empty resolution result indicates a contradiction between clauses
            else
              make_contra(collection, current_task->x, current_task->y);
          }
        }
        if (collection->verbose)
          print_bindings(collection, theta, 1, 1, buf);

        cleanup:
        cleanup_bindings(theta);
      }

      free(current_task);
    }
  }
  tommy_array_done(tasks);
  tommy_array_init(tasks);
}


// Given a new clause, add to the KB and maps
static void add_clause(kb *collection, clause *c) {
  // Add clause to overall clause list and index map
  index_mapping *ientry = malloc(sizeof(*ientry));
  c->index = ientry->key = collection->next_index++;
  c->acquired = collection->time;
  c->distrusted = -1;
  c->retired = -1;
  c->handled = -1;
  c->dirty_bit = (char) 1;
  c->pyobject_bit = (char) 1;
  ientry->value = c;
  tommy_list_insert_tail(&collection->clauses, &ientry->list_node, ientry);
  tommy_hashlin_insert(&collection->index_map, &ientry->hash_node, ientry, tommy_hash_u64(0, &ientry->key, sizeof(ientry->key)));

  if (c->tag == FIF) {
    char *name = c->fif->indexing_conc->name;
    // Index into fif hashmap
    fif_mapping *result = tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (result != NULL) {
      result->num_clauses++;
      result->clauses = realloc(result->clauses, sizeof(*result->clauses)*result->num_clauses);
      result->clauses[result->num_clauses-1] = c;
    }
    else {
      fif_mapping *entry = malloc(sizeof(*entry));
      entry->indexing_conc_name = malloc(strlen(name)+1);
      strcpy(entry->indexing_conc_name, name);
      entry->num_clauses = 1;
      entry->clauses = malloc(sizeof(*entry->clauses));
      entry->clauses[0] = c;
      tommy_hashlin_insert(&collection->fif_map, &entry->node, entry, tommy_hash_u64(0, entry->indexing_conc_name, strlen(entry->indexing_conc_name)));
    }
  }
  else {
    // If non-fif, indexes clause into pos/neg hashmaps/lists
    for (int j = 0; j < c->pos_count; j++)
      map_add_clause(&collection->pos_map, &collection->pos_list, c->pos_lits[j], c);
    for (int j = 0; j < c->neg_count; j++)
      map_add_clause(&collection->neg_map, &collection->neg_list, c->neg_lits[j], c);
  }
}

// Special semantic operator: true
// Must be a singleton positive literal with unary quote arg
// Process quoted material into new formulas with truth as parent
static void handle_true(kb *collection, clause *truth, kb_str *buf) {
  if (truth->pos_lits[0]->term_count == 1 && truth->pos_lits[0]->terms[0].type == QUOTE) {
    alma_quote *quote = truth->pos_lits[0]->terms[0].quote;
    tommy_array unquoted;
    tommy_array_init(&unquoted);
    // Raw sentence must be converted into clauses
    if (quote->type == SENTENCE) {
      alma_node *sentence_copy = malloc(sizeof(*sentence_copy));
      copy_alma_tree(quote->sentence, sentence_copy);
      make_cnf(sentence_copy);
      flatten_node(collection, sentence_copy, &unquoted, 0, buf);
      free_alma_tree(sentence_copy);
      free(sentence_copy);
    }
    // Quote clause can be extracted directly
    else {
      clause *u = malloc(sizeof(*u));
      copy_clause_structure(quote->clause_quote, u);

      // True formula has its outermost quotation withdrawn
      decrement_quote_level(u, 1);

      // Adjust variable IDs for the new formula
      set_variable_ids(u, 1, 0, NULL, collection);
      tommy_array_insert(&unquoted, u);
    }

    for (int i = 0; i < tommy_array_size(&unquoted); i++) {
      clause *curr = tommy_array_get(&unquoted, i);
      init_single_parent(curr, truth);

      // Inserted same timestep
      tommy_array_insert(&collection->timestep_delay_clauses, curr);
    }
    tommy_array_done(&unquoted);
  }
}

// Special semantic operator: distrust
// If argument unifies with a formula in the KB, derives distrusted
// Distrusted formulas cannot be more general than argument
// Hence, clauses found are placed in a quotation term with no added quasi-quotation
static void handle_distrust(kb *collection, clause *distrust, kb_str *buf) {
  alma_function *lit = distrust->pos_lits[0];
  if (lit->term_count == 1 && lit->terms[0].type == QUOTE && lit->terms[0].quote->type == CLAUSE) {
    void *mapping = clause_lookup(collection, lit->terms[0].quote->clause_quote);
    if (mapping != NULL) {
      alma_quote *q = malloc(sizeof(*q));
      q->type = CLAUSE;

      for (int i = mapping_num_clauses(mapping, lit->terms[0].quote->clause_quote->tag)-1; i >= 0; i--) {
        clause *ith = mapping_access(mapping, lit->terms[0].quote->clause_quote->tag, i);
        if (flags_negative(ith) && counts_match(ith, lit->terms[0].quote->clause_quote)) {
          q->clause_quote = ith;
          binding_list *theta = malloc(sizeof(*theta));
          init_bindings(theta);

          // Distrust each match found
          if (quote_term_unify(lit->terms[0].quote, q, theta)) {
            tommy_array_insert(&collection->distrust_set, ith);
            tommy_array_insert(&collection->distrust_parents, distrust);
          }
          cleanup_bindings(theta);
        }
      }
      free(q);
    }
  }
}

static int all_digits(char *str) {
  for (int i = 0; i < strlen(str); i++)
    if (!isdigit(str[i]))
      return 0;
  return 1;
}

// Special semantic operator: reinstate
// Reinstatement succeeds if given args of a quote matching distrusted formula(s) and a matching timestep for when distrusted
static void handle_reinstate(kb *collection, clause *reinstate, kb_str *buf) {
  alma_term *arg1 = reinstate->pos_lits[0]->terms+0;
  alma_term *arg2 = reinstate->pos_lits[0]->terms+1;

  void *mapping = clause_lookup(collection, arg1->quote->clause_quote);
  if (mapping != NULL) {
    alma_quote *q = malloc(sizeof(*q));
    q->type = CLAUSE;

    for (int j = mapping_num_clauses(mapping, arg1->quote->clause_quote->tag)-1; j >= 0; j--) {
      clause *jth = mapping_access(mapping, arg1->quote->clause_quote->tag, j);
      if (jth->distrusted == atol(arg2->function->name) && counts_match(jth, arg1->quote->clause_quote)) {
        q->clause_quote = jth;
        binding_list *theta = malloc(sizeof(*theta));
        init_bindings(theta);

        // Unification succeeded, create and add the reinstatement clause
        if (quote_term_unify(arg1->quote, q, theta)) {
          clause *reinstatement = malloc(sizeof(*reinstatement));
          copy_clause_structure(jth, reinstatement);
          copy_parents(jth, reinstatement);
          subst_clause(theta, reinstatement, 0);
          set_variable_ids(reinstatement, 1, 0, NULL, collection);
          tommy_array_insert(&collection->timestep_delay_clauses, reinstatement);

          if (reinstatement->pos_count + reinstatement->neg_count == 1 && reinstatement->fif == NULL) {
            // Look for contradicting() instances with reinstate target as a contradictand, to be handled()
            char *name = name_with_arity("contradicting", 3);
            predname_mapping *result = tommy_hashlin_search(&collection->pos_map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
            if (result != NULL) {
              // Select argument to match reinstated literal
              int contra_arg = (reinstatement->neg_count == 1);

              for (int k = 0; k < result->num_clauses; k++) {
                clause *con = result->clauses[k];
                if (con->pos_count == 1 && con->neg_count == 0 && con->fif == NULL &&
                    (flags_negative(con) || flag_min(con) == collection->time) &&
                    con->pos_lits[0]->term_count == 3 && con->pos_lits[0]->terms[contra_arg].type == QUOTE) {

                  binding_list *contra_theta = malloc(sizeof(*contra_theta));
                  init_bindings(contra_theta);

                  // Contradicting() formula found with argument matching reinstate arg; mark this contradiction as handled
                  if (quote_term_unify(con->pos_lits[0]->terms[contra_arg].quote, q, contra_theta)) {
                    tommy_array_insert(&collection->handle_set, con);
                    tommy_array_insert(&collection->handle_parents, reinstate);
                  }
                  cleanup_bindings(contra_theta);
                }
              }
            }
            free(name);

            // Look for distrusted() instances with reinstate target, to be retired (??)
            // TODO
          }
        }
        cleanup_bindings(theta);
      }
    }
    free(q);
  }

}

// Special semantic operator: update
// Updating succeeds if given a quote matching KB formula(s) and a quote for the updated clause
static void handle_update(kb *collection, clause *update, kb_str *buf) {
  alma_term *arg1 = update->pos_lits[0]->terms+0;
  alma_term *arg2 = update->pos_lits[0]->terms+1;

  void *mapping = clause_lookup(collection, arg1->quote->clause_quote);
  if (mapping != NULL) {
    alma_quote *q = malloc(sizeof(*q));
    q->type = CLAUSE;

    for (int j = mapping_num_clauses(mapping, arg1->quote->clause_quote->tag)-1; j >= 0; j--) {
      clause *jth = mapping_access(mapping, arg1->quote->clause_quote->tag, j);
      if (jth->distrusted < 0 && counts_match(jth, arg1->quote->clause_quote)) {
        q->clause_quote = jth;
        binding_list *theta = malloc(sizeof(*theta));
        init_bindings(theta);

        // Unification succeeded, create and add the updated clause and distrust original
        if (quote_term_unify(arg1->quote, q, theta)) {
          clause *update_clause = malloc(sizeof(*update_clause));
          copy_clause_structure(arg2->quote->clause_quote, update_clause);
          init_single_parent(update_clause, update);

          subst_clause(theta, update_clause, 0);
          set_variable_ids(update_clause, 1, 0, NULL, collection);
          tommy_array_insert(&collection->new_clauses, update_clause);

          tommy_array_insert(&collection->distrust_set, jth);
          tommy_array_insert(&collection->distrust_parents, update);
        }
        cleanup_bindings(theta);
      }
    }
    free(q);
  }
}

// Processing of new clauses: inserts non-duplicates derived, makes new tasks from if flag is active
// Special handling for true, reinstate, distrust, update
void process_new_clauses(kb *collection, kb_str *buf, int make_tasks) {
  fif_to_front(&collection->new_clauses);

  for (tommy_size_t i = 0; i < tommy_array_size(&collection->new_clauses); i++) {
    clause *c = tommy_array_get(&collection->new_clauses, i);
    clause *dupe;
    if ((dupe = duplicate_check(collection, c, 0)) == NULL) {
      //c->dirty_bit = 1;
      add_clause(collection, c);

      if (c->pos_count == 1 && c->neg_count == 0) {
        if (strcmp(c->pos_lits[0]->name, "true") == 0) {
          tommy_array_insert(&collection->trues, c);
          tommy_array_set(&collection->new_clauses, i, NULL);
        }
        else if (strcmp(c->pos_lits[0]->name, "distrust") == 0)
          handle_distrust(collection, c, buf);
      }

      // Case for reinstate
      // Reinstate() be a singleton positive literal with binary args, former as quotation term
      if (c->pos_count == 1 && c->neg_count == 0 && strcmp(c->pos_lits[0]->name, "reinstate") == 0) {
        if (c->tag == NONE && c->pos_lits[0]->term_count == 2 && c->pos_lits[0]->terms[0].type == QUOTE &&
            c->pos_lits[0]->terms[0].quote->type == CLAUSE && c->pos_lits[0]->terms[1].type == FUNCTION &&
            c->pos_lits[0]->terms[1].function->term_count == 0 && all_digits(c->pos_lits[0]->terms[1].function->name)) {
          if (c->pos_lits[0]->terms[0].quote->clause_quote->pos_count == 1 &&
              c->pos_lits[0]->terms[0].quote->clause_quote->neg_count == 0) {
            tommy_array_insert(&collection->pos_lit_reinstates, c);
            tommy_array_set(&collection->new_clauses, i, NULL);
          }
          else if (c->pos_lits[0]->terms[0].quote->clause_quote->neg_count == 1 &&
              c->pos_lits[0]->terms[0].quote->clause_quote->pos_count == 0) {
            tommy_array_insert(&collection->neg_lit_reinstates, c);
            tommy_array_set(&collection->new_clauses, i, NULL);
          }
          else
            handle_reinstate(collection, c, buf);
        }
      }
      // Case for update
      // Must be a singleton positive literal with binary args, both as quotation terms
      else if (c->pos_count == 1 && c->neg_count == 0 && strcmp(c->pos_lits[0]->name, "update") == 0) {
        if (c->tag == NONE && c->pos_lits[0]->term_count == 2 && c->pos_lits[0]->terms[0].type == QUOTE &&
            c->pos_lits[0]->terms[0].quote->type == CLAUSE && c->pos_lits[0]->terms[1].type == QUOTE &&
            c->pos_lits[0]->terms[1].quote->type == CLAUSE) {
          handle_update(collection, c, buf);
        }
      }
      // Non-reinstate/non-update sentences generate tasks
      else {
        // Update child info for parents of new clause
        if (c->parents != NULL) {
          for (int j = 0; j < c->parents[0].count; j++)
            add_child(c->parents[0].clauses[j], c);
        }

        if (make_tasks) {
          if (c->tag == FIF) {
            fif_task_map_init(collection, c, 1);
          }
          else {
            res_tasks_from_clause(collection, c, 1);
            fif_tasks_from_clause(collection, c);

            // Get tasks between new KB clauses and all bs clauses
            tommy_node *curr = tommy_list_head(&collection->backsearch_tasks);
            while (curr) {
              backsearch_task *t = curr->data;
              for (int j = 0; j < tommy_array_size(&t->clauses); j++) {
                clause *bt_c = tommy_array_get(&t->clauses, j);
                for (int k = 0; k < bt_c->pos_count; k++)
                  make_single_task(collection, bt_c, bt_c->pos_lits[k], c, &t->to_resolve, 1, 0);
                for (int k = 0; k < bt_c->neg_count; k++)
                  make_single_task(collection, bt_c, bt_c->neg_lits[k], c, &t->to_resolve, 1, 1);
              }
              curr = curr->next;
            }
          }
        }
      }
    }
    else {
      if (collection->verbose) {
        tee_alt("-a: Duplicate clause ", collection, buf);
        clause_print(collection, c, buf);
        tee_alt(" merged into %ld\n", collection, buf, dupe->index);
      }

      if (c->parents != NULL) {
        transfer_parent(collection, dupe, c, 1, buf);
        dupe->dirty_bit = 1;
      }

      free_clause(c);
      tommy_array_set(&collection->new_clauses, i, NULL);
    }
  }
  tommy_array_done(&collection->new_clauses);
  tommy_array_init(&collection->new_clauses);

  // Process reinstatements of literals to look for contradictory reinstatements between them
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->pos_lit_reinstates); i++) {
    clause *reinstate = tommy_array_get(&collection->pos_lit_reinstates, i);
    int contradictory = 0;
    for (tommy_size_t j = 0; j < tommy_array_size(&collection->neg_lit_reinstates); j++) {
      clause *reinstate_neg = tommy_array_get(&collection->neg_lit_reinstates, j);
      // Proceed if reinstates' target literals have same predicate name
      alma_function *pos = reinstate->pos_lits[0]->terms[0].quote->clause_quote->pos_lits[0];
      alma_function *neg = reinstate_neg->pos_lits[0]->terms[0].quote->clause_quote->neg_lits[0];
      if (strcmp(pos->name, neg->name) == 0) {
        binding_list *theta = malloc(sizeof(*theta));
        init_bindings(theta);

        // If unification succeeds, then they're contradictory reinstatement targets
        if (pred_unify(pos, neg, theta, 0)) {
          make_contra(collection, reinstate, reinstate_neg);
          contradictory = 1;
        }
        cleanup_bindings(theta);
      }
    }
    // If found no contra iterating the negative reinstate literals, handle to fully reinstate
    if (!contradictory)
      handle_reinstate(collection, reinstate, buf);
  }
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->neg_lit_reinstates); i++) {
    clause *reinstate = tommy_array_get(&collection->neg_lit_reinstates, i);
    int contradictory = 0;

    for (tommy_size_t j = 0; j < tommy_array_size(&collection->distrust_set); j++) {
      clause *distrusted = tommy_array_get(&collection->distrust_set, j);
      if (distrusted->index == reinstate->index) {
        contradictory = 1;
        break;
      }
    }

    // If negative reinstate literal is not distrusted from earlier contra, handle to fully reinstate
    if (!contradictory)
      handle_reinstate(collection, reinstate, buf);
  }
  tommy_array_done(&collection->pos_lit_reinstates);
  tommy_array_init(&collection->pos_lit_reinstates);
  tommy_array_done(&collection->neg_lit_reinstates);
  tommy_array_init(&collection->neg_lit_reinstates);

  // Alter flag and make distrusted() formula for those newly distrusted, as well as distrusted descendants
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->distrust_set); i++) {
    clause *c = tommy_array_get(&collection->distrust_set, i);
    clause *parent = tommy_array_get(&collection->distrust_parents, i);
    distrust_recursive(collection, c, parent, buf);
  }
  tommy_array_done(&collection->distrust_set);
  tommy_array_init(&collection->distrust_set);
  tommy_array_done(&collection->distrust_parents);
  tommy_array_init(&collection->distrust_parents);

  // Alter flag and make handled() formula for those newly handled
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->handle_set); i++) {
    clause *c = tommy_array_get(&collection->handle_set, i);
    clause *parent = tommy_array_get(&collection->handle_parents, i);
    c->handled = collection->time;
    clause *handled = make_meta_literal(collection, "handled", c, collection->time);
    init_single_parent(handled, parent);
    tommy_array_insert(&collection->new_clauses, handled);
  }
  tommy_array_done(&collection->handle_set);
  tommy_array_init(&collection->handle_set);
  tommy_array_done(&collection->handle_parents);
  tommy_array_init(&collection->handle_parents);

  // Alter flag and make retired() formula for those newly retired, as well as retired descendants
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->distrust_set); i++) {
    clause *c = tommy_array_get(&collection->retire_set, i);
    clause *parent = tommy_array_get(&collection->retire_parents, i);
    retire_recursive(collection, c, parent, buf);
  }
  tommy_array_done(&collection->retire_set);
  tommy_array_init(&collection->retire_set);
  tommy_array_done(&collection->retire_parents);
  tommy_array_init(&collection->retire_parents);

  for (tommy_size_t i = 0; i < tommy_array_size(&collection->trues); i++) {
    clause *c = tommy_array_get(&collection->trues, i);
    if (flags_negative(c))
      handle_true(collection, c, buf);
  }
  tommy_array_done(&collection->trues);
  tommy_array_init(&collection->trues);

}
