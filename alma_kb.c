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
        c->neg_lits = realloc(c->neg_lits, c->neg_count*sizeof(alma_function*));
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
      c->pos_lits = realloc(c->pos_lits, c->pos_count*sizeof(alma_function*));
      c->pos_lits[c->pos_count-1] = node->predicate;
      node->predicate = NULL;
    }
  }
}

// Flattens a single alma node and adds its contents to collection
// Recursively called by flatten function per node
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
    if (collection->num_clauses == collection->reserved) {
      collection->reserved *= 2;
      collection->clauses = realloc(collection->clauses, sizeof(clause*) * collection->reserved);
    }
    collection->clauses[collection->num_clauses] = c;
    collection->num_clauses++;
  }
}

// Caller will need to free collection
// trees also must be freed by the caller after this call, as it is not deallocated here
void kb_init(alma_node *trees, int num_trees, kb **collection) {
  // Flatten trees into clause list
  *collection = malloc(sizeof(kb));
  (*collection)->reserved = 8;
  (*collection)->clauses = malloc(sizeof(clause*) * (*collection)->reserved);
  (*collection)->num_clauses = 0;
  for (int i = 0; i < num_trees; i++) {
    flatten_node(trees+i, *collection);
  }

  // Generate maps and starting tasks
  populate_maps(*collection);
  get_tasks(*collection, (*collection)->clauses, (*collection)->num_clauses);
}

/*static void print_map_entry(void *kind, void *arg) {
  char *str = kind;
  map_entry *entry = arg;
  printf("Map entry%s: %s\n", str, entry->predname);
  for (int i = 0; i < entry->num_clauses; i++) {
    for (int j = 0; j < entry->clauses[i]->pos_count; j++) {
      printf("+");
      alma_function_print(entry->clauses[i]->pos_lits[j]);
      printf("\n");
    }
    for (int j = 0; j < entry->clauses[i]->neg_count; j++) {
      printf("-");
      alma_function_print(entry->clauses[i]->neg_lits[j]);
      printf("\n");
    }
    printf("\n");
  }
  printf("\n");
}*/

static void free_map_entry(void *arg) {
  map_entry *entry = arg;
  free(entry->predname);
  free(entry->clauses);
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in free_kb
  free(entry);
}

void free_kb(kb *collection) {
  //tommy_list_foreach_arg(&collection->pos_list, print_map_entry, " pos");
  //tommy_list_foreach_arg(&collection->neg_list, print_map_entry, " neg");

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

  //tommy_hashlin_foreach(&collection->pos_map, free_map_entry);
  tommy_list_foreach(&collection->pos_list, free_map_entry);
  tommy_hashlin_done(&collection->pos_map);

  //tommy_hashlin_foreach(&collection->neg_map, free_map_entry);
  tommy_list_foreach(&collection->neg_list, free_map_entry);
  tommy_hashlin_done(&collection->neg_map);

  // TODO: Free tasks

  free(collection);
}


void kb_print(kb *collection) {
  for (int i = 0; i < collection->num_clauses; i++) {
    printf("Clause %d\n", i);
    for (int j = 0; j < collection->clauses[i]->pos_count; j++) {
      printf("+");
      alma_function_print(collection->clauses[i]->pos_lits[j]);
      printf("\n");
    }
    for (int j = 0; j < collection->clauses[i]->neg_count; j++) {
      printf("-");
      alma_function_print(collection->clauses[i]->neg_lits[j]);
      printf("\n");
    }
  }
}


// Initializes hashmaps pos_map and neg_map for KB based on the starting sets of clauses
void populate_maps(kb *collection) {
  tommy_hashlin_init(&collection->pos_map);
  tommy_list_init(&collection->pos_list);
  tommy_hashlin_init(&collection->neg_map);
  tommy_list_init(&collection->neg_list);
  maps_add(collection, collection->clauses, collection->num_clauses);
}

// Compare function to be used by tommy_hashlin_search
static int compare(const void *arg, const void *obj) {
  return strcmp((const char*)arg, ((const map_entry*)obj)->predname);
}

// Inserts clause into the hashmap with key name
// If the map entry already exists, append; otherwise create a new one
static void add_clause(tommy_hashlin *map, tommy_list *list, char *name, clause *c) {
  map_entry *result = tommy_hashlin_search(map, compare, name, tommy_hash_u64(0, name, strlen(name)));
  if (result != NULL) {
    result->num_clauses++;
    result->clauses = realloc(result->clauses, sizeof(clause*)*result->num_clauses);
    result->clauses[result->num_clauses-1] = c; // Aliases with pointer assignment
  }
  else {
    map_entry *entry = malloc(sizeof(map_entry));
    entry->predname = malloc(sizeof(char) *(strlen(name)+1));
    strcpy(entry->predname, name);
    entry->num_clauses = 1;
    entry->clauses = malloc(sizeof(clause*));
    entry->clauses[0] = c; // Aliases with pointer assignment
    tommy_hashlin_insert(map, &entry->hash_node, entry, tommy_hash_u64(0, entry->predname, strlen(entry->predname)));
    tommy_list_insert_tail(list, &entry->list_node, entry);
  }
}

// Adds to hashaps of KB collection based on the list of clauses provided
void maps_add(kb *collection, clause **clauses, int num_clauses) {
  for (int i = 0; i < num_clauses; i++) {
    for (int j = 0; j < clauses[i]->pos_count; j++) {
      add_clause(&collection->pos_map, &collection->pos_list, clauses[i]->pos_lits[j]->name, clauses[i]);
    }
    for (int j = 0; j < clauses[i]->neg_count; j++) {
      add_clause(&collection->neg_map, &collection->neg_list, clauses[i]->neg_lits[j]->name, clauses[i]);
    }
  }
}

// Finds new tasks based on matching pos/neg predicate pairs, where one is from the KB and the other from new_clauses
// Tasks are added into the task_list of collection
void get_tasks(kb *collection, clause **new_clauses, int num_clauses) {

}


void resolve(task *t, binding_list *mgu, clause *result) {

}


void forward_chain(kb *collection) {

}
