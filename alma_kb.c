#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "alma_kb.h"
#include "alma_proc.h"
#include "alma_print.h"

static long next_index;

static void make_clause(alma_node *node, clause *c) {
  // Note: predicate null assignment necessary for freeing of notes without losing predicates
  if (node != NULL) {
    if (node->type == FOL) {
      // Neg lit case for NOT
      if (node->fol->op == NOT) {
        c->neg_count++;
        c->neg_lits = realloc(c->neg_lits, sizeof(*c->neg_lits) * c->neg_count);
        c->neg_lits[c->neg_count-1] = node->fol->arg1->predicate;
        node->fol->arg1->predicate = NULL;
      }
      // Case of node is OR
      else {
        make_clause(node->fol->arg1, c);
        make_clause(node->fol->arg2, c);
      }
      if (c->tag == NONE)
        c->tag = node->fol->tag;
    }
    // Otherwise, only pos lit left
    else {
      c->pos_count++;
      c->pos_lits = realloc(c->pos_lits, sizeof(*c->pos_lits) * c->pos_count);
      c->pos_lits[c->pos_count-1] = node->predicate;
      node->predicate = NULL;
    }
  }
}

static void find_variable_names(tommy_array *list, alma_term *term, int id_from_name) {
  switch (term->type) {
    case VARIABLE: {
      for (tommy_size_t i = 0; i < tommy_array_size(list); i++) {
        if (id_from_name) {
          if (strcmp(term->variable->name, tommy_array_get(list, i)) == 0)
            return;
        }
        else {
          if (term->variable->id == *(long long *)tommy_array_get(list, i))
            return;
        }
      }
      if (id_from_name) {
        tommy_array_insert(list, term->variable->name);
      }
      else {
        long long *id = malloc(sizeof(*id));
        *id =  term->variable->id;
        tommy_array_insert(list, id);
      }
      break;
    }
    case CONSTANT: {
      return;
    }
    case FUNCTION: {
      for (int i = 0; i < term->function->term_count; i++) {
        find_variable_names(list, term->function->terms+i, id_from_name);
      }
    }
  }
}

static void set_variable_names(tommy_array *list, alma_term *term, int id_from_name) {
  switch (term->type) {
    case VARIABLE: {
      for (tommy_size_t i = 0; i < tommy_array_size(list); i++) {
        if (id_from_name) {
          if (strcmp(term->variable->name, tommy_array_get(list, i)) == 0)
            term->variable->id = variable_id_count + i;
        }
        else {
          if (term->variable->id == *(long long *)tommy_array_get(list, i))
            term->variable->id = variable_id_count + i;
        }
      }
      break;
    }
    case CONSTANT: {
      return;
    }
    case FUNCTION: {
      for (int i = 0; i < term->function->term_count; i++) {
        set_variable_names(list, term->function->terms+i, id_from_name);
      }
    }
  }
}

// Given a clause, assign the ID fields of each variable
// Two cases:
// 1) If clause is result of resolution, replace existing matching IDs with fresh values each
// 2) Otherwise, give variables with the same name matching IDs
// Fresh ID values drawn from variable_id_count global variable
static void set_variable_ids(clause *c, int id_from_name, binding_list *bs_bindings) {
  tommy_array vars;
  tommy_array_init(&vars);
  for (int i = 0; i < c->pos_count; i++) {
    for (int j = 0; j < c->pos_lits[i]->term_count; j++) {
      find_variable_names(&vars, c->pos_lits[i]->terms+j, id_from_name);
    }
  }
  for (int i = 0; i < c->neg_count; i++) {
    for (int j = 0; j < c->neg_lits[i]->term_count; j++) {
      find_variable_names(&vars, c->neg_lits[i]->terms+j, id_from_name);
    }
  }
  for (int i = 0; i < c->pos_count; i++) {
    for (int j = 0; j < c->pos_lits[i]->term_count; j++) {
      set_variable_names(&vars, c->pos_lits[i]->terms+j, id_from_name);
    }
  }
  for (int i = 0; i < c->neg_count; i++) {
    for (int j = 0; j < c->neg_lits[i]->term_count; j++) {
      set_variable_names(&vars, c->neg_lits[i]->terms+j, id_from_name);
    }
  }

  // If bindings for a backsearch have been passed in, update variable names for them as well
  if (bs_bindings) {
    for (int i = 0; i < bs_bindings->num_bindings; i++) {
      set_variable_names(&vars, bs_bindings->list[i].term, id_from_name);
    }
  }

  variable_id_count += tommy_array_size(&vars);
  if (!id_from_name) {
    for (int i = 0; i < tommy_array_size(&vars); i++)
      free(tommy_array_get(&vars, i));
  }
  tommy_array_done(&vars);
}

static void init_ordering_rec(fif_info *info, alma_node *node, int *next, int *pos, int *neg) {
  if (node->type == FOL) {
    // Neg lit case for NOT
    if (node->fol->op == NOT) {
      info->ordering[(*next)++] = (*neg)--;
    }
    // Case of node is OR
    else {
      init_ordering_rec(info, node->fol->arg1, next, pos, neg);
      init_ordering_rec(info, node->fol->arg2, next, pos, neg);
    }
  }
  // Otherwise, pos lit
  else {
    info->ordering[(*next)++] = (*pos)++;
  }
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

// Flattens a single alma node and adds its contents to collection
// Recursively calls when an AND is found to separate conjunctions
void flatten_node(alma_node *node, tommy_array *clauses, int print) {
  if (node->type == FOL && node->fol->op == AND) {
    if (node->fol->arg1->type == FOL)
      node->fol->arg1->fol->tag = node->fol->tag;
    flatten_node(node->fol->arg1, clauses, print);
    if (node->fol->arg2->type == FOL)
      node->fol->arg2->fol->tag = node->fol->tag;
    flatten_node(node->fol->arg2, clauses, print);
  }
  else {
    clause *c = malloc(sizeof(*c));
    c->pos_count = c->neg_count = 0;
    c->pos_lits = c->neg_lits = NULL;
    c->parent_set_count = c->children_count = 0;
    c->parents = NULL;
    c->children = NULL;
    c->tag = NONE;
    c->fif = NULL;
    make_clause(node, c);
    set_variable_ids(c, 1, NULL);

    // If clause is fif, initialize additional info
    if (c->tag == FIF) {
      c->fif = malloc(sizeof(*c->fif));
      c->fif->premise_count = c->pos_count + c->neg_count - 1;
      c->fif->ordering = malloc(sizeof(*c->fif->ordering) * c->fif->premise_count);
      init_ordering(c->fif, node);
      c->fif->conclusion = c->pos_lits[c->pos_count-1]; // Conclusion will always be last pos_lit
    }

    // Non-fif clauses can be sorted be literal for ease of resolution
    // Fif clauses must retain original literal order to not affect their evaluation
    if (c->tag != FIF) {
      qsort(c->pos_lits, c->pos_count, sizeof(*c->pos_lits), function_compare);
      qsort(c->neg_lits, c->neg_count, sizeof(*c->neg_lits), function_compare);
    }

    if (print) {
      clause_print(c);
      printf(" added\n");
    }
    tommy_array_insert(clauses, c);
  }
}

void free_clause(clause *c) {
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
    free(c->fif);
  }

  free(c);
}

