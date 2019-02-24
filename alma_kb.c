#include <stdio.h>
#include <string.h>
#include "alma_kb.h"

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

static void find_variable_names(tommy_array *list, alma_term *term) {
  switch (term->type) {
    case VARIABLE: {
      char *varname = term->variable->name;
      for (tommy_size_t i = 0; i < tommy_array_size(list); i++) {
        if(strcmp(varname, tommy_array_get(list, i)) == 0)
          return;
      }
      tommy_array_insert(list, varname);
      break;
    }
    case CONSTANT: {
      return;
    }
    case FUNCTION: {
      for (int i = 0; i < term->function->term_count; i++) {
        find_variable_names(list, term->function->terms+i);
      }
    }
  }
}

static void set_variable_names(tommy_array *list, alma_term *term) {
  switch (term->type) {
    case VARIABLE: {
      char *varname = term->variable->name;
      for (tommy_size_t i = 0; i < tommy_array_size(list); i++) {
        if(strcmp(varname, tommy_array_get(list, i)) == 0)
          term->variable->id = variable_id_count + i;
      }
      break;
    }
    case CONSTANT: {
      return;
    }
    case FUNCTION: {
      for (int i = 0; i < term->function->term_count; i++) {
        set_variable_names(list, term->function->terms+i);
      }
    }
  }
}

