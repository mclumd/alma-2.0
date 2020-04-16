#include <stdlib.h>
#include "alma_command.h"
#include "tommy.h"
#include "alma_formula.h"
#include "alma_backsearch.h"
#include "alma_fif.h"
#include "alma_parser.h"
#include "alma_print.h"

// Caller will need to free collection with kb_halt
void kb_init(kb **collection, char *file, char *agent, int verbose) {
  // Allocate and initialize
  *collection = malloc(sizeof(**collection));
  kb *collec = *collection;

  collec->verbose = verbose;

  collec->time = 1;
  collec->prev = collec->now = NULL;
  collec->wallnow = collec->wallprev = NULL;
  collec->idling = 0;
  tommy_array_init(&collec->new_clauses);
  tommy_list_init(&collec->clauses);
  tommy_hashlin_init(&collec->index_map);
  tommy_hashlin_init(&collec->pos_map);
  tommy_list_init(&collec->pos_list);
  tommy_hashlin_init(&collec->neg_map);
  tommy_list_init(&collec->neg_list);
  tommy_hashlin_init(&collec->fif_map);
  tommy_array_init(&collec->res_tasks);
  tommy_hashlin_init(&collec->fif_tasks);
  tommy_list_init(&collec->backsearch_tasks);
  tommy_hashlin_init(&collec->distrusted);

  parse_init();

  // Given a file argument, obtain other initial clauses from
  if (file != NULL) {
    alma_node *trees;
    int num_trees;

    if (formulas_from_source(file, 1, &num_trees, &trees)) {
      nodes_to_clauses(trees, num_trees, &collec->new_clauses, 0);
      fif_to_front(&collec->new_clauses);
    }
    // If file cannot parse, cleanup and exit
    else {
      kb_halt(collec);
      exit(0);
    }
  }
  if (agent != NULL) {
    int namelen = strlen(agent);
    char *sentence = malloc(10 + namelen + 3);
    strcpy(sentence, "agentname(");
    strcpy(sentence + 10, agent);
    strcpy(sentence + 10 + namelen, ").");
    assert_formula(collec, sentence, 0);
    free(sentence);
  }

  collec->prev = now(collec->time);
  assert_formula(collec, collec->prev, 0);
  collec->wallprev = walltime();
  assert_formula(collec, collec->wallprev, 0);

  // Insert starting clauses
  for (tommy_size_t i = 0; i < tommy_array_size(&collec->new_clauses); i++) {
    clause *c = tommy_array_get(&collec->new_clauses, i);
    // Insert into KB if not duplicate
    if (duplicate_check(collec, c) == NULL)
      add_clause(collec, c);
    else
      free_clause(c);
  }
  tommy_array_done(&collec->new_clauses);
  tommy_array_init(&collec->new_clauses);

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

// System idles if there are no resolution tasks, backsearch tasks, or to_unify fif values
// First of these is true when no new clauses (source of res tasks) exist
// To_unify values are all removed in current implementation each step from exhaustive fif
static int idling_check(kb *collection, int new_clauses) {
  if (!new_clauses) {
    tommy_node *i = tommy_list_head(&collection->backsearch_tasks);
    while (i) {
      backsearch_task *bt = i->data;
      if (tommy_array_size(&bt->to_resolve) > 0)
        return 0;
      i = i->next;
    }
    return 1;
  }
  return 0;
}

void kb_step(kb *collection) {
  collection->time++;

  process_res_tasks(collection, &collection->res_tasks, &collection->new_clauses, NULL);
  process_fif_tasks(collection);
  process_backsearch_tasks(collection);

  int newc = tommy_array_size(&collection->new_clauses) > 0;
  process_new_clauses(collection);

  // New clock rules go last
  collection->now = now(collection->time);
  assert_formula(collection, collection->now, 0);
  collection->wallnow = walltime();
  assert_formula(collection, collection->wallnow, 0);
  delete_formula(collection, collection->prev, 0);
  free(collection->prev);
  collection->prev = collection->now;
  delete_formula(collection, collection->wallprev, 0);
  free(collection->wallprev);
  collection->wallprev = collection->wallnow;
  process_new_clauses(collection);

  tommy_node *i = tommy_list_head(&collection->backsearch_tasks);
  while (i) {
    generate_backsearch_tasks(collection, i->data);
    i = i->next;
  }

  collection->idling = idling_check(collection, newc);
  if (collection->idling)
    tee("-a: Idling...\n");
}

void kb_print(kb *collection) {
  tommy_node *i = tommy_list_head(&collection->clauses);
  while (i) {
    index_mapping *data = i->data;
    tee("%ld: ", data->key);
    clause_print(data->value);
    tee("\n");
    i = i->next;
  }

  i = tommy_list_head(&collection->backsearch_tasks);
  if (i) {
    tee("Back searches:\n");
    for (int count = 0; i != NULL; i = i->next, count++) {
      tee("%d\n", count);
      backsearch_task *t = i->data;
      for (tommy_size_t j = 0; j < tommy_array_size(&t->clauses); j++) {
        clause *c = tommy_array_get(&t->clauses, j);
        tee("%ld: ", c->index);
        clause_print(c);
        binding_mapping *m = tommy_hashlin_search(&t->clause_bindings, bm_compare, &c->index, tommy_hash_u64(0, &c->index, sizeof(c->index)));
        if (m != NULL) {
          tee(" (bindings: ");
          print_bindings(m->bindings);
          tee(")");
        }
        tee("\n");
      }
    }
  }
  tee("\n");
  fflush(almalog);
}

void kb_halt(kb *collection) {
  // now and prev alias at this point, only free one
  free(collection->now);
  if (collection->wallnow == NULL)
    free(collection->wallprev);
  free(collection->wallnow);

  for (tommy_size_t i = 0; i < tommy_array_size(&collection->new_clauses); i++)
    free_clause(tommy_array_get(&collection->new_clauses, i));
  tommy_array_done(&collection->new_clauses);

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
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->res_tasks); i++)
    free(tommy_array_get(&collection->res_tasks, i));
  tommy_array_done(&collection->res_tasks);

  tommy_hashlin_foreach(&collection->fif_tasks, free_fif_task_mapping);
  tommy_hashlin_done(&collection->fif_tasks);

  curr = tommy_list_head(&collection->backsearch_tasks);
  while (curr) {
    backsearch_task *data = curr->data;
    curr = curr->next;
    backsearch_halt(data);
  }

  tommy_hashlin_foreach(&collection->distrusted, free);
  tommy_hashlin_done(&collection->distrusted);

  free(collection);

  parse_cleanup();

  fclose(almalog);
}