// Flattens trees into set of clauses (tommy_array must be initialized prior)
// trees argument freed here
void nodes_to_clauses(alma_node *trees, int num_trees, tommy_array *clauses, int print) {
  for (int i = 0; i < num_trees; i++) {
    flatten_node(trees+i, clauses, print);
    free_alma_tree(trees+i);
  }
  free(trees);
}

// Reorders clause array to begin with fifs; for correctness of task generation
void fif_to_front(tommy_array *clauses) {
  tommy_size_t loc = 0;
  for (tommy_size_t i = 1; i < tommy_array_size(clauses); i++) {
    clause *c = tommy_array_get(clauses, i);
    if (c->tag == FIF) {
      clause *tmp = tommy_array_get(clauses, loc);
      tommy_array_set(clauses, loc, c);
      tommy_array_set(clauses, i, tmp);
      loc++;
    }
  }
}

void free_predname_mapping(void *arg) {
  predname_mapping *entry = arg;
  free(entry->predname);
  free(entry->clauses);
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in kb_halt
  free(entry);
}

void free_fif_mapping(void *arg) {
  fif_mapping *entry = arg;
  free(entry->conclude_name);
  free(entry->clauses);
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in kb_halt
  free(entry);
}

static void free_fif_task(fif_task *task) {
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in kb_halt
  cleanup_bindings(task->bindings);
  free(task->unified_clauses);
  free(task);
}

void free_fif_task_mapping(void *arg) {
  fif_task_mapping *entry = arg;
  free(entry->predname);
  tommy_node *curr = tommy_list_head(&entry->tasks);
  while (curr) {
    fif_task *data = curr->data;
    curr = curr->next;
    free_fif_task(data);
  }
  free(entry);
}


