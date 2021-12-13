#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
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

// Returns a boolean for presence of new clauses in any level of KB
static int new_clauses_present(kb *collection) {
  if (tommy_array_size(&collection->new_clauses) > 0)
    return 1;
  for (int i = 0; i < collection->agent_count; i++) {
    if (new_clauses_present(collection->agents[i].pos) || new_clauses_present(collection->agents[i].neg))
      return 1;
  }
  return 0;
}

// Checks for c's membership in KB with index search and matching pointer
static int clause_in_kb(kb *collection, clause *c) {
  index_mapping *result = tommy_hashlin_search(&collection->index_map, im_compare, &c->index, tommy_hash_u64(0, &c->index, sizeof(c->index)));
  return result != NULL && result->value == c;
}

// Regenerate tasks for unpaused clause
static int unpaused_task_regen(kb *collection, clause *c) {
  if (clause_in_kb(collection, c)) {
    fif_tasks_from_clause(&collection->fif_tasks, c);
    res_tasks_from_clause(collection, c, 1);
    return 1;
  }

  for (int j = 0; j < collection->agent_count; j++) {
    if (unpaused_task_regen(collection->agents[j].pos, c) || unpaused_task_regen(collection->agents[j].pos, c))
      return 1;
  }
  return 0;
}

// Boolean return based on if KB had any new (non-now) clauses this step
static int process_new_reasoner_clauses(alma *reasoner, kb_logger *logger, int make_tasks) {
  // Continue processing clauses until KB segments have new_clauses exhausted
  int has_new_clauses = new_clauses_present(reasoner->core_kb);
  tommy_array unpaused;
  tommy_array_init(&unpaused);
  do {
    process_new_clauses(reasoner->core_kb, reasoner->procs, reasoner->time, make_tasks, &unpaused, logger);
  } while (new_clauses_present(reasoner->core_kb));

  process_meta_clauses(reasoner->core_kb, reasoner->time, &unpaused, logger);

  // Clock rule goes into core KB last
  char *timestep = now(reasoner->time);
  assert_formula(reasoner->core_kb, timestep, 0, logger);
  if (reasoner->prev == NULL) {
    reasoner->prev = timestep;
  }
  else {
    reasoner->now = timestep;
    delete_formula(reasoner->core_kb, reasoner->time, reasoner->prev, 0, logger);
    free(reasoner->prev);
    reasoner->prev = reasoner->now;
  }

  // Meta clause processing may have given last clauses to sync and process (e.g. distrusted)
  process_new_clauses(reasoner->core_kb, reasoner->procs, reasoner->time, make_tasks, &unpaused, logger);

  for (int i = 0; i < tommy_array_size(&unpaused); i++)
    unpaused_task_regen(reasoner->core_kb, tommy_array_get(&unpaused, i));
  tommy_array_done(&unpaused);

  return has_new_clauses;
}

static void kb_task_init(kb *collection, alma_proc *procs, long time, kb_logger *logger) {
  // Generate starting tasks
  tommy_node *curr = tommy_list_head(&collection->clauses);
  while (curr) {
    clause *c = ((index_mapping*)curr->data)->value;
    if (c->tag == FIF)
      fif_task_map_init(collection, procs, &collection->fif_tasks, c, 0);
    else {
      res_tasks_from_clause(collection, c, 0);
      fif_tasks_from_clause(&collection->fif_tasks, c);
    }
    curr = curr->next;
  }

  for (int i = 0; i < collection->agent_count; i++) {
    kb_task_init(collection->agents[i].pos, procs, time, logger);
    kb_task_init(collection->agents[i].neg, procs, time, logger);
  }
}

// Caller will need to free reasoner with alma_halt
void alma_init(alma *reasoner, char **files, int file_count, char *agent, char *trialnum, char *log_dir, int verbose, kb_str *buf, int logon) {
  reasoner->time = 0;
  reasoner->prev = reasoner->now = NULL;
  reasoner->idling = 0;
  reasoner->core_kb = malloc(sizeof(*reasoner->core_kb));

  if (agent == NULL) {
    agent = "alma";
  }
  kb_init(reasoner->core_kb, agent, verbose);

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
      tommy_array temp;
      tommy_array_init(&temp);
      if (clauses_from_source(files[i], 1, &temp, &reasoner->core_kb->variable_id_count, &logger)) {
        for (tommy_size_t j = 0; j < tommy_array_size(&temp); j++) {
          tommy_array_insert(&reasoner->core_kb->new_clauses, tommy_array_get(&temp, j));
        }
        tommy_array_done(&temp);
      }
      // If any file cannot parse, cleanup and exit
      else {
        free(files);
        tommy_array_done(&temp);
        alma_halt(reasoner);
        exit(0);
      }
    }
  }

  int namelen = strlen(agent);
  char *namestr = malloc(10 + namelen + 3);
  strcpy(namestr, "agentname(");
  strcpy(namestr + 10, agent);
  strcpy(namestr + 10 + namelen, ").");
  assert_formula(reasoner->core_kb, namestr, 0, &logger);
  free(namestr);

  process_new_reasoner_clauses(reasoner, &logger, 0);

  kb_task_init(reasoner->core_kb, reasoner->procs, reasoner->time, &logger);
}

