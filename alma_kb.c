#include <stdio.h>
#include <string.h>
#include "alma_kb.h"

static long next_index;

// Assumes c is allocated by caller
// TODO: Case for something other than OR/NOT/pred appearing?
static void make_clause(alma_node *node, clause *c) {
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

// Comparison used by qsort of clauses -- orders by increasing function name and increasing arity
int function_compare (const void *p1, const void *p2) {
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
    c->parent_pair_count = c->children_count = 0;
    c->parents = NULL;
    c->children = NULL;
    c->tag = NONE;
    make_clause(node, c);
    set_variable_ids(c);
    qsort(c->pos_lits, c->pos_count, sizeof(*c->pos_lits), function_compare);
    qsort(c->neg_lits, c->neg_count, sizeof(*c->neg_lits), function_compare);

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
  free(c->parents);
  free(c->children);
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
  tommy_array_init(&collec->task_list);
  tommy_hashlin_init(&collec->distrusted);
  collec->task_count = 0;

  tommy_array clauses;
  tommy_array_init(&clauses);
  nodes_to_clauses(trees, num_trees, &clauses);

  // Insert into KB if not duplicate
  tommy_array duplicates;
  tommy_array_init(&duplicates);
  for (tommy_size_t i = 0; i < tommy_array_size(&clauses); i++) {
    clause *c = tommy_array_get(&clauses, i);
    if (duplicate_check(collec, c) == NULL)
      add_clause(collec, c);
    else
      tommy_array_insert(&duplicates, c);
  }
  tommy_array_done(&clauses);
  // Free unused duplicate clauses
  for (tommy_size_t i = 0; i < tommy_array_size(&duplicates); i++)
    free_clause(tommy_array_get(&duplicates, i));
  tommy_array_done(&duplicates);

  // Generate starting tasks
  tommy_node* i = tommy_list_head(&collec->clauses);
  while (i) {
    tasks_from_clause(collec, ((index_mapping *)i->data)->value, 0);
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

void free_kb(kb *collection) {
  tommy_node* curr = tommy_list_head(&collection->clauses);
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

  // Task pointers are aliases to those freed from collection->clauses, so only free overall task here
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->task_list); i++) {
    free(tommy_array_get(&collection->task_list, i));
  }
  tommy_array_done(&collection->task_list);

  tommy_hashlin_foreach(&collection->distrusted, free);
  tommy_hashlin_done(&collection->distrusted);

  free(collection);
}


static void clause_print(clause *c, int print_relatives) {
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
  if (print_relatives) {
    printf(" (parents: ");
    for (int i = 0; i < c->parent_pair_count; i++) {
      printf("[");
      clause_print(c->parents[i].x, 0);
      printf("; ");
      clause_print(c->parents[i].y, 0);
      printf("]");
      if (i < c->parent_pair_count-1)
        printf(", ");
    }
    printf(", children: ");
    for (int i = 0; i < c->children_count; i++) {
      clause_print(c->children[i], 0);
      if (i < c->children_count-1)
        printf(", ");
    }
    printf(")");
  }
}

/*static void task_print(task *t) {
  printf("Task pos literal: ");
  alma_function_print(t->pos);
  printf("\nTask neg literal: ");
  alma_function_print(t->neg);
  printf("\nClause with pos literal:\n");
  clause_print(t->x, 0);
  printf("\nClause with neg literal:\n");
  clause_print(t->y, 0);
  printf("\n\n");
}*/

/*static void print_predname_mapping(void *kind, void *arg) {
  char *str = kind;
  predname_mapping *entry = arg;
  printf("Map entry%s: %s\n", str, entry->predname);
  for (int i = 0; i < entry->num_clauses; i++) {
    clause_print(entry->clauses[i], 0);
    printf("\n\n");
  }
  printf("\n");
}*/

void kb_print(kb *collection) {
  tommy_node* i = tommy_list_head(&collection->clauses);
  while (i) {
    index_mapping *data = i->data;
    printf("%ld: ", data->key);
    clause_print(data->value, 1);
    printf("\n");
    i = i->next;
  }
  printf("\n");

  //tommy_list_foreach_arg(&collection->pos_list, print_predname_mapping, " pos");
  //tommy_list_foreach_arg(&collection->neg_list, print_predname_mapping, " neg");

  /*for (tommy_size_t i = 0; i < tommy_array_size(&collection->task_list); i++) {
    task_print(tommy_array_get(&collection->task_list, i));
  }*/
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
    // All literals compared as equal; return 0
    return release_matches(&matches, 0);
  }
  return 1;
}

