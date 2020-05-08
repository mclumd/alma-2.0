#include <stdlib.h>
#include "alma_command.h"
#include "tommy.h"
#include "alma_formula.h"
#include "alma_backsearch.h"
#include "alma_fif.h"
#include "alma_parser.h"
#include "alma_print.h"

extern char logs_on;
extern char python_mode;

// Caller will need to free collection with kb_halt
void kb_init(kb **collection, char *file, char *agent, int verbose, kb_str *buf) {
  // Allocate and initialize
  *collection = malloc(sizeof(**collection));
  kb *collec = *collection;

  collec->verbose = verbose;

  collec->size = 0;
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
      nodes_to_clauses(trees, num_trees, &collec->new_clauses, 0, buf);
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
    assert_formula(collec, sentence, 0, buf);
    free(sentence);
  }
  assert_formula(collec, "now(1).", 0, buf);
  collec->wallprev = walltime();
  assert_formula(collec, collec->wallprev, 0, buf);

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
      if (tommy_array_size(&bt->to_resolve) > 0)
        return 0;
      i = i->next;
    }
    return 1;
  }
  return 0;
}

void kb_step(kb *collection, kb_str *buf) {
  collection->time++;

  process_res_tasks(collection, &collection->res_tasks, &collection->new_clauses, NULL, buf);
  process_fif_tasks(collection);
  process_backsearch_tasks(collection, buf);

  collection->now_str = now(collection->time);
  assert_formula(collection, collection->now_str, 0, buf);
  collection->wallnow = walltime();
  assert_formula(collection, collection->wallnow, 0, buf);

  fif_to_front(&collection->new_clauses);
  // Insert new clauses derived that are not duplicates
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->new_clauses); i++) {
    clause *c = tommy_array_get(&collection->new_clauses, i);
    clause *dupe = duplicate_check(collection, c);
    int reinstate = c->pos_count == 1 && c->neg_count == 0 && strcmp(c->pos_lits[0]->name, "reinstate") == 0 && c->pos_lits[0]->term_count == 1;
    //    tee_alt("step iter %d/%d", buf, (int)i,(int)tommy_array_size(&collection->new_clauses)); //CMAXEY
    if (dupe == NULL || reinstate) {
      if (c->tag == FIF)
        fif_task_map_init(collection, c);

      if (reinstate) {
         alma_term *arg = c->pos_lits[0]->terms;
         if (arg->type == FUNCTION && arg->function->term_count == 0) {
           char *index_str = arg->function->name;
           int digits = 1;
           for (int j = 0; j < strlen(index_str); j++) {
             if (!isdigit(index_str[j])) {
               digits = 0;
               break;
             }
           }
           if (digits) {
             long index = atol(index_str);
             index_mapping *result = tommy_hashlin_search(&collection->index_map, im_compare, &index, tommy_hash_u64(0, &index, sizeof(index)));
             if (result != NULL && is_distrusted(collection, index)) {
               clause *to_reinstate = result->value;
               clause *reinstatement = malloc(sizeof(*reinstatement));
               copy_clause_structure(to_reinstate, reinstatement);
               reinstatement->parent_set_count = to_reinstate->parent_set_count;
               if (reinstatement->parent_set_count > 0) {
                 reinstatement->parents = malloc(sizeof(*reinstatement->parents)*reinstatement->parent_set_count);
                 for (int j = 0; j < reinstatement->parent_set_count; j++) {
                   reinstatement->parents[j].count = to_reinstate->parents[j].count;
                   reinstatement->parents[j].clauses = malloc(sizeof(*reinstatement->parents[j].clauses)*to_reinstate->parents[j].count);
                   for (int k = 0; k < reinstatement->parents[j].count; k++)
                     reinstatement->parents[j].clauses[k] = to_reinstate->parents[j].clauses[k];
                 }
               }
               tommy_array_insert(&collection->new_clauses, reinstatement);
             }
           }
         }
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
          distrust_recursive(collection, c, time_str, buf);
          free(time_str);
        }
      }
    }
    else {
      if (collection->verbose) {
        tee_alt("-a: Duplicate clause ", buf);
        clause_print(c, buf);
        tee_alt(" merged into %ld\n", buf, dupe->index);
      }

      if (c->parents != NULL)
        transfer_parent(collection, dupe, c, 1, buf);

      free_clause(c);
    }
  }

  // Generate new backsearch tasks
  tommy_node *i = tommy_list_head(&collection->backsearch_tasks);
  while (i) {
    generate_backsearch_tasks(collection, i->data, buf);
    i = i->next;
  }

  if (collection->prev_str != NULL) {
    delete_formula(collection, collection->prev_str, 0, buf);
    free(collection->prev_str);
  }
  else
    delete_formula(collection, "now(1).", 0, buf);
  collection->prev_str = collection->now_str;
  delete_formula(collection, collection->wallprev, 0, buf);
  free(collection->wallprev);
  collection->wallprev = collection->wallnow;

  collection->idling = idling_check(collection);

  if (collection->idling)
    tee_alt("-a: Idling...\n", buf);

  tommy_array_done(&collection->new_clauses);
  tommy_array_init(&collection->new_clauses);
}

