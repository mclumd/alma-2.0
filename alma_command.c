#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include "alma_command.h"
#include "tommy.h"
#include "alma_formula.h"
#include "alma_backsearch.h"
#include "alma_fif.h"
#include "alma_parser.h"
#include "alma_print.h"
#include "res_task_heap.h"
#include "compute_priority.h"


void parse_subjects_file(tommy_array *subj_list, int *num_subjects, char *subjects_file) {
  FILE *sf;
  sf = fopen(subjects_file, "r");
  if (sf == NULL) {
    fprintf(stderr, "Couldn't open subjects file %s\n", subjects_file);
    exit(1);
  }
  char subj_line[1024];
  int subj_len;
  char *subj_copy;
  tommy_array_init(subj_list);
  *num_subjects = 0;
  while (fgets(subj_line, 1024, sf) != NULL) {
    subj_len = strlen(subj_line) + 1; // Add one for terminating \0
    subj_copy = malloc(subj_len * sizeof(char));
    strncpy(subj_copy, subj_line, subj_len);
    subj_copy[subj_len-2] = '\0';
    tommy_array_insert(subj_list, subj_copy);
    (*num_subjects)++;
  }
  fclose(sf);
}

void parse_subjects_priorites(double *prior_list, int num, char *filename) {
  FILE *pf;
  pf = fopen(filename, "r");
  if (pf == NULL) {
    fprintf(stderr, "Couldn't subject priority file %s\n", filename);
    exit(1);
  }
  char line[1024];
  int i = 0;
  while ( (fgets(line, 1024, pf) != NULL) && ( i < num) ) {
    prior_list[i] = strtod(line, NULL);
    i++;
  }
  fclose(pf);
}

extern char logs_on;
extern char python_mode;