// Given a clause, assign the ID fields of each variable, giving those with the same name matching IDs
// Fresh ID values drawn from variable_id_count global variable
static void set_variable_ids(clause *c) {
  tommy_array var_names;
  tommy_array_init(&var_names);
  for (int i = 0; i < c->pos_count; i++) {
    for (int j = 0; j < c->pos_lits[i]->term_count; j++) {
      find_variable_names(&var_names, c->pos_lits[i]->terms+j);
    }
  }
  for (int i = 0; i < c->neg_count; i++) {
    for (int j = 0; j < c->neg_lits[i]->term_count; j++) {
      find_variable_names(&var_names, c->neg_lits[i]->terms+j);
    }
  }
  for (int i = 0; i < c->pos_count; i++) {
    for (int j = 0; j < c->pos_lits[i]->term_count; j++) {
      set_variable_names(&var_names, c->pos_lits[i]->terms+j);
    }
  }
  for (int i = 0; i < c->neg_count; i++) {
    for (int j = 0; j < c->neg_lits[i]->term_count; j++) {
      set_variable_names(&var_names, c->neg_lits[i]->terms+j);
    }
  }
  variable_id_count += tommy_array_size(&var_names);
  tommy_array_done(&var_names);
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
static void flatten_node(alma_node *node, tommy_array *clauses) {
  if (node->type == FOL && node->fol->op == AND) {
    flatten_node(node->fol->arg1, clauses);
    flatten_node(node->fol->arg2, clauses);
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
    set_variable_ids(c);

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

    tommy_array_insert(clauses, c);
  }
}

static void free_clause(clause *c) {
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
static void nodes_to_clauses(alma_node *trees, int num_trees, tommy_array *clauses) {
  for (int i = 0; i < num_trees; i++) {
    flatten_node(trees+i, clauses);
    free_alma_tree(trees+i);
  }
  free(trees);
}

// Reorders clause array to begin with fifs; for correctness of task generation
static void fif_to_front(tommy_array *clauses) {
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

// Caller will need to free collection
void kb_init(alma_node *trees, int num_trees, kb **collection) {
  // Allocate and initialize
  *collection = malloc(sizeof(**collection));
  kb *collec = *collection;
  tommy_list_init(&collec->clauses);
  tommy_hashlin_init(&collec->index_map);
  tommy_hashlin_init(&collec->pos_map);
  tommy_list_init(&collec->pos_list);
  tommy_hashlin_init(&collec->neg_map);
  tommy_list_init(&collec->neg_list);
  tommy_hashlin_init(&collec->fif_map);
  tommy_array_init(&collec->res_task_list);
  collec->res_task_count = 0;
  tommy_hashlin_init(&collec->fif_tasks);
  tommy_hashlin_init(&collec->distrusted);

  tommy_array clauses;
  tommy_array_init(&clauses);
  nodes_to_clauses(trees, num_trees, &clauses);
  fif_to_front(&clauses);

  // Insert into KB if not duplicate
  for (tommy_size_t i = 0; i < tommy_array_size(&clauses); i++) {
    clause *c = tommy_array_get(&clauses, i);
    if (duplicate_check(collec, c) == NULL)
      add_clause(collec, c);
    else
      free_clause(c);
  }
  tommy_array_done(&clauses);

  // Generate starting resolution tasks
  tommy_node *i = tommy_list_head(&collec->clauses);
  while (i) {
    clause *c = ((index_mapping*)i->data)->value;
    if (c->tag == FIF)
      fif_task_map_init(collec, c);
    else {
      res_tasks_from_clause(collec, c, 0);
      fif_tasks_from_clause(collec, c);
    }
    i = i->next;
  }
}

static void free_predname_mapping(void *arg) {
  predname_mapping *entry = arg;
  free(entry->predname);
  free(entry->clauses);
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in free_kb
  free(entry);
}

static void free_fif_mapping(void *arg) {
  fif_mapping *entry = arg;
  free(entry->conclude_name);
  free(entry->clauses);
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in free_kb
  free(entry);
}

static void free_fif_task(void *arg) {
  fif_task_mapping *entry = arg;
  free(entry->predname);
  tommy_node *curr = tommy_list_head(&entry->tasks);
  while (curr) {
    fif_task *data = curr->data;
    curr = curr->next;
    // Note: clause entries ARE NOT FREED because they alias the clause objects freed in free_kb
    cleanup_bindings(data->bindings);
    free(data->unified_clauses);
    free(data);
  }
  free(entry);
}

void free_kb(kb *collection) {
  tommy_node *curr = tommy_list_head(&collection->clauses);
  while (curr) {
    index_mapping *data = curr->data;
    curr = curr->next;
    free_clause(data->value);
    free(data);
  }
  tommy_hashlin_done(&collection->index_map);

  tommy_list_foreach(&collection->pos_list, free_predname_mapping);
  tommy_hashlin_done(&collection->pos_map);

  tommy_list_foreach(&collection->neg_list, free_predname_mapping);
  tommy_hashlin_done(&collection->neg_map);

  tommy_hashlin_foreach(&collection->fif_map, free_fif_mapping);
  tommy_hashlin_done(&collection->fif_map);

  // Res task pointers are aliases to those freed from collection->clauses, so only free overall task here
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->res_task_list); i++) {
    free(tommy_array_get(&collection->res_task_list, i));
  }
  tommy_array_done(&collection->res_task_list);

  tommy_hashlin_foreach(&collection->fif_tasks, free_fif_task);
  tommy_hashlin_done(&collection->fif_tasks);

  tommy_hashlin_foreach(&collection->distrusted, free);
  tommy_hashlin_done(&collection->distrusted);

  free(collection);
}


// Accesses the literal in position i of fif clause c
static alma_function* fif_access(clause *c, int i) {
  int next = c->fif->ordering[i];
  if (next < 0)
    return c->neg_lits[-next - 1];
  else
    return c->pos_lits[next];
}

static void clause_print(clause *c) {
  // Print fif in original format
  if (c->tag == FIF) {
    for (int i = 0; i < c->fif->premise_count; i++) {
      alma_function *f = fif_access(c, i);
      if (c->fif->ordering[i] < 0) {
        alma_function_print(f);
      }
      else {
        printf("not(");
        alma_function_print(f);
        printf(")");
      }
      if (i < c->fif->premise_count-1)
        printf(" /\\");
      printf(" ");
    }
    printf("-f-> ");
    alma_function_print(c->fif->conclusion);
  }
  // Non-fif case
  else {
    if (c->pos_count == 0) {
      for (int i = 0; i < c->neg_count; i++) {
        printf("not(");
        alma_function_print(c->neg_lits[i]);
        printf(")");
        if (i < c->neg_count-1)
          printf(" \\/ ");
      }
    }
    else if (c->neg_count == 0) {
      for (int i = 0; i < c->pos_count; i++) {
        alma_function_print(c->pos_lits[i]);
        if (i < c->pos_count-1)
          printf(" \\/ ");
      }
    }
    else {
      for (int i = 0; i < c->neg_count; i++) {
        alma_function_print(c->neg_lits[i]);
        if (i < c->neg_count-1)
          printf(" /\\");
        printf(" ");
      }
      printf("---> ");
      for (int i = 0; i < c->pos_count; i++) {
        alma_function_print(c->pos_lits[i]);
        if (i < c->pos_count-1)
          printf(" \\/ ");
      }
    }
    if (c->parents != NULL || c->children != NULL) {
      printf(" (");
      if (c->parents != NULL) {
        printf("parents: ");
        for (int i = 0; i < c->parent_set_count; i++) {
          printf("[");
          for (int j = 0; j < c->parents[i].count; j++) {
            printf("%ld", c->parents[i].clauses[j]->index);
            if (j < c->parents[i].count-1)
              printf(", ");
          }
          printf("]");
          if (i < c->parent_set_count-1)
            printf(", ");
        }
        if (c->children != NULL)
          printf(", ");
      }
      if (c->children != NULL) {
        printf("children: ");
        for (int i = 0; i < c->children_count; i++) {
          printf("%ld",c->children[i]->index);
          if (i < c->children_count-1)
            printf(", ");
        }
      }
      printf(")");
    }
  }
}

void kb_print(kb *collection) {
  tommy_node *i = tommy_list_head(&collection->clauses);
  while (i) {
    index_mapping *data = i->data;
    printf("%ld: ", data->key);
    clause_print(data->value);
    printf("\n");
    i = i->next;
  }
  printf("\n");
}

// Compare function to be used by tommy_hashlin_search for index_mapping
// Compares long arg to key of index_mapping
static int im_compare(const void *arg, const void *obj) {
  return *(const long*)arg - ((const index_mapping*)obj)->key;
}

// Compare function to be used by tommy_hashlin_search for predname_mapping
// Compares string arg to predname of predname_mapping
static int pm_compare(const void *arg, const void *obj) {
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

// Given "name" and integer arity, returns "name/arity"
// Allocated cstring is later placed in predname_mapping, and eventually freed by free_kb
static char* name_with_arity(char *name, int arity) {
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
    if (pos) {
      for (int i = 0; i < c->pos_count; i++) {
        if (strcmp(name, c->pos_lits[i]->name) == 0)
          return c->pos_lits[i];
      }
    }
    else {
      for (int i = 0; i < c->neg_count; i++) {
        if (strcmp(name, c->neg_lits[i]->name) == 0)
          return c->neg_lits[i];
      }
    }
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
// TODO May be inefficient -- check for deleted x and y when dealing with tasks? Or assume deletion is rare
static void remove_res_tasks(kb *collection, clause *c) {
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->res_task_list); i++) {
    res_task *current_task = tommy_array_get(&collection->res_task_list, i);
    if (current_task != NULL && (current_task->x == c || current_task->y == c)) {
      tommy_array_set(&collection->res_task_list, i, NULL);
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
void remove_clause(kb *collection, clause *c) {
  for (int i = 0; i < c->pos_count; i++)
    map_remove_clause(&collection->pos_map, &collection->pos_list, c->pos_lits[i], c);
  for (int i = 0; i < c->neg_count; i++)
    map_remove_clause(&collection->neg_map, &collection->neg_list, c->neg_lits[i], c);
  index_mapping *result = tommy_hashlin_search(&collection->index_map, im_compare, &c->index, tommy_hash_u64(0, &c->index, sizeof(c->index)));
  tommy_list_remove_existing(&collection->clauses, &result->list_node);
  tommy_hashlin_remove_existing(&collection->index_map, &result->hash_node);
  free(result);

  remove_res_tasks(collection, c);

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
        task->num_unified = 0;
        task->unified_clauses = NULL;
        task->to_unify = NULL;
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
            copy->num_unified = data->num_unified;
            copy->unified_clauses = malloc(sizeof(*data->unified_clauses) * data->num_unified);
            memcpy(copy->unified_clauses, data->unified_clauses, sizeof(*data->unified_clauses) * data->num_unified);
            copy->to_unify = c;
            tommy_list_insert_tail(&result->tasks, &copy->node, copy);
          }
          // Special case -- c should only -once- be put into to_unify of a task with none unified
          set_first = 1;
        }

        curr = curr->next;
        i++;
      }
    }
  }
}

