#include <string.h>
#include <limits.h>
#include "alma_clause.h"
#include "alma_fif.h"

static void add_lit(int *count, alma_function ***lits, alma_function *new_lit) {
  (*count)++;
  *lits = realloc(*lits, sizeof(**lits) * *count);
  (*lits)[*count-1] = new_lit;
  quote_convert_func((*lits)[*count-1]);
}

static void init_record_tree(record_tree *records, int quote_level, record_tree *parent) {
  records->quote_level = quote_level;
  records->next_level_count = 0;
  records->next_level = NULL;
  records->variable_count = 0;
  records->variables = NULL;
  records->parent = parent;
}

static void free_record_tree(record_tree *records) {
  for (int i = 0; i < records->next_level_count; i++) {
    free_record_tree(records->next_level[i]);
    free(records->next_level[i]);
  }
  free(records->next_level);

  for (int i = 0; i < records->variable_count; i++)
    free(records->variables[i]);
  free(records->variables);
}

static name_record* records_retrieve(record_tree *records, int quote_level, long long query_id) {
  if (quote_level == records->quote_level) {
    for (int i = 0; i < records->variable_count; i++)
      if (records->variables[i]->old_id == query_id)
        return records->variables[i];
  }
  // Otherwise, records->quote_level is less than query quote level
  else {
    for (int i = 0; i < records->next_level_count; i++) {
      name_record *rec = records_retrieve(records->next_level[i], quote_level, query_id);
      if (rec != NULL)
        return rec;
    }
  }
  return NULL;
}

/*static void records_print(record_tree *records) {
  printf("Level %d: ", records->quote_level);
  if (records->variable_count == 0)
    printf("None\n");
  for (int i = 0; i < records->variable_count; i++) {
    printf("%lld", records->variables[i]->new_id);
    if (i == records->variable_count-1)
      printf("\n");
    else
      printf(" ");
  }

  for (int i = 0; i < records->next_level_count; i++) {
    records_print(records->next_level[i]);
  }
}*/


// For a single ALMA variable, set fields in record_tree
static void set_var_name(record_tree *records, alma_variable *var, int id_from_name, int quote_level, long long *id_count) {
  // If a match for the variable is found in the record tree, set ID using new field
  for (int i = 0; i < records->variable_count; i++) {
    name_record *rec = records->variables[i];
    if ((!id_from_name && var->id == rec->old_id) ||
        (id_from_name && strcmp(var->name, rec->name) == 0)) {
      var->id = rec->new_id;
      return;
    }
  }
  // If did not find match from records, create a new one
  records->variable_count++;
  records->variables = realloc(records->variables, sizeof(*records->variables) * records->variable_count);
  records->variables[records->variable_count-1] = malloc(sizeof(*records->variables[records->variable_count-1]));
  if (id_from_name)
    records->variables[records->variable_count-1]->name = var->name;
  else
    records->variables[records->variable_count-1]->old_id = var->id;
  var->id = records->variables[records->variable_count-1]->new_id = *id_count;
  (*id_count)++;
}

static void set_variable_ids_rec(record_tree *records, clause *c, int id_from_name, int non_escaping_only, int quote_level, long long *id_count);

