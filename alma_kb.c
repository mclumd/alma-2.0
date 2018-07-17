#include <stdio.h>
#include <string.h>
#include "alma_kb.h"

// Assumes c is allocated by caller
// TODO: Case for something other than OR/NOT/pred appearing?
static void make_clause(alma_node *node, clause *c) {
  if (node != NULL) {
    if (node->type == FOL) {
      // Neg lit case for NOT
      if (node->fol->op == NOT) {
        c->neg_count++;
        c->neg_lits = realloc(c->neg_lits, sizeof(alma_function*) * c->neg_count);
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
      c->pos_lits = realloc(c->pos_lits, sizeof(alma_function*) * c->pos_count);
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

// Flattens a single alma node and adds its contents to collection
// Recursively calls when an AND is found to separate conjunctions
static void flatten_node(alma_node *node, tommy_array *clauses) {
  if (node->type == FOL && node->fol->op == AND) {
    flatten_node(node->fol->arg1, clauses);
    flatten_node(node->fol->arg2, clauses);
  }
  else {
    clause *c = malloc(sizeof(clause));
    c->pos_count = c->neg_count = 0;
    c->pos_lits = NULL;
    c->neg_lits = NULL;
    c->tag = NONE;
    make_clause(node, c);
    set_variable_ids(c);

    tommy_array_insert(clauses, c);
  }
}

// Caller will need to free collection
// trees also must be freed by the caller after this call, as it is not deallocated here
void kb_init(alma_node *trees, int num_trees, kb **collection) {
  // Allocate and initialize
  *collection = malloc(sizeof(kb));
  kb *collec = *collection;
  tommy_array_init(&collec->clauses);
  tommy_hashlin_init(&collec->pos_map);
  tommy_list_init(&collec->pos_list);
  tommy_hashlin_init(&collec->neg_map);
  tommy_list_init(&collec->neg_list);
  tommy_array_init(&collec->task_list);
  collec->task_count = 0;

  // Flatten trees into clause list
  tommy_array starting_clauses;
  tommy_array_init(&starting_clauses);
  for (int i = 0; i < num_trees; i++)
    flatten_node(trees+i, &starting_clauses);
  for (tommy_size_t i = 0; i < tommy_array_size(&starting_clauses); i++)
    add_new_clause(collec, tommy_array_get(&starting_clauses, i));
  tommy_array_done(&starting_clauses);

  // Generate starting tasks
  for (tommy_size_t i = 0; i < tommy_array_size(&collec->clauses); i++)
    tasks_from_clause(collec, tommy_array_get(&collec->clauses, i), 0);
}

static void free_map_entry(void *arg) {
  map_entry *entry = arg;
  free(entry->predname);
  free(entry->clauses);
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in free_kb
  free(entry);
}

static void free_clause(clause *c) {
  for (int i = 0; i < c->pos_count; i++)
    free_function(c->pos_lits[i]);
  for (int i = 0; i < c->neg_count; i++)
    free_function(c->neg_lits[i]);
  free(c->pos_lits);
  free(c->neg_lits);
  free(c);
}

void free_kb(kb *collection) {
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->clauses); i++) {
    free_clause(tommy_array_get(&collection->clauses, i));
  }
  tommy_array_done(&collection->clauses);

  tommy_list_foreach(&collection->pos_list, free_map_entry);
  tommy_hashlin_done(&collection->pos_map);

  tommy_list_foreach(&collection->neg_list, free_map_entry);
  tommy_hashlin_done(&collection->neg_map);

  // Task pointers are aliases to those freed from collection->clauses, so only free overall task here
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->task_list); i++) {
    free(tommy_array_get(&collection->task_list, i));
  }
  tommy_array_done(&collection->task_list);

  free(collection);
}


static void clause_print(clause *c) {
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
  printf("\n");
}

/*static void task_print(task *t) {
  printf("Task pos literal: ");
  alma_function_print(t->pos);
  printf("\nTask neg literal: ");
  alma_function_print(t->neg);
  printf("\nClause with pos literal:\n");
  clause_print(t->x);
  printf("Clause with neg literal:\n");
  clause_print(t->y);
  printf("\n");
}*/

/*static void print_map_entry(void *kind, void *arg) {
  char *str = kind;
  map_entry *entry = arg;
  printf("Map entry%s: %s\n", str, entry->predname);
  for (int i = 0; i < entry->num_clauses; i++) {
    clause_print(entry->clauses[i]);
    printf("\n");
  }
  printf("\n");
}*/

void kb_print(kb *collection) {
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->clauses); i++) {
    //printf("Clause %d\n", i);
    clause_print(tommy_array_get(&collection->clauses, i));
  }
  printf("\n");

  //tommy_list_foreach_arg(&collection->pos_list, print_map_entry, " pos");
  //tommy_list_foreach_arg(&collection->neg_list, print_map_entry, " neg");

  /*for (tommy_size_t i = 0; i < tommy_array_size(&collection->task_list); i++) {
    task_print(tommy_array_get(&collection->task_list, i));
  }*/
}