// Accesses the literal in position i of fif clause c
alma_function* fif_access(clause *c, int i) {
  int next = c->fif->ordering[i];
  if (next < 0)
    return c->neg_lits[-next - 1];
  else
    return c->pos_lits[next];
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

// Compare function to be used by tommy_hashlin_search for fif_mapping
// compares string arg to conclude_name of fif_mapping
static int fifm_compare(const void *arg, const void*obj) {
  return strcmp((const char*)arg, ((const fif_mapping*)obj)->conclude_name);
}

// Compare function to be used by tommy_hashlin_search for fif_task_mapping
// Compares string arg to predname of fif_task_mapping
static int fif_taskm_compare(const void *arg, const void *obj) {
  return strcmp((const char*)arg, ((const fif_task_mapping*)obj)->predname);
}

// Compare function to be used by tommy_hashlin_search for binding_mapping
// Compares long arg to key of binding_mapping
int bm_compare(const void *arg, const void *obj) {
  return *(const long*)arg - ((const binding_mapping*)obj)->key;
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


typedef struct var_matching {
  int count;
  long long *x;
  long long *y;
} var_matching;

// Returns 0 if functions are equal while respecting x and y matchings based on matches arg; otherwise returns 1
// (Further detail in clauses_differ)
static int functions_differ(alma_function *x, alma_function *y, var_matching *matches) {
  if (x->term_count == y->term_count && strcmp(x->name, y->name) == 0) {
    for (int i = 0; i < x->term_count; i++) {
      if (x->terms[i].type == y->terms[i].type) {
          switch(x->terms[i].type) {
            case VARIABLE: {
              long long xval = x->terms[i].variable->id;
              long long yval = y->terms[i].variable->id;
              // Look for matching variable in var_matching's x and y
              for (int j = 0; j < matches->count; j++) {
                // If only one of xval and yval matches, return unequal
                if ((xval == matches->x[j] && yval != matches->y[j]) || (yval == matches->y[j] && xval != matches->x[j]))
                  return 1;
              }
              // No match was found, add a new one to the matching
              matches->count++;
              matches->x = realloc(matches->x, sizeof(*matches->x) * matches->count);
              matches->x[matches->count -1] = xval;
              matches->y = realloc(matches->y, sizeof(*matches->y) * matches->count);
              matches->y[matches->count -1] = yval;
              break;
            }
            case CONSTANT: {
              if (strcmp(x->terms[i].constant->name, y->terms[i].constant->name) != 0)
                return 1;
              break;
            }
            case FUNCTION: {
              if (functions_differ(x->terms[i].function, y->terms[i].function, matches))
                return 1;
            }
          }
      }
      else
        return 1;
    }
    // All arguments compared as equal; return 0
    return 0;
  }
  return 1;
}

// Function to call when short-circuiting clauses_differ to properly free var_matching instance
static int release_matches(var_matching *matches, int retval) {
  if (matches->x != NULL)
    free(matches->x);
  if (matches->y != NULL)
    free(matches->y);
  return retval;
}

// Returns 0 if clauses have equal positive and negative literal sets; otherwise returns 1
// Equality of variables is that there is a one-to-one correspondence in the sets of variables x and y use,
// based on each location where a variable maps to another
// Thus a(X, X) and a(X, Y) are here considered different
// Naturally, literal ordering has no effect on clauses differing
// Clauses x and y must have no duplicate literals (ensured by earlier call to TODO on the clauses)
static int clauses_differ(clause *x, clause *y) {
  if (x->pos_count == y->pos_count && x->neg_count == y->neg_count){
    var_matching matches;
    matches.count = 0;
    matches.x = NULL;
    matches.y = NULL;
    if (x->tag == FIF) {
      for (int i = 0; i < x->fif->premise_count; i++) {
        alma_function *xf = fif_access(x, i);
        alma_function *yf = fif_access(y, i);
        if (function_compare(&xf, &yf) || functions_differ(xf, yf, &matches))
          return release_matches(&matches, 1);
      }
    }
    else {
      for (int i = 0; i < x->pos_count; i++) {
        // TODO: account for case in which may have several literals with name
        // Ignoring duplicate literal case, sorted literal lists allows comparing ith literals of each clause
        if (function_compare(x->pos_lits+i, y->pos_lits+i) || functions_differ(x->pos_lits[i], y->pos_lits[i], &matches))
          return release_matches(&matches, 1);
      }
      for (int i = 0; i < x->neg_count; i++) {
        // TODO: account for case in which may have several literals with name
        // Ignoring duplicate literal case, sorted literal lists allows comparing ith literals of each clause
        if (function_compare(x->neg_lits+i, y->neg_lits+i) || functions_differ(x->neg_lits[i], y->neg_lits[i], &matches))
          return release_matches(&matches, 1);
      }
    }
    // All literals compared as equal; return 0
    return release_matches(&matches, 0);
  }
  return 1;
}

// If c is found to be a clause duplicate, returns a pointer to that clause; null otherwise
// See comments preceding clauses_differ function for further detail
clause* duplicate_check(kb *collection, clause *c) {
  if (c->tag == FIF) {
    char *name = c->fif->conclusion->name;
    fif_mapping *result = tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (result != NULL) {
      for (int i = 0; i < result->num_clauses; i++) {
        if (c->tag == result->clauses[i]->tag && !clauses_differ(c, result->clauses[i]))
          return result->clauses[i];
      }
    }
  }
  else {
    if (c->pos_count > 0) {
      // If clause has a positive literal, all duplicate candidates must have that same positive literal
      // Arbitrarily pick first positive literal as one to use; may be able to do smarter literal choice later
      char *name = name_with_arity(c->pos_lits[0]->name, c->pos_lits[0]->term_count);
      predname_mapping *result = tommy_hashlin_search(&collection->pos_map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
      free(name);
      if (result != NULL) {
        for (int i = 0; i < result->num_clauses; i++) {
          if (c->tag == result->clauses[i]->tag && !clauses_differ(c, result->clauses[i]))
            return result->clauses[i];
        }
      }
    }
    else if (c->neg_count > 0) {
      // If clause has a negative literal, all duplicate candidates must have that same negative literal
      // Arbitrarily pick first negative literal as one to use; may be able to do smarter literal choice later
      char *name = name_with_arity(c->neg_lits[0]->name, c->neg_lits[0]->term_count);
      predname_mapping *result = tommy_hashlin_search(&collection->neg_map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
      free(name);
      if (result != NULL) {
        for (int i = 0; i < result->num_clauses; i++) {
          if (c->tag == result->clauses[i]->tag && !clauses_differ(c, result->clauses[i]))
            return result->clauses[i];
        }
      }
    }
  }
  return NULL;
}

// Given a new clause, add to the KB and maps
void add_clause(kb *collection, clause *c) {
  // TODO: call to duplicate literal filtering (add function and such)

  // Add clause to overall clause list and index map
  index_mapping *ientry = malloc(sizeof(*ientry));
  c->index = ientry->key = next_index++;
  ientry->value = c;
  c->learned = collection->time;
  tommy_list_insert_tail(&collection->clauses, &ientry->list_node, ientry);
  tommy_hashlin_insert(&collection->index_map, &ientry->hash_node, ientry, tommy_hash_u64(0, &ientry->key, sizeof(ientry->key)));

  if (c->tag == FIF) {
    char *name = c->fif->conclusion->name;
    // Index into fif hashmap
    fif_mapping *result = tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (result != NULL) {
      result->num_clauses++;
      result->clauses = realloc(result->clauses, sizeof(*result->clauses)*result->num_clauses);
      result->clauses[result->num_clauses-1] = c;
    }
    else {
      fif_mapping *entry = malloc(sizeof(*entry));
      entry->conclude_name = malloc(strlen(name)+1);
      strcpy(entry->conclude_name, name);
      entry->num_clauses = 1;
      entry->clauses = malloc(sizeof(*entry->clauses));
      entry->clauses[0] = c;
      tommy_hashlin_insert(&collection->fif_map, &entry->node, entry, tommy_hash_u64(0, entry->conclude_name, strlen(entry->conclude_name)));
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

// Given a clause already existing in KB, remove from data structures
// Note that fif removal, or removal of a singleton clause used in a fif rule, may be very expensive
void remove_clause(kb *collection, clause *c) {
  if (c->tag == FIF) {
    // fif must be removed from fif_map and all fif tasks using it deleted
    char *name = c->fif->conclusion->name;
    fif_mapping *fifm = tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (fifm != NULL) {
      tommy_hashlin_remove_existing(&collection->fif_map, &fifm->node);
      free_fif_mapping(fifm);
    }

    for (int i = 0; i < c->fif->premise_count; i++) {
      alma_function *f = fif_access(c, i);
      name = name_with_arity(f->name, f->term_count);
      fif_task_mapping *tm = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, name, tommy_hash_u64(0, name, strlen(name)));
      if (tm != NULL) {
        tommy_node *curr = tommy_list_head(&tm->tasks);
        while (curr) {
          fif_task *currdata = curr->data;
          curr = curr->next;
          if (currdata->fif == c) {
            tommy_list_remove_existing(&tm->tasks, &currdata->node);
            free_fif_task(currdata);
          }
        }
      }
      free(name);
    }
  }
  else {
    // non-fif must be unindexed from pos/neg maps, have resolution tasks and fif tasks (if in any) removed
    for (int i = 0; i < c->pos_count; i++)
      map_remove_clause(&collection->pos_map, &collection->pos_list, c->pos_lits[i], c);
    for (int i = 0; i < c->neg_count; i++)
      map_remove_clause(&collection->neg_map, &collection->neg_list, c->neg_lits[i], c);

    remove_res_tasks(collection, c);

    // May be used in fif tasks if it's a singleton clause
    if (c->pos_count + c->neg_count == 1) {
      int compare_pos = c->neg_count == 1;
      char *cname = compare_pos ? c->neg_lits[0]->name : c->pos_lits[0]->name;
      int cterms = compare_pos ? c->neg_lits[0]->term_count : c->pos_lits[0]->term_count;
      int count = 1;
      char **names = malloc(sizeof(*names));
      int prev_count = 0;
      char **prev_names = NULL;
      // Always process mapping for singleton's clause
      names[0] = compare_pos ? name_with_arity(c->neg_lits[0]->name, c->neg_lits[0]->term_count) : name_with_arity(c->pos_lits[0]->name, c->pos_lits[0]->term_count);

      // Check if fif tasks exist singleton
      if (tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, names[0], tommy_hash_u64(0, names[0], strlen(names[0]))) != NULL) {
        // Check all fif clauses for containing premise matching c
        tommy_size_t bucket_max = collection->fif_map.low_max + collection->fif_map.split;
        for (tommy_size_t pos = 0; pos < bucket_max; ++pos) {
          tommy_hashlin_node *node = *tommy_hashlin_pos(&collection->fif_map, pos);

          while (node) {
            fif_mapping *data = node->data;
            node = node->next;

            for (int i = 0; i < data->num_clauses; i++) {
              for (int j = 0; j < data->clauses[i]->fif->premise_count; j++) {
                alma_function *premise = fif_access(data->clauses[i], j);

                // For each match, collect premises after singleton's location
                if (strcmp(premise->name, cname) == 0 && premise->term_count == cterms) {
                  if (j > 0) {
                    prev_count++;
                    prev_names = realloc(prev_names, sizeof(*prev_names) * prev_count);
                    alma_function *pf = fif_access(data->clauses[i], j-1);
                    prev_names[prev_count-1] = name_with_arity(pf->name, pf->term_count);
                  }
                  names = realloc(names, sizeof(*names) * (count + data->clauses[i]->fif->premise_count - j- 1));
                  for (int k = j+1; k < data->clauses[i]->fif->premise_count; k++) {
                    premise = fif_access(data->clauses[i], k);
                    names[count] = name_with_arity(premise->name, premise->term_count);
                    count++;
                  }
                  break;
                }
              }
            }
          }
        }

        // fif_task_mappings have tasks deleted if they have unified with c
        for (int i = 0; i < count; i++) {
          fif_task_mapping *tm = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, names[i], tommy_hash_u64(0, names[i], strlen(names[i])));
          if (tm != NULL) {
            tommy_node *curr = tommy_list_head(&tm->tasks);
            while (curr) {
              fif_task *currdata = curr->data;
              curr = curr->next;

              if (currdata->to_unify == c) {
                currdata->to_unify = NULL;
              }
            }
          }

          free(names[i]);
        }
        free(names);

        for (int i = 0; i < prev_count; i++) {
          fif_task_mapping *tm = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, prev_names[i], tommy_hash_u64(0, prev_names[i], strlen(prev_names[i])));
          if (tm != NULL) {
            tommy_node *curr = tommy_list_head(&tm->tasks);
            while (curr) {
              fif_task *currdata = curr->data;
              curr = curr->next;

              for (int j = 0; j < currdata->num_unified; j++) {
                if (currdata->unified_clauses[j] == c->index) {
                  tommy_list_remove_existing(&tm->tasks, &currdata->node);
                  free_fif_task(currdata);
                }
              }
            }
          }

          free(prev_names[i]);
        }
      free(prev_names);
      }
      else {
        // Doesn't appear in fif tasks, no need to check
        free(names[0]);
        free(names);
      }
    }
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
            }
            break;
          }
        }
      }

      if (new_count > 0) {
        int loc = child->parent_set_count-1;
        // Replace empty parent sets with clauses from end of parents
        for (int j = child->parent_set_count-2; j >= 0; j--) {
          if (child->parents[j].clauses == NULL) {
            child->parents[j] = child->parents[loc];
            loc--;
          }
        }
        child->parents = realloc(child->parents, sizeof(*child->parents)*new_count);
      }
      else {
        free(child->parents);
        child->parents = NULL;
      }
    }
  }

  // Remove clause from the children list of each of its parents
  for (int i = 0; i < c->parent_set_count; i++)
    for (int j = 0; j < c->parents[i].count; j++)
      remove_child(c->parents[i].clauses[j], c);

  free_clause(c);
}


