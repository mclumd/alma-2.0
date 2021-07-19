#include <string.h>
#include "alma_fif.h"
#include "alma_proc.h"
#include "alma_print.h"
#include "alma_unify.h"

// Given a new fif clause, initializes fif task mappings held by fif_tasks for each premise of c
// Also places single fif_task into fif_task_mapping for first premise
void fif_task_map_init(kb *collection, clause *c, int init_to_unify) {
  if (c->tag == FIF) {
    for (int i = 0; i < c->fif->premise_count; i++) {
      alma_function *f = fif_access(c, i);

      // Don't make task mappings for middle proc premises
      if (i > 0 && is_proc(f, collection))
        continue;
      char *name = name_with_arity(f->name, f->term_count);
      fif_task_mapping *result = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, name, tommy_hash_u64(0, name, strlen(name)));

      // If a task map entry doesn't exist for name of a literal, create one
      if (result == NULL) {
        result = malloc(sizeof(*result));
        result->predname = malloc(strlen(name)+1);
        strcpy(result->predname, name);
        tommy_list_init(&result->tasks);
        tommy_hashlin_insert(&collection->fif_tasks, &result->node, result, tommy_hash_u64(0, result->predname, strlen(result->predname)));
      }

      // For first premise, initialze fif_task root for c
      if (i == 0) {
        fif_task *task = malloc(sizeof(*task));
        task->fif = c;
        task->bindings = malloc(sizeof(*task->bindings));
        init_bindings(task->bindings);
        task->premises_done = task->num_unified = 0;
        task->unified_clauses = NULL;
        task->num_to_unify = 0;
        task->to_unify = NULL;

        if (init_to_unify) {
          // Searches for existing singletons to set to_unify
          int neg = c->fif->ordering[0] < 0;
          tommy_hashlin *map = neg ? &collection->pos_map : &collection->neg_map; // Sign flip for clauses to unify with first fif premise

          predname_mapping *pm_result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
          if (pm_result != NULL) {
            for (int j = 0; j < pm_result->num_clauses; j++) {
              if (flags_negative(pm_result->clauses[j]) && pm_result->clauses[j]->pos_count + pm_result->clauses[j]->neg_count == 1) {
                task->num_to_unify++;
                task->to_unify = realloc(task->to_unify, sizeof(*task->to_unify) * task->num_to_unify);
                task->to_unify[task->num_to_unify-1] = pm_result->clauses[j];
              }
            }
          }
        }

        task->proc_next = is_proc(fif_access(c, 0), collection);
        tommy_list_insert_tail(&result->tasks, &task->node, task);
      }

      free(name);
    }
  }
}

// Sets to_unify entries for fif tasks
void fif_tasks_from_clause(kb *collection, clause *c) {
  // If a non-fif singleton clause, sets to_unify for tasks with a matching next clause to process
  if (c->tag == NONE && c->pos_count + c->neg_count == 1) {
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

      // Process original list contents
      int i = 0;
      while (curr && i < len) {
        fif_task *data = curr->data;

        // Only modify add entry to to_unify if next for fif isn't a procedure predicate
        if (flags_negative(data->fif) && !data->proc_next) {
          int sign_match = (pos && data->fif->fif->ordering[data->num_unified] < 0) || (!pos && data->fif->fif->ordering[data->num_unified] >= 0);
          if (sign_match) {
            data->num_to_unify++;
            data->to_unify = realloc(data->to_unify, sizeof(*data->to_unify) * data->num_to_unify);
            data->to_unify[data->num_to_unify-1] = c;
          }
        }

        curr = curr->next;
        i++;
      }
    }
  }
}

// Returns 1 if any of of task's unified_clauses is flagged as distrusted/retired/handled
static int fif_unified_flagged(kb *collection, fif_task *task) {
  for (int i = 0; i < task->num_unified; i++) {
    if (!flags_negative(task->unified_clauses[i]))
      return 1;
  }
  return 0;
}