// Compare function to be used by tommy_hashlin_search
static int compare(const void *arg, const void *obj) {
  return strcmp((const char*)arg, ((const map_entry*)obj)->predname);
}

// Inserts clause into the hashmap with key name
// If the map entry already exists, append; otherwise create a new one
static void map_add_clause(tommy_hashlin *map, tommy_list *list, char *name, clause *c) {
  map_entry *result = tommy_hashlin_search(map, compare, name, tommy_hash_u64(0, name, strlen(name)));
  if (result != NULL) {
    result->num_clauses++;
    result->clauses = realloc(result->clauses, sizeof(clause*) * result->num_clauses);
    result->clauses[result->num_clauses-1] = c; // Aliases with pointer assignment
    free(name); // Name not added into hashmap must be freed (frees alloc in name_with_arity)
  }
  else {
    map_entry *entry = malloc(sizeof(map_entry));
    entry->predname = name;
    entry->num_clauses = 1;
    entry->clauses = malloc(sizeof(clause*));
    entry->clauses[0] = c; // Aliases with pointer assignment
    tommy_hashlin_insert(map, &entry->hash_node, entry, tommy_hash_u64(0, entry->predname, strlen(entry->predname)));
    tommy_list_insert_tail(list, &entry->list_node, entry);
  }
}

// Given "name" and integer arity, returns "name/arity"
// Allocated cstring is later placed in map_entry, and eventually freed by free_kb
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

// Returns alma_function pointer for literal with given name in clause
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

// Returns 0 functions are equal while respecting x and y matchings based on matches arg; otherwise returns 1
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
              matches->x = realloc(matches->x, sizeof(long long) * matches->count);
              matches->x[matches->count -1] = xval;
              matches->y = realloc(matches->y, sizeof(long long) * matches->count);
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
// Clause x must have no duplicate literals (ensured by earlier call to TODO on the clause)
static int clauses_differ(clause *x, clause *y) {
  if (x->pos_count == y->pos_count && x->neg_count == y->neg_count){
    var_matching matches;
    matches.count = 0;
    matches.x = NULL;
    matches.y = NULL;
    for (int i = 0; i < x->pos_count; i++) {
      alma_function* lit = literal_by_name(y, x->pos_lits[i]->name, 1);
      if (lit == NULL || functions_differ(x->pos_lits[i], lit, &matches))
        return release_matches(&matches, 1);
    }
    for (int i = 0; i < x->neg_count; i++) {
      alma_function* lit = literal_by_name(y, x->neg_lits[i]->name, 0);
      if (lit == NULL || functions_differ(x->neg_lits[i], lit, &matches))
        return release_matches(&matches, 1);
    }
    // All literals compared as equal; return 0
    return release_matches(&matches, 0);
  }
  return 1;
}

// Returns 0 if c is found to be a duplicate of a clause existing in collection; 1 otherwise
// See commends preceding clauses_differ function for further detail
static int duplicate_check(kb *collection, clause *c) {
  if (c->pos_count > 0) {
    // If clause has a positive literal, all duplicate candidates must have that same positive literal
    char *name = name_with_arity(c->pos_lits[0]->name, c->pos_lits[0]->term_count);
    map_entry *result = tommy_hashlin_search(&collection->pos_map, compare, name, tommy_hash_u64(0, name, strlen(name)));
    free(name);
    if (result != NULL) {
      for (int i = 0; i < result->num_clauses; i++) {
        if (!clauses_differ(c, result->clauses[i]))
          return 0;
      }
    }
  }
  else if (c->neg_count > 0) {
    // If clause has a negative literal, all duplicate candidates must have that same negative literal
    char *name = name_with_arity(c->neg_lits[0]->name, c->neg_lits[0]->term_count);
    map_entry *result = tommy_hashlin_search(&collection->neg_map, compare, name, tommy_hash_u64(0, name, strlen(name)));
    free(name);
    if (result != NULL) {
      for (int i = 0; i < result->num_clauses; i++) {
        if (!clauses_differ(c, result->clauses[i]))
          return 0;
      }
    }
  }
  return 1;
}

// Given a new clause, if it is not a duplicate is added them to the KB and maps
void add_new_clause(kb *collection, clause *c) {
  // TODO: call to duplicate literal filtering (add function and such)

  if (duplicate_check(collection, c)) {
    // Indexes clause into hashaps of KB collection
    for (int j = 0; j < c->pos_count; j++) {
      char *name = name_with_arity(c->pos_lits[j]->name, c->pos_lits[j]->term_count);
      map_add_clause(&collection->pos_map, &collection->pos_list, name, c);
    }
    for (int j = 0; j < c->neg_count; j++) {
      char *name = name_with_arity(c->neg_lits[j]->name, c->neg_lits[j]->term_count);
      map_add_clause(&collection->neg_map, &collection->neg_list, name, c);
    }

    tommy_array_insert(&collection->clauses, c);
  }
}

