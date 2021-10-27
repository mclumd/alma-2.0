#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "alma_kb.h"
#include "alma_backsearch.h"
#include "alma_fif.h"
#include "alma_proc.h"

void kb_init(kb* collection, int verbose) {
  collection->variable_id_count = 0;
  collection->next_index = 0;
  collection->prefix = NULL;
  collection->verbose = verbose;

  tommy_list_init(&collection->clauses);
  tommy_hashlin_init(&collection->index_map);
  tommy_hashlin_init(&collection->fif_map);

  tommy_hashlin_init(&collection->pos_map);
  tommy_list_init(&collection->pos_list);
  tommy_hashlin_init(&collection->neg_map);
  tommy_list_init(&collection->neg_list);

  tommy_array_init(&collection->new_clauses);
  tommy_array_init(&collection->timestep_delay_clauses);

  tommy_array_init(&collection->distrust_set);
  tommy_array_init(&collection->distrust_parents);
  tommy_array_init(&collection->handle_set);
  tommy_array_init(&collection->handle_parents);
  tommy_array_init(&collection->retire_set);
  tommy_array_init(&collection->retire_parents);

  tommy_array_init(&collection->pos_lit_reinstates);
  tommy_array_init(&collection->neg_lit_reinstates);
  tommy_array_init(&collection->trues);

  tommy_array_init(&collection->res_tasks);
  tommy_hashlin_init(&collection->fif_tasks);
}

void kb_print(kb *collection, kb_logger *logger) {
  tommy_node *i = tommy_list_head(&collection->clauses);
  while (i) {
    index_mapping *data = i->data;
    if (collection->verbose || data->value->dirty_bit) {
      tee_alt("%ld: ", logger, data->key);
      clause_print(data->value, logger);
      tee_alt("\n", logger);
    }
    i = i->next;
  }
}


void free_predname_mapping(void *arg) {
  predname_mapping *entry = arg;
  free(entry->predname);
  free(entry->clauses);
  // Note: clause entries ARE NOT FREED because they alias the clause objects freed in kb_halt
  free(entry);
}

// Compare function to be used by tommy_hashlin_search for index_mapping
// Compares long arg to key of index_mapping
static int im_compare(const void *arg, const void *obj) {
  return *(const long*)arg - ((const index_mapping*)obj)->key;
}

// Compare function to be used by tommy_hashlin_search for predname_mapping
// Compares string arg to predname of predname_mapping
int pm_compare(const void *arg, const void *obj) {
  return strcmp((const char*)arg, ((const predname_mapping*)obj)->predname);
}

// Returns a mapping holding a set of clauses that contains those unifiable with clause c
// Return type is a fif_mapping or predname_mapping depending on c's tag
// For predname_mapping, looks up mapping for a predicate appearing as a literal in clause c;
//  currently, simply picks the first positive or negative literal
void* clause_lookup(kb *collection, clause *c) {
  if (c->tag == FIF) {
    char *name = c->fif->indexing_conc->name;
    return tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
  }
  else {
    tommy_hashlin *map = &collection->pos_map;
    alma_function *pred;
    if (c->pos_count != 0) {
      pred = c->pos_lits[0];
    }
    else {
      pred = c->neg_lits[0];
      map = &collection->neg_map;
    }
    char *name = name_with_arity(pred->name, pred->term_count);
    predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    free(name);
    return result;
  }
}

// Retrieves index-th item from mapping
// Tag determines interpreting it as fif_mapping or predname_mapping
clause* mapping_access(void *mapping, if_tag tag, int index) {
  if (tag == FIF)
    return ((fif_mapping*)mapping)->clauses[index];
  else
    return ((predname_mapping*)mapping)->clauses[index];
}

// Retrieves number of clauses held by mapping
// Tag determines interpreting it as fif_mapping or predname_mapping
int mapping_num_clauses(void *mapping, if_tag tag) {
  if (tag == FIF)
    return ((fif_mapping*)mapping)->num_clauses;
  else
    return ((predname_mapping*)mapping)->num_clauses;
}


// Inserts clause into the hashmap (with key based on lit), as well as the linked list
// If the map entry already exists, append; otherwise create a new one
static void map_add_clause(tommy_hashlin *map, tommy_list *list, alma_function *lit, clause *c) {
  char *name = name_with_arity(lit->name, lit->term_count);
  predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
  if (result != NULL) {
    result->num_clauses++;
    result->clauses = realloc(result->clauses, sizeof(*result->clauses) * result->num_clauses);
    result->clauses[result->num_clauses-1] = c; // Aliases with pointer assignment
    free(name); // Name not added into hashmap must be freed
  }
  else {
    predname_mapping *entry = malloc(sizeof(*entry));
    entry->predname = name;
    entry->num_clauses = 1;
    entry->clauses = malloc(sizeof(*entry->clauses));
    entry->clauses[0] = c; // Aliases with pointer assignment
    tommy_hashlin_insert(map, &entry->hash_node, entry, tommy_hash_u64(0, entry->predname, strlen(entry->predname)));
    tommy_list_insert_tail(list, &entry->list_node, entry);
  }
}

// Removes clause from hashmap and list if it exists
static void map_remove_clause(tommy_hashlin *map, tommy_list *list, alma_function *lit, clause *c) {
  char *name = name_with_arity(lit->name, lit->term_count);
  predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
  if (result != NULL) {
    for (int i = 0; i < result->num_clauses; i++) {
      if (result->clauses[i] == c) {
        if (i < result->num_clauses-1)
          result->clauses[i] = result->clauses[result->num_clauses-1];
        result->num_clauses--;

        // Shrink size of clause list of result
        if (result->num_clauses > 0) {
          result->clauses = realloc(result->clauses, sizeof(*result->clauses) * result->num_clauses);
        }
        // Remove predname_mapping from hashmap and linked list
        else {
          tommy_hashlin_remove_existing(map, &result->hash_node);
          tommy_list_remove_existing(list, &result->list_node);
          free_predname_mapping(result);
          free(name);
          return;
        }
      }
    }
  }
  free(name);
}


// Checks if x and y are ground literals that are identical
// Currently does not check within quotes; any quotation term found returns 0
static int ground_duplicate_literals(alma_function *x, alma_function *y) {
  if (strcmp(x->name, y->name) == 0 && x->term_count == y->term_count) {
    for (int i = 0; i < x->term_count; i++)
      if (x->terms[i].type != y->terms[i].type || x->terms[i].type == VARIABLE || x->terms[i].type == QUASIQUOTE || x->terms[i].type == QUOTE
          || (x->terms[i].type == FUNCTION && !ground_duplicate_literals(x->terms[i].function, y->terms[i].function)))
        return 0;
    return 1;
  }
  return 0;
}

