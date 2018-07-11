#include <stdio.h>
#include <string.h>
#include "tommyds/tommyds/tommyhashlin.h"
#include "tommyds/tommyds/tommyhash.h"
#include "alma_kb.h"
#include "alma_formula.h"
#include "alma_unify.h"

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

// Adds a clause as another entry in the KB's clauses list
// To add to the hashmaps and lits, must separately use map_add_clause
void add_clause(clause *c, kb *collection) {
  if (collection->num_clauses == collection->reserved) {
    collection->reserved *= 2;
    collection->clauses = realloc(collection->clauses, sizeof(clause*) * collection->reserved);
  }
  collection->clauses[collection->num_clauses] = c;
  collection->num_clauses++;
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

    // Place in collection
    add_clause(c, collection);
  }
}

// Caller will need to free collection
// trees also must be freed by the caller after this call, as it is not deallocated here
void kb_init(alma_node *trees, int num_trees, kb **collection) {
  // Flatten trees into clause list
  *collection = malloc(sizeof(kb));
  kb *collec = *collection;
  collec->reserved = 8;
  collec->clauses = malloc(sizeof(clause*) * collec->reserved);
  collec->num_clauses = 0;
  for (int i = 0; i < num_trees; i++) {
    flatten_node(trees+i, collec);
  }

  // Generate maps and starting tasks
  tommy_hashlin_init(&collec->pos_map);
  tommy_list_init(&collec->pos_list);
  tommy_hashlin_init(&collec->neg_map);
  tommy_list_init(&collec->neg_list);
  maps_add(collec, collec->clauses, collec->num_clauses);

  tommy_list_init(&collec->task_list);
  get_tasks(collec, collec->clauses, collec->num_clauses, 0);
}

static void free_map_entry(void *arg) {
  map_entry *entry = arg;
  free(entry->predname);
  free(entry->clauses);
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in free_kb
  free(entry);
}

void free_kb(kb *collection) {
  for (int i = 0; i < collection->num_clauses; i++) {
    for (int j = 0; j < collection->clauses[i]->pos_count; j++) {
      free_function(collection->clauses[i]->pos_lits[j]);
    }
    for (int j = 0; j < collection->clauses[i]->neg_count; j++) {
      free_function(collection->clauses[i]->neg_lits[j]);
    }
    free(collection->clauses[i]->pos_lits);
    free(collection->clauses[i]->neg_lits);
    free(collection->clauses[i]);
  }
  free(collection->clauses);

  tommy_list_foreach(&collection->pos_list, free_map_entry);
  tommy_hashlin_done(&collection->pos_map);

  tommy_list_foreach(&collection->neg_list, free_map_entry);
  tommy_hashlin_done(&collection->neg_map);

  // Task pointers are aliases to those freed from collection->clauses, so only free overall task here
  tommy_list_foreach(&collection->task_list, free);

  free(collection);
}