// Given a new fif clause, initializes fif task mappings held by fif_tasks for each premise of c
// Also places single fif_task into fif_task_mapping for first premise
void fif_task_map_init(kb *collection, clause *c) {
  if (c->tag == FIF) {
    for (int i = 0; i < c->fif->premise_count; i++) {
      alma_function *f = fif_access(c, i);
      char *name = name_with_arity(f->name, f->term_count);
      fif_task_mapping *result = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, name, tommy_hash_u64(0, name, strlen(name)));

      // If a task map entry doesn't exist for name of a literal, create one
      if (result == NULL) {
        result = malloc(sizeof(*result));
        result->predname = name;
        tommy_list_init(&result->tasks);
        tommy_hashlin_insert(&collection->fif_tasks, &result->node, result, tommy_hash_u64(0, result->predname, strlen(result->predname)));
      }
      else
        free(name);

      // For first premise, initialze fif_task root for c
      if (i == 0) {
        fif_task *task = malloc(sizeof(*task));
        task->fif = c;
        task->bindings = malloc(sizeof(*task->bindings));
        task->bindings->list = NULL;
        task->bindings->num_bindings = 0;
        task->premises_done = 0;
        task->num_unified = 0;
        task->unified_clauses = NULL;
        task->to_unify = NULL;
        task->proc_next = proc_valid(fif_access(c, 0));
        tommy_list_insert_tail(&result->tasks, &task->node, task);
      }
    }
  }
}

// Given a non-fif singleton clause, sets to_unify pointers for fif tasks that have next clause to process matching it
// If fif tasks have to_unify assigned; branches off new tasks as copies
void fif_tasks_from_clause(kb *collection, clause *c) {
  if (c->fif == NONE && c->pos_count + c->neg_count == 1) {
    char *name;
    int pos = 1;
    if (c->pos_count == 1)
      name = name_with_arity(c->pos_lits[0]->name, c->pos_lits[0]->term_count);
    else {
      name = name_with_arity(c->neg_lits[0]->name, c->neg_lits[0]->term_count);
      pos = 0;
    }

    fif_task_mapping *result = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    free(name);
    if (result != NULL) {
      int len = tommy_list_count(&result->tasks);
      tommy_node *curr = tommy_list_head(&result->tasks);

      // Process original list contents and deal with to_unify
      int set_first = 0;
      int i = 0;
      while (curr && i < len) {
        fif_task *data = curr->data;

        // Only modify to_unify if next for fif isn't a procedure predicate
        if (!data->proc_next) {
          int sign_match = (pos && data->fif->fif->ordering[data->num_unified] < 0) || (!pos && data->fif->fif->ordering[data->num_unified] >= 0);
          if (sign_match && !(data->num_unified == 0 && set_first)) {
            if (data->to_unify == NULL)
              data->to_unify = c;
            else {
              // If to_unify is already non-null, duplicate the task and assign to_unify of duplicate
              fif_task *copy = malloc(sizeof(*copy));
              copy->fif = data->fif;
              copy->bindings = malloc(sizeof(*copy->bindings));
              copy_bindings(copy->bindings, data->bindings);
              copy->premises_done = data->premises_done;
              copy->num_unified = data->num_unified;
              copy->unified_clauses = malloc(sizeof(*data->unified_clauses) * data->num_unified);
              memcpy(copy->unified_clauses, data->unified_clauses, sizeof(*data->unified_clauses) * data->num_unified);
              copy->to_unify = c;
              copy->proc_next = data->proc_next;
              tommy_list_insert_tail(&result->tasks, &copy->node, copy);
            }
            // Special case -- c should only -once- be put into to_unify of a task with none unified
            set_first = 1;
          }
        }

        curr = curr->next;
        i++;
      }
    }
  }
}

// Compare function to be used by tommy_hashlin_search for distrust_mapping
// Compares long arg to key of distrust_mapping
static int dm_compare(const void *arg, const void *obj) {
  return *(const long*)arg - ((const distrust_mapping*)obj)->key;
}

int is_distrusted(kb *collection, long index) {
  return tommy_hashlin_search(&collection->distrusted, dm_compare, &index, tommy_hash_u64(0, &index, sizeof(index))) != NULL;
}

// Checks if each of task's unified_clauses is distrusted
// Returns 1 if any are
static int fif_unified_distrusted(kb *collection, fif_task *task) {
  for (int i = 0; i < task->num_unified; i++)
    if (is_distrusted(collection, task->unified_clauses[i]))
      return 1;
  return 0;
}

// Called when each premise of a fif is satisfied
static clause* fif_conclude(kb *collection, fif_task *task, binding_list *bindings) {
  clause *conclusion = malloc(sizeof(*conclusion));
  conclusion->pos_count = 1;
  conclusion->neg_count = 0;
  conclusion->pos_lits = malloc(sizeof(*conclusion->pos_lits));
  conclusion->neg_lits = NULL;

  // Using task's overall bindings, obtain proper conclusion predicate
  alma_function *conc_func = malloc(sizeof(*conc_func));
  copy_alma_function(task->fif->fif->conclusion, conc_func);
  for (int k = 0; k < conc_func->term_count; k++)
    subst(bindings, conc_func->terms+k);
  cleanup_bindings(bindings);
  conclusion->pos_lits[0] = conc_func;

  conclusion->parent_set_count = 1;
  conclusion->parents = malloc(sizeof(*conclusion->parents));
  conclusion->parents[0].count = task->num_unified + 1;
  conclusion->parents[0].clauses = malloc(sizeof(*conclusion->parents[0].clauses) * conclusion->parents[0].count);
  for (int i = 0; i < conclusion->parents[0].count-1; i++) {
    long index = task->unified_clauses[i];
    index_mapping *result = tommy_hashlin_search(&collection->index_map, im_compare, &index, tommy_hash_u64(0, &index, sizeof(index)));
    conclusion->parents[0].clauses[i] = result->value;
  }
  conclusion->parents[0].clauses[conclusion->parents[0].count-1] = task->fif;
  conclusion->children_count = 0;
  conclusion->children = NULL;
  conclusion->tag = NONE;
  conclusion->fif = NULL;

  set_variable_ids(conclusion, 1, NULL);

  return conclusion;
}