// Called when each premise of a fif is satisfied
clause* fif_conclude(kb *collection, fif_task *task, binding_list *bindings, clause *last_unified) {
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
  for (int i = 0; i < conclusion->parents[0].count-2; i++) {
    long index = task->unified_clauses[i];
    index_mapping *result = tommy_hashlin_search(&collection->index_map, im_compare, &index, tommy_hash_u64(0, &index, sizeof(index)));
    conclusion->parents[0].clauses[i] = result->value;
  }
  conclusion->parents[0].clauses[conclusion->parents[0].count-2] = last_unified;
  conclusion->parents[0].clauses[conclusion->parents[0].count-1] = task->fif;
  conclusion->children_count = 0;
  conclusion->children = NULL;
  conclusion->tag = NONE;
  conclusion->fif = NULL;

  return conclusion;
}

// Continues attempting to unify from tasks set
// Task instances for which cannot be further unified with current KB are added to stopped
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

    alma_function *next_func = fif_access(next_task->fif, next_task->num_unified);
    char *name = name_with_arity(next_func->name, next_func->term_count);

    // Important: because of conversion to CNF, a negative in ordering means fif premise is positive
    int search_pos = next_task->fif->fif->ordering[next_task->num_unified] < 0;
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
            if (next_task->num_unified + 1 == next_task->fif->fif->premise_count) {
              next_task->num_unified++;
              clause *conclusion = fif_conclude(collection, next_task, copy, jth);
              tommy_array_insert(new_clauses, conclusion);
              next_task->num_unified--;
            }
            // Otherwise, to continue processing, create second task for results and place in tasks
            else {
              fif_task *latest = malloc(sizeof(*latest));
              latest->fif = next_task->fif;
              latest->bindings = copy;
              latest->num_unified = next_task->num_unified + 1;
              latest->unified_clauses = malloc(sizeof(*latest->unified_clauses)*latest->num_unified);
              memcpy(latest->unified_clauses, next_task->unified_clauses, sizeof(*next_task->unified_clauses) * next_task->num_unified);
              latest->unified_clauses[latest->num_unified-1] = jth->index;
              latest->to_unify = NULL;
              tommy_list_insert_tail(tasks, &latest->node, latest);
            }

          }
          else {
            // Unfication failure
            cleanup_bindings(copy);
            free(copy);
          }
        }
      }
    }
    tommy_list_insert_tail(stopped, &next_task->node, next_task);
  }

  // If tasks list is nonempty, have newly unified tasks to recurse on
  if (!tommy_list_empty(tasks))
    fif_task_unify_loop(collection, tasks, stopped, new_clauses);
}

