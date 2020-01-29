#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "alma_kb.h"
#include "alma_backsearch.h"
#include "alma_fif.h"
#include "alma_proc.h"
#include "alma_print.h"

static long next_index;

static void make_clause(alma_node *node, clause *c) {
  // Note: predicate null assignment necessary for freeing of notes without losing predicates
  if (node != NULL) {
    if (node->type == FOL) {
      // Neg lit case for NOT
      if (node->fol->op == NOT) {
        c->neg_count++;
        c->neg_lits = realloc(c->neg_lits, sizeof(*c->neg_lits) * c->neg_count);
        c->neg_lits[c->neg_count-1] = node->fol->arg1->predicate;
        node->fol->arg1->predicate = NULL;
      }
      // Case of node is OR
      else {
        make_clause(node->fol->arg1, c);
        make_clause(node->fol->arg2, c);
      }
      if (c->tag == NONE)
        c->tag = node->fol->tag;
    }
    // Otherwise, only pos lit left
    else {
      c->pos_count++;
      c->pos_lits = realloc(c->pos_lits, sizeof(*c->pos_lits) * c->pos_count);
      c->pos_lits[c->pos_count-1] = node->predicate;
      node->predicate = NULL;
    }
  }
}

static void find_variable_names(tommy_array *list, alma_term *term, int id_from_name) {
  if (term->type == VARIABLE) {
    for (tommy_size_t i = 0; i < tommy_array_size(list); i++) {
      if (id_from_name) {
        if (strcmp(term->variable->name, tommy_array_get(list, i)) == 0)
          return;
      }
      else if (term->variable->id == *(long long *)tommy_array_get(list, i))
        return;
    }
    if (id_from_name) {
      tommy_array_insert(list, term->variable->name);
    }
    else {
      long long *id = malloc(sizeof(*id));
      *id =  term->variable->id;
      tommy_array_insert(list, id);
    }
  }
  else
    for (int i = 0; i < term->function->term_count; i++)
      find_variable_names(list, term->function->terms+i, id_from_name);
}

static void set_variable_names(tommy_array *list, alma_term *term, int id_from_name) {
  if (term->type == VARIABLE) {
    for (tommy_size_t i = 0; i < tommy_array_size(list); i++) {
      if (id_from_name) {
        if (strcmp(term->variable->name, tommy_array_get(list, i)) == 0)
          term->variable->id = variable_id_count + i;
      }
      else if (term->variable->id == *(long long *)tommy_array_get(list, i))
        term->variable->id = variable_id_count + i;
    }
  }
  else
    for (int i = 0; i < term->function->term_count; i++)
      set_variable_names(list, term->function->terms+i, id_from_name);
}

// Given a clause, assign the ID fields of each variable
// Two cases:
// 1) If clause is result of resolution, replace existing matching IDs with fresh values each
// 2) Otherwise, give variables with the same name matching IDs
// Fresh ID values drawn from variable_id_count global variable
void set_variable_ids(clause *c, int id_from_name, binding_list *bs_bindings) {
  tommy_array vars;
  tommy_array_init(&vars);

  for (int i = 0; i < c->pos_count; i++)
    for (int j = 0; j < c->pos_lits[i]->term_count; j++)
      find_variable_names(&vars, c->pos_lits[i]->terms+j, id_from_name);
  for (int i = 0; i < c->neg_count; i++)
    for (int j = 0; j < c->neg_lits[i]->term_count; j++)
      find_variable_names(&vars, c->neg_lits[i]->terms+j, id_from_name);

  for (int i = 0; i < c->pos_count; i++)
    for (int j = 0; j < c->pos_lits[i]->term_count; j++)
      set_variable_names(&vars, c->pos_lits[i]->terms+j, id_from_name);
  for (int i = 0; i < c->neg_count; i++)
    for (int j = 0; j < c->neg_lits[i]->term_count; j++)
      set_variable_names(&vars, c->neg_lits[i]->terms+j, id_from_name);

  // If bindings for a backsearch have been passed in, update variable names for them as well
  if (bs_bindings)
    for (int i = 0; i < bs_bindings->num_bindings; i++)
      set_variable_names(&vars, bs_bindings->list[i].term, id_from_name);

  variable_id_count += tommy_array_size(&vars);
  if (!id_from_name) {
    for (int i = 0; i < tommy_array_size(&vars); i++)
      free(tommy_array_get(&vars, i));
  }
  tommy_array_done(&vars);
}