// Continues attempting to unify from tasks set
// Task instances for which cannot be further progressed with current KB are added to stopped
// Completed tasks result in additions to new_clauses
static void fif_task_unify_loop(kb *collection, tommy_list *tasks, tommy_list *stopped, tommy_array *new_clauses) {
  tommy_node *curr = tommy_list_head(tasks);
  int len = tommy_list_count(tasks);
  // Process original list contents
  while (curr && len > 0) {
    fif_task *next_task = curr->data;
    curr = curr->next;
    len--;
    // Withdraw current task from set
    tommy_list_remove_existing(tasks, &next_task->node);

    int task_erased = 0;
    alma_function *next_func = fif_access(next_task->fif, next_task->premises_done);
    // Proc case
    if (proc_valid(next_func)) {
      if (proc_bound_check(next_func, next_task->bindings)) {
        binding_list *copy = malloc(sizeof(*copy));
        copy_bindings(copy, next_task->bindings);

        int progressed = proc_run(next_func, copy, collection);
        if (progressed) {
          // If task is now completed, obtain resulting clause and insert to new_clauses
          if (next_task->premises_done + 1 == next_task->fif->fif->premise_count) {
            if (fif_unified_distrusted(collection, next_task)) {
              // If any unified clauses became distrusted, delete task
              free_fif_task(next_task);
              task_erased = 1;
              break;
            }
            else {
              next_task->premises_done++;
              clause *conclusion = fif_conclude(collection, next_task, copy);
              tommy_array_insert(new_clauses, conclusion);
              next_task->premises_done--;
            }
          }
          // Otherwise, to continue processing, create second task for results and place in tasks
          else {
            fif_task *latest = malloc(sizeof(*latest));
            latest->fif = next_task->fif;
            latest->bindings = copy;
            latest->premises_done = next_task->premises_done + 1;
            latest->num_unified = next_task->num_unified;
            latest->unified_clauses = malloc(sizeof(*latest->unified_clauses)*latest->num_unified);
            memcpy(latest->unified_clauses, next_task->unified_clauses, sizeof(*next_task->unified_clauses) * next_task->num_unified);
            latest->to_unify = NULL;
            latest->proc_next = proc_valid(fif_access(latest->fif, latest->premises_done));
            tommy_list_insert_tail(tasks, &latest->node, latest);
          }
        }
        else {
          cleanup_bindings(copy);
        }
      }

    }
    // Unify case
    else {
      char *name = name_with_arity(next_func->name, next_func->term_count);

      // Important: because of conversion to CNF, a negative in ordering means fif premise is positive
      int search_pos = next_task->fif->fif->ordering[next_task->premises_done] < 0;
      predname_mapping *m = tommy_hashlin_search(search_pos ? &collection->pos_map : &collection->neg_map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
      free(name);

      if (m != NULL) {
        for (int j = 0; j < m->num_clauses; j++) {
          clause *jth = m->clauses[j];
          if (jth->pos_count + jth->neg_count == 1) {
            alma_function *to_unify = search_pos ? jth->pos_lits[0] : jth->neg_lits[0];

            binding_list *copy = malloc(sizeof(*copy));
            copy_bindings(copy, next_task->bindings);
            if (pred_unify(next_func, to_unify, copy)) {
              // If task is now completed, obtain resulting clause and insert to new_clauses
              if (next_task->premises_done + 1 == next_task->fif->fif->premise_count) {
                if (fif_unified_distrusted(collection, next_task)) {
                  // If any unified clauses became distrusted, delete task
                  free_fif_task(next_task);
                  task_erased = 1;
                  break;
                }
                else {
                  next_task->num_unified++;
                  next_task->premises_done++;
                  next_task->unified_clauses = realloc(next_task->unified_clauses, sizeof(*next_task->unified_clauses)*next_task->num_unified);
                  next_task->unified_clauses[next_task->num_unified-1] = jth->index;
                  clause *conclusion = fif_conclude(collection, next_task, copy);
                  tommy_array_insert(new_clauses, conclusion);
                  next_task->num_unified--;
                  next_task->premises_done--;
                }
              }
              // Otherwise, to continue processing, create second task for results and place in tasks
              else {
                fif_task *latest = malloc(sizeof(*latest));
                latest->fif = next_task->fif;
                latest->bindings = copy;
                latest->premises_done = next_task->premises_done + 1;
                latest->num_unified = next_task->num_unified + 1;
                latest->unified_clauses = malloc(sizeof(*latest->unified_clauses)*latest->num_unified);
                memcpy(latest->unified_clauses, next_task->unified_clauses, sizeof(*next_task->unified_clauses) * next_task->num_unified);
                latest->unified_clauses[latest->num_unified-1] = jth->index;
                latest->to_unify = NULL;
                latest->proc_next = proc_valid(fif_access(latest->fif, latest->premises_done));
                tommy_list_insert_tail(tasks, &latest->node, latest);
              }

            }
            else {
              // Unfication failure
              cleanup_bindings(copy);
            }
          }
        }
      }
    }

    if (!task_erased)
      tommy_list_insert_tail(stopped, &next_task->node, next_task);
  }

  // If tasks list is nonempty, have newly unified tasks to recurse on
  if (!tommy_list_empty(tasks))
    fif_task_unify_loop(collection, tasks, stopped, new_clauses);
}

// Process fif tasks
// For each task in fif_task_mapping:
// - Attempt to progress with either proc predicate or unification on to_unify
// - If progression succeeds, progresses further on next literals as able
// Above process repeats per task until unification fails or fif task is fully satisfied (and then asserts conclusion)
static void process_fif_task_mapping(kb *collection, fif_task_mapping *entry, tommy_list *progressed, tommy_array *new_clauses) {
  tommy_node *curr = tommy_list_head(&entry->tasks);
  while (curr) {
    fif_task *f = curr->data;
    curr = curr->next;

    if (f->to_unify != NULL || f->proc_next) {
      int premise_progress = 0;
      binding_list *copy = malloc(sizeof(*copy));
      copy_bindings(copy, f->bindings);

      if (f->to_unify != NULL) {
        // Clause to_unify must not have become distrusted since it was connected to the fif task
        if (!is_distrusted(collection, f->to_unify->index)) {
          alma_function *fif_func = fif_access(f->fif, f->premises_done);
          alma_function *to_unify_func = (f->to_unify->pos_count > 0) ? f->to_unify->pos_lits[0] : f->to_unify->neg_lits[0];

          premise_progress = pred_unify(fif_func, to_unify_func, f->bindings);

          if (!premise_progress) {
            f->to_unify = NULL;
            // Reset from copy
            cleanup_bindings(f->bindings);
            f->bindings = copy;
            copy = NULL;
          }
        }
        else {
          f->to_unify = NULL;
        }
      }
      else {
        alma_function *proc = fif_access(f->fif, f->premises_done);
        if (proc_bound_check(proc, f->bindings)) {
          premise_progress = proc_run(proc, f->bindings, collection);
        }
      }

      if (premise_progress) {
        // If task is now completed, obtain resulting clause and insert to new_clauses
        if (f->premises_done + 1 == f->fif->fif->premise_count) {
          if (fif_unified_distrusted(collection, f)) {
            // If any unified clauses became distrusted, delete task
            tommy_list_remove_existing(&entry->tasks, &f->node);
            free_fif_task(f);
          }
          else {
            f->premises_done++;
            if (f->to_unify != NULL) {
              f->num_unified++;
              f->unified_clauses = realloc(f->unified_clauses, sizeof(*f->unified_clauses)*f->num_unified);
              f->unified_clauses[f->num_unified-1] = f->to_unify->index;
            }

            clause *conclusion = fif_conclude(collection, f, f->bindings);
            tommy_array_insert(new_clauses, conclusion);
            // Reset from copy
            f->bindings = copy;

            f->premises_done--;
            if (f->to_unify != NULL) {
              f->to_unify = NULL;
              f->proc_next = proc_valid(fif_access(f->fif, f->premises_done));
              f->num_unified--;
            }
          }
        }
        // Otherwise, adjust task and do unify loop call
        else {
          f->premises_done++;
          if (f->to_unify != NULL) {
            f->num_unified++;
            f->unified_clauses = realloc(f->unified_clauses, sizeof(*f->unified_clauses)*f->num_unified);
            f->unified_clauses[f->num_unified-1] = f->to_unify->index;
            f->to_unify = NULL;
            f->proc_next = proc_valid(fif_access(f->fif, f->premises_done));
          }
          cleanup_bindings(copy);

          tommy_list_remove_existing(&entry->tasks, &f->node);
          tommy_list to_progress;
          tommy_list_init(&to_progress);
          tommy_list_insert_tail(&to_progress, &f->node, f);

          // Unification succeeded; progress further on task as able in call
          fif_task_unify_loop(collection, &to_progress, progressed, new_clauses);
        }
      }
      else {
        cleanup_bindings(copy);
      }
    }
  }
}

// Process fif tasks and place results in new_clauses
// See helper process_fif_task_mapping for comment
void process_fif_tasks(kb *collection) {
  tommy_list progressed;
  tommy_list_init(&progressed);

  // Loop over hashlin contents of collection's fif tasks; based on tommy_hashlin_foreach
  // TODO: may want double indexing if this loops too many times
  tommy_size_t bucket_max = collection->fif_tasks.low_max + collection->fif_tasks.split;
	for (tommy_size_t pos = 0; pos < bucket_max; ++pos) {
		tommy_hashlin_node *node = *tommy_hashlin_pos(&collection->fif_tasks, pos);

		while (node) {
			fif_task_mapping *data = node->data;
			node = node->next;
			process_fif_task_mapping(collection, data, &progressed, &collection->new_clauses);
		}
  }

  // Incorporates tasks from progressed list back into task mapping
  tommy_node *curr = tommy_list_head(&progressed);
  while (curr) {
    fif_task *data = curr->data;
    curr = curr->next;

    alma_function *f = fif_access(data->fif, data->premises_done);
    char *name = name_with_arity(f->name, f->term_count);
    fif_task_mapping *result = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    tommy_list_insert_tail(&result->tasks, &data->node, data);
    free(name);
  }
}

void make_single_task(kb *collection, clause *c, alma_function *c_lit, clause *other, tommy_array *tasks, int use_bif, int pos) {
  if (c != other && (other->tag != BIF || use_bif)) {
    alma_function *other_lit = literal_by_name(other, c_lit->name, pos);
    if (other_lit != NULL) {
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
      if (collection->idling)
        collection->idling = 0;
    }
  }
}

// Helper of res_tasks_from_clause
static void make_res_tasks(kb *collection, clause *c, int count, alma_function **c_lits, tommy_hashlin *map, tommy_array *tasks, int use_bif, int pos) {
  for (int i = 0; i < count; i++) {
    char *name = name_with_arity(c_lits[i]->name, c_lits[i]->term_count);
    predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));

    // New tasks are from Cartesian product of result's clauses with clauses' ith
    if (result != NULL)
      for (int j = 0; j < result->num_clauses; j++)
        make_single_task(collection, c, c_lits[i], result->clauses[j], tasks, use_bif, pos);

    free(name);
  }
}