static void remove_dupe_literals(int *count, alma_function ***triple) {
  alma_function **lits = *triple;
  int deleted = 0;
  int new_count = *count;
  for (int i = 0; i < *count-1; i++) {
    if (ground_duplicate_literals(lits[i], lits[i+1])) {
      new_count--;
      deleted = 1;
      free_function(lits[i]);
      lits[i] = NULL;
    }
  }
  if (deleted) {
    // Collect non-null literals at start of set
    int loc = 1;
    for (int i = 0; i < *count-1; i++) {
      if (lits[i] == NULL) {
        loc = i+1;
        while (loc < *count && lits[loc] == NULL)
          loc++;
        if (loc >= *count)
          break;
        lits[i] = lits[loc];
        lits[loc] = NULL;
      }
    }
    *triple = realloc(lits, sizeof(*lits) * new_count);
    *count = new_count;
  }
}

// If c is found to be a clause's duplicate, returns a pointer to that clause; null otherwise
// See comments preceding clauses_differ function for further detail
// Check_distrusted flag determines whether distrusted clauses are used for dupe comparison as well
clause* duplicate_check(kb *collection, long time, clause *c, int check_distrusted) {
  if (c->tag == FIF) {
    char *name = c->fif->indexing_conc->name;
    fif_mapping *result = tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (result != NULL) {
      for (int i = 0; i < result->num_clauses; i++) {
        if (c->tag == result->clauses[i]->tag && !clauses_differ(c, result->clauses[i]))
          return result->clauses[i];
      }
    }
  }
  else {
    // Check for ground literal duplicates, and alter clause to remove any found
    // Necessary for dupe checking to work in such cases, even if side effects are wasted for a discarded duplicate
    qsort(c->pos_lits, c->pos_count, sizeof(*c->pos_lits), function_compare);
    qsort(c->neg_lits, c->neg_count, sizeof(*c->neg_lits), function_compare);
    remove_dupe_literals(&c->pos_count, &c->pos_lits);
    remove_dupe_literals(&c->neg_count, &c->neg_lits);

    char *name;
    tommy_hashlin *map;
    // If clause has a positive literal, all duplicate candidates must have that same positive literal
    // Arbitrarily pick first positive literal as one to use; may be able to do smarter literal choice later
    if (c->pos_count > 0) {
      map = &collection->pos_map;
      name = name_with_arity(c->pos_lits[0]->name, c->pos_lits[0]->term_count);
    }
    else {
      map = &collection->neg_map;
      name = name_with_arity(c->neg_lits[0]->name, c->neg_lits[0]->term_count);
    }
    predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    free(name);
    if (result != NULL) {
      for (int i = 0; i < result->num_clauses; i++) {
        // A clause distrusted this timestep is still checked against
        if ((check_distrusted || flags_negative(result->clauses[i]) || flag_min(result->clauses[i]) == time) &&
            c->tag == result->clauses[i]->tag && !clauses_differ(c, result->clauses[i]))
          return result->clauses[i];
      }
    }
  }
  return NULL;
}


// Returns first alma_function pointer for literal with given name in clause
// Searching positive or negative literals is decided by pos boolean
static alma_function* literal_by_name(clause *c, char *name, int pos) {
  if (c != NULL) {
    int count = pos ? c->pos_count : c->neg_count;
    alma_function **lits = pos ? c->pos_lits : c->neg_lits;
    for (int i = 0; i < count; i++)
      if (strcmp(name, lits[i]->name) == 0)
        return lits[i];
  }
  return NULL;
}

void make_single_task(clause *c, alma_function *c_lit, clause *other, tommy_array *tasks, int use_bif, int pos) {
  if (c != other && (other->tag != BIF || use_bif) && other->tag != FIF) {
    alma_function *other_lit = literal_by_name(other, c_lit->name, pos);
    if (other_lit != NULL && other_lit != c_lit) {
      res_task *t = malloc(sizeof(*t));
      if (!pos) {
        t->x = c;
        t->pos = c_lit;
        t->y = other;
        t->neg = other_lit;
      }
      else {
        t->x = other;
        t->pos = other_lit;
        t->y = c;
        t->neg = c_lit;
      }

      tommy_array_insert(tasks, t);
    }
  }
}

// Helper of res_tasks_from_clause
void make_res_tasks(clause *c, int count, alma_function **c_lits, tommy_hashlin *map, tommy_array *tasks, int use_bif, int pos) {
  for (int i = 0; i < count; i++) {
    char *name = name_with_arity(c_lits[i]->name, c_lits[i]->term_count);
    predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));

    // New tasks are from Cartesian product of result's clauses with clauses' ith
    if (result != NULL)
      for (int j = 0; j < result->num_clauses; j++)
        if (flags_negative(result->clauses[j]))
          make_single_task(c, c_lits[i], result->clauses[j], tasks, use_bif, pos);

    free(name);
  }
}

// Finds new res tasks based on matching pos/neg predicate pairs, where one is from the KB and the other from arg
// Tasks are added into the res_tasks of collection
// Used only for non-bif resolution tasks; hence checks tag of c
void res_tasks_from_clause(kb *collection, clause *c, int process_negatives) {
  if (c->tag != BIF && c->tag != FIF) {
    make_res_tasks(c, c->pos_count, c->pos_lits, &collection->neg_map, &collection->res_tasks, 0, 0);
    // Only done if clauses differ from KB's clauses (i.e. after first task generation)
    if (process_negatives)
      make_res_tasks(c, c->neg_count, c->neg_lits, &collection->pos_map, &collection->res_tasks, 0, 1);
  }
}

// Returns boolean based on success of string parse
// If parses successfully, adds to collection's new_clauses
// Returns pointer to first clause asserted, if any
clause* assert_formula(kb *collection, char *string, int print, kb_logger *logger) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas, logger)) {
    tommy_array temp;
    tommy_array_init(&temp);
    nodes_to_clauses(formulas, formula_count, &temp, print, &collection->variable_id_count, logger);
    clause *ret = tommy_array_size(&temp) > 0 ? tommy_array_get(&temp, 0) : NULL;
    for (tommy_size_t i = 0; i < tommy_array_size(&temp); i++)
      tommy_array_insert(&collection->new_clauses, tommy_array_get(&temp, i));
    tommy_array_done(&temp);
    return ret;
  }
  return NULL;
}


// Remove res tasks using clause
static void remove_res_tasks(tommy_array *res_tasks, clause *c) {
  for (tommy_size_t i = 0; i < tommy_array_size(res_tasks); i++) {
    res_task *current_task = tommy_array_get(res_tasks, i);
    if (current_task != NULL && (current_task->x == c || current_task->y == c)) {
      // Removed task set to null; null checked for when processing later
      tommy_array_set(res_tasks, i, NULL);
      free(current_task);
    }
  }
}