static void set_variable_names(record_tree *records, alma_term *term, int id_from_name, int non_escaping_only, int quote_level, long long *id_count) {
  if (term->type == VARIABLE && (!non_escaping_only || quote_level > 0)) {
    set_var_name(records, term->variable, id_from_name, quote_level, id_count);
  }
  else if (term->type == FUNCTION) {
    for (int i = 0; i < term->function->term_count; i++)
      set_variable_names(records, term->function->terms+i, id_from_name, non_escaping_only, quote_level, id_count);
  }
  else if (term->type == QUOTE) {
    if (term->quote->type == CLAUSE) {
      // Create new child record_tree for further nested quotation
      records->next_level_count++;
      records->next_level = realloc(records->next_level, sizeof(*records->next_level) * records->next_level_count);
      records->next_level[records->next_level_count-1] = malloc(sizeof(*records->next_level[records->next_level_count-1]));
      init_record_tree(records->next_level[records->next_level_count-1], quote_level+1, records);

      set_variable_ids_rec(records->next_level[records->next_level_count-1], term->quote->clause_quote, id_from_name, non_escaping_only, quote_level+1, id_count);
    }
  }
  else if (term->type == QUASIQUOTE && (!non_escaping_only || quote_level > term->quasiquote->backtick_count)) {
    // Trace back up along parents based on amount of quasi-quotation
    for (int i = 0; i < term->quasiquote->backtick_count; i++)
      records = records->parent;
    set_var_name(records, term->quasiquote->variable, id_from_name, quote_level - term->quasiquote->backtick_count, id_count);
  }
}

static void set_variable_ids_rec(record_tree *records, clause *c, int id_from_name, int non_escaping_only, int quote_level, long long *id_count) {
  for (int i = 0; i < c->pos_count; i++)
    for (int j = 0; j < c->pos_lits[i]->term_count; j++)
      set_variable_names(records, c->pos_lits[i]->terms+j, id_from_name, non_escaping_only, quote_level, id_count);
  for (int i = 0; i < c->neg_count; i++)
    for (int j = 0; j < c->neg_lits[i]->term_count; j++)
      set_variable_names(records, c->neg_lits[i]->terms+j, id_from_name, non_escaping_only, quote_level, id_count);

  if (c->tag == FIF)
    for (int i = 0; i < c->fif->num_conclusions; i++)
      set_variable_ids_rec(records, c->fif->conclusions[i], id_from_name, non_escaping_only, quote_level, id_count);
}


static void set_clause_from_records(record_tree *records, clause *c, int quote_level, long long *id_count);

// Use existing assembled record_tree to update ID values from old_id to new_id in the argument term
static void set_term_from_records(record_tree *records, alma_term *term, int quote_level, long long *id_count) {
  if (term->type == VARIABLE) {
    name_record *rec = records_retrieve(records, quote_level, term->variable->id);
    if (rec != NULL)
      term->variable->id = rec->new_id;
  }
  else if (term->type == FUNCTION) {
    for (int i = 0; i < term->function->term_count; i++)
      set_term_from_records(records, term->function->terms+i, quote_level, id_count);
  }
  else if (term->type == QUOTE) {
    if (term->quote->type == CLAUSE) {
      set_clause_from_records(records, term->quote->clause_quote, quote_level+1, id_count);
    }
  }
  else {
    // Trace back up along parents based on amount of quasi-quotation
    for (int i = 0; i < term->quasiquote->backtick_count; i++)
      records = records->parent;
    name_record *rec = records_retrieve(records, quote_level - term->quasiquote->backtick_count, term->quasiquote->variable->id);
    if (rec != NULL)
      term->variable->id = rec->new_id;
  }
}

static void set_clause_from_records(record_tree *records, clause *c, int quote_level, long long *id_count) {
  for (int i = 0; i < c->pos_count; i++)
    for (int j = 0; j < c->pos_lits[i]->term_count; j++)
      set_term_from_records(records, c->pos_lits[i]->terms+j, quote_level, id_count);
  for (int i = 0; i < c->neg_count; i++)
    for (int j = 0; j < c->neg_lits[i]->term_count; j++)
      set_term_from_records(records, c->neg_lits[i]->terms+j, quote_level, id_count);

  if (c->tag == FIF)
    for (int i = 0; i < c->fif->num_conclusions; i++)
      set_clause_from_records(records, c->fif->conclusions[i], quote_level, id_count);
}