// If c is found to be a clause duplicate, returns a pointer to that clause; null otherwise
// See comments preceding clauses_differ function for further detail
clause* duplicate_check(kb *collection, clause *c) {
  if (c->pos_count > 0) {
    // If clause has a positive literal, all duplicate candidates must have that same positive literal
    // Arbitrarily pick first positive literal as one to use; may be able to do smarter literal choice later
    char *name = name_with_arity(c->pos_lits[0]->name, c->pos_lits[0]->term_count);
    predname_mapping *result = tommy_hashlin_search(&collection->pos_map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    free(name);
    if (result != NULL) {
      for (int i = 0; i < result->num_clauses; i++) {
        if (!clauses_differ(c, result->clauses[i]))
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
        if (!clauses_differ(c, result->clauses[i]))
          return result->clauses[i];
      }
    }
  }
  return NULL;
}

// Given a new clause, add to the KB and maps
void add_clause(kb *collection, clause *c) {
  // TODO: call to duplicate literal filtering (add function and such)

  // Indexes clause into hashaps of KB collection
  for (int j = 0; j < c->pos_count; j++)
    map_add_clause(&collection->pos_map, &collection->pos_list, c->pos_lits[j], c);
  for (int j = 0; j < c->neg_count; j++)
    map_add_clause(&collection->neg_map, &collection->neg_list, c->neg_lits[j], c);

  index_mapping *entry = malloc(sizeof(*entry));
  c->index = entry->key = next_index++;
  entry->value = c;
  tommy_list_insert_tail(&collection->clauses, &entry->list_node, entry);
  tommy_hashlin_insert(&collection->index_map, &entry->hash_node, entry, tommy_hash_u64(0, &entry->key, sizeof(entry->key)));
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

  // Remove tasks using clause
  // TODO May be inefficient -- check for deleted x and y when dealing with tasks? Or assume deletion is rare
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->task_list); i++) {
    task *current_task = tommy_array_get(&collection->task_list, i);
    if (current_task->x == c || current_task->y == c) {
      tommy_array_set(&collection->task_list, i, NULL);
      free(current_task);
    }
  }

  // Remove clause from the parents list of each of its children
  for (int i = 0; i < c->children_count; i++) {
    clause *child = c->children[i];
    if (child != NULL) {
      for (int j = 0; j < child->parent_pair_count; j++) {
        // Parent pairs should be unique; uses this assumption
        if (child->parents[j].x == c || child->parents[j].y == c) {
          if (j < child->parent_pair_count-1)
            child->parents[j] = child->parents[child->parent_pair_count-1];
          child->parent_pair_count--;
          if (child->parent_pair_count > 0) {
            child->parents = realloc(child->parents, sizeof(*child->parents) * child->parent_pair_count);
          }
          else {
            free(child->parents);
            child->parents = NULL;
          }
          break;
        }
      }
    }
  }

  // Remove clause from the children list of each of its parents
  for (int i = 0; i < c->parent_pair_count; i++) {
    remove_child(c->parents[i].x, c);
    remove_child(c->parents[i].y, c);
  }

  free_clause(c);
}