static void init_ordering_rec(fif_info *info, alma_node *node, int *next, int *pos, int *neg) {
  if (node->type == FOL) {
    // Neg lit case for NOT
    if (node->fol->op == NOT)
      info->ordering[(*next)++] = (*neg)--;
    // Case of node is OR
    else {
      init_ordering_rec(info, node->fol->arg1, next, pos, neg);
      init_ordering_rec(info, node->fol->arg2, next, pos, neg);
    }
  }
  // Otherwise, pos lit
  else
    info->ordering[(*next)++] = (*pos)++;
}

// Given a node for a fif formula, record inorder traversal ofpositive and negative literals
static void init_ordering(fif_info *info, alma_node *node) {
  int next = 0;
  int pos = 0;
  int neg = -1;
  // Fif node must necessarily at top level have an OR -- ignore second branch of with conclusion
  init_ordering_rec(info, node->fol->arg1, &next, &pos, &neg);
}

// Comparison used by qsort of clauses -- orders by increasing function name and increasing arity
static int function_compare(const void *p1, const void *p2) {
  alma_function **f1 = (alma_function **)p1;
  alma_function **f2 = (alma_function **)p2;
  int compare = strcmp((*f1)->name, (*f2)->name);
  if (compare == 0)
    return (*f1)->term_count - (*f2)->term_count;
  else
    return compare;
}

// Flattens a single alma node and adds its contents to collection
// Recursively calls when an AND is found to separate conjunctions
void flatten_node(alma_node *node, tommy_array *clauses, int print) {
  if (node->type == FOL && node->fol->op == AND) {
    if (node->fol->arg1->type == FOL)
      node->fol->arg1->fol->tag = node->fol->tag;
    flatten_node(node->fol->arg1, clauses, print);
    if (node->fol->arg2->type == FOL)
      node->fol->arg2->fol->tag = node->fol->tag;
    flatten_node(node->fol->arg2, clauses, print);
  }
  else {
    clause *c = malloc(sizeof(*c));
    c->pos_count = c->neg_count = 0;
    c->pos_lits = c->neg_lits = NULL;
    c->parent_set_count = c->children_count = 0;
    c->parents = NULL;
    c->children = NULL;
    c->tag = NONE;
    c->fif = NULL;
    make_clause(node, c);
    set_variable_ids(c, 1, NULL);

    // If clause is fif, initialize additional info
    if (c->tag == FIF) {
      c->fif = malloc(sizeof(*c->fif));
      c->fif->premise_count = c->pos_count + c->neg_count - 1;
      c->fif->ordering = malloc(sizeof(*c->fif->ordering) * c->fif->premise_count);
      init_ordering(c->fif, node);
      c->fif->conclusion = c->pos_lits[c->pos_count-1]; // Conclusion will always be last pos_lit
    }

    // Non-fif clauses can be sorted by literal for ease of resolution
    // Fif clauses must retain original literal order to not affect their evaluation
    if (c->tag != FIF) {
      qsort(c->pos_lits, c->pos_count, sizeof(*c->pos_lits), function_compare);
      qsort(c->neg_lits, c->neg_count, sizeof(*c->neg_lits), function_compare);
    }

    if (print) {
      tee("-a: ");
      clause_print(c);
      tee(" added\n");
    }
    tommy_array_insert(clauses, c);
  }
}

void free_clause(clause *c) {
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
    free(c->fif);
  }

  free(c);
}

