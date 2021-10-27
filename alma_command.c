#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "alma_command.h"
#include "tommy.h"
#include "alma_formula.h"
#include "alma_backsearch.h"
#include "alma_fif.h"
#include "alma_parser.h"
#include "alma_proc.h"
#include "alma_print.h"

extern char logs_on;
extern char python_mode;

static char* now(long t) {
  int len = snprintf(NULL, 0, "%ld", t);
  char *str = malloc(4 + len+1 + 2);
  strcpy(str, "now(");
  snprintf(str+4, len+1, "%ld", t);
  strcpy(str+4+len, ").");
  return str;
}

// Caller will need to free reasoner with alma_halt
void alma_init(alma *reasoner, char **files, int file_count, char *agent, char *trialnum, char *log_dir, int verbose, kb_str *buf, int logon) {
  reasoner->time = 0;
  reasoner->prev = reasoner->now = NULL;

  reasoner->idling = 0;
  reasoner->core_kb = malloc(sizeof(*reasoner->core_kb));
  kb_init(reasoner->core_kb, verbose);
  reasoner->agents_count = 0;
  reasoner->agents_kb = NULL;
  tommy_list_init(&reasoner->backsearch_tasks);
  const alma_proc procs[17] = {{"neg_int", 1}, {"neg_int_spec", 1}, {"neg_int_gen", 1}, {"pos_int", 1}, {"pos_int_spec", 1},
                   {"pos_int_gen", 1}, {"acquired", 2}, {"pos_int_past", 3}, {"neg_int_past", 3}, {"ancestor", 3}, {"non_ancestor", 3},
                   {"parent", 3}, {"parents_defaults", 2}, {"parent_non_default", 2}, {"less_than", 2}, {"quote_cons", 2}, {"not_equal", 2}};
  memcpy(reasoner->procs, procs, sizeof(procs));

  parse_init();

  if (logon) {
    time_t rawtime;
    time(&rawtime);
    char *time = ctime(&rawtime);
    int timelen = strlen(time)-1;
    for (int idx = 0; idx < timelen; idx++)
      if (time[idx] == ' ' || time[idx] == ':')
        time[idx] = '-';
    int agentlen = agent != NULL ? strlen(agent) : 0;
    int triallen = trialnum != NULL ? strlen(trialnum) : 0;
    int log_dir_len = log_dir != NULL ? strlen(log_dir) : 0;
    char *logname = malloc(log_dir_len + 1 + 4 + agentlen + 1 + triallen + 1 + timelen + 9);
    if (log_dir != NULL) {
      strcpy(logname, log_dir);
      logname[log_dir_len] = '/';
      log_dir_len += 1;
    }

    strcpy(logname+log_dir_len, "alma");
    if (agent != NULL)
      strcpy(logname+log_dir_len+4, agent);
    logname[log_dir_len+4+agentlen] = '-';
    if (trialnum != NULL)
      strcpy(logname+log_dir_len+4+agentlen+1,trialnum);
    logname[log_dir_len+4+agentlen+1+triallen] = '-';
    strncpy(logname+log_dir_len+6+agentlen+triallen, time, 24);
    strcpy(logname+log_dir_len+6+agentlen+triallen+timelen, "-log.txt");
    reasoner->almalog = fopen(logname, "w");
    free(logname);
  } else {
    reasoner->almalog = NULL;
  }

  kb_logger logger;
  logger.log = reasoner->almalog;
  logger.buf = buf;

  // Given a file argument, obtain other initial clauses from
  if (files != NULL) {
    for (int i = 0; i < file_count; i++) {
      alma_node *trees;
      int num_trees;

      if (formulas_from_source(files[i], 1, &num_trees, &trees,  &logger)) {
        tommy_array temp;
        tommy_array_init(&temp);
        nodes_to_clauses(trees, num_trees, &temp, 0, &reasoner->core_kb->variable_id_count, &logger);
        for (tommy_size_t j = 0; j < tommy_array_size(&temp); j++) {
          tommy_array_insert(&reasoner->core_kb->new_clauses, tommy_array_get(&temp, j));
        }
        tommy_array_done(&temp);
      }
      // If any file cannot parse, cleanup and exit
      else {
        free(files);
        alma_halt(reasoner);
        exit(0);
      }
    }
  }
  if (agent != NULL) {
    int namelen = strlen(agent);
    char *sentence = malloc(10 + namelen + 3);
    strcpy(sentence, "agentname(");
    strcpy(sentence + 10, agent);
    strcpy(sentence + 10 + namelen, ").");
    assert_formula(reasoner->core_kb, sentence, 0, &logger);
    free(sentence);
  }

  reasoner->prev = now(reasoner->time);
  assert_formula(reasoner->core_kb, reasoner->prev, 0, &logger);

  fif_to_front(&reasoner->core_kb->new_clauses);
  // Insert starting clauses
  process_new_clauses(reasoner->core_kb, reasoner->procs, reasoner->time, &logger, 0);
  // Second pass of process_new_clauses for late-added meta-formulas like contra from end of first pass
  process_new_clauses(reasoner->core_kb, reasoner->procs, reasoner->time, &logger, 0);

  // Generate starting tasks
  tommy_node *i = tommy_list_head(&reasoner->core_kb->clauses);
  while (i) {
    clause *c = ((index_mapping*)i->data)->value;
    if (c->tag == FIF)
      fif_task_map_init(reasoner->core_kb, reasoner->procs, &reasoner->core_kb->fif_tasks, c, 0);
    else {
      res_tasks_from_clause(reasoner->core_kb, c, 0);
      fif_tasks_from_clause(&reasoner->core_kb->fif_tasks, c);
    }
    i = i->next;
  }
}