// Finds new res tasks based on matching pos/neg predicate pairs, where one is from the KB and the other from arg
// Tasks are added into the res_tasks of collection
// Used only for non-bif resolution tasks; hence checks tag of c
void res_tasks_from_clause(kb *collection, clause *c, int process_negatives) {
  if (c->tag != BIF) {
    make_res_tasks(collection, c, c->pos_count, c->pos_lits, &collection->neg_map, &collection->res_tasks, 0, 0);
    // Only done if clauses differ from KB's clauses (i.e. after first task generation)
    if (process_negatives)
      make_res_tasks(collection, c, c->neg_count, c->neg_lits, &collection->pos_map, &collection->res_tasks, 0, 1);
  }
}

// Returns boolean based on success of string parse
// If parses successfully, adds to collection's new_clauses
int assert_formula(kb *collection, char *string, int print) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas)) {
    nodes_to_clauses(formulas, formula_count, &collection->new_clauses, print);
    return 1;
  }
  return 0;
}

int delete_formula(kb *collection, char *string, int print) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas)) {
    // Convert formulas to clause array
    tommy_array clauses;
    tommy_array_init(&clauses);
    for (int i = 0; i < formula_count; i++) {
      flatten_node(formulas+i, &clauses, 0);
      free_alma_tree(formulas+i);
    }
    free(formulas);

    // Process and remove each clause
    for (tommy_size_t i = 0; i < tommy_array_size(&clauses); i++) {
      clause *curr = tommy_array_get(&clauses, i);
      clause *c = duplicate_check(collection, curr);
      if (c != NULL) {
        if (print) {
          clause_print(c);
          printf(" removed\n");
        }
        remove_clause(collection, c);
      }
      free_clause(curr);
    }
    tommy_array_done(&clauses);

    return 1;
  }
  return 0;
}


// Resolve helper
static void lits_copy(int count, alma_function **lits, alma_function **cmp, binding_list *mgu, alma_function **res_lits, int *res_count) {
  for (int i = 0; i < count; i++) {
    // In calls cmp can be assigned to resolvent to not copy it
    if (cmp == NULL || lits[i] != *cmp) {
      alma_function *litcopy = malloc(sizeof(*litcopy));
      copy_alma_function(lits[i], litcopy);
      for (int j = 0; j < litcopy->term_count; j++)
        subst(mgu, litcopy->terms+j);
      res_lits[*res_count] = litcopy;
      (*res_count)++;
    }
  }
}

// Given an MGU, substitute on literals other than pair unified and make a single resulting clause
void resolve(res_task *t, binding_list *mgu, clause *result) {
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
}

char* now(long t) {
  int len = snprintf(NULL, 0, "%ld", t);
  char *str = malloc(4 + len+1 + 2);
  strcpy(str, "now(");
  snprintf(str+4, len+1, "%ld", t);
  strcpy(str+4+len, ").");
  return str;
}