// Flattens trees into set of clauses (tommy_array must be initialized prior)
// trees argument freed here
void nodes_to_clauses(alma_node *trees, int num_trees, tommy_array *clauses, int print) {
  for (int i = 0; i < num_trees; i++) {
    flatten_node(trees+i, clauses, print);
    free_alma_tree(trees+i);
  }
  free(trees);
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
int im_compare(const void *arg, const void *obj) {
  return *(const long*)arg - ((const index_mapping*)obj)->key;
}

// Compare function to be used by tommy_hashlin_search for predname_mapping
// Compares string arg to predname of predname_mapping
int pm_compare(const void *arg, const void *obj) {
  return strcmp((const char*)arg, ((const predname_mapping*)obj)->predname);
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


typedef struct var_matching {
  int count;
  long long *x;
  long long *y;
} var_matching;

// Returns 0 if functions are equal while respecting x and y matchings based on matches arg; otherwise returns 1
// (Further detail in clauses_differ)
static int functions_differ(alma_function *x, alma_function *y, var_matching *matches) {
  if (x->term_count == y->term_count && strcmp(x->name, y->name) == 0) {
    for (int i = 0; i < x->term_count; i++) {
      if (x->terms[i].type == y->terms[i].type) {
        if (x->terms[i].type == VARIABLE) {
          long long xval = x->terms[i].variable->id;
          long long yval = y->terms[i].variable->id;
          // Look for matching variable in var_matching's x and y
          for (int j = 0; j < matches->count; j++) {
            // If only one of xval and yval matches, return unequal
            if ((xval == matches->x[j] && yval != matches->y[j]) || (yval == matches->y[j] && xval != matches->x[j]))
              return 1;
          }
          // No match was found, add a new one to the matching
          matches->count++;
          matches->x = realloc(matches->x, sizeof(*matches->x) * matches->count);
          matches->x[matches->count -1] = xval;
          matches->y = realloc(matches->y, sizeof(*matches->y) * matches->count);
          matches->y[matches->count -1] = yval;
        }
        else if (functions_differ(x->terms[i].function, y->terms[i].function, matches))
            return 1;
      }
      else
        return 1;
    }
    // All arguments compared as equal; return 0
    return 0;
  }
  return 1;
}

// Function to call when short-circuiting clauses_differ to properly free var_matching instance
static int release_matches(var_matching *matches, int retval) {
  free(matches->x);
  free(matches->y);
  return retval;
}

// Returns 0 if clauses have equal positive and negative literal sets; otherwise returns 1
// Equality of variables is that there is a one-to-one correspondence in the sets of variables x and y use,
// based on each location where a variable maps to another
// Thus a(X, X) and a(X, Y) are here considered different
int clauses_differ(clause *x, clause *y) {
  if (x->pos_count == y->pos_count && x->neg_count == y->neg_count){
    var_matching matches;
    matches.count = 0;
    matches.x = NULL;
    matches.y = NULL;
    if (x->tag == FIF) {
      for (int i = 0; i < x->fif->premise_count; i++) {
        alma_function *xf = fif_access(x, i);
        alma_function *yf = fif_access(y, i);
        if (function_compare(&xf, &yf) || functions_differ(xf, yf, &matches))
          return release_matches(&matches, 1);
      }
      if (function_compare(&x->fif->conclusion, &y->fif->conclusion) || functions_differ(x->fif->conclusion, y->fif->conclusion, &matches))
        return release_matches(&matches, 1);
    }
    else {
      for (int i = 0; i < x->pos_count; i++) {
        // TODO: account for case in which may have several literals with name
        // Ignoring this case, sorted literal lists allows comparing ith literals of each clause
        if (function_compare(x->pos_lits+i, y->pos_lits+i) || functions_differ(x->pos_lits[i], y->pos_lits[i], &matches))
          return release_matches(&matches, 1);
      }
      for (int i = 0; i < x->neg_count; i++) {
        // TODO: account for case in which may have several literals with name
        // Ignoring this case, sorted literal lists allows comparing ith literals of each clause
        if (function_compare(x->neg_lits+i, y->neg_lits+i) || functions_differ(x->neg_lits[i], y->neg_lits[i], &matches))
          return release_matches(&matches, 1);
      }
    }
    // All literals compared as equal; return 0
    return release_matches(&matches, 0);
  }
  return 1;
}

// Checks if x and y are ground literals that are identical
static int ground_duplicate_literals(alma_function *x, alma_function *y) {
  if (strcmp(x->name, y->name) == 0 && x->term_count == y->term_count) {
    for (int i = 0; i < x->term_count; i++)
      if (x->terms[i].type != y->terms[i].type || x->terms[i].type == VARIABLE || !ground_duplicate_literals(x->terms[i].function, y->terms[i].function))
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
        while (loc < *count && lits[loc] == NULL)
          loc++;
        if (loc >= *count)
          break;
        lits[i] = lits[loc];
        lits[loc] = NULL;
        loc++;
      }
    }
    *triple = realloc(lits, sizeof(*lits) * new_count);
    *count = new_count;
  }
}