// Given a clause, assign the ID fields of each variable
// Two cases:
// 1) If clause is result of resolution, replace existing matching IDs with fresh values each
// id_from_name is false in this case; distinct variables are tracked using ID given names may not be unique
// 2) Otherwise, give variables with the same name matching IDs
// id_from_name is true in this case; distinct variables are tracked by name
// non_escaping_only sets variable IDs only in the case where they don't escape quotation
// Fresh ID values drawn from variable_id_count global variable
void set_variable_ids(clause *c, int id_from_name, int non_escaping_only, binding_list *bs_bindings, long long *id_count) {
  record_tree *records;
  records = malloc(sizeof(*records));
  init_record_tree(records, 0, NULL);

  set_variable_ids_rec(records, c, id_from_name, non_escaping_only, 0, id_count);

  // If bindings for a backsearch have been passed in, update variable names for them as well
  // Id_from_name always false in this case; omits that parameter
  if (bs_bindings)
    for (int i = 0; i < bs_bindings->num_bindings; i++)
      // TODO: eventually needs to get quote_level out based on binding?
      set_term_from_records(records, bs_bindings->list[i].term, 0, id_count);

  //records_print(records);
  free_record_tree(records);
  free(records);
}

static void init_ordering_rec(fif_info *info, alma_node *node, int *next, int *pos, int *neg) {
  if (node->type == FOL) {
    // Pos lit case for NOT
    if (node->fol->op == NOT)
      info->ordering[(*next)++] = (*pos)++;
    // Case of node is AND
    else {
      init_ordering_rec(info, node->fol->arg1, next, pos, neg);
      init_ordering_rec(info, node->fol->arg2, next, pos, neg);
    }
  }
  // Otherwise, neg lit
  else
    info->ordering[(*next)++] = (*neg)--;
}

// Given a node for a fif formula, record inorder traversal ofpositive and negative literals
static void init_ordering(fif_info *info, alma_node *node) {
  int next = 0;
  int pos = 0;
  int neg = -1;
  // Fif node must necessarily at top level have an OR -- ignore second branch of with conclusion
  init_ordering_rec(info, node->fol->arg1, &next, &pos, &neg);
}

int clauses_from_source(char *source, int file_src, tommy_array *clauses, long long *id_count, kb_logger *logger) {
  alma_node *formulas;
  int formula_count = fol_from_source(source, file_src, &formulas, logger);
  if (formula_count == 0)
    return 0;

  nodes_to_clauses(formulas, formula_count, clauses, id_count);
  return 1;
}

static void make_fif_conclusions(alma_node *node, clause *c) {
  if (node != NULL) {
    if (node->type == FOL && node->fol->op == AND) {
      make_fif_conclusions(node->fol->arg1, c);
      make_fif_conclusions(node->fol->arg2, c);
    }
    else {
      c->fif->num_conclusions++;
      c->fif->conclusions = realloc(c->fif->conclusions, sizeof(*c->fif->conclusions) * c->fif->num_conclusions);
      clause *latest = malloc(sizeof(*latest));
      make_clause(node, latest);
      c->fif->conclusions[c->fif->num_conclusions-1] = latest;
    }
  }
}

static void make_clause_rec(alma_node *node, clause *c, int invert) {
  // Note: predicate null assignment necessary for freeing of nodes without losing predicates
  if (node != NULL) {
    if (node->type == FOL) {
      // Neg lit case for NOT
      if (node->fol->op == NOT) {
        if (invert)
          add_lit(&c->pos_count, &c->pos_lits, node->fol->arg1->predicate);
        else
          add_lit(&c->neg_count, &c->neg_lits, node->fol->arg1->predicate);
        node->fol->arg1->predicate = NULL;
      }
      // Case if node is OR (or AND when processing FIF fragments)
      else {
        make_clause_rec(node->fol->arg1, c, invert);
        make_clause_rec(node->fol->arg2, c, invert);
      }
      if (c->tag == NONE)
        c->tag = node->fol->tag;
    }
    // Otherwise, only pos lit left
    else {
      if (invert)
        add_lit(&c->neg_count, &c->neg_lits, node->predicate);
      else
        add_lit(&c->pos_count, &c->pos_lits, node->predicate);
      node->predicate = NULL;
    }
  }
}