// Process fif tasks
// For each task in fif_task_mapping:
// - Attempt unification on to_unify
// - If unification succeeds, 1) searches for possible singleton matches against next literal, 2) attempts unification on these
// Above process repeats per task until unification fails or fif task is fully satisfied (and then asserts conclusion)
static void process_fif_task_mapping(kb *collection, fif_task_mapping *entry, tommy_list *progressed, tommy_array *new_clauses) {
  tommy_node *curr = tommy_list_head(&entry->tasks);
  while (curr) {
    fif_task *f = curr->data;
    curr = curr->next;

    if (f->to_unify != NULL) {
      alma_function *fif_func = fif_access(f->fif, f->num_unified);
      alma_function *to_unify_func = (f->to_unify->pos_count > 0) ? f->to_unify->pos_lits[0] : f->to_unify->neg_lits[0];

      binding_list *copy = malloc(sizeof(*copy));
      copy_bindings(copy, f->bindings);
      if (pred_unify(fif_func, to_unify_func, f->bindings)) {
        // If task is now completed, obtain resulting clause and insert to new_clauses
        if (f->num_unified + 1 == f->fif->fif->premise_count) {
          f->num_unified++;
          clause *conclusion = fif_conclude(collection, f, f->bindings, f->to_unify);
          tommy_array_insert(new_clauses, conclusion);
          f->num_unified--;

          f->to_unify = NULL;
          // Reset from copy
          f->bindings = copy;
        }
        // Otherwise, adjust task and do unify loop call
        else {
          cleanup_bindings(copy);
          f->num_unified++;
          f->unified_clauses = realloc(f->unified_clauses, sizeof(*f->unified_clauses)*f->num_unified);
          f->unified_clauses[f->num_unified-1] = f->to_unify->index;
          f->to_unify = NULL;

          tommy_list_remove_existing(&entry->tasks, &f->node);
          tommy_list to_progress;
          tommy_list_init(&to_progress);
          tommy_list_insert_tail(&to_progress, &f->node, f);

          // Unification succeeded; progress further on task as able in call
          fif_task_unify_loop(collection, &to_progress, progressed, new_clauses);
        }
      }
      else {
        f->to_unify = NULL;
        // Reset from copy
        cleanup_bindings(f->bindings);
        f->bindings = copy;
      }
    }
  }
}