// If c is found to be a clause duplicate, returns a pointer to that clause; null otherwise
// See comments preceding clauses_differ function for further detail
clause* duplicate_check(kb *collection, clause *c) {
  if (c->tag == FIF) {
    char *name = c->fif->conclusion->name;
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

    if (c->pos_count > 0) {
      // If clause has a positive literal, all duplicate candidates must have that same positive literal
      // Arbitrarily pick first positive literal as one to use; may be able to do smarter literal choice later
      char *name = name_with_arity(c->pos_lits[0]->name, c->pos_lits[0]->term_count);
      predname_mapping *result = tommy_hashlin_search(&collection->pos_map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
      free(name);
      if (result != NULL) {
        for (int i = 0; i < result->num_clauses; i++) {
          if (c->tag == result->clauses[i]->tag && !clauses_differ(c, result->clauses[i]))
            return result->clauses[i];
        }
      }
    }
    else if (c->neg_count > 0) {
      // If clause has a negative literal, all duplicate candidates must have that same negative literal
      // Arbitrarily pick first negative literal as one to use; may be able to do smarter literal choice later
      char *name = name_with_arity(c->neg_lits[0]->name, c->neg_lits[0]->term_count);
      predname_mapping *result = tommy_hashlin_search(&collection->neg_map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));
      free(name);
      if (result != NULL) {
        for (int i = 0; i < result->num_clauses; i++) {
          if (c->tag == result->clauses[i]->tag && !clauses_differ(c, result->clauses[i]))
            return result->clauses[i];
        }
      }
    }
  }
  return NULL;
}

