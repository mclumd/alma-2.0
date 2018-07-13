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
static void flatten_node(alma_node *node, kb *collection) {
  if (node->type == FOL && node->fol->op == AND) {
    flatten_node(node->fol->arg1, collection);
    flatten_node(node->fol->arg2, collection);
  }
  else {
    clause *c = malloc(sizeof(clause));
    c->pos_count = c->neg_count = 0;
    c->pos_lits = NULL;
    c->neg_lits = NULL;
    c->tag = NONE;
    make_clause(node, c);
    set_variable_ids(c);

    tommy_array_insert(&collection->clauses, c);
  }
}

// Returns 0 if c is found to be a duplicate of a clause existing in collection; 1 otherwise
/*static int duplicate_check(kb *collection, clause *c) {
  // TODO
  return 1;
}*/

// Caller will need to free collection
// trees also must be freed by the caller after this call, as it is not deallocated here
void kb_init(alma_node *trees, int num_trees, kb **collection) {
  // Flatten trees into clause list
  *collection = malloc(sizeof(kb));
  kb *collec = *collection;
  tommy_array_init(&collec->clauses); // TODO: Place in tommy_array to use (new) function for adding from
  for (int i = 0; i < num_trees; i++) {
    flatten_node(trees+i, collec);
  }

  // Generate maps and starting tasks
  tommy_hashlin_init(&collec->pos_map);
  tommy_list_init(&collec->pos_list);
  tommy_hashlin_init(&collec->neg_map);
  tommy_list_init(&collec->neg_list);
  maps_add(collec, &collec->clauses);

  tommy_array_init(&collec->task_list);
  collec->task_count = 0;
  get_tasks(collec, &collec->clauses);
}

static void free_map_entry(void *arg) {
  map_entry *entry = arg;
  free(entry->predname);
  free(entry->clauses);
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in free_kb
  free(entry);
}

void free_kb(kb *collection) {
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->clauses); i++) {
    clause *curr = tommy_array_get(&collection->clauses, i);
    for (int j = 0; j < curr->pos_count; j++)
      free_function(curr->pos_lits[j]);
    for (int j = 0; j < curr->neg_count; j++)
      free_function(curr->neg_lits[j]);
    free(curr->pos_lits);
    free(curr->neg_lits);
    free(curr);
    // Remove from tommy_array?
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

// Adds to hashaps of KB collection based on the tommy_array of clauses provided
void maps_add(kb *collection, tommy_array *clauses) {
  for (tommy_size_t i = 0; i < tommy_array_size(clauses); i++) {
    clause *curr = tommy_array_get(clauses, i);
    for (int j = 0; j < curr->pos_count; j++) {
      char *name = name_with_arity(curr->pos_lits[j]->name, curr->pos_lits[j]->term_count);
      map_add_clause(&collection->pos_map, &collection->pos_list, name, curr);
    }
    for (int j = 0; j < curr->neg_count; j++) {
      char *name = name_with_arity(curr->neg_lits[j]->name, curr->neg_lits[j]->term_count);
      map_add_clause(&collection->neg_map, &collection->neg_list, name, curr);
    }
  }
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

// Finds new tasks based on matching pos/neg predicate pairs, where one is from the KB and the other from new_clauses
// Tasks are added into the task_list of collection
// TODO: Rewrite to avoid duplicates in task list (currently ignored for convenience)
// TODO: Refactor to remove code duplication
void get_tasks(kb *collection, tommy_array *new_clauses) {
  for (tommy_size_t i = 0; i < tommy_array_size(new_clauses); i++) {
    clause *curr = tommy_array_get(new_clauses, i);
    // Iterate positive literals of new_clauses and match against negative literals of collection
    for (int j = 0; j < curr->pos_count; j++) {
      char *name = name_with_arity(curr->pos_lits[j]->name, curr->pos_lits[j]->term_count);
      map_entry *result = tommy_hashlin_search(&collection->neg_map, compare, name, tommy_hash_u64(0, name, strlen(name)));
      // If a negative literal is found to match a positive literal, add tasks
      // New tasks are from Cartesian product of result's clauses with new_clauses' ith
      if (result != NULL) {
        for (int k = 0; k < result->num_clauses; k++) {
          alma_function *lit = literal_by_name(result->clauses[k], curr->pos_lits[j]->name, 0);
          if (lit != NULL) {
            task *t = malloc(sizeof(task));
            t->x = curr;
            t->pos = curr->pos_lits[j];
            t->y = result->clauses[k];
            t->neg = lit;
            tommy_array_insert(&collection->task_list, t);
            collection->task_count++;
          }
        }
      }
      free(name);
    }
    // Iterate negative literals of new_clauses and match against positive literals of collection
    // Only done if new_clauses differ from KB's clauses (i.e. after first task generation)
    if (new_clauses != &collection->clauses) {
      for (int j = 0; j < curr->neg_count; j++) {
        char *name = name_with_arity(curr->neg_lits[j]->name, curr->neg_lits[j]->term_count);
        map_entry *result = tommy_hashlin_search(&collection->pos_map, compare, name, tommy_hash_u64(0, name, strlen(name)));
        // If a positive literal is found to match a negative literal, add tasks
        // New tasks are from Cartesian product of result's clauses with new_clauses' ith
        if (result != NULL) {
          for (int k = 0; k < result->num_clauses; k++) {
            alma_function *lit = literal_by_name(result->clauses[k], curr->neg_lits[j]->name, 1);
            if (lit != NULL) {
              task *t = malloc(sizeof(task));
              t->x = result->clauses[k];
              t->pos = lit;
              t->y = curr;
              t->neg = curr->neg_lits[j];
              tommy_array_insert(&collection->task_list, t);
              collection->task_count++;
            }
          }
        }
        free(name);
      }
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
      get_tasks(collection, &new_clauses);
      // Insert new clauses derived
      maps_add(collection, &new_clauses);
      for (tommy_size_t i = 0; i < tommy_array_size(&new_clauses); i++) {
        tommy_array_insert(&collection->clauses, tommy_array_get(&new_clauses, i));
      }

      kb_print(collection);
    }
    else {
      chain = 0;
      printf("Idling...\n");
    }
    tommy_array_done(&new_clauses);
  }
}