void kb_assert(kb *collection, char *string) {
  assert_formula(collection, string, 1);
}

void kb_remove(kb *collection, char *string) {
  delete_formula(collection, string, 1);
}

void kb_update(kb *collection, char *string) {
  update_formula(collection, string);
}

void kb_observe(kb *collection, char *string) {
  // Parse string into clauses
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas)) {
    tommy_array arr;
    tommy_array_init(&arr);
    nodes_to_clauses(formulas, formula_count, &arr, 0);

    for (int i = 0; i < tommy_array_size(&arr); i++) {
      clause *c = tommy_array_get(&arr, i);
      if (c->pos_count + c->neg_count == 1) {
        alma_function *lit = (c->pos_count == 1) ? c->pos_lits[0] : c->neg_lits[0];
        lit->term_count++;
        lit->terms = realloc(lit->terms, sizeof(*lit->terms)*lit->term_count);
        alma_term time_term;
        time_term.type = FUNCTION;
        time_term.function = malloc(sizeof(*time_term.function));
        time_term.function->name = long_to_str(collection->time);
        time_term.function->term_count = 0;
        time_term.function->terms = NULL;
        lit->terms[lit->term_count-1] = time_term;
        tommy_array_insert(&collection->new_clauses, c);
        tee("-a: ");
        clause_print(c);
        tee(" observed\n");
      }
      else {
        clause_print(c);
        tee(" has too many literals\n");
        free_clause(c);
      }
    }
    tommy_array_done(&arr);
  }
}

void kb_backsearch(kb *collection, char *string) {
  // Parse string into clauses
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas)) {
    tommy_array arr;
    tommy_array_init(&arr);
    nodes_to_clauses(formulas, formula_count, &arr, 0);

    clause *c = tommy_array_get(&arr, 0);
    // Free all after first
    for (int i = 1; i < tommy_array_size(&arr); i++)
      free_clause(tommy_array_get(&arr, i));
    tommy_array_done(&arr);
    if (c != NULL) {
      if (c->pos_count + c->neg_count > 1) {
        tee("-a: query clause has too many literals\n");
        free_clause(c);
      }
      else {
        collection->idling = 0;
        backsearch_from_clause(collection, c);
        tee("-a: Backsearch initiated for ");
        clause_print(c);
        tee("\n");
      }
    }
  }
}