// Converts a node into a clause destructively (leaves are moved to the clause literals)
void make_clause(alma_node *node, clause *c) {
  c->pos_count = c->neg_count = 0;
  c->pos_lits = c->neg_lits = NULL;
  c->parent_set_count = c->children_count = 0;
  c->parents = NULL;
  c->children = NULL;
  c->equiv_belief = NULL;
  c->tag = NONE;
  c->fif = NULL;

  if (node->type == FOL && node->fol->tag == FIF) {
    // Clause pos and neg lits initialize from fif premises
    make_clause_rec(node->fol->arg1, c, 1);

    // Initialize fif info except for conclusions
    c->tag = FIF;
    c->fif = malloc(sizeof(*c->fif));
    c->fif->premise_count = c->pos_count + c->neg_count;
    c->fif->ordering = malloc(sizeof(*c->fif->ordering) * c->fif->premise_count);
    init_ordering(c->fif, node);

    // Initialize fif conclusion structures
    c->fif->num_conclusions = 0;
    c->fif->conclusions = NULL;
    make_fif_conclusions(node->fol->arg2, c);

    if (c->fif->conclusions[0]->pos_count > 0)
      c->fif->indexing_conc = c->fif->conclusions[0]->pos_lits[0];
    else
      c->fif->indexing_conc = c->fif->conclusions[0]->neg_lits[0];
  }
  else {
    make_clause_rec(node, c, 0);
    // Non-fif clauses can be sorted by literal for ease of resolution
    qsort(c->pos_lits, c->pos_count, sizeof(*c->pos_lits), function_compare);
    qsort(c->neg_lits, c->neg_count, sizeof(*c->neg_lits), function_compare);
  }
}

// Flattens a single alma node and adds its contents to clauses array
// Recursively calls when an AND is found to separate conjunctions
static void flatten_node(alma_node *node, tommy_array *clauses) {
  if (node->type == FOL && node->fol->tag != FIF && node->fol->op == AND) {
    if (node->fol->arg1->type == FOL)
      node->fol->arg1->fol->tag = node->fol->tag;
    flatten_node(node->fol->arg1, clauses);
    if (node->fol->arg2->type == FOL)
      node->fol->arg2->fol->tag = node->fol->tag;
    flatten_node(node->fol->arg2, clauses);
  }
  else {
    clause *c = malloc(sizeof(*c));
    c->dirty_bit = 1;
    c->pyobject_bit = (char) 1;
    make_clause(node, c);
    tommy_array_insert(clauses, c);
  }
}

void free_clause(clause *c) {
  if (c == NULL)
    return;

  for (int i = 0; i < c->pos_count; i++)
    free_function(c->pos_lits[i]);
  for (int i = 0; i < c->neg_count; i++)
    free_function(c->neg_lits[i]);
  free(c->pos_lits);
  free(c->neg_lits);
  for (int i = 0; i < c->parent_set_count; i++)
    free(c->parents[i].clauses);
  free(c->parents);
  free(c->children);

  if (c->tag == FIF) {
    free(c->fif->ordering);
    for (int i = 0; i < c->fif->num_conclusions; i++)
      free_clause(c->fif->conclusions[i]);
    free(c->fif->conclusions);
    free(c->fif);
  }

  free(c);
}