// Given a clause already existing in KB, remove from data structures
// Note that fif removal, or removal of a singleton clause used in a fif rule, may be very expensive
static void remove_clause(kb *collection, clause *c) {
  if (c->tag == FIF) {
    char *name = c->fif->indexing_conc->name;
    fif_mapping *fifm = tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (fifm != NULL) {
      for (int i = 0; i < fifm->num_clauses; i++) {
        if (fifm->clauses[i]->index == c->index) {
          if (i < fifm->num_clauses-1) {
            fifm->clauses[i] = fifm->clauses[fifm->num_clauses-1];
          }
          fifm->num_clauses--;
          if (fifm->num_clauses == 0) {
            tommy_hashlin_remove_existing(&collection->fif_map, &fifm->node);
            free_fif_mapping(fifm);
          }
          else {
            fifm->clauses = realloc(fifm->clauses, sizeof(*fifm->clauses)*fifm->num_clauses);
          }
          break;
        }
      }
    }

    remove_fif_tasks(&collection->fif_tasks, c);
  }
  else {
    // Non-fif must be unindexed from pos/neg maps, have resolution tasks and fif tasks (if in any) removed
    for (int i = 0; i < c->pos_count; i++)
      map_remove_clause(&collection->pos_map, &collection->pos_list, c->pos_lits[i], c);
    for (int i = 0; i < c->neg_count; i++)
      map_remove_clause(&collection->neg_map, &collection->neg_list, c->neg_lits[i], c);

    remove_res_tasks(&collection->res_tasks, c);

    // May be used in fif tasks if it's a singleton clause
    if (c->pos_count + c->neg_count == 1)
      remove_fif_singleton_tasks(&collection->fif_tasks, &collection->fif_map, c);
  }

  index_mapping *result = tommy_hashlin_search(&collection->index_map, im_compare, &c->index, tommy_hash_u64(0, &c->index, sizeof(c->index)));
  tommy_list_remove_existing(&collection->clauses, &result->list_node);
  tommy_hashlin_remove_existing(&collection->index_map, &result->hash_node);
  free(result);

  // Remove clause from the parents list of each of its children
  for (int i = 0; i < c->children_count; i++) {
    clause *child = c->children[i];
    if (child != NULL) {
      int new_count = child->parent_set_count;

      //  If a parent set has a clause matching, remove match
      for (int j = 0; j < child->parent_set_count; j++) {
        for (int k = 0; k < child->parents[j].count; k++) {
          if (child->parents[j].clauses[k] == c) {
            // If last parent in set, deallocate
            if (child->parents[j].count == 1) {
              new_count--;
              child->parents[j].count = 0;
              free(child->parents[j].clauses);
              child->parents[j].clauses = NULL;
            }
            // Otherwise, reallocate without parent
            else {
              child->parents[j].count--;
              child->parents[j].clauses[k] = child->parents[j].clauses[child->parents[j].count];
              child->parents[j].clauses = realloc(child->parents[j].clauses, sizeof(*child->parents[j].clauses)*child->parents[j].count);

              // After reallocating, check for parent set identical to this one and delete it if one exists
              for (int m = 0; m < child->parent_set_count; m++) {
                if (m != j && child->parents[m].clauses != NULL && child->parents[j].count == child->parents[m].count) {
                  int dupe = 1;
                  for (int n = 0; n < child->parents[m].count; n++) {
                    if (child->parents[m].clauses[n]->index != child->parents[j].clauses[n]->index) {
                      dupe = 0;
                      break;
                    }
                  }
                  if (dupe) {
                    new_count--;
                    child->parents[j].count = 0;
                    free(child->parents[j].clauses);
                    child->parents[j].clauses = NULL;
                    break;
                  }
                }
              }
            }
            break;
          }
        }
      }

      if (new_count > 0 && new_count != child->parent_set_count) {
        // Collect non-null parent sets at start of parents
        int loc = 1;
        for (int j = 0; j < child->parent_set_count-1; j++, loc++) {
          if (child->parents[j].clauses == NULL) {
            while (loc < child->parent_set_count && child->parents[loc].clauses == NULL)
              loc++;
            if (loc >= child->parent_set_count)
              break;
            child->parents[j].clauses = child->parents[loc].clauses;
            child->parents[j].count = child->parents[loc].count;
            child->parents[loc].clauses = NULL;
          }
        }
        child->parents = realloc(child->parents, sizeof(*child->parents)*new_count);
      }
      if (new_count == 0 && child->parents != NULL) {
        free(child->parents);
        child->parents = NULL;
      }
      child->parent_set_count = new_count;
    }
  }

  // Remove clause from the children list of each of its parents
  for (int i = 0; i < c->parent_set_count; i++)
    for (int j = 0; j < c->parents[i].count; j++)
      remove_child(c->parents[i].clauses[j], c);

  free_clause(c);
}

int delete_formula(kb *collection, long time, char *string, int print, kb_logger *logger) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas, logger)) {
    // Convert formulas to clause array
    tommy_array clauses;
    tommy_array_init(&clauses);
    nodes_to_clauses(formulas, formula_count, &clauses, 0, &collection->variable_id_count, logger);

    // Process and remove each clause
    for (tommy_size_t i = 0; i < tommy_array_size(&clauses); i++) {
      clause *curr = tommy_array_get(&clauses, i);
      clause *c = duplicate_check(collection, time, curr, 1);
      if (c != NULL) {
        if (print) {
          tee_alt("-a: ", logger);
          clause_print(c, logger);
          tee_alt(" removed\n", logger);
        }
        remove_clause(collection, c);
      }
      else if (print) {
        tee_alt("-a: ", logger);
        clause_print(curr, logger);
        tee_alt(" not found\n", logger);
      }
      free_clause(curr);
    }

    tommy_array_done(&clauses);

    return 1;
  }
  return 0;
}