// Given a new clause, add to the KB and maps
void add_clause(kb *collection, clause *c) {
  // Add clause to overall clause list and index map
  index_mapping *ientry = malloc(sizeof(*ientry));
  c->index = ientry->key = next_index++;
  ientry->value = c;
  c->learned = collection->time;
  tommy_list_insert_tail(&collection->clauses, &ientry->list_node, ientry);
  tommy_hashlin_insert(&collection->index_map, &ientry->hash_node, ientry, tommy_hash_u64(0, &ientry->key, sizeof(ientry->key)));

  if (c->tag == FIF) {
    char *name = c->fif->conclusion->name;
    // Index into fif hashmap
    fif_mapping *result = tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (result != NULL) {
      result->num_clauses++;
      result->clauses = realloc(result->clauses, sizeof(*result->clauses)*result->num_clauses);
      result->clauses[result->num_clauses-1] = c;
    }
    else {
      fif_mapping *entry = malloc(sizeof(*entry));
      entry->conclude_name = malloc(strlen(name)+1);
      strcpy(entry->conclude_name, name);
      entry->num_clauses = 1;
      entry->clauses = malloc(sizeof(*entry->clauses));
      entry->clauses[0] = c;
      tommy_hashlin_insert(&collection->fif_map, &entry->node, entry, tommy_hash_u64(0, entry->conclude_name, strlen(entry->conclude_name)));
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

// Remove res tasks using clause
static void remove_res_tasks(kb *collection, clause *c) {
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->res_tasks); i++) {
    res_task *current_task = tommy_array_get(&collection->res_tasks, i);
    if (current_task != NULL && (current_task->x == c || current_task->y == c)) {
      // Removed task set to null; null checked for when processing later
      tommy_array_set(&collection->res_tasks, i, NULL);
      free(current_task);
    }
  }
}

// Removes c from children list of p
static void remove_child(clause *p, clause *c) {
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

// Given a clause already existing in KB, remove from data structures
// Note that fif removal, or removal of a singleton clause used in a fif rule, may be very expensive
void remove_clause(kb *collection, clause *c) {
  if (c->tag == FIF) {
    // fif must be removed from fif_map and all fif tasks using it deleted
    char *name = c->fif->conclusion->name;
    fif_mapping *fifm = tommy_hashlin_search(&collection->fif_map, fifm_compare, name, tommy_hash_u64(0, name, strlen(name)));
    if (fifm != NULL) {
      tommy_hashlin_remove_existing(&collection->fif_map, &fifm->node);
      free_fif_mapping(fifm);
    }

    for (int i = 0; i < c->fif->premise_count; i++) {
      alma_function *f = fif_access(c, i);
      name = name_with_arity(f->name, f->term_count);
      fif_task_mapping *tm = tommy_hashlin_search(&collection->fif_tasks, fif_taskm_compare, name, tommy_hash_u64(0, name, strlen(name)));
      if (tm != NULL) {
        tommy_node *curr = tommy_list_head(&tm->tasks);
        while (curr) {
          fif_task *currdata = curr->data;
          curr = curr->next;
          if (currdata->fif == c) {
            tommy_list_remove_existing(&tm->tasks, &currdata->node);
            free_fif_task(currdata);
          }
        }
      }
      free(name);
    }
  }
  else {
    // non-fif must be unindexed from pos/neg maps, have resolution tasks and fif tasks (if in any) removed
    for (int i = 0; i < c->pos_count; i++)
      map_remove_clause(&collection->pos_map, &collection->pos_list, c->pos_lits[i], c);
    for (int i = 0; i < c->neg_count; i++)
      map_remove_clause(&collection->neg_map, &collection->neg_list, c->neg_lits[i], c);

    remove_res_tasks(collection, c);

    // May be used in fif tasks if it's a singleton clause
    if (c->pos_count + c->neg_count == 1)
      remove_fif_singleton_tasks(collection, c);
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

      if (new_count > 0) {
        // Collect non-null parent sets at start of parents
        int loc = 1;
        for (int j = 0; j < child->parent_set_count-1; j++) {
          if (child->parents[j].clauses == NULL) {
            while (loc < child->parent_set_count && child->parents[loc].clauses == NULL)
              loc++;
            if (loc >= child->parent_set_count)
              break;
            child->parents[j].clauses = child->parents[loc].clauses;
            child->parents[loc].clauses = NULL;
            loc++;
          }
        }
        child->parents = realloc(child->parents, sizeof(*child->parents)*new_count);
      }
      else {
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

// Compare function to be used by tommy_hashlin_search for distrust_mapping
// Compares long arg to key of distrust_mapping
static int dm_compare(const void *arg, const void *obj) {
  return *(const long*)arg - ((const distrust_mapping*)obj)->key;
}

int is_distrusted(kb *collection, long index) {
  return tommy_hashlin_search(&collection->distrusted, dm_compare, &index, tommy_hash_u64(0, &index, sizeof(index))) != NULL;
}

void make_single_task(kb *collection, clause *c, alma_function *c_lit, clause *other, tommy_array *tasks, int use_bif, int pos) {
  if (c != other && (other->tag != BIF || use_bif)) {
    alma_function *other_lit = literal_by_name(other, c_lit->name, pos);
    if (other_lit != NULL) {
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
void make_res_tasks(kb *collection, clause *c, int count, alma_function **c_lits, tommy_hashlin *map, tommy_array *tasks, int use_bif, int pos) {
  for (int i = 0; i < count; i++) {
    char *name = name_with_arity(c_lits[i]->name, c_lits[i]->term_count);
    predname_mapping *result = tommy_hashlin_search(map, pm_compare, name, tommy_hash_u64(0, name, strlen(name)));

    // New tasks are from Cartesian product of result's clauses with clauses' ith
    if (result != NULL)
      for (int j = 0; j < result->num_clauses; j++)
        make_single_task(collection, c, c_lits[i], result->clauses[j], tasks, use_bif, pos);

    free(name);
  }
}

// Finds new res tasks based on matching pos/neg predicate pairs, where one is from the KB and the other from arg
// Tasks are added into the res_tasks of collection
// Used only for non-bif resolution tasks; hence checks tag of c
void res_tasks_from_clause(kb *collection, clause *c, int process_negatives) {
  if (c->tag != BIF) {
    make_res_tasks(collection, c, c->pos_count, c->pos_lits, &collection->neg_map, &collection->res_tasks, 0, 0);
    // Only done if clauses differ from KB's clauses (i.e. after first task generation)
    if (process_negatives)
      make_res_tasks(collection, c, c->neg_count, c->neg_lits, &collection->pos_map, &collection->res_tasks, 0, 1);
  }
}

// Returns boolean based on success of string parse
// If parses successfully, adds to collection's new_clauses
int assert_formula(kb *collection, char *string, int print) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas)) {
    nodes_to_clauses(formulas, formula_count, &collection->new_clauses, print);
    return 1;
  }
  return 0;
}

int delete_formula(kb *collection, char *string, int print) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas)) {
    // Convert formulas to clause array
    tommy_array clauses;
    tommy_array_init(&clauses);
    for (int i = 0; i < formula_count; i++) {
      flatten_node(formulas+i, &clauses, 0);
      free_alma_tree(formulas+i);
    }
    free(formulas);

    // Process and remove each clause
    for (tommy_size_t i = 0; i < tommy_array_size(&clauses); i++) {
      clause *curr = tommy_array_get(&clauses, i);
      clause *c = duplicate_check(collection, curr);
      if (c != NULL) {
        if (print) {
          tee("-a: ");
          clause_print(c);
          tee(" removed\n");
        }
        remove_clause(collection, c);
      }
      free_clause(curr);
    }
    tommy_array_done(&clauses);

    return 1;
  }
  return 0;
}

int update_formula(kb *collection, char *string) {
  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(string, 0, &formula_count, &formulas)) {
    // Must supply two formula arguments to update
    if (formula_count != 2) {
      for (int i = 0; i < formula_count; i++)
        free_alma_tree(formulas+i);
      free(formulas);
      tee("-a: Incorrect number of arguments to update\n");
      return 0;
    }

    // Convert formulas to clause array
    tommy_array clauses;
    tommy_array_init(&clauses);
    for (int i = 0; i < formula_count; i++) {
      flatten_node(formulas+i, &clauses, 0);
      free_alma_tree(formulas+i);
    }
    free(formulas);

    int update_fail = 0;
    clause *target = tommy_array_get(&clauses, 0);
    clause *update = tommy_array_get(&clauses, 1);
    tommy_array_done(&clauses);
    clause *t_dupe;
    clause *u_dupe;
    if (target->tag == FIF || update->tag == FIF) {
      tee("-a: Cannot update with fif clause\n");
      update_fail = 1;
    }
    else if ((t_dupe = duplicate_check(collection, target)) == NULL) {
      tee("-a: Clause ");
      clause_print(target);
      tee(" to update not present\n");
      update_fail = 1;
    }
    else if ((u_dupe = duplicate_check(collection, update)) != NULL) {
      tee("-a: New version of clause already present\n");
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
      remove_res_tasks(collection, t_dupe);
      if (t_dupe->pos_count + t_dupe->neg_count == 1)
        remove_fif_singleton_tasks(collection, t_dupe);

      tee("-a: ");
      clause_print(target);
      tee(" updated to ");
      clause_print(update);
      tee("\n");
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

      // Generate new tasks with updated clause
      res_tasks_from_clause(collection, t_dupe, 1);
      fif_tasks_from_clause(collection, t_dupe);
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
      alma_function *litcopy = malloc(sizeof(*litcopy));
      copy_alma_function(lits[i], litcopy);
      for (int j = 0; j < litcopy->term_count; j++)
        subst(mgu, litcopy->terms+j);
      res_lits[*res_count] = litcopy;
      (*res_count)++;
    }
  }
}

// Given an MGU, substitute on literals other than pair unified and make a single resulting clause
void resolve(res_task *t, binding_list *mgu, clause *result) {
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
}

char* now(long t) {
  int len = snprintf(NULL, 0, "%ld", t);
  char *str = malloc(4 + len+1 + 2);
  strcpy(str, "now(");
  snprintf(str+4, len+1, "%ld", t);
  strcpy(str+4+len, ").");
  return str;
}

char* walltime() {
  struct timeval tval;
  gettimeofday(&tval, NULL);
  int sec_len = snprintf(NULL, 0, "%ld", (long int)tval.tv_sec);
  char *str = malloc(8 + sec_len + 10 + 1);
  strcpy(str, "wallnow(");
  snprintf(str+8, sec_len+1, "%ld", (long int)tval.tv_sec);
  strcpy(str+8+sec_len, ", ");
  snprintf(str+8+sec_len+2, 6+1, "%06ld", (long int)tval.tv_usec);
  strcpy(str+8+sec_len+8, ").");
  return str;
}

char* long_to_str(long x) {
  int length = snprintf(NULL, 0, "%ld", x);
  char *str = malloc(length+1);
  snprintf(str, length+1, "%ld", x);
  return str;
}

void add_child(clause *parent, clause *child) {
  parent->children_count++;
  parent->children = realloc(parent->children, sizeof(*parent->children) * parent->children_count);
  parent->children[parent->children_count-1] = child;
}

// Transfers first parent of source into parent collection of target if it's not a repeat
void transfer_parent(kb *collection, clause *target, clause *source, int add_children) {
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
      // Also check if parents of source are distrusted
      int distrust = 0;
      for (int j = 0; j < source->parents[0].count; j++) {
        int insert = 1;
        if (is_distrusted(collection, source->parents[0].clauses[j]->index))
          distrust = 1;
        for (int k = 0; k < source->parents[0].clauses[j]->children_count; k++) {
          if (source->parents[0].clauses[j]->children[k] == target) {
            insert = 0;
            break;
          }
        }
        if (insert)
          add_child(source->parents[0].clauses[j], target);
      }
      if (distrust && !is_distrusted(collection, target->index)) {
        char *time_str = long_to_str(collection->time);
        distrust_recursive(collection, target, time_str);
        free(time_str);
      }
    }
  }
}