// Called when each premise of a fif is satisfied
static void fif_conclude(kb *collection, fif_task *task, binding_list *bindings, tommy_array *new_clauses) {
  for (int i = 0; i < task->fif->fif->num_conclusions; i++) {
    clause *conclusion = malloc(sizeof(*conclusion));
    copy_clause_structure(task->fif->fif->conclusions[i], conclusion);
    conclusion->parent_set_count = 1;
    conclusion->parents = malloc(sizeof(*conclusion->parents));
    conclusion->parents[0].count = task->num_unified + 1;
    conclusion->parents[0].clauses = malloc(sizeof(*conclusion->parents[0].clauses) * conclusion->parents[0].count);
    for (int j = 0; j < conclusion->parents[0].count-1; j++) {
      conclusion->parents[0].clauses[j] = task->unified_clauses[j];
    }
    conclusion->parents[0].clauses[conclusion->parents[0].count-1] = task->fif;

    // Using task's overall bindings, obtain proper conclusion clause
    subst_clause(bindings, conclusion, 0);

    set_variable_ids(conclusion, 1, 0, NULL, collection);
    tommy_array_insert(new_clauses, conclusion);
  }
}

// Continues attempting to unify from tasks set
// Task instances for which cannot be further progressed with current KB are added to stopped
static void fif_task_unify_loop(kb *collection, tommy_list *tasks, tommy_list *stopped, kb_str *buf) {
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
    if (next_task->proc_next) {
      if (collection->verbose)
        print_bindings(collection, next_task->bindings, 1, 0, buf);

      if (proc_bound_check(next_func, next_task->bindings, collection) && proc_run(next_func, next_task->bindings, collection)) {
        if (collection->verbose)
          print_bindings(collection, next_task->bindings, 1, 1, buf);

        next_task->premises_done++;
        if (next_task->premises_done == next_task->fif->fif->premise_count) {
          if (!fif_unified_flagged(collection, next_task))
            fif_conclude(collection, next_task, next_task->bindings, &collection->new_clauses);
          free_fif_task(next_task);
        }
        // If incomplete modify current task for results to continue processing
        else {
          next_task->proc_next = is_proc(fif_access(next_task->fif, next_task->premises_done), collection);
          tommy_list_insert_tail(tasks, &next_task->node, next_task);
        }
        task_erased = 1; // Set to not place next_task into stopped
      }
      else {
        free_fif_task(next_task);
        task_erased = 1;
      }
    }
    // Unify case
    else {
      char *name = name_with_arity(next_func->name, next_func->term_count);

      // Important: because of conversion to CNF, a negative in ordering means fif premise is positive
      int search_pos = (next_task->fif->fif->ordering[next_task->premises_done] < 0);
      predname_mapping *m = tommy_hashlin_search(search_pos ? &collection->pos_map : &collection->neg_map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
      free(name);

      if (m != NULL) {
        for (int j = 0; j < m->num_clauses; j++) {
          clause *jth = m->clauses[j];
          if (flags_negative(jth) && jth->pos_count + jth->neg_count == 1) {
            alma_function *to_unify = search_pos ? jth->pos_lits[0] : jth->neg_lits[0];

            binding_list *copy = malloc(sizeof(*copy));
            copy_bindings(copy, next_task->bindings);
            if (collection->verbose) {
              print_bindings(collection, copy, 1, 0, buf);
              print_unify(collection, next_func, next_task->fif->index, to_unify, jth->index, buf);
            }

            if (pred_unify(next_func, to_unify, copy, collection->verbose)) {
              if (collection->verbose) {
                print_bindings(collection, copy, 1, 1, buf);
              }

              // If task is now completed, obtain resulting clause and insert to new_clauses
              if (next_task->premises_done + 1 == next_task->fif->fif->premise_count) {
                if (fif_unified_flagged(collection, next_task)) {
                  // If any unified clauses became flagged, delete task
                  free_fif_task(next_task);
                  task_erased = 1;
                  break;
                }
                else {
                  next_task->num_unified++;
                  next_task->premises_done++;
                  next_task->unified_clauses = realloc(next_task->unified_clauses, sizeof(*next_task->unified_clauses)*next_task->num_unified);
                  next_task->unified_clauses[next_task->num_unified-1] = jth;
                  fif_conclude(collection, next_task, copy, &collection->new_clauses);
                  cleanup_bindings(copy);
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
                latest->unified_clauses[latest->num_unified-1] = jth;
                latest->num_to_unify = 0;
                latest->to_unify = NULL;
                latest->proc_next = is_proc(fif_access(latest->fif, latest->premises_done), collection);
                tommy_list_insert_tail(tasks, &latest->node, latest);
              }
            }
            else {
              if (collection->verbose) {
                tee_alt("Unification failed\n", collection, buf);
                print_bindings(collection, copy, 1, 1, buf);
              }

              // Unification failure
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
    fif_task_unify_loop(collection, tasks, stopped, buf);
}

// Process fif tasks
// For each task in fif_task_mapping:
// - Attempt to progress with either proc predicate or unification on to_unify
// - If progression succeeds, progresses further on next literals as able
// Above process repeats per task until unification fails or fif task is fully satisfied (and then asserts conclusion)
static void process_fif_task_mapping(kb *collection, fif_task_mapping *entry, tommy_list *to_progress, kb_str *buf) {
  tommy_node *curr = tommy_list_head(&entry->tasks);
  while (curr) {
    fif_task *f = curr->data;
    curr = curr->next;

    if (f->to_unify != NULL || f->proc_next) {
      binding_list *copy = malloc(sizeof(*copy));
      copy_bindings(copy, f->bindings);

      if (f->to_unify != NULL) {
        if (fif_unified_flagged(collection, f)) {
          // If any unified clauses became flagged, delete task
          tommy_list_remove_existing(&entry->tasks, &f->node);
          free_fif_task(f);
        }
        else {
          for (int i = 0; i < f->num_to_unify; i++) {
            clause *unify_target = f->to_unify[i];

            // Clause to unify must not have become flagged since it was connected to the fif task
            if (flags_negative(unify_target)) {
              alma_function *to_unify_func = (unify_target->pos_count > 0) ? unify_target->pos_lits[0] : unify_target->neg_lits[0];

              if (collection->verbose) {
                print_bindings(collection, f->bindings, 1, 0, buf);
                print_unify(collection, to_unify_func, unify_target->index, fif_access(f->fif, f->premises_done), f->fif->index, buf);
              }

              if (pred_unify(fif_access(f->fif, f->premises_done), to_unify_func, f->bindings, collection->verbose)) {
                if (collection->verbose) {
                  print_bindings(collection, f->bindings, 1, 1, buf);
                }

                // If task is now completed, obtain resulting clause and insert to new_clauses
                if (f->premises_done + 1 == f->fif->fif->premise_count) {
                  f->premises_done++;
                  f->num_unified++;
                  f->unified_clauses = realloc(f->unified_clauses, sizeof(*f->unified_clauses)*f->num_unified);
                  f->unified_clauses[f->num_unified-1] = unify_target;
                  fif_conclude(collection, f, f->bindings, &collection->new_clauses);
                  cleanup_bindings(f->bindings);
                  f->premises_done--;
                  f->num_unified--;
                  f->unified_clauses = realloc(f->unified_clauses, sizeof(*f->unified_clauses)*f->num_unified);
                }
                // Otherwise, create copy of task and do unify loop call
                else {
                  fif_task *advanced = malloc(sizeof(*advanced));
                  advanced->fif = f->fif;
                  advanced->bindings = f->bindings;
                  advanced->premises_done = f->premises_done + 1;
                  advanced->num_unified = f->num_unified + 1;
                  advanced->unified_clauses = malloc(sizeof(*advanced->unified_clauses)*advanced->num_unified);
                  memcpy(advanced->unified_clauses, f->unified_clauses, sizeof(*f->unified_clauses)*f->num_unified);
                  advanced->unified_clauses[advanced->num_unified-1] = unify_target;
                  advanced->num_to_unify = 0;
                  advanced->to_unify = NULL;
                  advanced->proc_next = is_proc(fif_access(advanced->fif, advanced->premises_done), collection);

                  tommy_list_insert_tail(to_progress, &advanced->node, advanced);
                }
                f->bindings = malloc(sizeof(*f->bindings));
                copy_bindings(f->bindings, copy);
              }
              else {
                if (collection->verbose) {
                  print_bindings(collection, f->bindings, 1, 1, buf);
                }

                cleanup_bindings(f->bindings);
                f->bindings = malloc(sizeof(*f->bindings));
                copy_bindings(f->bindings, copy);
              }
            }
          }
          free(f->to_unify);
          f->to_unify = NULL;
          f->num_to_unify = 0;
          f->proc_next = is_proc(fif_access(f->fif, f->premises_done), collection);
        }
        cleanup_bindings(copy);
      }
      else {
        alma_function *proc = fif_access(f->fif, f->premises_done);
        if (proc_bound_check(proc, f->bindings, collection) && proc_run(proc, f->bindings, collection)) {
          if (collection->verbose)
            print_bindings(collection, f->bindings, 1, 1, buf);

          // If task is now completed, obtain resulting clause and insert to new_clauses
          if (f->premises_done + 1 == f->fif->fif->premise_count) {
            if (fif_unified_flagged(collection, f)) {
              // If any unified clauses became flagged, delete task
              tommy_list_remove_existing(&entry->tasks, &f->node);
              free_fif_task(f);
              cleanup_bindings(copy);
            }
            else {
              f->premises_done++;
              fif_conclude(collection, f, f->bindings, &collection->new_clauses);
              cleanup_bindings(f->bindings);
              f->bindings = copy;
              f->premises_done--;
            }
          }
          // Otherwise, create copy of task and do unify loop call
          else {
            fif_task *advanced = malloc(sizeof(*advanced));
            advanced->fif = f->fif;
            advanced->bindings = f->bindings;
            f->bindings = copy;
            advanced->premises_done = f->premises_done + 1;
            advanced->num_unified = f->num_unified;
            advanced->unified_clauses = malloc(sizeof(*advanced->unified_clauses)*advanced->num_unified);
            memcpy(advanced->unified_clauses, f->unified_clauses, sizeof(*advanced->unified_clauses)*advanced->num_unified);
            advanced->num_to_unify = 0;
            advanced->to_unify = NULL;
            advanced->proc_next = is_proc(fif_access(advanced->fif, advanced->premises_done), collection);

            tommy_list_insert_tail(to_progress, &advanced->node, advanced);
          }
        }
        else
          cleanup_bindings(copy);
      }
    }
  }
}

// Process fif tasks and place results in new_clauses
void process_fif_tasks(kb *collection, kb_str *buf) {
  tommy_list to_progress;
  tommy_list_init(&to_progress);

  // Loop over hashlin contents of collection's fif tasks; based on tommy_hashlin_foreach
  // TODO: may want double indexing if this loops too many times
  tommy_size_t bucket_max = collection->fif_tasks.low_max + collection->fif_tasks.split;
  for (tommy_size_t pos = 0; pos < bucket_max; ++pos) {
    tommy_hashlin_node *node = *tommy_hashlin_pos(&collection->fif_tasks, pos);

    while (node) {
      fif_task_mapping *data = node->data;
      node = node->next;
      process_fif_task_mapping(collection, data, &to_progress, buf);
    }
  }

  tommy_list progressed;
  tommy_list_init(&progressed);
  // Progress further on tasks; as far as able
  fif_task_unify_loop(collection, &to_progress, &progressed, buf);

  // Incorporates tasks from progressed list back into task mapping
  tommy_node *curr = tommy_list_head(&progressed);
  while (curr) {
    fif_task *data = curr->data;
    curr = curr->next;

    alma_function *f = fif_access(data->fif, data->premises_done);
    char *name = name_with_arity(f->name, f->term_count);
    fif_task_mapping *result = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (result != NULL)
      tommy_list_insert_tail(&result->tasks, &data->node, data);
    free(name);
  }
}

// Reorders clause array to begin with fifs; for correctness of task generation
void fif_to_front(tommy_array *clauses) {
  tommy_size_t loc = 0;
  for (tommy_size_t i = 0; i < tommy_array_size(clauses); i++) {
    clause *c = tommy_array_get(clauses, i);
    if (c->tag == FIF) {
      if (loc != i) {
        clause *tmp = tommy_array_get(clauses, loc);
        tommy_array_set(clauses, loc, c);
        tommy_array_set(clauses, i, tmp);
      }
      loc++;
    }
  }
}

void free_fif_mapping(void *arg) {
  fif_mapping *entry = arg;
  free(entry->indexing_conc_name);
  free(entry->clauses);
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in kb_halt
  free(entry);
}

void free_fif_task(fif_task *task) {
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in kb_halt
  cleanup_bindings(task->bindings);
  free(task->unified_clauses);
  free(task->to_unify);
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

// Used in removing a singleton clause -- remove partial fif tasks involving the clause
void remove_fif_singleton_tasks(kb *collection, clause *c) {
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

    for (int i = 0; i < count; i++) {
      fif_task_mapping *tm = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, names[i], tommy_hash_u64(0, names[i], strlen(names[i])));
      if (tm != NULL) {
        tommy_node *curr = tommy_list_head(&tm->tasks);
        while (curr) {
          fif_task *currdata = curr->data;
          curr = curr->next;

          // Remove to_unify list entries of current clause, which is first in names array
          if (i == 0) {
            for (int j = 0; j < currdata->num_to_unify; j++) {
              if (currdata->to_unify[j]->index == c->index) {
                currdata->num_to_unify--;
                currdata->to_unify[j] = currdata->to_unify[currdata->num_to_unify];
                currdata->to_unify = realloc(currdata->to_unify, sizeof(*currdata->to_unify)*currdata->num_to_unify);
                break;
              }
            }
          }
          // Otherwise, delete partial fif tasks that have unfied with current clause
          else {
            for (int j = 0; j < currdata->num_unified; j++) {
              if (currdata->unified_clauses[j]->index == c->index) {
                tommy_list_remove_existing(&tm->tasks, &currdata->node);
                free_fif_task(currdata);
                break;
              }
            }
          }
        }
      }

      free(names[i]);
    }
    free(names);

    // fif_task_mappings have tasks deleted if they have unified with c
    for (int i = 0; i < prev_count; i++) {
      fif_task_mapping *tm = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, prev_names[i], tommy_hash_u64(0, prev_names[i], strlen(prev_names[i])));
      if (tm != NULL) {
        tommy_node *curr = tommy_list_head(&tm->tasks);
        while (curr) {
          fif_task *currdata = curr->data;
          curr = curr->next;

          for (int j = 0; j < currdata->num_unified; j++) {
            if (currdata->unified_clauses[j]->index == c->index) {
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

// Compare function to be used by tommy_hashlin_search for fif_mapping
// compares string arg to indexing_conc_name of fif_mapping
int fifm_compare(const void *arg, const void *obj) {
  return strcmp((const char*)arg, ((const fif_mapping*)obj)->indexing_conc_name);
}

// Compare function to be used by tommy_hashlin_search for fif_task_mapping
// Compares string arg to predname of fif_task_mapping
int fif_taskm_compare(const void *arg, const void *obj) {
  return strcmp((const char*)arg, ((const fif_task_mapping*)obj)->predname);
}