int update_formula(kb *collection, long time, char *string, kb_logger *logger) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas, logger)) {
    // Must supply two formula arguments to update
    if (formula_count != 2) {
      for (int i = 0; i < formula_count; i++)
        free_alma_tree(formulas+i);
      free(formulas);
      tee_alt("-a: Incorrect number of arguments to update\n", logger);
      return 0;
    }

    // Convert formulas to clause array
    tommy_array clauses;
    tommy_array_init(&clauses);
    nodes_to_clauses(formulas, formula_count, &clauses, 0, &collection->variable_id_count, logger);

    int update_fail = 0;
    clause *target = tommy_array_get(&clauses, 0);
    clause *update = tommy_array_get(&clauses, 1);
    tommy_array_done(&clauses);
    clause *t_dupe;
    clause *u_dupe;
    if (target->tag == FIF || update->tag == FIF) {
      tee_alt("-a: Cannot update with fif clause\n", logger);
      update_fail = 1;
    }
    else if ((t_dupe = duplicate_check(collection, time, target, 1)) == NULL) {
      tee_alt("-a: Clause ", logger);
      clause_print(target, logger);
      tee_alt(" to update not present\n", logger);
      update_fail = 1;
    }
    else if ((u_dupe = duplicate_check(collection, time, update, 1)) != NULL) {
      tee_alt("-a: New version of clause already present\n", logger);
      update_fail = 1;
    }

    if (update_fail) {
      free_clause(target);
      free_clause(update);
      return 0;
    }
    else {
      // Clear out res and fif tasks with old version of clause
      for (int i = 0; i < t_dupe->pos_count; i++)
        map_remove_clause(&collection->pos_map, &collection->pos_list, t_dupe->pos_lits[i], t_dupe);
      for (int i = 0; i < t_dupe->neg_count; i++)
        map_remove_clause(&collection->neg_map, &collection->neg_list, t_dupe->neg_lits[i], t_dupe);
      remove_res_tasks(&collection->res_tasks, t_dupe);
      if (t_dupe->pos_count + t_dupe->neg_count == 1)
        remove_fif_singleton_tasks(&collection->fif_tasks, &collection->fif_map, t_dupe);

      tee_alt("-a: ", logger);
      clause_print(target, logger);
      tee_alt(" updated to ", logger);
      clause_print(update, logger);
      tee_alt("\n", logger);
      free_clause(target);

      // Swap clause contents from update
      int count_temp = t_dupe->pos_count;
      t_dupe->pos_count = update->pos_count;
      update->pos_count = count_temp;
      alma_function **lits_temp = t_dupe->pos_lits;
      t_dupe->pos_lits = update->pos_lits;
      update->pos_lits = lits_temp;
      count_temp = t_dupe->neg_count;
      t_dupe->neg_count = update->neg_count;
      update->neg_count = count_temp;
      lits_temp = t_dupe->neg_lits;
      t_dupe->neg_lits = update->neg_lits;
      update->neg_lits = lits_temp;
      t_dupe->dirty_bit = 1;
      t_dupe->pyobject_bit = 1;

      // Generate new tasks with updated clause
      res_tasks_from_clause(collection, t_dupe, 1);
      fif_tasks_from_clause(&collection->fif_tasks, t_dupe);
      for (int j = 0; j < t_dupe->pos_count; j++)
        map_add_clause(&collection->pos_map, &collection->pos_list, t_dupe->pos_lits[j], t_dupe);
      for (int j = 0; j < t_dupe->neg_count; j++)
        map_add_clause(&collection->neg_map, &collection->neg_list, t_dupe->neg_lits[j], t_dupe);

      free_clause(update);
      return 1;
    }
  }
  return 0;
}

// Resolve helper
static void lits_copy(int count, alma_function **lits, alma_function **cmp, binding_list *mgu, alma_function **res_lits, int *res_count) {
  for (int i = 0; i < count; i++) {
    // In calls cmp can be assigned to resolvent to not copy it
    if (cmp == NULL || lits[i] != *cmp) {
      res_lits[*res_count] = malloc(sizeof(*res_lits[*res_count]));
      copy_alma_function(lits[i], res_lits[*res_count]);
      (*res_count)++;
    }
  }
}

// Given an MGU, make a single resulting clause without unified literal pair, then substitute
static void resolve(res_task *t, binding_list *mgu, clause *result) {
  result->pos_count = 0;
  if (t->x->pos_count + t->y->pos_count - 1 > 0) {
    result->pos_lits = malloc(sizeof(*result->pos_lits) * (t->x->pos_count + t->y->pos_count - 1));
    // Copy positive literals from t->x and t->y
    lits_copy(t->x->pos_count, t->x->pos_lits, &t->pos, mgu, result->pos_lits, &result->pos_count);
    lits_copy(t->y->pos_count, t->y->pos_lits, NULL, mgu, result->pos_lits, &result->pos_count);
  }
  else
    result->pos_lits = NULL;

  result->neg_count = 0;
  if (t->x->neg_count + t->y->neg_count - 1 > 0) {
    result->neg_lits = malloc(sizeof(*result->neg_lits) * (t->x->neg_count + t->y->neg_count - 1));
    // Copy negative literals from t->x and t->y
    lits_copy(t->y->neg_count, t->y->neg_lits, &t->neg, mgu, result->neg_lits, &result->neg_count);
    lits_copy(t->x->neg_count, t->x->neg_lits, NULL, mgu, result->neg_lits, &result->neg_count);
  }
  else
    result->neg_lits = NULL;

  result->tag = NONE;
  result->fif = NULL;
  subst_clause(mgu, result, 0);
}


// Places clause into term as clause_quote type of quote, with copy of structure
static void quote_from_clause(alma_term *t, clause *c) {
  t->type = QUOTE;
  t->quote = malloc(sizeof(*t->quote));
  t->quote->type = CLAUSE;
  t->quote->clause_quote = malloc(sizeof(*t->quote->clause_quote));
  copy_clause_structure(c, t->quote->clause_quote);
}

void func_from_long(alma_term *t, long l) {
  t->type = FUNCTION;
  t->function = malloc(sizeof(*t->function));
  int length = snprintf(NULL, 0, "%ld", l);
  t->function->name = malloc(length+1);
  snprintf(t->function->name, length+1, "%ld", l);
  t->function->term_count = 0;
  t->function->terms = NULL;
}

static int derivations_distrusted(clause *c) {
  for (int i = 0; i < c->parent_set_count; i++) {
    int parent_distrusted = 0;
    for (int j = 0; j < c->parents[i].count; j++) {
      if (c->parents[i].clauses[j]->distrusted >= 0) {
        parent_distrusted = 1;
        break;
      }
    }
    if (!parent_distrusted)
      return 0;
  }
  return 1;
}

// Commonly used for special clauses like true, distrust, contra, etc.
static void init_single_parent(clause *child, clause *parent) {
  if (parent != NULL) {
    child->parent_set_count = 1;
    child->parents = malloc(sizeof(*child->parents));
    child->parents[0].count = 1;
    child->parents[0].clauses = malloc(sizeof(*child->parents[0].clauses));
    child->parents[0].clauses[0] = parent;
  }
}

// Creates a meta-formula literal with binary arguments of quotation term for c and the time
static clause* make_meta_literal(kb *collection, char *predname, clause *c, long time) {
  clause *res = malloc(sizeof(*res));
  res->pos_count = 1;
  res->neg_count = 0;
  res->pos_lits = malloc(sizeof(*res->pos_lits));
  res->pos_lits[0] = malloc(sizeof(*res->pos_lits[0]));
  res->pos_lits[0]->name = malloc(strlen(predname)+1);
  strcpy(res->pos_lits[0]->name, predname);
  res->pos_lits[0]->term_count = 2;
  res->pos_lits[0]->terms = malloc(sizeof(*res->pos_lits[0]->terms) * 2);
  quote_from_clause(res->pos_lits[0]->terms+0, c);
  func_from_long(res->pos_lits[0]->terms+1, time);
  res->neg_lits = NULL;
  res->parent_set_count = 0;
  res->children_count = 0;
  res->parents = NULL;
  res->children = NULL;
  res->tag = NONE;
  res->fif = NULL;
  set_variable_ids(res, 1, 0, NULL, &collection->variable_id_count);
  return res;
}