// Space for copy must be allocated before call
// Does not copy parents/children/index/acquired/equiv_belief
void copy_clause_structure(clause *original, clause *copy) {
  copy->pos_count = original->pos_count;
  copy->neg_count = original->neg_count;
  copy->pos_lits = copy->pos_count > 0 ? malloc(sizeof(*copy->pos_lits)*copy->pos_count) : NULL;
  for (int i = 0; i < copy->pos_count; i++) {
    copy->pos_lits[i] = malloc(sizeof(*copy->pos_lits[i]));
    copy_alma_function(original->pos_lits[i], copy->pos_lits[i]);
  }
  copy->neg_lits = copy->neg_count > 0 ? malloc(sizeof(*copy->neg_lits)*copy->neg_count) : NULL;
  for (int i = 0; i < copy->neg_count; i++) {
    copy->neg_lits[i] = malloc(sizeof(*copy->neg_lits[i]));
    copy_alma_function(original->neg_lits[i], copy->neg_lits[i]);
  }
  copy->parent_set_count = copy->children_count = 0;
  copy->parents = NULL;
  copy->children = NULL;
  copy->equiv_belief = NULL;
  copy->tag = original->tag;
  if (copy->tag == FIF) {
    copy->fif = malloc(sizeof(*copy->fif));
    copy->fif->premise_count = original->fif->premise_count;
    copy->fif->ordering = malloc(sizeof(*copy->fif->ordering)*copy->fif->premise_count);
    memcpy(copy->fif->ordering, original->fif->ordering, sizeof(*copy->fif->ordering)*copy->fif->premise_count);
    copy->fif->num_conclusions = original->fif->num_conclusions;
    copy->fif->conclusions = malloc(sizeof(*copy->fif->conclusions) * copy->fif->num_conclusions);
    for (int i = 0; i < copy->fif->num_conclusions; i++) {
      copy->fif->conclusions[i] = malloc(sizeof(*copy->fif->conclusions[i]));
      copy_clause_structure(original->fif->conclusions[i], copy->fif->conclusions[i]);
    }
    if (copy->fif->conclusions[0]->pos_count > 0)
      copy->fif->indexing_conc = copy->fif->conclusions[0]->pos_lits[0];
    else
      copy->fif->indexing_conc = copy->fif->conclusions[0]->neg_lits[0];
  }
  else
    copy->fif = NULL;
}

void copy_parents(clause *original, clause *copy) {
  copy->parent_set_count = original->parent_set_count;
  if (copy->parent_set_count > 0) {
    copy->parents = malloc(sizeof(*copy->parents)*copy->parent_set_count);
    for (int i = 0; i < copy->parent_set_count; i++) {
      copy->parents[i].count = original->parents[i].count;
      copy->parents[i].clauses = malloc(sizeof(*copy->parents[i].clauses)*copy->parents[i].count);
      for (int j = 0; j < copy->parents[i].count; j++)
        copy->parents[i].clauses[j] = original->parents[i].clauses[j];
    }
  }
  else
    copy->parents = NULL;
}

// Transfers first parent of source into parent sets of target if it's not a repeat
void transfer_parent(clause *target, clause *source, int add_children) {
  // Check if the duplicate clause is a repeat from the same parents
  int repeat_parents = 0;
  for (int j = 0; j < target->parent_set_count; j++) {
    int k = 0;
    for (; k < source->parents[0].count && k < target->parents[j].count; k++) {
      if (source->parents[0].clauses[k] != target->parents[j].clauses[k])
        break;
    }
    if (k == target->parents[j].count && k == source->parents[0].count) {
      repeat_parents = 1;
      break;
    }
  }

  // A duplicate clause derivation should be acknowledged by adding extra parents to the clause it duplicates
  // Copy parents of source to target
  // As a new clause target has only one parent set
  if (!repeat_parents) {
    target->parents = realloc(target->parents, sizeof(*target->parents) * (target->parent_set_count + 1));
    int last = target->parent_set_count;
    target->parents[last].count = source->parents[0].count;
    target->parents[last].clauses = malloc(sizeof(*target->parents[last].clauses) * target->parents[last].count);
    memcpy(target->parents[last].clauses, source->parents[0].clauses, sizeof(*target->parents[last].clauses) * target->parents[last].count);
    target->parent_set_count++;

    if (add_children) {
      // Parents of source also gain new child in target
      for (int j = 0; j < source->parents[0].count; j++) {
        int insert = 1;
        for (int k = 0; k < source->parents[0].clauses[j]->children_count; k++) {
          if (source->parents[0].clauses[j]->children[k] == target) {
            insert = 0;
            break;
          }
        }
        if (insert)
          add_child(source->parents[0].clauses[j], target);
      }
    }
  }
}