// Caller will need to free collection with kb_halt
/* TODO:  Examine for merge issues. */
void kb_init(kb **collection, char *file, char *agent, int verbose, int differential_priorities, int res_heap_size, rl_init_params rip, kb_str *buf, int logon) {
  // Allocate and initialize
  *collection = malloc(sizeof(**collection));
  kb *collec = *collection;

  collec->verbose = verbose;
  collec->diff_prior = differential_priorities;
  //collec->calc_priority = differential_priorities ? (compute_priority) : (zero_priority);
  collec->calc_priority = differential_priorities ? (compute_priority) : (base_priority);
  collec->tracking_resolutions = rip.tracking_resolutions;
  collec->subject_list = malloc(sizeof(tommy_array));

  collec->size = 0;
  collec->time = 1;
  collec->prev = collec->now = NULL;
  collec->wallnow = collec->wallprev = NULL;
  collec->idling = 0;
  collec->res_heap_size = res_heap_size;
  collec->variable_id_count = 0;
  collec->next_index = 0;
  tommy_array_init(&collec->new_clauses);
  tommy_list_init(&collec->clauses);
  tommy_hashlin_init(&collec->index_map);
  tommy_hashlin_init(&collec->pos_map);
  tommy_list_init(&collec->pos_list);
  tommy_hashlin_init(&collec->neg_map);
  tommy_list_init(&collec->neg_list);
  tommy_hashlin_init(&collec->fif_map);
  //tommy_array_init(&collec->res_tasks);
  res_task_heap_init(&(collec->res_tasks), res_heap_size);
  //collec->res_tasks_idx = 0;
  tommy_hashlin_init(&collec->fif_tasks);
  tommy_list_init(&collec->backsearch_tasks);
  tommy_hashlin_init(&collec->distrusted);

  parse_init();

  if (rip.tracking_resolutions)
    {
      if (rip.use_lists)
        {
          collec->subject_list = rip.tracking_subjects;
          collec->num_subjects = rip.tracking_subjects->count;
        }
      else
        {
          parse_subjects_file (collec->subject_list, &(collec->num_subjects), rip.subjects_file);
        }
      init_resolution_choices (&(collec->resolution_choices), collec->num_subjects, rip.resolutions_horizon);
      if (rip.use_lists)
        {
          collec->subject_priorities = rip.tracking_priorities;
        }
      else
        {
          collec->subject_priorities = malloc (sizeof (double) * collec->num_subjects);
          if (rip.rl_priority_file == NULL)
            {
              for (int i = 0; i < collec->num_subjects; i++) collec->subject_priorities[i] = 0.0;
            }
          else
            {
              parse_subjects_priorites (collec->subject_priorities, collec->num_subjects, rip.rl_priority_file);
            }
        }
    }


  // Given a file argument, obtain other initial clauses from
  if (file != NULL) {
    alma_node *trees;
    int num_trees;

    if (formulas_from_source(file, 1, &num_trees, &trees)) {
      nodes_to_clauses(collec, trees, num_trees, &collec->new_clauses, 0, buf);
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


  collec->prev = now(collec->time);
  assert_formula(collec, collec->prev, 0, buf);
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

  if (logon) {
    time_t rawtime;
    time(&rawtime);
    char *time = ctime(&rawtime);
    int timelen = strlen(time)-1;
    for (int idx = 0; idx < timelen; idx++)
      if (time[idx] == ' ' || time[idx] == ':')
	time[idx] = '-';
    int agentlen = agent != NULL ? strlen(agent) : 0;
    char *logname = malloc(4 + agentlen + 1 + timelen + 9);
    strcpy(logname, "alma");
    if (agent != NULL)
      strcpy(logname+4, agent);
    logname[4+agentlen] = '-';
    strncpy(logname+5+agentlen, time, 24);
    strcpy(logname+5+agentlen+timelen, "-log.txt");

    collec->almalog = fopen(logname, "w");
    free(logname);
  } else {
    collec->almalog = NULL;
  }
}


int load_file(kb *collec, char *filename, kb_str *buf) {
  FILE *fp;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  fp = fopen(filename, "r");
  if (fp == NULL) return 0;


  while ((read = getline(&line, &len, fp)) != -1) kb_assert(collec, line, buf);
  fclose(fp);
  if (line) free(line);

  return 1;
}


// System idles if there are no resolution tasks, backsearch tasks, or to_unify fif values
// First of these is true when no new clauses (source of res tasks) exist
// To_unify values are all removed in current implementation each step from exhaustive fif
static int idling_check(kb *collection, int new_clauses) {
  if (!new_clauses) {
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
void kb_step(kb *collection, int singleton, kb_str *buf) {
  collection->time++;

  if (!singleton) {
    process_res_tasks(collection, &collection->res_tasks, &collection->new_clauses, NULL, buf);
  } else{
    process_single_res_task(collection, &collection->res_tasks, &collection->new_clauses, NULL, buf);
  }
  process_fif_tasks(collection);
  process_backsearch_tasks(collection, buf);

  int newc = tommy_array_size(&collection->new_clauses) > 0;
  process_new_clauses(collection, buf);

  // New clock rules go last
  collection->now = now(collection->time);
  assert_formula(collection, collection->now, 0, buf);
  collection->wallnow = walltime();
  assert_formula(collection, collection->wallnow, 0, buf);
  delete_formula(collection, collection->prev, 0, buf);
  free(collection->prev);
  collection->prev = collection->now;
  delete_formula(collection, collection->wallprev, 0, buf);
  free(collection->wallprev);
  collection->wallprev = collection->wallnow;
  process_new_clauses(collection, buf);

  // Generate new backsearch tasks
  tommy_node *i = tommy_list_head(&collection->backsearch_tasks);
  while (i) {
    generate_backsearch_tasks(collection, i->data, buf);
    i = i->next;
  }

  collection->idling = idling_check(collection, newc);
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
    if (data->value->dirty_bit ) {
      tee_alt("%ld: ", collection, buf, data->key);
      clause_print(collection, data->value, buf);
      tee_alt("\n", collection, buf);
    }
    i = i->next;
  }

  i = tommy_list_head(&collection->backsearch_tasks);
  if (i) {
    tee_alt("Back searches:\n", collection, buf);
    for (int count = 0; i != NULL; i = i->next, count++) {
      tee_alt("%d\n", collection, buf, count);
      backsearch_task *t = i->data;
      for (tommy_size_t j = 0; j < tommy_array_size(&t->clauses); j++) {
        clause *c = tommy_array_get(&t->clauses, j);
        tee_alt("%ld: ", collection, buf, c->index);
        clause_print(collection, c, buf);
        binding_mapping *m = tommy_hashlin_search(&t->clause_bindings, bm_compare, &c->index, tommy_hash_u64(0, &c->index, sizeof(c->index)));
        if (m != NULL) {
          tee_alt(" (bindings: ", collection, buf);
          print_bindings(collection, m->bindings, buf);
          tee_alt(")", collection, buf);
        }
        tee_alt("\n", collection, buf);
      }
    }
  }
  tee_alt("\n", collection, buf);

  if (logs_on) {
    fflush(almalog);
  }
}


void kb_print_res_heap(kb *collection, kb_str *buf) {

  res_task_heap *res_tasks = &collection->res_tasks;
  res_task_pri tp;
  res_task *t;
  int j = 0;

  if ((res_tasks->count > 0)) {
    tee("Begin RH---------------------------------------\n");
    tee("Resolution tasks.\n");
    tee("Heap count: %d\n\n", res_tasks->count);
    tee("Pri\t\tx\t\ty\t\tPos\t\tNeg\n");
    for (j=0; j< res_tasks->count; j++) {
      tp = *res_task_heap_item(res_tasks, j);
      tee_alt("%g\t\t", buf, tp.priority);
      t = tp.res_task;
      res_task_print(collection, t, buf);
      tee_alt("\n", buf);
    }
  tee("End RH---------------------------------------\n");
  }
  tee_alt("\n", buf);
  if (logs_on) {
    fflush(collection->almalog);
  }
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

  if (logs_on) {
    fclose(collection->almalog);
  }

  free(collection);

  parse_cleanup();
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
    nodes_to_clauses(collection, formulas, formula_count, &arr, 0, buf);

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
        tee_alt("-a: ", collection, buf);
        clause_print(collection, c, buf);
        tee_alt(" observed\n", collection, buf);
      }
      else {
        clause_print(collection, c, buf);
        tee_alt(" has too many literals\n", collection, buf);
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
    nodes_to_clauses(collection, formulas, formula_count, &arr, 0, buf);

    clause *c = tommy_array_get(&arr, 0);
    // Free all after first
    for (int i = 1; i < tommy_array_size(&arr); i++)
      free_clause(tommy_array_get(&arr, i));
    tommy_array_done(&arr);
    if (c != NULL) {
      if (c->pos_count + c->neg_count > 1) {
        tee_alt("-a: query clause has too many literals\n", collection, buf);
        free_clause(c);
      }
      else {
        collection->idling = 0;
        backsearch_from_clause(collection, c);
        tee_alt("-a: Backsearch initiated for ", collection, buf);
        clause_print(collection, c, buf);
        tee_alt("\n", collection, buf);
      }
    }
  }
}