void distrust_recursive(kb *collection, clause *c, char *time) {
  // Add c to distrusted set
  distrust_mapping *d = malloc(sizeof(*d));
  d->key = c->index;
  d->value = c;
  tommy_hashlin_insert(&collection->distrusted, &d->node, d, tommy_hash_u64(0, &d->key, sizeof(d->key)));

  // Assert distrusted formula
  char *index = long_to_str(c->index);
  char *distrust_str = malloc(strlen(index) + strlen(time) + 15);
  strcpy(distrust_str, "distrusted(");
  int loc = 11;
  strcpy(distrust_str+loc, index);
  loc += strlen(index);
  free(index);
  distrust_str[loc++] = ',';
  strcpy(distrust_str+loc, time);
  loc += strlen(time);
  strcpy(distrust_str+loc, ").");
  assert_formula(collection, distrust_str, 0);
  free(distrust_str);

  // Unindex c from pos_map/list, neg_map/list to prevent use in inference
  for (int i = 0; i < c->pos_count; i++)
    map_remove_clause(&collection->pos_map, &collection->pos_list, c->pos_lits[i], c);
  for (int i = 0; i < c->neg_count; i++)
    map_remove_clause(&collection->neg_map, &collection->neg_list, c->neg_lits[i], c);

  // Recursively distrust children that aren't distrusted already
  if (c->children != NULL) {
    for (int i = 0; i < c->children_count; i++) {
      if (!is_distrusted(collection, c->children[i]->index)) {
        distrust_recursive(collection, c->children[i], time);
      }
    }
  }
}