char* long_to_str(long x) {
  int length = snprintf(NULL, 0, "%ld", x);
  char *str = malloc(length+1);
  snprintf(str, length+1, "%ld", x);
  return str;
}

void add_child(clause *parent, clause *child) {
  parent->children_count++;
  parent->children = realloc(parent->children, sizeof(*parent->children) * parent->children_count);
  parent->children[parent->children_count-1] = child;
}

// Transfers first parent of source into parent collection of target if it's not a repeat
void transfer_parent(kb *collection, clause *target, clause *source, int add_children) {
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
      // Also check if parents of source are distrusted
      int distrust = 0;
      for (int j = 0; j < source->parents[0].count; j++) {
        int insert = 1;
        if (is_distrusted(collection, source->parents[0].clauses[j]->index))
          distrust = 1;
        for (int k = 0; k < source->parents[0].clauses[j]->children_count; k++) {
          if (source->parents[0].clauses[j]->children[k] == target) {
            insert = 0;
            break;
          }
        }
        if (insert)
          add_child(source->parents[0].clauses[j], target);
      }
      if (distrust && !is_distrusted(collection, target->index)) {
        char *time_str = long_to_str(collection->time);
        distrust_recursive(collection, target, time_str);
        free(time_str);
      }
    }
  }
}

void distrust_recursive(kb *collection, clause *c, char *time) {
  // Add c to distrusted set
  distrust_mapping *d = malloc(sizeof(*d));
  d->key = c->index;
  d->value = c;
  tommy_hashlin_insert(&collection->distrusted, &d->node, d, tommy_hash_u64(0, &d->key, sizeof(d->key)));

  // Assert distrusted formula
  char *index = long_to_str(c->index);
  char *distrust_str = malloc(strlen(index) + strlen(time) + 15);
  strcpy(distrust_str, "distrusted(");
  int loc = 11;
  strcpy(distrust_str+loc, index);
  loc += strlen(index);
  free(index);
  distrust_str[loc++] = ',';
  strcpy(distrust_str+loc, time);
  loc += strlen(time);
  strcpy(distrust_str+loc, ").");
  assert_formula(collection, distrust_str, 0);
  free(distrust_str);

  // Unindex c from pos_map/list, neg_map/list to prevent use in inference
  for (int i = 0; i < c->pos_count; i++)
    map_remove_clause(&collection->pos_map, &collection->pos_list, c->pos_lits[i], c);
  for (int i = 0; i < c->neg_count; i++)
    map_remove_clause(&collection->neg_map, &collection->neg_list, c->neg_lits[i], c);

  // Recursively distrust children that aren't distrusted already
  if (c->children != NULL) {
    for (int i = 0; i < c->children_count; i++) {
      if (!is_distrusted(collection, c->children[i]->index)) {
        distrust_recursive(collection, c->children[i], time);
      }
    }
  }
}

static void binding_subst(binding_list *target, binding_list *theta) {
  for (int i = 0; i < target->num_bindings; i++)
    subst(theta, target->list[i].term);
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

// Process resolution tasks from argument and place results in new_arr
void process_res_tasks(kb *collection, tommy_array *tasks, tommy_array *new_arr, backsearch_task *bs) {
  for (tommy_size_t i = 0; i < tommy_array_size(tasks); i++) {
    res_task *current_task = tommy_array_get(tasks, i);
    if (current_task != NULL) {
      // Does not do resolution with a distrusted clause
      if (!is_distrusted(collection, current_task->x->index) && !is_distrusted(collection,  current_task->y->index)) {
        binding_list *theta = malloc(sizeof(*theta));
        theta->list = NULL;
        theta->num_bindings = 0;
        // Given a res_task, attempt unification
        if (pred_unify(current_task->pos, current_task->neg, theta)) {
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
              parent_theta->num_bindings = 0;
              parent_theta->list = NULL;
              int unify_fail = 0;
              for (int j = 0; j < x_bindings->num_bindings; j++) {
                if (!unify(x_bindings->list[j].term, y_bindings->list[j].term, parent_theta)) {
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
                for (int j = 0; j < res_result->pos_count; j++)
                  for (int k = 0; k < res_result->pos_lits[j]->term_count; k++)
                    subst(parent_theta, res_result->pos_lits[j]->terms+k);
                for (int j = 0; j < res_result->neg_count; j++)
                  for (int k = 0; k < res_result->neg_lits[j]->term_count; k++)
                    subst(parent_theta, res_result->neg_lits[j]->terms+k);

                cleanup_bindings(parent_theta);
              }
            }
            else if (x_bindings == NULL) {
              // Consolidate to only x_bindings
              x_bindings = y_bindings;
            }
          }

          // An empty resolution result is not a valid clause to add to KB
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

            set_variable_ids(res_result, 0, x_bindings);

            tommy_array_insert(new_arr, res_result);
            if (bs)
              tommy_array_insert(&bs->new_clause_bindings, x_bindings);
          }
          else {
            free(res_result);

            if (bs) {
              clause *answer = malloc(sizeof(*answer));
              memcpy(answer, bs->target, sizeof(*answer));
              if (bs->target->pos_count == 1){
                answer->pos_lits = malloc(sizeof(*answer->pos_lits));
                answer->pos_lits[0] = malloc(sizeof(*answer->pos_lits[0]));
                copy_alma_function(bs->target->pos_lits[0], answer->pos_lits[0]);
                for (int j = 0; j < answer->pos_lits[0]->term_count; j++)
                  subst(x_bindings, answer->pos_lits[0]->terms+j);
              }
              else {
                answer->neg_lits = malloc(sizeof(*answer->neg_lits));
                answer->neg_lits[0] = malloc(sizeof(*answer->neg_lits[0]));
                copy_alma_function(bs->target->neg_lits[0], answer->neg_lits[0]);
                for (int j = 0; j < answer->neg_lits[0]->term_count; j++)
                  subst(x_bindings, answer->neg_lits[0]->terms+j);
              }

              // TODO: parent setup for answer?
              tommy_array_insert(&collection->new_clauses, answer);
              cleanup_bindings(x_bindings);
            }
            else {
              // If not a backward search, empty resolution result indicates a contradiction between clauses

              char *arg1 = long_to_str(current_task->x->index);
              char *arg2 = long_to_str(current_task->y->index);
              char *time_str = long_to_str(collection->time);

              char *contra_str = malloc(strlen(arg1) + strlen(arg2) + strlen(time_str) + 12);
              strcpy(contra_str, "contra(");
              int loc = 7;
              strcpy(contra_str+loc, arg1);
              loc += strlen(arg1);
              free(arg1);
              contra_str[loc++] = ',';
              strcpy(contra_str+loc, arg2);
              loc += strlen(arg2);
              free(arg2);
              contra_str[loc++] = ',';
              strcpy(contra_str+loc, time_str);
              loc += strlen(time_str);
              strcpy(contra_str+loc, ").");

              // Assert contra and distrusted
              assert_formula(collection, contra_str, 0);
              free(contra_str);
              distrust_recursive(collection, current_task->x, time_str);
              distrust_recursive(collection, current_task->y, time_str);
              free(time_str);
            }
          }
        }
        cleanup:
        cleanup_bindings(theta);
      }

      free(current_task);
    }
  }
  tommy_array_done(tasks);
  tommy_array_init(tasks);
}


