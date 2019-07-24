#include "alma_fif.h"
#include "alma_proc.h"

// Given a new fif clause, initializes fif task mappings held by fif_tasks for each premise of c
// Also places single fif_task into fif_task_mapping for first premise
void fif_task_map_init(kb *collection, clause *c) {
  if (c->tag == FIF) {
    for (int i = 0; i < c->fif->premise_count; i++) {
      alma_function *f = fif_access(c, i);

      // Don't make task mappings for middle proc premises
      if (i > 0 && strcmp(f->name, "proc") == 0)
        continue;
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
        task->premises_done = 0;
        task->num_unified = 0;
        task->unified_clauses = NULL;
        task->to_unify = NULL;
        task->proc_next = proc_valid(fif_access(c, 0));
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
      int i = 0;
      while (curr && i < len) {
        fif_task *data = curr->data;

        // Only modify to_unify if next for fif isn't a procedure predicate
        if (!data->proc_next) {
          int sign_match = (pos && data->fif->fif->ordering[data->num_unified] < 0) || (!pos && data->fif->fif->ordering[data->num_unified] >= 0);
          if (sign_match) {
            if (data->to_unify == NULL)
              data->to_unify = c;
            else {
              // If to_unify is already non-null, duplicate the task and assign to_unify of duplicate
              fif_task *copy = malloc(sizeof(*copy));
              copy->fif = data->fif;
              copy->bindings = malloc(sizeof(*copy->bindings));
              copy_bindings(copy->bindings, data->bindings);
              copy->premises_done = data->premises_done;
              copy->num_unified = data->num_unified;
              copy->unified_clauses = malloc(sizeof(*data->unified_clauses) * data->num_unified);
              memcpy(copy->unified_clauses, data->unified_clauses, sizeof(*data->unified_clauses) * data->num_unified);
              copy->to_unify = c;
              copy->proc_next = data->proc_next;
              tommy_list_insert_tail(&result->tasks, &copy->node, copy);
            }
          }
        }

        curr = curr->next;
        i++;
      }
    }
  }
}

// Checks if each of task's unified_clauses is distrusted
// Returns 1 if any are
static int fif_unified_distrusted(kb *collection, fif_task *task) {
  for (int i = 0; i < task->num_unified; i++)
    if (is_distrusted(collection, task->unified_clauses[i]))
      return 1;
  return 0;
}

// Called when each premise of a fif is satisfied
static clause* fif_conclude(kb *collection, fif_task *task, binding_list *bindings) {
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
  conclusion->pos_lits[0] = conc_func;

  conclusion->parent_set_count = 1;
  conclusion->parents = malloc(sizeof(*conclusion->parents));
  conclusion->parents[0].count = task->num_unified + 1;
  conclusion->parents[0].clauses = malloc(sizeof(*conclusion->parents[0].clauses) * conclusion->parents[0].count);
  for (int i = 0; i < conclusion->parents[0].count-1; i++) {
    long index = task->unified_clauses[i];
    index_mapping *result = tommy_hashlin_search(&collection->index_map, im_compare, &index, tommy_hash_u64(0, &index, sizeof(index)));
    conclusion->parents[0].clauses[i] = result->value;
  }
  conclusion->parents[0].clauses[conclusion->parents[0].count-1] = task->fif;
  conclusion->children_count = 0;
  conclusion->children = NULL;
  conclusion->tag = NONE;
  conclusion->fif = NULL;

  set_variable_ids(conclusion, 1, NULL);

  return conclusion;
}

