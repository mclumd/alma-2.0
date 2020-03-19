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
  collec->prev_str = NULL;
  collec->now_str = NULL;
  collec->wallnow = NULL;
  collec->wallprev = NULL;
  collec->idling = 0;
  tommy_array_init(&collec->new_clauses);
  tommy_list_init(&collec->clauses);
  tommy_hashlin_init(&collec->index_map);
  tommy_hashlin_init(&collec->pos_map);
  tommy_list_init(&collec->pos_list);
  tommy_hashlin_init(&collec->neg_map);
  tommy_list_init(&collec->neg_list);
  tommy_hashlin_init(&collec->fif_map);
  //tommy_array_init(&collec->res_tasks);
  res_task_heap_init(&(collec->res_tasks));
  collec->res_tasks_idx = 0;
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
  assert_formula(collec, "now(1).", 0);
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
static int idling_check(kb *collection) {
  if (tommy_array_size(&collection->new_clauses) <= 2) {
    tommy_node *i = tommy_list_head(&collection->backsearch_tasks);
    while (i) {
      backsearch_task *bt = i->data;
      //if (tommy_array_size(&bt->to_resolve) > 0)
      if (  (bt->to_resolve.count) > 0)
        return 0;
      i = i->next;
    }
    return 1;
  }
  return 0;
}

// Step through the KB.  If singleton==1, only process the highest priority resolution task.
void kb_step(kb *collection, int singleton) {
  collection->time++;

  if (!singleton) {
    process_res_tasks(collection, &collection->res_tasks, &collection->new_clauses, NULL);
  } else{
    process_single_res_task(collection, &collection->res_tasks, &collection->new_clauses, NULL);
  }
  process_fif_tasks(collection);
  process_backsearch_tasks(collection);

  collection->now_str = now(collection->time);
  assert_formula(collection, collection->now_str, 0);
  collection->wallnow = walltime();
  assert_formula(collection, collection->wallnow, 0);

  fif_to_front(&collection->new_clauses);
  // Insert new clauses derived that are not duplicates
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->new_clauses); i++) {
    clause *c = tommy_array_get(&collection->new_clauses, i);
    clause *dupe = duplicate_check(collection, c);
    if (dupe == NULL) {
      if (c->tag == FIF)
        fif_task_map_init(collection, c);

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

      add_clause(collection, c);

      if (c->parents != NULL) {
        int distrust = 0;
        // Update child info for parents of new clause, check for distrusted parents
        for (int j = 0; j < c->parents[0].count; j++) {
          add_child(c->parents[0].clauses[j], c);
          if (is_distrusted(collection, c->parents[0].clauses[j]->index))
            distrust = 1;
        }
        if (distrust) {
          char *time_str = long_to_str(collection->time);
          distrust_recursive(collection, c, time_str);
          free(time_str);
        }
      }
    }
    else {
      if (collection->verbose) {
        tee("-a: Duplicate clause ");
        clause_print(c);
        tee(" merged into %ld\n", dupe->index);
      }

      if (c->parents != NULL)
        transfer_parent(collection, dupe, c, 1);

      free_clause(c);
    }
  }

  // Generate new backsearch tasks
  tommy_node *i = tommy_list_head(&collection->backsearch_tasks);
  while (i) {
    generate_backsearch_tasks(collection, i->data);
    i = i->next;
  }

  if (collection->prev_str != NULL) {
    delete_formula(collection, collection->prev_str, 0);
    free(collection->prev_str);
  }
  else
    delete_formula(collection, "now(1).", 0);
  collection->prev_str = collection->now_str;
  delete_formula(collection, collection->wallprev, 0);
  free(collection->wallprev);
  collection->wallprev = collection->wallnow;

  collection->idling = idling_check(collection);

  if (collection->idling)
    tee("-a: Idling...\n");

  tommy_array_done(&collection->new_clauses);
  tommy_array_init(&collection->new_clauses);
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
}

void kb_halt(kb *collection) {
  // now_str and prev_str alias at this point, only free one
  free(collection->now_str);
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

  // TODO:  Modify this for res_tasks as a heap.  
  /*for (tommy_size_t i = 0; i < tommy_array_size(&collection->res_tasks); i++)
    free(tommy_array_get(&collection->res_tasks, i));
  tommy_array_done(&collection->res_tasks);*/

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