// See helper process_fif_task_mapping for comment
void process_fif_tasks(kb *collection, tommy_array *new_clauses) {
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
			process_fif_task_mapping(collection, data, &progressed, new_clauses);
		}
  }

  // Incorporates tasks from progressed list back into task mapping
  tommy_node *curr = tommy_list_head(&progressed);
  while (curr) {
    fif_task *data = curr->data;
    curr = curr->next;

    alma_function *f = fif_access(data->fif, data->num_unified);
    char *name = name_with_arity(f->name, f->term_count);
    fif_task_mapping *result = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    tommy_list_insert_tail(&result->tasks, &data->node, data);
    free(name);
  }
}

// Finds new res tasks based on matching pos/neg predicate pairs, where one is from the KB and the other from arg
// Tasks are added into the res_task_list of collection
// TODO: Refactor to remove code duplication
void res_tasks_from_clause(kb *collection, clause *c, int process_negatives) {
  // Iterate positive literals of clauses and match against negative literals of collection
  for (int j = 0; j < c->pos_count; j++) {
    char *name = name_with_arity(c->pos_lits[j]->name, c->pos_lits[j]->term_count);
    predname_mapping *result = tommy_hashlin_search(&collection->neg_map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    // If a negative literal is found to match a positive literal, add tasks
    // New tasks are from Cartesian product of result's clauses with clauses' ith
    if (result != NULL) {
      for (int k = 0; k < result->num_clauses; k++) {
        if (c != result->clauses[k]) {
          alma_function *lit = literal_by_name(result->clauses[k], c->pos_lits[j]->name, 0);
          if (lit != NULL) {
            res_task *t = malloc(sizeof(*t));
            t->x = c;
            t->pos = c->pos_lits[j];
            t->y = result->clauses[k];
            t->neg = lit;
            tommy_array_insert(&collection->res_task_list, t);
            collection->res_task_count++;
          }
        }
      }
    }
    free(name);
  }
  // Iterate negative literals of clauses and match against positive literals of collection
  // Only done if clauses differ from KB's clauses (i.e. after first task generation)
  if (process_negatives) {
    for (int j = 0; j < c->neg_count; j++) {
      char *name = name_with_arity(c->neg_lits[j]->name, c->neg_lits[j]->term_count);
      predname_mapping *result = tommy_hashlin_search(&collection->pos_map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
      // If a positive literal is found to match a negative literal, add tasks
      // New tasks are from Cartesian product of result's clauses with clauses' ith
      if (result != NULL) {
        for (int k = 0; k < result->num_clauses; k++) {
          if (c != result->clauses[k]) {
            alma_function *lit = literal_by_name(result->clauses[k], c->neg_lits[j]->name, 1);
            if (lit != NULL) {
              res_task *t = malloc(sizeof(*t));
              t->x = result->clauses[k];
              t->pos = lit;
              t->y = c;
              t->neg = c->neg_lits[j];
              tommy_array_insert(&collection->res_task_list, t);
              collection->res_task_count++;
            }
          }
        }
      }
      free(name);
    }
  }
}

// Returns boolean based on success of string parse
// If parses successfully, adds to tommy_array arg -- which caller must allocate/init
int assert_formula(char *string, tommy_array *clauses) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas)) {
    nodes_to_clauses(formulas, formula_count, clauses);
    return 1;
  }
  return 0;
}