static void clause_print(clause *c) {
  /*
  for (int i = 0; i < c->pos_count; i++) {
    printf("+");
    alma_function_print(c->pos_lits[i]);
    printf("\n");
  }
  for (int i = 0; i < c->neg_count; i++) {
    printf("-");
    alma_function_print(c->neg_lits[i]);
    printf("\n");
  }*/

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

/*static void print_task(void *arg) {
  task *t = arg;
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

void kb_print(kb *collection) {
  for (int i = 0; i < collection->num_clauses; i++) {
    //printf("Clause %d\n", i);
    clause_print(collection->clauses[i]);
  }
  printf("\n");

  //tommy_list_foreach_arg(&collection->pos_list, print_map_entry, " pos");
  //tommy_list_foreach_arg(&collection->neg_list, print_map_entry, " neg");

  //tommy_list_foreach(&collection->task_list, print_task);
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

// Adds to hashaps of KB collection based on the list of clauses provided
void maps_add(kb *collection, clause **clauses, int num_clauses) {
  for (int i = 0; i < num_clauses; i++) {
    for (int j = 0; j < clauses[i]->pos_count; j++) {
      char *name = name_with_arity(clauses[i]->pos_lits[j]->name, clauses[i]->pos_lits[j]->term_count);
      map_add_clause(&collection->pos_map, &collection->pos_list, name, clauses[i]);
    }
    for (int j = 0; j < clauses[i]->neg_count; j++) {
      char *name = name_with_arity(clauses[i]->neg_lits[j]->name, clauses[i]->neg_lits[j]->term_count);
      map_add_clause(&collection->neg_map, &collection->neg_list, name, clauses[i]);
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
void get_tasks(kb *collection, clause **new_clauses, int num_clauses, int process_negs) {
  for (int i = 0; i < num_clauses; i++) {
    // Iterate positive literals of new_clauses and match against negative literals of collection
    for (int j = 0; j < new_clauses[i]->pos_count; j++) {
      char *name = name_with_arity(new_clauses[i]->pos_lits[j]->name, new_clauses[i]->pos_lits[j]->term_count);
      map_entry *result = tommy_hashlin_search(&collection->neg_map, compare, name, tommy_hash_u64(0, name, strlen(name)));
      // If a negative literal is found to match a positive literal, add tasks
      // New tasks are from Cartesian product of result's clauses with new_clauses[i]
      if (result != NULL) {
        for (int k = 0; k < result->num_clauses; k++) {
          alma_function *lit = literal_by_name(result->clauses[k], new_clauses[i]->pos_lits[j]->name, 0);
          if (lit != NULL) {
            task *t = malloc(sizeof(task));
            t->x = new_clauses[i];
            t->y = result->clauses[k];
            t->pos = new_clauses[i]->pos_lits[j];
            t->neg = lit;
            tommy_list_insert_tail(&collection->task_list, &t->node, t);
          }
        }
      }
      free(name);
    }
    // Iterate negative literals of new_clauses and match against positive literals of collection
    // Only if process_negs flag; set to false in the case of new_clauses being same as KB's clauses (i.e. first task generation)
    if (process_negs) {
      for (int j = 0; j < new_clauses[i]->neg_count; j++) {
        char *name = name_with_arity(new_clauses[i]->neg_lits[j]->name, new_clauses[i]->neg_lits[j]->term_count);
        map_entry *result = tommy_hashlin_search(&collection->pos_map, compare, name, tommy_hash_u64(0, name, strlen(name)));
        // If a positive literal is found to match a negative literal, add tasks
        // New tasks are from Cartesian product of result's clauses with new_clauses[i]
        if (result != NULL) {
          for (int k = 0; k < result->num_clauses; k++) {
            alma_function *lit = literal_by_name(result->clauses[k], new_clauses[i]->pos_lits[j]->name, 0);
            if (lit != NULL) {
              task *t = malloc(sizeof(task));
              t->x = result->clauses[k];
              t->y = new_clauses[i];
              t->pos = lit;
              t->neg = new_clauses[i]->neg_lits[j];
              tommy_list_insert_tail(&collection->task_list, &t->node, t);
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
      }
    }
    for (int i = 0; i < t->y->pos_count; i++) {
      alma_function *litcopy = malloc(sizeof(alma_function));
      copy_alma_function(t->y->pos_lits[i], litcopy);
      for (int j = 0; j < litcopy->term_count; j++)
        subst(mgu, litcopy->terms+j);
      result->pos_lits[result->pos_count] = litcopy;
      result->pos_count++;
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
      }
    }
    for (int i = 0; i < t->x->neg_count; i++) {
      alma_function *litcopy = malloc(sizeof(alma_function));
      copy_alma_function(t->x->neg_lits[i], litcopy);
      for (int j = 0; j < litcopy->term_count; j++)
        subst(mgu, litcopy->terms+j);
      result->neg_lits[result->neg_count] = litcopy;
      result->neg_count++;
    }
  }
  else {
    result->neg_lits = NULL;
  }
  result->tag = NONE; // TODO: Deal with tags properly for fif/bif cases
}

// TODO: Incorporate print statments for demo as well
void forward_chain(kb *collection) {
  int num_new = 0;
  clause **new_clauses = NULL;
  int chain = 1;

  int step = 0;
  while(chain) {
    while (!tommy_list_empty(&collection->task_list)) {
      tommy_node *iter = tommy_list_head(&collection->task_list);
      while (iter) {
          task *current_task = iter->data;

          // Given a task, attempt unification
          binding_list *theta = malloc(sizeof(binding_list));
          if (pred_unify(current_task->pos, current_task->neg, theta)) {
            // If successful, create clause for result of resolution and add to new_clauses
            clause *res_result = malloc(sizeof(clause));
            resolve(current_task, theta, res_result);

            // An empty resolution result is not a valid clause to add to KB
            if (res_result->pos_count > 0 || res_result->neg_count > 0) {
              num_new++;
              // Reallocing the entire thing is based on the assumption of (fairly) few new clauses per step
              // Should this assumption significantly change, this must be altered
              new_clauses = realloc(new_clauses, sizeof(clause)*num_new);
              new_clauses[num_new-1] = res_result;
            }
            else {
              free(res_result);
            }
          }
          cleanup_bindings(theta);

          tommy_node *next = iter->next;
          tommy_list_remove_existing(&collection->task_list, iter);
          iter = next;
          free(current_task);
      }
    }

    if (num_new > 0) {
      maps_add(collection, new_clauses, num_new);
      for (int i = 0; i < num_new; i++) {
        add_clause(new_clauses[i], collection);
      }
      get_tasks(collection, new_clauses, num_new, 1);
      free(new_clauses);
      num_new = 0;
      new_clauses = NULL;

      printf("Step %d KB:\n", step);
      step++;
      kb_print(collection);
    }
    else {
      chain = 0;
      printf("Idling...\n");
    }
  }
}