// Finds new tasks based on matching pos/neg predicate pairs, where one is from the KB and the other from clauses
// Tasks are added into the task_list of collection
// TODO: Refactor to remove code duplication
void tasks_from_clause(kb *collection, clause *c, int process_negatives) {
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
            task *t = malloc(sizeof(*t));
            t->x = c;
            t->pos = c->pos_lits[j];
            t->y = result->clauses[k];
            t->neg = lit;
            tommy_array_insert(&collection->task_list, t);
            collection->task_count++;
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
              task *t = malloc(sizeof(*t));
              t->x = result->clauses[k];
              t->pos = lit;
              t->y = c;
              t->neg = c->neg_lits[j];
              tommy_array_insert(&collection->task_list, t);
              collection->task_count++;
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
// Last argument must be allocated by caller
void resolve(task *t, binding_list *mgu, clause *result) {
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
  result->tag = NONE; // TODO: Deal with tags properly for fif/bif cases
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
  char* str = malloc(length+1);
  snprintf(str, length+1, "%ld", x);
  return str;
}

static void add_child(clause *parent, clause *child) {
  parent->children_count++;
  parent->children = realloc(parent->children, sizeof(*parent->children) * parent->children_count);
  parent->children[parent->children_count-1] = child;
}

static void distrust_recursive(kb *collection, clause *c, long time, tommy_array *new_clauses) {
  // Add C to distrusted set
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
  strcpy(distrust_str, ").");
  assert_formula(distrust_str, new_clauses);
  free(distrust_str);

  // Unindex c from pos_map/list, neg_map/list to prevent use in inference
  // TODO

  if (c->children != NULL) {
    for (int i = 0; i < c->children_count; i++) {
      clause *child = c->children[i];
      if (tommy_hashlin_search(&collection->distrusted, /* TODO, compare func */, child->index, tommy_hash_u64(0, &child->index, sizeof(child->index))) {
        distrust_recursive(collection, child, time, new_clauses);
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

    for (tommy_size_t i = 0; i < tommy_array_size(&collection->task_list); i++) {
      task *current_task = tommy_array_get(&collection->task_list, i);
      if (current_task != NULL) {
        // Given a task, attempt unification
        binding_list *theta = malloc(sizeof(*theta));
        if (pred_unify(current_task->pos, current_task->neg, theta)) {
          // If successful, create clause for result of resolution and add to new_clauses
          clause *res_result = malloc(sizeof(*res_result));
          resolve(current_task, theta, res_result);

          // An empty resolution result is not a valid clause to add to KB
          if (res_result->pos_count > 0 || res_result->neg_count > 0) {
            // Initialize parents of result
            res_result->parent_pair_count = 1;
            res_result->parents = malloc(sizeof(*res_result->parents));
            res_result->parents[0].x = current_task->x;
            res_result->parents[0].y = current_task->y;
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

        collection->task_count--;
        free(current_task);
      }
    }
    tommy_array_done(&collection->task_list);
    tommy_array_init(&collection->task_list);

    // Time always advances; continues while other clauses are added
    if (tommy_array_size(&new_clauses) > 1) {
      // Get new tasks before adding new clauses into collection
      for (tommy_size_t i = 0; i < tommy_array_size(&new_clauses); i++) {
        clause *c = tommy_array_get(&new_clauses, i);
        if (duplicate_check(collection, c) == NULL)
          tasks_from_clause(collection, c, 1);
      }

      tommy_array duplicates;
      tommy_array_init(&duplicates);
      // Insert new clauses derived that are not duplicates
      // TODO: Make more efficient, as currently checks for second time duplicate status of some known duplicates
      for (tommy_size_t i = 0; i < tommy_array_size(&new_clauses); i++) {
        clause *c = tommy_array_get(&new_clauses, i);
        clause *dupe = duplicate_check(collection, c);
        if (dupe == NULL) {
          // Update child info for parents of new clause
          if (c->parents != NULL) {
            add_child(c->parents[0].x, c);
            add_child(c->parents[0].y, c);
          }

          add_clause(collection, c);
        }
        else {
          tommy_array_insert(&duplicates, c);

          // A duplicate clause derivation should be acknowledged by adding extra parents to the clause it duplicates
          // Copy parents of c to dupe
          dupe->parents = realloc(dupe->parents, sizeof(*dupe->parents) * (dupe->parent_pair_count + c->parent_pair_count));
          memcpy(dupe->parents + dupe->parent_pair_count, c->parents, sizeof(*c->parents) * c->parent_pair_count);
          dupe->parent_pair_count += c->parent_pair_count;

          // Parents of c also gain new child in dupe
          clause *parent1 = c->parents[0].x;
          int insert = 1;
          for (int j = 0; j < parent1->children_count; j++) {
            if (parent1->children[j] == dupe) {
              insert = 0;
              break;
            }
          }
          if (insert)
            add_child(parent1, dupe);
          clause *parent2 = c->parents[0].y;
          insert = 1;
          for (int j = 0; j < parent2->children_count; j++) {
            if (parent2->children[j] == dupe) {
              insert = 0;
              break;
            }
          }
          if (insert)
            add_child(parent2, dupe);
        }
      }

      // Replace duplicate entries in task list with null
      for (tommy_size_t i = 0; i < tommy_array_size(&collection->task_list); i++) {
        task *t = tommy_array_get(&collection->task_list, i);
        // TODO: Switch to a more efficient lookup in duplicates
        for (tommy_size_t j = 0; j < tommy_array_size(&duplicates); j++) {
          if (tommy_array_get(&duplicates, j) == t->x || tommy_array_get(&duplicates, j) == t->y) {
            free(t);
            tommy_array_set(&collection->task_list, i, NULL);
            break;
          }
        }
      }

      // Free duplicates
      for (tommy_size_t i = 0; i < tommy_array_size(&duplicates); i++) {
        free_clause(tommy_array_get(&duplicates, i));
      }
      tommy_array_done(&duplicates);

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