static void collect_variables(alma_term *term, binding_list *b) {
  switch(term->type) {
    case VARIABLE: {
      b->num_bindings++;
      b->list = realloc(b->list, sizeof(*b->list) * b->num_bindings);
      b->list[b->num_bindings-1].var = malloc(sizeof(alma_variable));
      copy_alma_var(term->variable, b->list[b->num_bindings-1].var);
      b->list[b->num_bindings-1].term = malloc(sizeof(alma_term));
      copy_alma_term(term, b->list[b->num_bindings-1].term);
      break;
    }
    case CONSTANT: {
      return;
    }
    case FUNCTION: {
      for (int i = 0; i < term->function->term_count; i++)
        collect_variables(term->function->terms+i, b);
    }
  }
}

// Initializes backward search with negation of clause argument
void backsearch_from_clause(kb *collection, clause *c) {
  backsearch_task *bt = malloc(sizeof(*bt));
  bt->clause_count = 0;
  c->index = --bt->clause_count;
  bt->target = c;

  clause *negated = malloc(sizeof(*negated));
  memcpy(negated, c, sizeof(*negated));
  alma_function *negated_lit;
  if (c->pos_count == 1){
    negated->neg_count = 1;
    negated->neg_lits = malloc(sizeof(*negated->neg_lits));
    negated_lit = negated->neg_lits[0] = malloc(sizeof(*negated->neg_lits[0]));
    copy_alma_function(c->pos_lits[0], negated->neg_lits[0]);
    negated->pos_count = 0;
    negated->pos_lits = NULL;
  }
  else {
    negated->pos_count = 1;
    negated->pos_lits = malloc(sizeof(*negated->pos_lits));
    negated_lit = negated->pos_lits[0] = malloc(sizeof(*negated->pos_lits[0]));
    copy_alma_function(c->neg_lits[0], negated->pos_lits[0]);
    negated->neg_count = 0;
    negated->neg_lits = NULL;
  }

  tommy_array_init(&bt->clauses);
  tommy_hashlin_init(&bt->clause_bindings);

  tommy_array_init(&bt->new_clauses);
  tommy_array_insert(&bt->new_clauses, negated);
  tommy_array_init(&bt->new_clause_bindings);
  binding_list *negated_bindings = malloc(sizeof(*negated_bindings));
  negated_bindings->list = NULL;
  negated_bindings->num_bindings = 0;
  // Create initial bindings where each variable in negated target is present, but each term is set to NULL
  for (int i = 0; i < negated_lit->term_count; i++)
    collect_variables(negated_lit->terms+i, negated_bindings);
  bt->target_vars = negated_bindings;
  tommy_array_insert(&bt->new_clause_bindings, negated_bindings);

  tommy_array_init(&bt->to_resolve);
  tommy_list_insert_tail(&collection->backsearch_tasks, &bt->node, bt);
}

static clause* backsearch_duplicate_check(kb *collection, backsearch_task *task, clause *c) {
  for (int i = 0; i < tommy_array_size(&task->clauses); i++) {
    clause *compare = tommy_array_get(&task->clauses, i);
    if (compare->tag == c->tag && !clauses_differ(c, compare))
      return compare;
  }
  return NULL;
}

// Given particular task, populate to_resolve based on each clause
void generate_backsearch_tasks(kb *collection, backsearch_task *bt) {
  for (int i = 0; i < tommy_array_size(&bt->new_clauses); i++) {
    clause *c = tommy_array_get(&bt->new_clauses, i);
    clause *dupe = duplicate_check(collection, c);
    int bs_dupe = 0;
    if (dupe == NULL) {
      dupe = backsearch_duplicate_check(collection, bt, c);
      bs_dupe = 1;
    }

    if (dupe == NULL) {
      c->index = --bt->clause_count;

      // Obtain tasks between new backward search clauses and KB items
      make_res_tasks(collection, c, c->pos_count, c->pos_lits, &collection->neg_map, &bt->to_resolve, 1, 0);
      make_res_tasks(collection, c, c->neg_count, c->neg_lits, &collection->pos_map, &bt->to_resolve, 1, 1);

      // Obtain tasks between pairs of backsearch clauses
      // Currently a linear scan, perhaps to adjust later
      for (int j = 0; j < tommy_array_size(&bt->clauses); j++) {
        clause *jth = tommy_array_get(&bt->clauses, j);

        for (int k = 0; k < c->pos_count; k++)
          make_single_task(collection, c, c->pos_lits[k], jth, &bt->to_resolve, 1, 0);
        for (int k = 0; k < c->neg_count; k++)
          make_single_task(collection, c, c->neg_lits[k], jth, &bt->to_resolve, 1, 1);
      }

      tommy_array_insert(&bt->clauses, c);

      // Insert corresponding binding into hashmap
      binding_mapping *entry = malloc(sizeof(*entry));
      entry->key = c->index;
      entry->bindings = tommy_array_get(&bt->new_clause_bindings, i);
      tommy_hashlin_insert(&bt->clause_bindings, &entry->node, entry, tommy_hash_u64(0, &entry->key, sizeof(entry->key)));
    }
    else {
      if (bs_dupe)
        transfer_parent(collection, dupe, c, 0);
      cleanup_bindings(tommy_array_get(&bt->new_clause_bindings, i));
      free_clause(c);
    }
  }
  tommy_array_done(&bt->new_clauses);
  tommy_array_init(&bt->new_clauses);
  tommy_array_done(&bt->new_clause_bindings);
  tommy_array_init(&bt->new_clause_bindings);
}

// Advances by resolving available tasks
void process_backsearch_tasks(kb *collection) {
  tommy_node *curr = tommy_list_head(&collection->backsearch_tasks);
  while (curr) {
    backsearch_task *t = curr->data;
    process_res_tasks(collection, &t->to_resolve, &t->new_clauses, t);
    curr = curr->next;
  }
}

static void free_binding_mapping(void *arg) {
  binding_mapping *bm = arg;
  cleanup_bindings(bm->bindings);
  free(bm);
}

void backsearch_halt(backsearch_task *t) {
  free_clause(t->target);

  for (tommy_size_t i = 0; i < tommy_array_size(&t->clauses); i++)
    free_clause(tommy_array_get(&t->clauses, i));
  tommy_array_done(&t->clauses);
  tommy_hashlin_foreach(&t->clause_bindings, free_binding_mapping);
  tommy_hashlin_done(&t->clause_bindings);

  for (tommy_size_t i = 0; i < tommy_array_size(&t->new_clauses); i++)
    free_clause(tommy_array_get(&t->new_clauses, i));
  tommy_array_done(&t->new_clauses);
  for (tommy_size_t i = 0; i < tommy_array_size(&t->new_clause_bindings); i++)
    free_clause(tommy_array_get(&t->new_clause_bindings, i));
  tommy_array_done(&t->new_clause_bindings);

  for (tommy_size_t i = 0; i < tommy_array_size(&t->to_resolve); i++)
    free(tommy_array_get(&t->to_resolve, i));
  tommy_array_done(&t->to_resolve);

  free(t);
}
