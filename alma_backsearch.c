#include <string.h>
#include "alma_backsearch.h"

static void collect_variables(alma_term *term, binding_list *b, int quote_level, void *parent) {
  if (term->type == VARIABLE) {
    b->num_bindings++;
    b->list = realloc(b->list, sizeof(*b->list) * b->num_bindings);
    b->list[b->num_bindings-1].var = malloc(sizeof(alma_variable));
    copy_alma_var(term->variable, b->list[b->num_bindings-1].var);
    b->list[b->num_bindings-1].quote_level = quote_level;
    b->list[b->num_bindings-1].term = malloc(sizeof(alma_term));
    copy_alma_term(term, b->list[b->num_bindings-1].term);
  }
  else if (term->type == FUNCTION) {
    for (int i = 0; i < term->function->term_count; i++)
      collect_variables(term->function->terms+i, b, quote_level, parent);
  }
  else if (term->type == QUOTE && term->quote->type == CLAUSE) {
    clause *c = term->quote->clause_quote;
    for (int i = 0; i < c->pos_count; i++)
      for (int j = 0; j < c->pos_lits[i]->term_count; j++)
        collect_variables(c->pos_lits[i]->terms+j, b, quote_level+1, parent);
    for (int i = 0; i < c->neg_count; i++)
      for (int j = 0; j < c->neg_lits[i]->term_count; j++)
        collect_variables(c->neg_lits[i]->terms+j, b, quote_level+1, parent);
  }
  else {
    // TODO quasi-quote case
  }
}

// Initializes backward search with negation of clause argument
void backsearch_from_clause(tommy_list *backsearch_tasks, clause *c) {
  backsearch_task *bt = malloc(sizeof(*bt));
  bt->clause_count = 0;
  c->index = --bt->clause_count;
  bt->target = c;

  clause *negated = malloc(sizeof(*negated));
  memcpy(negated, c, sizeof(*negated));
  alma_function *negated_lit;
  if (c->pos_count == 1) {
    negated->neg_count = 1;
    negated->neg_lits = malloc(sizeof(*negated->neg_lits));
    negated_lit = negated->neg_lits[0] = malloc(sizeof(*negated->neg_lits[0]));
    copy_alma_function(c->pos_lits[0], negated->neg_lits[0]);
    negated->pos_count = 0;
    negated->pos_lits = NULL;
  }
  else {
    negated->pos_count = 1;
    negated->pos_lits = malloc(sizeof(*negated->pos_lits));
    negated_lit = negated->pos_lits[0] = malloc(sizeof(*negated->pos_lits[0]));
    copy_alma_function(c->neg_lits[0], negated->pos_lits[0]);
    negated->neg_count = 0;
    negated->neg_lits = NULL;
  }

  tommy_array_init(&bt->clauses);
  tommy_hashlin_init(&bt->clause_bindings);

  tommy_array_init(&bt->new_clauses);
  tommy_array_insert(&bt->new_clauses, negated);
  tommy_array_init(&bt->new_clause_bindings);
  binding_list *negated_bindings = malloc(sizeof(*negated_bindings));
  init_bindings(negated_bindings);
  // Create initial bindings where each variable in negated target is present, but each term is set to NULL
  for (int i = 0; i < negated_lit->term_count; i++)
    collect_variables(negated_lit->terms+i, negated_bindings, 0, negated_lit);
  bt->target_vars = negated_bindings;
  tommy_array_insert(&bt->new_clause_bindings, negated_bindings);

  tommy_array_init(&bt->to_resolve);
  tommy_list_insert_tail(backsearch_tasks, &bt->node, bt);
}

static clause* backsearch_duplicate_check(backsearch_task *task, clause *c) {
  for (int i = 0; i < tommy_array_size(&task->clauses); i++) {
    clause *compare = tommy_array_get(&task->clauses, i);
    if (compare->tag == c->tag && !clauses_differ(c, compare, 0))
      return compare;
  }
  return NULL;
}