// Finds new tasks based on matching pos/neg predicate pairs, where one is from the KB and the other from clauses
// Tasks are added into the task_list of collection
// TODO: Refactor to remove code duplication
void tasks_from_clause(kb *collection, clause *c, int process_negatives) {
  // Iterate positive literals of clauses and match against negative literals of collection
  for (int j = 0; j < c->pos_count; j++) {
    char *name = name_with_arity(c->pos_lits[j]->name, c->pos_lits[j]->term_count);
    map_entry *result = tommy_hashlin_search(&collection->neg_map, compare, name, tommy_hash_u64(0, name, strlen(name)));
    // If a negative literal is found to match a positive literal, add tasks
    // New tasks are from Cartesian product of result's clauses with clauses' ith
    if (result != NULL) {
      for (int k = 0; k < result->num_clauses; k++) {
        if (c != result->clauses[k]) {
          alma_function *lit = literal_by_name(result->clauses[k], c->pos_lits[j]->name, 0);
          if (lit != NULL) {
            task *t = malloc(sizeof(task));
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
      map_entry *result = tommy_hashlin_search(&collection->pos_map, compare, name, tommy_hash_u64(0, name, strlen(name)));
      // If a positive literal is found to match a negative literal, add tasks
      // New tasks are from Cartesian product of result's clauses with clauses' ith
      if (result != NULL) {
        for (int k = 0; k < result->num_clauses; k++) {
          if (c != result->clauses[k]) {
            alma_function *lit = literal_by_name(result->clauses[k], c->neg_lits[j]->name, 1);
            if (lit != NULL) {
              task *t = malloc(sizeof(task));
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

// Given an MGU, substitute on literals other than pair unified and make a single resulting clause
// Last argument must be allocated by caller
void resolve(task *t, binding_list *mgu, clause *result) {
  //int max_pos = t->x->pos_count + t->y->pos_count - 1;
  //int max_neg = t->x->neg_count + t->y->neg_count - 1;
  result->pos_count = 0;
  if (t->x->pos_count + t->y->pos_count - 1 > 0) {
    result->pos_lits = malloc(sizeof(alma_function*) * (t->x->pos_count + t->y->pos_count - 1));

    // Copy positive literal from t->x and t->y
    for (int i = 0; i < t->x->pos_count; i++) {
      if (t->x->pos_lits[i] != t->pos) {
        alma_function *litcopy = malloc(sizeof(alma_function));
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
      alma_function *litcopy = malloc(sizeof(alma_function));
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
    result->neg_lits = malloc(sizeof(alma_function*) * (t->x->neg_count + t->y->neg_count - 1));

    // Copy negative literal from t->x and t->y
    for (int i = 0; i < t->y->neg_count; i++) {
      if (t->y->neg_lits[i] != t->neg) {
        alma_function *litcopy = malloc(sizeof(alma_function));
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
      alma_function *litcopy = malloc(sizeof(alma_function));
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

void forward_chain(kb *collection) {
  int chain = 1;
  int step = 0;
  while(chain) {
    tommy_array new_clauses;
    tommy_array_init(&new_clauses);

    for (tommy_size_t i = 0; i < tommy_array_size(&collection->task_list); i++) {
      task *current_task = tommy_array_get(&collection->task_list, i);

      // Given a task, attempt unification
      binding_list *theta = malloc(sizeof(binding_list));
      if (pred_unify(current_task->pos, current_task->neg, theta)) {
        // If successful, create clause for result of resolution and add to new_clauses
        clause *res_result = malloc(sizeof(clause));
        resolve(current_task, theta, res_result);

        // An empty resolution result is not a valid clause to add to KB
        if (res_result->pos_count > 0 || res_result->neg_count > 0) {
          tommy_array_insert(&new_clauses, res_result);
        }
        else {
          // TODO: Contradiction detection when you have empty resolution result!
          free(res_result);
        }
      }
      cleanup_bindings(theta);

      collection->task_count--;
      free(current_task);
    }
    tommy_array_done(&collection->task_list);
    tommy_array_init(&collection->task_list);

    if (tommy_array_size(&new_clauses) > 0) {
      printf("Step %d:\n", step);
      step++;

      // Get new tasks before adding new clauses into collection
      // TODO Fix problems with redundant calls to
      // TODO Fix issues with valgrind memory errors from
      // TODO Fix issue with not freeing certain duplicates which should be
      for (tommy_size_t i = 0; i < tommy_array_size(&new_clauses); i++) {
        clause *c = tommy_array_get(&new_clauses, i);
        if (duplicate_check(collection, c))
          tasks_from_clause(collection, c, 1);
      }
      // Insert new clauses derived
      for (tommy_size_t i = 0; i < tommy_array_size(&new_clauses); i++)
        add_new_clause(collection, tommy_array_get(&new_clauses, i));

      //kb_print(collection);
    }
    else {
      chain = 0;
      printf("Idling...\n");
    }
    tommy_array_done(&new_clauses);
  }
}