int delete_formula(kb *collection, char *string) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas)) {
    // Convert formulas to clause array
    tommy_array clauses;
    tommy_array_init(&clauses);
    for (int i = 0; i < formula_count; i++) {
      flatten_node(formulas+i, &clauses);
      free_alma_tree(formulas+i);
    }
    free(formulas);

    // Process and remove each clause
    for (tommy_size_t i = 0; i < tommy_array_size(&clauses); i++) {
      clause *curr = tommy_array_get(&clauses, i);
      clause *c = duplicate_check(collection, curr);
      if (c != NULL) {
        remove_clause(collection, c);
      }
      free_clause(curr);
    }
    tommy_array_done(&clauses);

    return 1;
  }
  return 0;
}

// Given an MGU, substitute on literals other than pair unified and make a single resulting clause
void resolve(res_task *t, binding_list *mgu, clause *result) {
  //int max_pos = t->x->pos_count + t->y->pos_count - 1;
  //int max_neg = t->x->neg_count + t->y->neg_count - 1;
  result->pos_count = 0;
  if (t->x->pos_count + t->y->pos_count - 1 > 0) {
    result->pos_lits = malloc(sizeof(*result->pos_lits) * (t->x->pos_count + t->y->pos_count - 1));

    // Copy positive literal from t->x and t->y
    for (int i = 0; i < t->x->pos_count; i++) {
      if (t->x->pos_lits[i] != t->pos) {
        alma_function *litcopy = malloc(sizeof(*litcopy));
        copy_alma_function(t->x->pos_lits[i], litcopy);
        for (int j = 0; j < litcopy->term_count; j++)
          subst(mgu, litcopy->terms+j);
        result->pos_lits[result->pos_count] = litcopy;
        result->pos_count++;
        // if (result->pos_count > max_pos)
        //   abort();
      }
    }
    for (int i = 0; i < t->y->pos_count; i++) {
      alma_function *litcopy = malloc(sizeof(*litcopy));
      copy_alma_function(t->y->pos_lits[i], litcopy);
      for (int j = 0; j < litcopy->term_count; j++)
        subst(mgu, litcopy->terms+j);
      result->pos_lits[result->pos_count] = litcopy;
      result->pos_count++;
      // if (result->pos_count > max_pos)
      //   abort();
    }
  }
  else {
    result->pos_lits = NULL;
  }

  result->neg_count = 0;
  if (t->x->neg_count + t->y->neg_count - 1 > 0) {
    result->neg_lits = malloc(sizeof(*result->neg_lits) * (t->x->neg_count + t->y->neg_count - 1));

    // Copy negative literal from t->x and t->y
    for (int i = 0; i < t->y->neg_count; i++) {
      if (t->y->neg_lits[i] != t->neg) {
        alma_function *litcopy = malloc(sizeof(*litcopy));
        copy_alma_function(t->y->neg_lits[i], litcopy);
        for (int j = 0; j < litcopy->term_count; j++)
          subst(mgu, litcopy->terms+j);
        result->neg_lits[result->neg_count] = litcopy;
        result->neg_count++;
        // if (result->neg_count > max_neg)
        //   abort();
      }
    }
    for (int i = 0; i < t->x->neg_count; i++) {
      alma_function *litcopy = malloc(sizeof(*litcopy));
      copy_alma_function(t->x->neg_lits[i], litcopy);
      for (int j = 0; j < litcopy->term_count; j++)
        subst(mgu, litcopy->terms+j);
      result->neg_lits[result->neg_count] = litcopy;
      result->neg_count++;
      // if (result->neg_count > max_neg)
      //   abort();
    }
  }
  else {
    result->neg_lits = NULL;
  }
  result->tag = NONE; // TODO: Deal with tags for bif case
  result->fif = NULL;
}