static void distrust_recursive(kb *collection, clause *c, clause *contra, long time) {
  // Formula becomes distrusted at current time
  c->distrusted = time;

  if (c->tag == FIF)
    remove_fif_tasks(&collection->fif_tasks, c);

  // Assert atomic distrusted() formula
  clause *d = make_meta_literal(collection, "distrusted", c, time);
  init_single_parent(d, contra);
  tommy_array_insert(&collection->new_clauses, d);

  // Recursively distrust children
  if (c->children != NULL) {
    for (int i = 0; i < c->children_count; i++) {
      if (c->children[i]->distrusted < 0 && derivations_distrusted(c->children[i])) {
        distrust_recursive(collection, c->children[i], contra, time);
      }
    }
  }
}

static void retire_recursive(kb *collection, clause *c, clause *contra) {
  // TODO: appropriate retired-handling code to recurse through clause and descendants as needed
}

static void binding_subst(binding_list *target, binding_list *theta) {
  for (int i = 0; i < target->num_bindings; i++)
    subst_term(theta, target->list[i].term, 0);
}

static void make_contra(kb *collection, clause *contradictand_pos, clause *contradictand_neg, long time) {
  clause *contra = malloc(sizeof(*contra));
  contra->pos_count = 1;
  contra->neg_count = 0;
  contra->pos_lits = malloc(sizeof(*contra->pos_lits));
  contra->pos_lits[0] = malloc(sizeof(*contra->pos_lits[0]));
  contra->pos_lits[0]->name = malloc(strlen("contra_event")+1);
  strcpy(contra->pos_lits[0]->name, "contra_event");
  contra->pos_lits[0]->term_count = 3;
  contra->pos_lits[0]->terms = malloc(sizeof(*contra->pos_lits[0]->terms) * 3);
  quote_from_clause(contra->pos_lits[0]->terms+0, contradictand_pos);
  quote_from_clause(contra->pos_lits[0]->terms+1, contradictand_neg);
  func_from_long(contra->pos_lits[0]->terms+2, time);
  contra->neg_lits = NULL;
  contra->parent_set_count = contra->children_count = 0;
  contra->parents = NULL;
  contra->children = NULL;
  contra->tag = NONE;
  contra->fif = NULL;
  set_variable_ids(contra, 1, 0, NULL, &collection->variable_id_count);
  tommy_array_insert(&collection->new_clauses, contra);

  clause *contradicting = malloc(sizeof(*contradicting));
  copy_clause_structure(contra, contradicting);
  contradicting->pos_lits[0]->name = realloc(contradicting->pos_lits[0]->name, strlen("contradicting")+1);
  strcpy(contradicting->pos_lits[0]->name, "contradicting");
  init_single_parent(contradicting, contra);
  set_variable_ids(contradicting, 1, 0, NULL, &collection->variable_id_count);
  tommy_array_insert(&collection->new_clauses, contradicting);

  tommy_array_insert(&collection->distrust_set, contradictand_pos);
  tommy_array_insert(&collection->distrust_parents, contra);
  tommy_array_insert(&collection->distrust_set, contradictand_neg);
  tommy_array_insert(&collection->distrust_parents, contra);
}

static binding_list* parent_binding_prepare(backsearch_task *bs, long parent_index, binding_list *theta) {
  // Retrieve parent existing bindings, if any
  binding_mapping *mapping = NULL;
  if (parent_index < 0)
    mapping = tommy_hashlin_search(&bs->clause_bindings, bm_compare, &parent_index, tommy_hash_u64(0, &parent_index, sizeof(parent_index)));

  // Substitute based on MGU obtained when unifying
  if (mapping != NULL) {
    binding_list *copy = malloc(sizeof(*copy));
    copy_bindings(copy, mapping->bindings);
    binding_subst(copy, theta);
    return copy;
  }
  else
    return NULL;
}


// Process resolution tasks from argument and place results in new_arr
void process_res_tasks(kb *collection, long time, tommy_array *tasks, tommy_array *new_arr, backsearch_task *bs, kb_logger *logger) {
  for (tommy_size_t i = 0; i < tommy_array_size(tasks); i++) {
    res_task *current_task = tommy_array_get(tasks, i);
    if (current_task != NULL) {
      // Does not do resolution with a distrusted clause
      if (flags_negative(current_task->x) && flags_negative(current_task->y)) {
        binding_list *theta = malloc(sizeof(*theta));
        init_bindings(theta);
        if (collection->verbose)
          print_unify(current_task->pos, current_task->x->index, current_task->neg, current_task->y->index, logger);

        // Given a res_task, attempt unification
        if (pred_unify(current_task->pos, current_task->neg, theta, collection->verbose)) {
          // If successful, create clause for result of resolution and add to new_clauses
          clause *res_result = malloc(sizeof(*res_result));
          resolve(current_task, theta, res_result);

          binding_list *x_bindings = NULL;
          if (bs) {
            x_bindings = parent_binding_prepare(bs, current_task->x->index, theta);
            binding_list *y_bindings = parent_binding_prepare(bs, current_task->y->index, theta);
            if (x_bindings != NULL && y_bindings != NULL) {
              // Check that tracked bindings for unified pair are compatible/unifiable
              binding_list *parent_theta = malloc(sizeof(*parent_theta));
              init_bindings(parent_theta);
              int unify_fail = 0;
              for (int j = 0; j < x_bindings->num_bindings; j++) {
                if (!term_unify(x_bindings->list[j].term, y_bindings->list[j].term, parent_theta)) {
                  unify_fail = 1;
                  break;
                }
              }
              cleanup_bindings(y_bindings);
              if (unify_fail) {
                cleanup_bindings(parent_theta);
                cleanup_bindings(x_bindings);

                if (res_result->pos_count > 0) {
                  for (int j = 0; j < res_result->pos_count; j++)
                    free_function(res_result->pos_lits[j]);
                  free(res_result->pos_lits);
                }
                if (res_result->neg_count > 0) {
                  for (int j = 0; j < res_result->neg_count; j++)
                    free_function(res_result->neg_lits[j]);
                  free(res_result->neg_lits);
                }
                free(res_result);
                goto cleanup;
              }
              else {
                // Use x_bindings with collected info as binding stuff
                binding_subst(x_bindings, parent_theta);

                // Apply parent_theta to resolution result as well
                subst_clause(parent_theta, res_result, 0);

                cleanup_bindings(parent_theta);
              }
            }
            else if (x_bindings == NULL) {
              // Consolidate to only x_bindings
              x_bindings = y_bindings;
            }
          }

          // A resolution result must be empty to be a valid clause to add to KB
          if (res_result->pos_count > 0 || res_result->neg_count > 0) {
            // Initialize parents of result
            res_result->parent_set_count = 1;
            res_result->parents = malloc(sizeof(*res_result->parents));
            res_result->parents[0].count = 2;
            res_result->parents[0].clauses = malloc(sizeof(*res_result->parents[0].clauses) * 2);
            res_result->parents[0].clauses[0] = current_task->x;
            res_result->parents[0].clauses[1] = current_task->y;
            res_result->children_count = 0;
            res_result->children = NULL;

            set_variable_ids(res_result, 0, 0, x_bindings, &collection->variable_id_count);

            tommy_array_insert(new_arr, res_result);
            if (bs)
              tommy_array_insert(&bs->new_clause_bindings, x_bindings);
          }
          else {
            free(res_result);

            if (bs) {
              clause *answer = malloc(sizeof(*answer));
              memcpy(answer, bs->target, sizeof(*answer));
              if (bs->target->pos_count == 1) {
                answer->pos_lits = malloc(sizeof(*answer->pos_lits));
                answer->pos_lits[0] = malloc(sizeof(*answer->pos_lits[0]));
                copy_alma_function(bs->target->pos_lits[0], answer->pos_lits[0]);
                for (int j = 0; j < answer->pos_lits[0]->term_count; j++)
                  subst_term(x_bindings, answer->pos_lits[0]->terms+j, 0);
              }
              else {
                answer->neg_lits = malloc(sizeof(*answer->neg_lits));
                answer->neg_lits[0] = malloc(sizeof(*answer->neg_lits[0]));
                copy_alma_function(bs->target->neg_lits[0], answer->neg_lits[0]);
                for (int j = 0; j < answer->neg_lits[0]->term_count; j++)
                  subst_term(x_bindings, answer->neg_lits[0]->terms+j, 0);
              }
              set_variable_ids(answer, 0, 0, NULL, &collection->variable_id_count);

              // TODO: parent setup for backsearch answer?
              tommy_array_insert(&collection->new_clauses, answer);
              cleanup_bindings(x_bindings);
            }
            // If not a backward search, empty resolution result indicates a contradiction between clauses
            else
              make_contra(collection, current_task->x, current_task->y, time);
          }
        }
        if (collection->verbose)
          print_bindings(theta, 1, 1, logger);

        cleanup:
        cleanup_bindings(theta);
      }

      free(current_task);
    }
  }
  tommy_array_done(tasks);
  tommy_array_init(tasks);
}