// Checks whether any KB area has time-delay clauses to be added next timestep
static int has_delay_clauses(kb *collection) {
  if (tommy_array_size(&collection->timestep_delay_clauses) > 0)
    return 1;
  for (int i = 0; i < collection->agent_count; i++) {
    if (has_delay_clauses(collection->agents[i].pos) || has_delay_clauses(collection->agents[i].neg))
      return 1;
  }
  return 0;
}

// Processes tasks in all KB areas
static void kb_task_process(kb *collection, struct alma_proc *procs, long time, kb_logger *logger) {
  // Move delayed clauses into new_clauses
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->timestep_delay_clauses); i++) {
    clause *c = tommy_array_get(&collection->timestep_delay_clauses, i);
    tommy_array_insert(&collection->new_clauses, c);
  }
  tommy_array_done(&collection->timestep_delay_clauses);
  tommy_array_init(&collection->timestep_delay_clauses);

  // Process resolution and fif tasks
  process_res_tasks(collection, time, &collection->res_tasks, &collection->new_clauses, NULL, logger);
  process_fif_tasks(collection, procs, logger);

  for (int i = 0; i < collection->agent_count; i++) {
    kb_task_process(collection->agents[i].pos, procs, time, logger);
    kb_task_process(collection->agents[i].neg, procs, time, logger);
  }
}

void alma_step(alma *reasoner, kb_str *buf) {
  kb_logger logger;
  logger.log = reasoner->almalog;
  logger.buf = buf;

  reasoner->time++;

  kb_task_process(reasoner->core_kb, reasoner->procs, reasoner->time, &logger);
  process_backsearch_tasks(reasoner->core_kb, reasoner->time, &reasoner->backsearch_tasks, &logger);

  int has_new_clauses = process_new_reasoner_clauses(reasoner, &logger, 1);

  tommy_node *i = tommy_list_head(&reasoner->backsearch_tasks);
  while (i) {
    generate_backsearch_tasks(reasoner->core_kb, reasoner->time, i->data);
    i = i->next;
  }

  // System idles if there are no resolution tasks, backsearch tasks, or to_unify fif values
  // First of these is true when no new clauses (source of res tasks) exist
  // To_unify values are all removed in current implementation each step from exhaustive fif
  // Thus, idling when there are no new_clauses AND not finding tasks in backsearch
  reasoner->idling = !has_new_clauses && idling_backsearch(&reasoner->backsearch_tasks) && !has_delay_clauses(reasoner->core_kb);
  if (reasoner->idling)
    tee_alt("-a: Idling...\n", &logger);
}

void alma_print(alma *reasoner, kb_str *buf) {
  kb_logger logger;
  logger.log = reasoner->almalog;
  logger.buf = buf;

  kb_print(reasoner->core_kb, 0, &logger);

  tommy_node* i = tommy_list_head(&reasoner->backsearch_tasks);
  if (i) {
    tee_alt("Back searches:\n", &logger);
    for (int count = 0; i != NULL; i = i->next, count++) {
      tee_alt("bs %d\n",  &logger, count);
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

  kb_halt(reasoner->core_kb);

  tommy_node *curr = tommy_list_head(&reasoner->backsearch_tasks);
  while (curr) {
    backsearch_task *data = curr->data;
    curr = curr->next;
    backsearch_halt(data);
  }

  if (logs_on) {
    fclose(reasoner->almalog);
  }

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
  tommy_array arr;
  tommy_array_init(&arr);
  if (clauses_from_source(string, 0, &arr, &reasoner->core_kb->variable_id_count, &logger)) {

    for (int i = 0; i < tommy_array_size(&arr); i++) {
      clause *c = tommy_array_get(&arr, i);
      if (c->pos_count + c->neg_count == 1) {
        alma_function *lit = (c->pos_count == 1) ? c->pos_lits[0] : c->neg_lits[0];
        lit->term_count++;
        lit->terms = realloc(lit->terms, sizeof(*lit->terms)*lit->term_count);
        func_from_long(lit->terms+(lit->term_count-1), reasoner->time+1);
        tommy_array_insert(&reasoner->core_kb->new_clauses, c);
        tee_alt("-a: ", &logger);
        non_kb_clause_print(c, &logger);
        tee_alt(" observed\n", &logger);
      }
      else {
        non_kb_clause_print(c, &logger);
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

  tommy_array arr;
  tommy_array_init(&arr);
  if (clauses_from_source(string, 0, &arr, &reasoner->core_kb->variable_id_count, &logger)) {
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
        non_kb_clause_print(c, &logger);
        tee_alt("\n", &logger);
      }
    }
  }
}