static char* now(long t) {
  int len = snprintf(NULL, 0, "%ld", t);
  char *str = malloc(4 + len+1 + 2);
  strcpy(str, "now(");
  snprintf(str+4, len+1, "%ld", t);
  strcpy(str+4+len, ").");
  return str;
}

static char* long_to_str(long x) {
  int length = snprintf(NULL, 0, "%ld", x);
  char *str = malloc(length+1);
  snprintf(str, length+1, "%ld", x);
  return str;
}

static void add_child(clause *parent, clause *child) {
  parent->children_count++;
  parent->children = realloc(parent->children, sizeof(*parent->children) * parent->children_count);
  parent->children[parent->children_count-1] = child;
}


// Compare function to be used by tommy_hashlin_search for distrust_mapping
// Compares long arg to key of distrust_mapping
static int dm_compare(const void *arg, const void *obj) {
  return *(const long*)arg - ((const distrust_mapping*)obj)->key;
}

static int is_distrusted(kb *collection, long index) {
  return tommy_hashlin_search(&collection->distrusted, dm_compare, &index, tommy_hash_u64(0, &index, sizeof(index))) != NULL;
}

static void distrust_recursive(kb *collection, clause *c, long time, tommy_array *new_clauses) {
  // Add c to distrusted set
  distrust_mapping *d = malloc(sizeof(*d));
  d->key = c->index;
  d->value = c;
  tommy_hashlin_insert(&collection->distrusted, &d->node, d, tommy_hash_u64(0, &d->key, sizeof(d->key)));

  // Assert distrusted formula
  char *index = long_to_str(c->index);
  char *time_str = long_to_str(time);
  char *distrust_str = malloc(strlen(index) + strlen(time_str) + 15);
  strcpy(distrust_str, "distrusted(");
  int loc = 11;
  strcpy(distrust_str+loc, index);
  loc += strlen(index);
  free(index);
  distrust_str[loc++] = ',';
  strcpy(distrust_str+loc, time_str);
  loc += strlen(time_str);
  free(time_str);
  strcpy(distrust_str+loc, ").");
  assert_formula(distrust_str, new_clauses);
  free(distrust_str);

  // Unindex c from pos_map/list, neg_map/list to prevent use in inference
  for (int i = 0; i < c->pos_count; i++)
    map_remove_clause(&collection->pos_map, &collection->pos_list, c->pos_lits[i], c);
  for (int i = 0; i < c->neg_count; i++)
    map_remove_clause(&collection->neg_map, &collection->neg_list, c->neg_lits[i], c);

  // Recursively distrust children that aren't distrusted already
  if (c->children != NULL) {
    for (int i = 0; i < c->children_count; i++) {
      if (is_distrusted(collection, c->children[i]->index)) {
        distrust_recursive(collection, c->children[i], time, new_clauses);
      }
    }
  }
}