void add_child(clause *parent, clause *child) {
  parent->children_count++;
  parent->children = realloc(parent->children, sizeof(*parent->children) * parent->children_count);
  parent->children[parent->children_count-1] = child;
}

// Removes c from children list of p
void remove_child(clause *p, clause *c) {
  if (p != NULL) {
    for (int j = 0; j < p->children_count; j++) {
      if (p->children[j] == c) {
        if (j < p->children_count-1)
          p->children[j] = p->children[p->children_count-1];
        p->children_count--;
        if (p->children_count > 0) {
          p->children = realloc(p->children, sizeof(*p->children) * p->children_count);
        }
        else {
          free(p->children);
          p->children = NULL;
        }
        break;
      }
    }
  }
}


// Flattens trees into set of clauses (tommy_array must be initialized prior)
// trees argument freed here
void nodes_to_clauses(alma_node *trees, int num_trees, tommy_array *clauses, long long *id_count) {
  for (int i = 0; i < num_trees; i++) {
    flatten_node(trees+i, clauses);
    free_alma_tree(trees+i);
  }
  free(trees);
  for (int i = 0; i < tommy_array_size(clauses); i++) {
    clause *c = tommy_array_get(clauses, i);
    set_variable_ids(c, 1, 0, NULL, id_count);
  }
}

// Returns 0 if variable pair respects x and y matchings in matches arg outside of quotes
static int variables_differ(alma_variable *x, alma_variable *y, var_match_set *matches, int quote_level) {
  return !var_match_consistent(matches, quote_level, x, y);
}

static int functions_differ(alma_function *x, alma_function *y, var_match_set *matches, int quote_level);

static int clauses_differ_rec(clause *x, clause *y, var_match_set *matches, int quote_level) {
  if (x->tag != y->tag || x->pos_count != y->pos_count || x->neg_count != y->neg_count)
    return 1;

  if (x->tag == FIF) {
    if (x->fif->num_conclusions != y->fif->num_conclusions)
      return 1;
    for (int i = 0; i < x->fif->premise_count; i++)
      if (functions_differ(fif_access(x, i), fif_access(y, i), matches, quote_level))
        return 1;
    for (int i = 0; i < x->fif->num_conclusions; i++)
      if (clauses_differ_rec(x->fif->conclusions[i], y->fif->conclusions[i], matches, quote_level))
        return 1;
  }
  else {
    // TODO: account for case in which may have several literals with name
    // Ignoring this case, sorted literal lists allows comparing ith literals of each clause
    for (int i = 0; i < x->pos_count; i++)
      if (functions_differ(x->pos_lits[i], y->pos_lits[i], matches, quote_level))
        return 1;
    // TODO: account for case in which may have several literals with name
    // Ignoring this case, sorted literal lists allows comparing ith literals of each clause
    for (int i = 0; i < x->neg_count; i++)
      if (functions_differ(x->neg_lits[i], y->neg_lits[i], matches, quote_level))
        return 1;
  }

  return 0;
}

// Returns 0 if quotation terms are equal while respecting x and y matchings based on matches arg; otherwise returns 1
// Pairs of variables according to var_match_set arg corresponding to their level of quotation
static int quotes_differ(alma_quote *x, alma_quote *y, var_match_set *matches, int quote_level) {
  if (x->type == y->type) {
    if (x->type == SENTENCE) {
      // TODO when handling case for quote of formula equivalent to more than one clause
      return 1;
    }
    else
      return clauses_differ_rec(x->clause_quote, y->clause_quote, matches, quote_level);
  }
  return 1;
}