// Given a new clause, add to the KB and maps
static void add_clause(kb *collection, clause *c, long time) {
  // Add clause to overall clause list and index map
  index_mapping *ientry = malloc(sizeof(*ientry));
  c->index = ientry->key = collection->next_index++;
  c->acquired = time;
  c->distrusted = -1;
  c->retired = -1;
  c->handled = -1;
  c->dirty_bit = (char) 1;
  c->pyobject_bit = (char) 1;
  ientry->value = c;
  tommy_list_insert_tail(&collection->clauses, &ientry->list_node, ientry);
  tommy_hashlin_insert(&collection->index_map, &ientry->hash_node, ientry, tommy_hash_u64(0, &ientry->key, sizeof(ientry->key)));

  if (c->tag == FIF) {
    char *name = c->fif->indexing_conc->name;
    // Index into fif hashmap
    fif_mapping *result = tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (result != NULL) {
      result->num_clauses++;
      result->clauses = realloc(result->clauses, sizeof(*result->clauses)*result->num_clauses);
      result->clauses[result->num_clauses-1] = c;
    }
    else {
      fif_mapping *entry = malloc(sizeof(*entry));
      entry->indexing_conc_name = malloc(strlen(name)+1);
      strcpy(entry->indexing_conc_name, name);
      entry->num_clauses = 1;
      entry->clauses = malloc(sizeof(*entry->clauses));
      entry->clauses[0] = c;
      tommy_hashlin_insert(&collection->fif_map, &entry->node, entry, tommy_hash_u64(0, entry->indexing_conc_name, strlen(entry->indexing_conc_name)));
    }
  }
  else {
    // If non-fif, indexes clause into pos/neg hashmaps/lists
    for (int j = 0; j < c->pos_count; j++)
      map_add_clause(&collection->pos_map, &collection->pos_list, c->pos_lits[j], c);
    for (int j = 0; j < c->neg_count; j++)
      map_add_clause(&collection->neg_map, &collection->neg_list, c->neg_lits[j], c);
  }
}

// Special semantic operator: true
// Must be a singleton positive literal with unary quote arg
// Process quoted material into new formulas with truth as parent
static void handle_true(kb *collection, clause *truth, kb_logger *logger) {
  if (truth->pos_lits[0]->term_count == 1 && truth->pos_lits[0]->terms[0].type == QUOTE) {
    alma_quote *quote = truth->pos_lits[0]->terms[0].quote;
    tommy_array unquoted;
    tommy_array_init(&unquoted);
    // Raw sentence must be converted into clauses
    if (quote->type == SENTENCE) {
      alma_node *sentence_copy = malloc(sizeof(*sentence_copy));
      copy_alma_tree(quote->sentence, sentence_copy);
      make_cnf(sentence_copy);
      nodes_to_clauses(sentence_copy, 1, &unquoted, 0, &collection->variable_id_count, logger);
    }
    // Quote clause can be extracted directly
    else {
      clause *u = malloc(sizeof(*u));
      copy_clause_structure(quote->clause_quote, u);

      // True formula has its outermost quotation withdrawn
      decrement_quote_level(u, 1);

      // Adjust variable IDs for the new formula
      set_variable_ids(u, 1, 0, NULL, &collection->variable_id_count);
      tommy_array_insert(&unquoted, u);
    }

    for (int i = 0; i < tommy_array_size(&unquoted); i++) {
      clause *curr = tommy_array_get(&unquoted, i);
      init_single_parent(curr, truth);

      // Inserted same timestep
      tommy_array_insert(&collection->timestep_delay_clauses, curr);
    }
    tommy_array_done(&unquoted);
  }
}

// Special semantic operator: distrust
// If argument unifies with a formula in the KB, derives distrusted
// Distrusted formulas cannot be more general than argument
// Hence, clauses found are placed in a quotation term with no added quasi-quotation
static void handle_distrust(kb *collection, clause *distrust) {
  alma_function *lit = distrust->pos_lits[0];
  if (lit->term_count == 1 && lit->terms[0].type == QUOTE && lit->terms[0].quote->type == CLAUSE) {
    void *mapping = clause_lookup(collection, lit->terms[0].quote->clause_quote);
    if (mapping != NULL) {
      alma_quote *q = malloc(sizeof(*q));
      q->type = CLAUSE;

      for (int i = mapping_num_clauses(mapping, lit->terms[0].quote->clause_quote->tag)-1; i >= 0; i--) {
        clause *ith = mapping_access(mapping, lit->terms[0].quote->clause_quote->tag, i);
        if (flags_negative(ith) && counts_match(ith, lit->terms[0].quote->clause_quote)) {
          q->clause_quote = ith;
          binding_list *theta = malloc(sizeof(*theta));
          init_bindings(theta);

          // Distrust each match found
          if (quote_term_unify(lit->terms[0].quote, q, theta)) {
            tommy_array_insert(&collection->distrust_set, ith);
            tommy_array_insert(&collection->distrust_parents, distrust);
          }
          cleanup_bindings(theta);
        }
      }
      free(q);
    }
  }
}