// Continues attempting to unify from tasks set
// Task instances for which cannot be further progressed with current KB are added to stopped
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

    int task_erased = 0;
    alma_function *next_func = fif_access(next_task->fif, next_task->premises_done);
    // Proc case
    if (proc_valid(next_func)) {
      if (proc_bound_check(next_func, next_task->bindings) && proc_run(next_func, next_task->bindings, collection)) {
        next_task->premises_done++;
        if (next_task->premises_done == next_task->fif->fif->premise_count) {
          if (!fif_unified_distrusted(collection, next_task))
            tommy_array_insert(new_clauses, fif_conclude(collection, next_task, next_task->bindings));
          free_fif_task(next_task);
        }
        // If incomplete modify current task for results to continue processing
        else {
          next_task->proc_next = proc_valid(fif_access(next_task->fif, next_task->premises_done));
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
      int search_pos = next_task->fif->fif->ordering[next_task->premises_done] < 0;
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
              if (next_task->premises_done + 1 == next_task->fif->fif->premise_count) {
                if (fif_unified_distrusted(collection, next_task)) {
                  // If any unified clauses became distrusted, delete task
                  free_fif_task(next_task);
                  task_erased = 1;
                  break;
                }
                else {
                  next_task->num_unified++;
                  next_task->premises_done++;
                  next_task->unified_clauses = realloc(next_task->unified_clauses, sizeof(*next_task->unified_clauses)*next_task->num_unified);
                  next_task->unified_clauses[next_task->num_unified-1] = jth->index;
                  tommy_array_insert(new_clauses, fif_conclude(collection, next_task, copy));
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
                latest->unified_clauses[latest->num_unified-1] = jth->index;
                latest->to_unify = NULL;
                latest->proc_next = proc_valid(fif_access(latest->fif, latest->premises_done));
                tommy_list_insert_tail(tasks, &latest->node, latest);
              }
            }
            else {
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
    fif_task_unify_loop(collection, tasks, stopped, new_clauses);
}

// Process fif tasks
// For each task in fif_task_mapping:
// - Attempt to progress with either proc predicate or unification on to_unify
// - If progression succeeds, progresses further on next literals as able
// Above process repeats per task until unification fails or fif task is fully satisfied (and then asserts conclusion)
static void process_fif_task_mapping(kb *collection, fif_task_mapping *entry, tommy_list *progressed, tommy_array *new_clauses) {
  tommy_node *curr = tommy_list_head(&entry->tasks);
  while (curr) {
    fif_task *f = curr->data;
    curr = curr->next;

    if (f->to_unify != NULL || f->proc_next) {
      int premise_progress = 0;
      binding_list *copy = malloc(sizeof(*copy));
      copy_bindings(copy, f->bindings);

      if (f->to_unify != NULL) {
        // Clause to_unify must not have become distrusted since it was connected to the fif task
        if (!is_distrusted(collection, f->to_unify->index)) {
          alma_function *fif_func = fif_access(f->fif, f->premises_done);
          alma_function *to_unify_func = (f->to_unify->pos_count > 0) ? f->to_unify->pos_lits[0] : f->to_unify->neg_lits[0];

          premise_progress = pred_unify(fif_func, to_unify_func, f->bindings);

          if (!premise_progress) {
            f->to_unify = NULL;
            // Reset from copy
            cleanup_bindings(f->bindings);
            f->bindings = copy;
            copy = NULL;
          }
        }
        else {
          f->to_unify = NULL;
        }
      }
      else {
        alma_function *proc = fif_access(f->fif, f->premises_done);
        if (proc_bound_check(proc, f->bindings)) {
          premise_progress = proc_run(proc, f->bindings, collection);
        }
      }

      if (premise_progress) {
        // If task is now completed, obtain resulting clause and insert to new_clauses
        if (f->premises_done + 1 == f->fif->fif->premise_count) {
          if (fif_unified_distrusted(collection, f)) {
            // If any unified clauses became distrusted, delete task
            tommy_list_remove_existing(&entry->tasks, &f->node);
            free_fif_task(f);
          }
          else {
            f->premises_done++;
            if (f->to_unify != NULL) {
              f->num_unified++;
              f->unified_clauses = realloc(f->unified_clauses, sizeof(*f->unified_clauses)*f->num_unified);
              f->unified_clauses[f->num_unified-1] = f->to_unify->index;
            }

            tommy_array_insert(new_clauses, fif_conclude(collection, f, f->bindings));
            cleanup_bindings(f->bindings);
            // Reset from copy
            f->bindings = copy;

            f->premises_done--;
            if (f->to_unify != NULL) {
              f->to_unify = NULL;
              f->proc_next = proc_valid(fif_access(f->fif, f->premises_done));
              f->num_unified--;
            }
          }
        }
        // Otherwise, adjust task and do unify loop call
        else {
          f->premises_done++;
          if (f->to_unify != NULL) {
            f->num_unified++;
            f->unified_clauses = realloc(f->unified_clauses, sizeof(*f->unified_clauses)*f->num_unified);
            f->unified_clauses[f->num_unified-1] = f->to_unify->index;
            f->to_unify = NULL;
            f->proc_next = proc_valid(fif_access(f->fif, f->premises_done));
          }
          cleanup_bindings(copy);

          tommy_list_remove_existing(&entry->tasks, &f->node);
          tommy_list to_progress;
          tommy_list_init(&to_progress);
          tommy_list_insert_tail(&to_progress, &f->node, f);

          // Unification succeeded; progress further on task as able in call
          fif_task_unify_loop(collection, &to_progress, progressed, new_clauses);
        }
      }
      else if (copy != NULL){
        cleanup_bindings(copy);
      }
    }
  }
}

// Process fif tasks and place results in new_clauses
// See helper process_fif_task_mapping for comment
void process_fif_tasks(kb *collection) {
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
			process_fif_task_mapping(collection, data, &progressed, &collection->new_clauses);
		}
  }

  // Incorporates tasks from progressed list back into task mapping
  tommy_node *curr = tommy_list_head(&progressed);
  while (curr) {
    fif_task *data = curr->data;
    curr = curr->next;

    alma_function *f = fif_access(data->fif, data->premises_done);
    char *name = name_with_arity(f->name, f->term_count);
    fif_task_mapping *result = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    tommy_list_insert_tail(&result->tasks, &data->node, data);
    free(name);
  }
}

// Reorders clause array to begin with fifs; for correctness of task generation
void fif_to_front(tommy_array *clauses) {
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

void free_fif_mapping(void *arg) {
  fif_mapping *entry = arg;
  free(entry->conclude_name);
  free(entry->clauses);
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in kb_halt
  free(entry);
}

void free_fif_task(fif_task *task) {
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in kb_halt
  cleanup_bindings(task->bindings);
  free(task->unified_clauses);
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

// Compare function to be used by tommy_hashlin_search for fif_mapping
// compares string arg to conclude_name of fif_mapping
int fifm_compare(const void *arg, const void*obj) {
  return strcmp((const char*)arg, ((const fif_mapping*)obj)->conclude_name);
}

// Compare function to be used by tommy_hashlin_search for fif_task_mapping
// Compares string arg to predname of fif_task_mapping
int fif_taskm_compare(const void *arg, const void *obj) {
  return strcmp((const char*)arg, ((const fif_task_mapping*)obj)->predname);
}