// System idles if there are no resolution tasks, backsearch tasks, or to_unify fif values
// First of these is true when no new clauses (source of res tasks) exist
// To_unify values are all removed in current implementation each step from exhaustive fif
// Thus, answer is either from having new_clauses OR finding tasks in backsearch
static int idling_check(alma *reasoner, int new_clauses) {
  if (!new_clauses) {
    tommy_node *i = tommy_list_head(&reasoner->backsearch_tasks);
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

void alma_step(alma *reasoner, kb_str *buf) {
  kb_logger logger;
  logger.log = reasoner->almalog;
  logger.buf = buf;

  reasoner->time++;

  // Move delayed clauses into new_clauses
  for (tommy_size_t i = 0; i < tommy_array_size(&reasoner->core_kb->timestep_delay_clauses); i++) {
    clause *c = tommy_array_get(&reasoner->core_kb->timestep_delay_clauses, i);
    tommy_array_insert(&reasoner->core_kb->new_clauses, c);
  }
  tommy_array_done(&reasoner->core_kb->timestep_delay_clauses);
  tommy_array_init(&reasoner->core_kb->timestep_delay_clauses);

  process_res_tasks(reasoner->core_kb, reasoner->time, &reasoner->core_kb->res_tasks, &reasoner->core_kb->new_clauses, NULL, &logger);
  process_fif_tasks(reasoner->core_kb, reasoner->procs, &logger);
  process_backsearch_tasks(reasoner->core_kb, reasoner->time, &reasoner->backsearch_tasks, &logger);

  int newc = tommy_array_size(&reasoner->core_kb->new_clauses) > 0;
  process_new_clauses(reasoner->core_kb, reasoner->procs, reasoner->time, &logger, 1);

  // New clock rules go last
  reasoner->now = now(reasoner->time);
  assert_formula(reasoner->core_kb, reasoner->now, 0, &logger);
  delete_formula(reasoner->core_kb, reasoner->time, reasoner->prev, 0, &logger);
  free(reasoner->prev);
  reasoner->prev = reasoner->now;

  // Second pass of process_new_clauses covers clock rules as well as late-added meta-formulas like contra from end of first pass
  process_new_clauses(reasoner->core_kb, reasoner->procs, reasoner->time, &logger, 1);

  tommy_node *i = tommy_list_head(&reasoner->backsearch_tasks);
  while (i) {
    generate_backsearch_tasks(reasoner->core_kb, reasoner->time, i->data);
    i = i->next;
  }

  reasoner->idling = idling_check(reasoner, newc);
  if (reasoner->idling)
    tee_alt("-a: Idling...\n", &logger);
}

void alma_print(alma *reasoner, kb_str *buf) {
  kb_logger logger;
  logger.log = reasoner->almalog;
  logger.buf = buf;

  kb_print(reasoner->core_kb, &logger);

  tommy_node* i = tommy_list_head(&reasoner->backsearch_tasks);
  if (i) {
    tee_alt("Back searches:\n", &logger);
    for (int count = 0; i != NULL; i = i->next, count++) {
      tee_alt("%d\n",  &logger, count);
      backsearch_task *t = i->data;
      for (tommy_size_t j = 0; j < tommy_array_size(&t->clauses); j++) {
        clause *c = tommy_array_get(&t->clauses, j);
        tee_alt("%ld: ", &logger, c->index);
        clause_print(c, &logger);
        binding_mapping *m = tommy_hashlin_search(&t->clause_bindings, bm_compare, &c->index, tommy_hash_u64(0, &c->index, sizeof(c->index)));
        if (m != NULL) {
          tee_alt(" (bindings: ",  &logger);
          print_bindings(m->bindings, 0, 0,  &logger);
          tee_alt(")", &logger);
        }
        tee_alt("\n", &logger);
      }
    }
  }
  tee_alt("\n", &logger);
  if (logs_on) {
    fflush(reasoner->almalog);
  }
}

void alma_halt(alma *reasoner) {
  // now and prev alias at this point, only free one
  free(reasoner->now);
  if (reasoner->now == NULL)
    free(reasoner->prev);

  for (tommy_size_t i = 0; i < tommy_array_size(&reasoner->core_kb->new_clauses); i++)
    free_clause(tommy_array_get(&reasoner->core_kb->new_clauses, i));
  tommy_array_done(&reasoner->core_kb->new_clauses);
  tommy_array_done(&reasoner->core_kb->timestep_delay_clauses);

  tommy_array_done(&reasoner->core_kb->distrust_set);
  tommy_array_done(&reasoner->core_kb->distrust_parents);
  tommy_array_done(&reasoner->core_kb->handle_set);
  tommy_array_done(&reasoner->core_kb->handle_parents);
  tommy_array_done(&reasoner->core_kb->retire_set);
  tommy_array_done(&reasoner->core_kb->retire_parents);

  tommy_array_done(&reasoner->core_kb->pos_lit_reinstates);
  tommy_array_done(&reasoner->core_kb->neg_lit_reinstates);

  tommy_array_done(&reasoner->core_kb->trues);

  tommy_node *curr = tommy_list_head(&reasoner->core_kb->clauses);
  while (curr) {
    index_mapping *data = curr->data;
    curr = curr->next;
    free_clause(data->value);
    free(data);
  }
  tommy_hashlin_done(&reasoner->core_kb->index_map);

  tommy_list_foreach(&reasoner->core_kb->pos_list, free_predname_mapping);
  tommy_hashlin_done(&reasoner->core_kb->pos_map);

  tommy_list_foreach(&reasoner->core_kb->neg_list, free_predname_mapping);
  tommy_hashlin_done(&reasoner->core_kb->neg_map);

  tommy_hashlin_foreach(&reasoner->core_kb->fif_map, free_fif_mapping);
  tommy_hashlin_done(&reasoner->core_kb->fif_map);

  // Res task pointers are aliases to those freed from reasoner->clauses, so only free overall task here
  for (tommy_size_t i = 0; i < tommy_array_size(&reasoner->core_kb->res_tasks); i++)
    free(tommy_array_get(&reasoner->core_kb->res_tasks, i));
  tommy_array_done(&reasoner->core_kb->res_tasks);

  tommy_hashlin_foreach(&reasoner->core_kb->fif_tasks, free_fif_task_mapping);
  tommy_hashlin_done(&reasoner->core_kb->fif_tasks);

  curr = tommy_list_head(&reasoner->backsearch_tasks);
  while (curr) {
    backsearch_task *data = curr->data;
    curr = curr->next;
    backsearch_halt(data);
  }

  if (logs_on) {
    fclose(reasoner->almalog);
  }

  free(reasoner->core_kb);
  free(reasoner);

  parse_cleanup();
}

void alma_assert(alma *reasoner, char *string, kb_str *buf) {
  kb_logger logger;
  logger.log = reasoner->almalog;
  logger.buf = buf;
  assert_formula(reasoner->core_kb, string, 1, &logger);
}

void alma_remove(alma *reasoner, char *string, kb_str *buf) {
  kb_logger logger;
  logger.log = reasoner->almalog;
  logger.buf = buf;
  delete_formula(reasoner->core_kb, reasoner->time, string, 1, &logger);
}

void alma_update(alma *reasoner, char *string, kb_str *buf) {
  kb_logger logger;
  logger.log = reasoner->almalog;
  logger.buf = buf;
  update_formula(reasoner->core_kb, reasoner->time, string, &logger);
}

void alma_observe(alma *reasoner, char *string, kb_str *buf) {
  kb_logger logger;
  logger.log = reasoner->almalog;
  logger.buf = buf;

  // Parse string into clauses
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas, &logger)) {
    tommy_array arr;
    tommy_array_init(&arr);
    nodes_to_clauses(formulas, formula_count, &arr, 0, &reasoner->core_kb->variable_id_count, &logger);

    for (int i = 0; i < tommy_array_size(&arr); i++) {
      clause *c = tommy_array_get(&arr, i);
      if (c->pos_count + c->neg_count == 1) {
        alma_function *lit = (c->pos_count == 1) ? c->pos_lits[0] : c->neg_lits[0];
        lit->term_count++;
        lit->terms = realloc(lit->terms, sizeof(*lit->terms)*lit->term_count);
        func_from_long(lit->terms+(lit->term_count-1), reasoner->time+1);
        tommy_array_insert(&reasoner->core_kb->new_clauses, c);
        tee_alt("-a: ", &logger);
        clause_print(c, &logger);
        tee_alt(" observed\n", &logger);
      }
      else {
        clause_print(c, &logger);
        tee_alt(" has too many literals\n", &logger);
        free_clause(c);
      }
    }
    tommy_array_done(&arr);
  }
}

void alma_backsearch(alma *reasoner, char *string, kb_str *buf) {
  kb_logger logger;
  logger.log = reasoner->almalog;
  logger.buf = buf;

  // Parse string into clauses
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas, &logger)) {
    tommy_array arr;
    tommy_array_init(&arr);
    nodes_to_clauses(formulas, formula_count, &arr, 0, &reasoner->core_kb->variable_id_count, &logger);

    clause *c = tommy_array_get(&arr, 0);
    // Free all after first
    for (int i = 1; i < tommy_array_size(&arr); i++)
      free_clause(tommy_array_get(&arr, i));
    tommy_array_done(&arr);
    if (c != NULL) {
      if (c->pos_count + c->neg_count > 1) {
        tee_alt("-a: query clause has too many literals\n", &logger);
        free_clause(c);
      }
      else {
        reasoner->idling = 0;
        backsearch_from_clause(&reasoner->backsearch_tasks, c);
        tee_alt("-a: Backsearch initiated for ", &logger);
        clause_print(c, &logger);
        tee_alt("\n", &logger);
      }
    }
  }
}