static int quasiquotes_differ(alma_quasiquote *x, alma_quasiquote *y, var_match_set *matches, int quote_level) {
  return x->backtick_count != y->backtick_count || !var_match_consistent(matches, quote_level - x->backtick_count, x->variable, y->variable);
}

// Returns 0 if functions are equal while respecting x and y matchings based on matches arg; otherwise returns 1
// Further detail in clauses_differ
static int functions_differ(alma_function *x, alma_function *y, var_match_set *matches, int quote_level) {
  if (x->term_count == y->term_count && strcmp(x->name, y->name) == 0) {
    for (int i = 0; i < x->term_count; i++)
      if (x->terms[i].type != y->terms[i].type
          || (x->terms[i].type == VARIABLE && variables_differ(x->terms[i].variable, y->terms[i].variable, matches, quote_level))
          || (x->terms[i].type == FUNCTION && functions_differ(x->terms[i].function, y->terms[i].function, matches, quote_level))
          || (x->terms[i].type == QUOTE && quotes_differ(x->terms[i].quote, y->terms[i].quote, matches, quote_level+1))
          || (x->terms[i].type == QUASIQUOTE && quasiquotes_differ(x->terms[i].quasiquote, y->terms[i].quasiquote, matches, quote_level)))
        return 1;

    // All arguments compared as equal; return 0
    return 0;
  }
  return 1;
}

// Returns 0 if clauses have equal positive and negative literal sets; otherwise returns 1
// Equality of variables is that there is a one-to-one correspondence in the sets of variables x and y use,
// based on each location where a variable maps to another
// Thus a(X, X) and a(X, Y) are here considered different
int clauses_differ(clause *x, clause *y) {
    var_match_set matches;
    var_match_init(&matches);
    if (clauses_differ_rec(x, y, &matches, 0))
      return release_matches(&matches, 1);
    // All literals compared as equal; return 0
    return release_matches(&matches, 0);
}


// Returns true if clause is not distrusted, retired, or handled
int flags_negative(clause *c) {
  return c->distrusted < 0 && c->retired < 0 && c->handled < 0;
}

// Returns minimum among the flags of value >= 0
long flag_min(clause *c) {
  long min = LONG_MAX;
  if (c->distrusted >= 0)
    min = c->distrusted;
  if (c->retired >= 0 && c->retired < min)
    min = c->retired;
  if (c->handled >= 0 && c->handled < min)
    min = c->handled;
  return min;
}

// Given "name" and integer arity, returns "name/arity"
// Allocated cstring is later placed in predname_mapping, and eventually freed by kb_halt
char* name_with_arity(char *name, int arity) {
  int arity_len = snprintf(NULL, 0, "%d", arity);
  char *arity_str = malloc(arity_len+1);
  snprintf(arity_str, arity_len+1, "%d", arity);
  char *name_with_arity = malloc(strlen(name) + strlen(arity_str) + 2);
  memcpy(name_with_arity, name, strlen(name));
  name_with_arity[strlen(name)] = '/';
  strcpy(name_with_arity + strlen(name) + 1, arity_str);
  free(arity_str);
  return name_with_arity;
}

// Determines of clauses have combatible tags and counts of arguments and conclusions
// Performs additional checks for fif conclusions
int counts_match(clause *x, clause *y) {
  if (x->tag == y->tag && x->pos_count == y->pos_count && x->neg_count == y->neg_count) {
    if (x->tag == FIF && x->fif->num_conclusions != y->fif->num_conclusions)
      return 0;
    return 1;
  }
  return 0;
}

// Comparison used by qsort of clauses -- orders by increasing function name and increasing arity
int function_compare(const void *p1, const void *p2) {
  alma_function **f1 = (alma_function **)p1;
  alma_function **f2 = (alma_function **)p2;
  int compare = strcmp((*f1)->name, (*f2)->name);
  if (compare == 0)
    return (*f1)->term_count - (*f2)->term_count;
  else
    return compare;
}