void kb_print(kb *collection, kb_str *buf) {
  //  char temp_buf[1000];
  //  tee_alt("in kb_print:\n",buf);

  //  sprintf(temp_buf," %p:%p",(void *)buf,(void *)temp);
  //  strcpy(&(buf->buffer[buf->size]),temp_buf);
  //  buf->size += strlen(temp_buf);

  tommy_node *i = tommy_list_head(&collection->clauses);
  while (i) {
    index_mapping *data = i->data;
    tee_alt("%ld: ", buf, data->key);
    clause_print(data->value, buf);
    tee_alt("\n", buf);
    i = i->next;
  }

  i = tommy_list_head(&collection->backsearch_tasks);
  if (i) {
    tee_alt("Back searches:\n", buf);
    for (int count = 0; i != NULL; i = i->next, count++) {
      tee_alt("%d\n", buf, count);
      backsearch_task *t = i->data;
      for (tommy_size_t j = 0; j < tommy_array_size(&t->clauses); j++) {
        clause *c = tommy_array_get(&t->clauses, j);
        tee_alt("%ld: ", buf, c->index);
        clause_print(c, buf);
        binding_mapping *m = tommy_hashlin_search(&t->clause_bindings, bm_compare, &c->index, tommy_hash_u64(0, &c->index, sizeof(c->index)));
        if (m != NULL) {
          tee_alt(" (bindings: ", buf);
          print_bindings(m->bindings, buf);
          tee_alt(")", buf);
        }
        tee_alt("\n", buf);
      }
    }
  }
  tee_alt("\n", buf);
  if (logs_on) {
    fflush(almalog);
  }
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

  if (logs_on) {
    fclose(almalog);
  }
}

void kb_assert(kb *collection, char *string, kb_str *buf) {
  assert_formula(collection, string, 1, buf);
}

void kb_remove(kb *collection, char *string, kb_str *buf) {
  delete_formula(collection, string, 1, buf);
}

void kb_update(kb *collection, char *string, kb_str *buf) {
  update_formula(collection, string, buf);
}

void kb_observe(kb *collection, char *string, kb_str *buf) {
  // Parse string into clauses
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas)) {
    tommy_array arr;
    tommy_array_init(&arr);
    nodes_to_clauses(formulas, formula_count, &arr, 0, buf);

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
        tee_alt("-a: ", buf);
        clause_print(c, buf);
        tee_alt(" observed\n", buf);
      }
      else {
        clause_print(c, buf);
        tee_alt(" has too many literals\n", buf);
        free_clause(c);
      }
    }
    tommy_array_done(&arr);
  }
}

void kb_backsearch(kb *collection, char *string, kb_str *buf) {
  // Parse string into clauses
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas)) {
    tommy_array arr;
    tommy_array_init(&arr);
    nodes_to_clauses(formulas, formula_count, &arr, 0, buf);

    clause *c = tommy_array_get(&arr, 0);
    // Free all after first
    for (int i = 1; i < tommy_array_size(&arr); i++)
      free_clause(tommy_array_get(&arr, i));
    tommy_array_done(&arr);
    if (c != NULL) {
      if (c->pos_count + c->neg_count > 1) {
        tee_alt("-a: query clause has too many literals\n", buf);
        free_clause(c);
      }
      else {
        collection->idling = 0;
        backsearch_from_clause(collection, c);
        tee_alt("-a: Backsearch initiated for ", buf);
        clause_print(c, buf);
        tee_alt("\n", buf);
      }
    }
  }
}