static int all_digits(char *str) {
  for (int i = 0; i < strlen(str); i++)
    if (!isdigit(str[i]))
      return 0;
  return 1;
}

// Special semantic operator: reinstate
// Reinstatement succeeds if given args of a quote matching distrusted formula(s) and a matching timestep for when distrusted
static void handle_reinstate(kb *collection, clause *reinstate, long time) {
  alma_term *arg1 = reinstate->pos_lits[0]->terms+0;
  alma_term *arg2 = reinstate->pos_lits[0]->terms+1;

  void *mapping = clause_lookup(collection, arg1->quote->clause_quote);
  if (mapping != NULL) {
    alma_quote *q = malloc(sizeof(*q));
    q->type = CLAUSE;

    for (int j = mapping_num_clauses(mapping, arg1->quote->clause_quote->tag)-1; j >= 0; j--) {
      clause *jth = mapping_access(mapping, arg1->quote->clause_quote->tag, j);
      if (jth->distrusted == atol(arg2->function->name) && counts_match(jth, arg1->quote->clause_quote)) {
        q->clause_quote = jth;
        binding_list *theta = malloc(sizeof(*theta));
        init_bindings(theta);

        // Unification succeeded, create and add the reinstatement clause
        if (quote_term_unify(arg1->quote, q, theta)) {
          clause *reinstatement = malloc(sizeof(*reinstatement));
          copy_clause_structure(jth, reinstatement);
          copy_parents(jth, reinstatement);
          subst_clause(theta, reinstatement, 0);
          set_variable_ids(reinstatement, 1, 0, NULL, &collection->variable_id_count);
          tommy_array_insert(&collection->timestep_delay_clauses, reinstatement);

          if (reinstatement->pos_count + reinstatement->neg_count == 1 && reinstatement->fif == NULL) {
            // Look for contradicting() instances with reinstate target as a contradictand, to be handled()
            char *name = name_with_arity("contradicting", 3);
            predname_mapping *result = tommy_hashlin_search(&collection->pos_map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
            if (result != NULL) {
              // Select argument to match reinstated literal
              int contra_arg = (reinstatement->neg_count == 1);

              for (int k = 0; k < result->num_clauses; k++) {
                clause *con = result->clauses[k];
                if (con->pos_count == 1 && con->neg_count == 0 && con->fif == NULL &&
                    (flags_negative(con) || flag_min(con) == time) &&
                    con->pos_lits[0]->term_count == 3 && con->pos_lits[0]->terms[contra_arg].type == QUOTE) {

                  binding_list *contra_theta = malloc(sizeof(*contra_theta));
                  init_bindings(contra_theta);

                  // Contradicting() formula found with argument matching reinstate arg; mark this contradiction as handled
                  if (quote_term_unify(con->pos_lits[0]->terms[contra_arg].quote, q, contra_theta)) {
                    tommy_array_insert(&collection->handle_set, con);
                    tommy_array_insert(&collection->handle_parents, reinstate);
                  }
                  cleanup_bindings(contra_theta);
                }
              }
            }
            free(name);

            // Look for distrusted() instances with reinstate target, to be retired (??)
            // TODO
          }
        }
        cleanup_bindings(theta);
      }
    }
    free(q);
  }

}

// Special semantic operator: update
// Updating succeeds if given a quote matching KB formula(s) and a quote for the updated clause
static void handle_update(kb *collection, clause *update) {
  alma_term *arg1 = update->pos_lits[0]->terms+0;
  alma_term *arg2 = update->pos_lits[0]->terms+1;

  void *mapping = clause_lookup(collection, arg1->quote->clause_quote);
  if (mapping != NULL) {
    alma_quote *q = malloc(sizeof(*q));
    q->type = CLAUSE;

    for (int j = mapping_num_clauses(mapping, arg1->quote->clause_quote->tag)-1; j >= 0; j--) {
      clause *jth = mapping_access(mapping, arg1->quote->clause_quote->tag, j);
      if (jth->distrusted < 0 && counts_match(jth, arg1->quote->clause_quote)) {
        q->clause_quote = jth;
        binding_list *theta = malloc(sizeof(*theta));
        init_bindings(theta);

        // Unification succeeded, create and add the updated clause and distrust original
        if (quote_term_unify(arg1->quote, q, theta)) {
          clause *update_clause = malloc(sizeof(*update_clause));
          copy_clause_structure(arg2->quote->clause_quote, update_clause);
          init_single_parent(update_clause, update);

          subst_clause(theta, update_clause, 0);
          set_variable_ids(update_clause, 1, 0, NULL, &collection->variable_id_count);
          tommy_array_insert(&collection->new_clauses, update_clause);

          tommy_array_insert(&collection->distrust_set, jth);
          tommy_array_insert(&collection->distrust_parents, update);
        }
        cleanup_bindings(theta);
      }
    }
    free(q);
  }
}

// Processing of new clauses: inserts non-duplicates derived, makes new tasks from if flag is active
// Special handling for true, reinstate, distrust, update
void process_new_clauses(kb *collection, alma_proc *procs, long time, kb_logger *logger, int make_tasks) {
  fif_to_front(&collection->new_clauses);

  for (tommy_size_t i = 0; i < tommy_array_size(&collection->new_clauses); i++) {
    clause *c = tommy_array_get(&collection->new_clauses, i);
    clause *dupe;
    if ((dupe = duplicate_check(collection, time, c, 0)) == NULL) {
      //c->dirty_bit = 1;
      add_clause(collection, c, time);

      if (c->pos_count == 1 && c->neg_count == 0) {
        if (strcmp(c->pos_lits[0]->name, "true") == 0) {
          tommy_array_insert(&collection->trues, c);
          tommy_array_set(&collection->new_clauses, i, NULL);
        }
        else if (strcmp(c->pos_lits[0]->name, "distrust") == 0)
          handle_distrust(collection, c);
      }

      // Case for reinstate
      // Reinstate() be a singleton positive literal with binary args, former as quotation term
      if (c->pos_count == 1 && c->neg_count == 0 && strcmp(c->pos_lits[0]->name, "reinstate") == 0) {
        if (c->tag == NONE && c->pos_lits[0]->term_count == 2 && c->pos_lits[0]->terms[0].type == QUOTE &&
            c->pos_lits[0]->terms[0].quote->type == CLAUSE && c->pos_lits[0]->terms[1].type == FUNCTION &&
            c->pos_lits[0]->terms[1].function->term_count == 0 && all_digits(c->pos_lits[0]->terms[1].function->name)) {
          if (c->pos_lits[0]->terms[0].quote->clause_quote->pos_count == 1 &&
              c->pos_lits[0]->terms[0].quote->clause_quote->neg_count == 0) {
            tommy_array_insert(&collection->pos_lit_reinstates, c);
            tommy_array_set(&collection->new_clauses, i, NULL);
          }
          else if (c->pos_lits[0]->terms[0].quote->clause_quote->neg_count == 1 &&
              c->pos_lits[0]->terms[0].quote->clause_quote->pos_count == 0) {
            tommy_array_insert(&collection->neg_lit_reinstates, c);
            tommy_array_set(&collection->new_clauses, i, NULL);
          }
          else
            handle_reinstate(collection, c, time);
        }
      }
      // Case for update
      // Must be a singleton positive literal with binary args, both as quotation terms
      else if (c->pos_count == 1 && c->neg_count == 0 && strcmp(c->pos_lits[0]->name, "update") == 0) {
        if (c->tag == NONE && c->pos_lits[0]->term_count == 2 && c->pos_lits[0]->terms[0].type == QUOTE &&
            c->pos_lits[0]->terms[0].quote->type == CLAUSE && c->pos_lits[0]->terms[1].type == QUOTE &&
            c->pos_lits[0]->terms[1].quote->type == CLAUSE) {
          handle_update(collection, c);
        }
      }
      // Non-reinstate/non-update sentences generate tasks
      else {
        // Update child info for parents of new clause
        if (c->parents != NULL) {
          for (int j = 0; j < c->parents[0].count; j++)
            add_child(c->parents[0].clauses[j], c);
        }

        if (make_tasks) {
          if (c->tag == FIF) {
            fif_task_map_init(collection, procs, &collection->fif_tasks, c, 1);
          }
          else {
            res_tasks_from_clause(collection, c, 1);
            fif_tasks_from_clause(&collection->fif_tasks, c);
          }
        }
      }
    }
    else {
      if (collection->verbose) {
        tee_alt("-a: Duplicate clause ", logger);
        clause_print(c, logger);
        tee_alt(" merged into %ld\n", logger, dupe->index);
      }

      if (c->parents != NULL) {
        transfer_parent(dupe, c, 1);
        dupe->dirty_bit = 1;
      }

      free_clause(c);
      tommy_array_set(&collection->new_clauses, i, NULL);
    }
  }
  tommy_array_done(&collection->new_clauses);
  tommy_array_init(&collection->new_clauses);

  // Process reinstatements of literals to look for contradictory reinstatements between them
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->pos_lit_reinstates); i++) {
    clause *reinstate = tommy_array_get(&collection->pos_lit_reinstates, i);
    int contradictory = 0;
    for (tommy_size_t j = 0; j < tommy_array_size(&collection->neg_lit_reinstates); j++) {
      clause *reinstate_neg = tommy_array_get(&collection->neg_lit_reinstates, j);
      // Proceed if reinstates' target literals have same predicate name
      alma_function *pos = reinstate->pos_lits[0]->terms[0].quote->clause_quote->pos_lits[0];
      alma_function *neg = reinstate_neg->pos_lits[0]->terms[0].quote->clause_quote->neg_lits[0];
      if (strcmp(pos->name, neg->name) == 0) {
        binding_list *theta = malloc(sizeof(*theta));
        init_bindings(theta);

        // If unification succeeds, then they're contradictory reinstatement targets
        if (pred_unify(pos, neg, theta, 0)) {
          make_contra(collection, reinstate, reinstate_neg, time);
          contradictory = 1;
        }
        cleanup_bindings(theta);
      }
    }
    // If found no contra iterating the negative reinstate literals, handle to fully reinstate
    if (!contradictory)
      handle_reinstate(collection, reinstate, time);
  }
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->neg_lit_reinstates); i++) {
    clause *reinstate = tommy_array_get(&collection->neg_lit_reinstates, i);
    int contradictory = 0;

    for (tommy_size_t j = 0; j < tommy_array_size(&collection->distrust_set); j++) {
      clause *distrusted = tommy_array_get(&collection->distrust_set, j);
      if (distrusted->index == reinstate->index) {
        contradictory = 1;
        break;
      }
    }

    // If negative reinstate literal is not distrusted from earlier contra, handle to fully reinstate
    if (!contradictory)
      handle_reinstate(collection, reinstate, time);
  }
  tommy_array_done(&collection->pos_lit_reinstates);
  tommy_array_init(&collection->pos_lit_reinstates);
  tommy_array_done(&collection->neg_lit_reinstates);
  tommy_array_init(&collection->neg_lit_reinstates);

  // Alter flag and make distrusted() formula for those newly distrusted, as well as distrusted descendants
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->distrust_set); i++) {
    clause *c = tommy_array_get(&collection->distrust_set, i);
    clause *parent = tommy_array_get(&collection->distrust_parents, i);
    distrust_recursive(collection, c, parent, time);
  }
  tommy_array_done(&collection->distrust_set);
  tommy_array_init(&collection->distrust_set);
  tommy_array_done(&collection->distrust_parents);
  tommy_array_init(&collection->distrust_parents);

  // Alter flag and make handled() formula for those newly handled
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->handle_set); i++) {
    clause *c = tommy_array_get(&collection->handle_set, i);
    clause *parent = tommy_array_get(&collection->handle_parents, i);
    c->handled = time;
    clause *handled = make_meta_literal(collection, "handled", c, time);
    init_single_parent(handled, parent);
    tommy_array_insert(&collection->new_clauses, handled);
  }
  tommy_array_done(&collection->handle_set);
  tommy_array_init(&collection->handle_set);
  tommy_array_done(&collection->handle_parents);
  tommy_array_init(&collection->handle_parents);

  // Alter flag and make retired() formula for those newly retired, as well as retired descendants
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->distrust_set); i++) {
    clause *c = tommy_array_get(&collection->retire_set, i);
    clause *parent = tommy_array_get(&collection->retire_parents, i);
    retire_recursive(collection, c, parent);
  }
  tommy_array_done(&collection->retire_set);
  tommy_array_init(&collection->retire_set);
  tommy_array_done(&collection->retire_parents);
  tommy_array_init(&collection->retire_parents);

  for (tommy_size_t i = 0; i < tommy_array_size(&collection->trues); i++) {
    clause *c = tommy_array_get(&collection->trues, i);
    if (flags_negative(c))
      handle_true(collection, c, logger);
  }
  tommy_array_done(&collection->trues);
  tommy_array_init(&collection->trues);

}