void forward_chain(kb *collection) {
  int chain = 1;
  long time = 1;
  char *prev_str = NULL;
  char *now_str = NULL;
  while(chain) {
    time++;
    tommy_array new_clauses;
    tommy_array_init(&new_clauses);

    now_str = now(time);
    assert_formula(now_str, &new_clauses);
    if (prev_str != NULL) {
      delete_formula(collection, prev_str);
      free(prev_str);
    }
    else
      delete_formula(collection, "now(1).");
    prev_str = now_str;

    // Process resolution tasks and place results in new_clauses
    for (tommy_size_t i = 0; i < tommy_array_size(&collection->res_task_list); i++) {
      res_task *current_task = tommy_array_get(&collection->res_task_list, i);
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

              tommy_array_insert(&new_clauses, res_result);
            }
            else {
              free(res_result);

              // Empty resolution result indicates a contradiction between clauses
              char *arg1 = long_to_str(current_task->x->index);
              char *arg2 = long_to_str(current_task->y->index);
              char *time_str = long_to_str(time);

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
              free(time_str);
              strcpy(contra_str+loc, ").");

              // Assert contra and distrusted
              assert_formula(contra_str, &new_clauses);
              free(contra_str);
              distrust_recursive(collection, current_task->x, time, &new_clauses);
              distrust_recursive(collection, current_task->y, time, &new_clauses);
            }
          }
          cleanup_bindings(theta);
        }

        collection->res_task_count--;
        free(current_task);
      }
    }
    tommy_array_done(&collection->res_task_list);
    tommy_array_init(&collection->res_task_list);

    // Process fif tasks and place results in new_clauses
    process_fif_tasks(collection, &new_clauses);

    // Time always advances; continues while other clauses are added
    if (tommy_array_size(&new_clauses) > 1) {
      fif_to_front(&new_clauses);

      // Insert new clauses derived that are not duplicates
      for (tommy_size_t i = 0; i < tommy_array_size(&new_clauses); i++) {
        clause *c = tommy_array_get(&new_clauses, i);
        clause *dupe = duplicate_check(collection, c);
        if (dupe == NULL) {
          res_tasks_from_clause(collection, c, 1);
          fif_tasks_from_clause(collection, c);
          // Dupe fif tasks are then used after clause freed

          if (c->parents != NULL) {
            // Update child info for parents of new clause
            for (int j = 0; j < c->parents[0].count; j++)
              add_child(c->parents[0].clauses[j], c);
          }
          add_clause(collection, c);
        }
        else {
          if (c->parents != NULL) {
            // A duplicate clause derivation should be acknowledged by adding extra parents to the clause it duplicates
            // Copy parents of c to dupe
            dupe->parents = realloc(dupe->parents, sizeof(*dupe->parents) * (dupe->parent_set_count + c->parent_set_count));
            for (int j = dupe->parent_set_count, k = 0; j < dupe->parent_set_count + c->parent_set_count; j++, k++) {
              dupe->parents[j].count = c->parents[k].count;
              dupe->parents[j].clauses = malloc(sizeof(*dupe->parents[j].clauses) * dupe->parents[j].count);
              memcpy(dupe->parents[j].clauses, c->parents[k].clauses, sizeof(*dupe->parents[j].clauses) * dupe->parents[j].count);
            }
            //memcpy(dupe->parents + dupe->parent_set_count, c->parents, sizeof(*c->parents) * c->parent_set_count);
            dupe->parent_set_count += c->parent_set_count;

            // Parents of c also gain new child in dupe
            for (int j = 0; j < c->parents[0].count; j++) {
              int insert = 1;
              for (int k = 0; k < c->parents[0].clauses[j]->children_count; k++) {
                if (c->parents[0].clauses[j]->children[k] == dupe) {
                  insert = 0;
                  break;
                }
              }
              if (insert)
                add_child(c->parents[0].clauses[j], dupe);
            }
          }

          free_clause(c);
        }
      }

      kb_print(collection);
    }
    else {
      chain = 0;
      free(now_str);
      free_clause(tommy_array_get(&new_clauses, 0));
      printf("Idling...\n");
    }
    tommy_array_done(&new_clauses);
  }
}