static void binding_subst(binding_list *target, binding_list *theta) {
  for (int i = 0; i < target->num_bindings; i++)
    subst(theta, target->list[i].term);
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
void process_res_tasks(kb *collection, tommy_array *tasks, tommy_array *new_arr, backsearch_task *bs) {
  for (tommy_size_t i = 0; i < tommy_array_size(tasks); i++) {
    res_task *current_task = tommy_array_get(tasks, i);
    if (current_task != NULL) {
      // Does not do resolution with a distrusted clause
      if (!is_distrusted(collection, current_task->x->index) && !is_distrusted(collection,  current_task->y->index)) {
        binding_list *theta = malloc(sizeof(*theta));
        theta->list = NULL;
        theta->num_bindings = 0;
        // Given a res_task, attempt unification
        if (pred_unify(current_task->pos, current_task->neg, theta)) {
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
              parent_theta->num_bindings = 0;
              parent_theta->list = NULL;
              int unify_fail = 0;
              for (int j = 0; j < x_bindings->num_bindings; j++) {
                if (!unify(x_bindings->list[j].term, y_bindings->list[j].term, parent_theta)) {
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
                for (int j = 0; j < res_result->pos_count; j++)
                  for (int k = 0; k < res_result->pos_lits[j]->term_count; k++)
                    subst(parent_theta, res_result->pos_lits[j]->terms+k);
                for (int j = 0; j < res_result->neg_count; j++)
                  for (int k = 0; k < res_result->neg_lits[j]->term_count; k++)
                    subst(parent_theta, res_result->neg_lits[j]->terms+k);

                cleanup_bindings(parent_theta);
              }
            }
            else if (x_bindings == NULL) {
              // Consolidate to only x_bindings
              x_bindings = y_bindings;
            }
          }

          // An empty resolution result is not a valid clause to add to KB
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

            set_variable_ids(res_result, 0, x_bindings);

            tommy_array_insert(new_arr, res_result);
            if (bs)
              tommy_array_insert(&bs->new_clause_bindings, x_bindings);
          }
          else {
            free(res_result);

            if (bs) {
              clause *answer = malloc(sizeof(*answer));
              memcpy(answer, bs->target, sizeof(*answer));
              if (bs->target->pos_count == 1){
                answer->pos_lits = malloc(sizeof(*answer->pos_lits));
                answer->pos_lits[0] = malloc(sizeof(*answer->pos_lits[0]));
                copy_alma_function(bs->target->pos_lits[0], answer->pos_lits[0]);
                for (int j = 0; j < answer->pos_lits[0]->term_count; j++)
                  subst(x_bindings, answer->pos_lits[0]->terms+j);
              }
              else {
                answer->neg_lits = malloc(sizeof(*answer->neg_lits));
                answer->neg_lits[0] = malloc(sizeof(*answer->neg_lits[0]));
                copy_alma_function(bs->target->neg_lits[0], answer->neg_lits[0]);
                for (int j = 0; j < answer->neg_lits[0]->term_count; j++)
                  subst(x_bindings, answer->neg_lits[0]->terms+j);
              }

              // TODO: parent setup for answer?
              tommy_array_insert(&collection->new_clauses, answer);
              cleanup_bindings(x_bindings);
            }
            else {
              // If not a backward search, empty resolution result indicates a contradiction between clauses

              char *arg1 = long_to_str(current_task->x->index);
              char *arg2 = long_to_str(current_task->y->index);
              char *time_str = long_to_str(collection->time);

              char *contra_str = malloc(strlen(arg1) + strlen(arg2) + strlen(time_str) + 12);
              strcpy(contra_str, "contra(");
              int loc = 7;
              strcpy(contra_str+loc, arg1);
              loc += strlen(arg1);
              free(arg1);
              contra_str[loc++] = ',';
              strcpy(contra_str+loc, arg2);
              loc += strlen(arg2);
              free(arg2);
              contra_str[loc++] = ',';
              strcpy(contra_str+loc, time_str);
              loc += strlen(time_str);
              strcpy(contra_str+loc, ").");

              // Assert contra and distrusted
              assert_formula(collection, contra_str, 0);
              free(contra_str);
              distrust_recursive(collection, current_task->x, time_str);
              distrust_recursive(collection, current_task->y, time_str);
              free(time_str);
            }
          }
        }
        cleanup:
        cleanup_bindings(theta);
      }

      free(current_task);
    }
  }
  tommy_array_done(tasks);
  tommy_array_init(tasks);
}