// Given particular task, populate to_resolve based on each clause
void generate_backsearch_tasks(kb *collection, long time, backsearch_task *bt) {
  for (int i = 0; i < tommy_array_size(&bt->new_clauses); i++) {
    clause *c = tommy_array_get(&bt->new_clauses, i);
    clause *dupe = duplicate_check(collection, time, c, 0);
    int bs_dupe = 0;
    if (dupe == NULL) {
      dupe = backsearch_duplicate_check(bt, c);
      bs_dupe = 1;
    }

    if (dupe == NULL) {
      c->index = --bt->clause_count;

      // Obtain tasks between new backward search clauses and KB items
      make_res_tasks(c, c->pos_count, c->pos_lits, &collection->neg_map, &bt->to_resolve, 1, 0);
      make_res_tasks(c, c->neg_count, c->neg_lits, &collection->pos_map, &bt->to_resolve, 1, 1);

      // Obtain tasks between pairs of backsearch clauses
      // Currently a linear scan, perhaps to adjust later
      for (int j = 0; j < tommy_array_size(&bt->clauses); j++) {
        clause *jth = tommy_array_get(&bt->clauses, j);

        for (int k = 0; k < c->pos_count; k++)
          make_single_task(c, c->pos_lits[k], jth, &bt->to_resolve, 1, 0);
        for (int k = 0; k < c->neg_count; k++)
          make_single_task(c, c->neg_lits[k], jth, &bt->to_resolve, 1, 1);
      }

      tommy_array_insert(&bt->clauses, c);

      // Insert corresponding binding into hashmap
      binding_mapping *entry = malloc(sizeof(*entry));
      entry->key = c->index;
      entry->bindings = tommy_array_get(&bt->new_clause_bindings, i);
      tommy_hashlin_insert(&bt->clause_bindings, &entry->node, entry, tommy_hash_u64(0, &entry->key, sizeof(entry->key)));
    }
    else {
      if (bs_dupe)
        transfer_parent(dupe, c, 0);
      cleanup_bindings(tommy_array_get(&bt->new_clause_bindings, i));
      free_clause(c);
    }
  }
  tommy_array_done(&bt->new_clauses);
  tommy_array_init(&bt->new_clauses);
  tommy_array_done(&bt->new_clause_bindings);
  tommy_array_init(&bt->new_clause_bindings);
}

// Advances by resolving available tasks
void process_backsearch_tasks(kb *collection, long time, tommy_list *backsearch_tasks, kb_logger *logger) {
  tommy_node *curr = tommy_list_head(backsearch_tasks);
  while (curr) {
    backsearch_task *t = curr->data;
    process_res_tasks(collection, time, &t->to_resolve, &t->new_clauses, t, logger);
    curr = curr->next;
  }
}

static void free_binding_mapping(void *arg) {
  binding_mapping *bm = arg;
  cleanup_bindings(bm->bindings);
  free(bm);
}

void backsearch_halt(backsearch_task *t) {
  free_clause(t->target);

  for (tommy_size_t i = 0; i < tommy_array_size(&t->clauses); i++)
    free_clause(tommy_array_get(&t->clauses, i));
  tommy_array_done(&t->clauses);
  tommy_hashlin_foreach(&t->clause_bindings, free_binding_mapping);
  tommy_hashlin_done(&t->clause_bindings);

  for (tommy_size_t i = 0; i < tommy_array_size(&t->new_clauses); i++)
    free_clause(tommy_array_get(&t->new_clauses, i));
  tommy_array_done(&t->new_clauses);
  for (tommy_size_t i = 0; i < tommy_array_size(&t->new_clause_bindings); i++)
    cleanup_bindings(tommy_array_get(&t->new_clause_bindings, i));
  tommy_array_done(&t->new_clause_bindings);

  for (tommy_size_t i = 0; i < tommy_array_size(&t->to_resolve); i++)
    free(tommy_array_get(&t->to_resolve, i));
  tommy_array_done(&t->to_resolve);

  free(t);
}

int idling_backsearch(tommy_list *backsearch_tasks) {
  tommy_node *i = tommy_list_head(backsearch_tasks);
  while (i) {
    backsearch_task *bt = i->data;
    if (tommy_array_size(&bt->to_resolve) > 0)
      return 0;
    i = i->next;
  }
  return 1;
}

// Compare function to be used by tommy_hashlin_search for binding_mapping
// Compares long arg to key of binding_mapping
int bm_compare(const void *arg, const void *obj) {
  return *(const long*)arg - ((const binding_mapping*)obj)->key;
}
