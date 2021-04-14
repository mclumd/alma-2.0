#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "alma_formula.h"
#include "alma_print.h"
#include "alma_parser.h"
#include "alma_kb.h"
#include "mpc/mpc.h"

// TODO: Longer term, check for error codes of library functions used

static int alma_function_init(alma_function *func, int quote_level, mpc_ast_t *ast);
static int alma_quote_init(alma_quote *quote, int quote_level, mpc_ast_t *ast);

// Recursively constructs ALMA FOL term representation of AST argument into term pointer
static int alma_term_init(alma_term *term, int quote_level, mpc_ast_t *ast) {
  // Variable
  if (strstr(ast->tag, "variable") != NULL) {
    term->type = VARIABLE;
    term->variable = malloc(sizeof(*term->variable));
    term->variable->name = malloc(strlen(ast->contents)+1);
    term->variable->id = 0;
    strcpy(term->variable->name, ast->contents);
    return 1;
  }
  // Function
  else if (strstr(ast->tag, "constant") != NULL || strstr(ast->children[0]->tag, "funcname") != NULL) {
    term->type = FUNCTION;
    term->function = malloc(sizeof(*term->function));
    return alma_function_init(term->function, quote_level, ast);
  }
  // Quasi-quote
  // Constructs ALMA quasi-quoted variable, counting backtick marks
  // If excessive quasi-quotation marks are given, they are reduced to the amount matching outer quotation
  // All quasi-quotation may be removed if erroneously given to non-quoted content
  else if (strstr(ast->tag, "quasiquote") != NULL) {
    int backtick_count = 0;
    alma_variable *var = malloc(sizeof(*var));
    while (strstr(ast->tag, "quasiquote") != NULL) {
      backtick_count++;
      ast = ast->children[1];
    }
    if (strstr(ast->tag, "variable") != NULL) {
      var->name = malloc(strlen(ast->contents)+1);
      var->id = 0;
      strcpy(var->name, ast->contents);
    }

    term->type = QUASIQUOTE;
    term->quasiquote = malloc(sizeof(*term->quasiquote));
    term->quasiquote->backtick_count = backtick_count;
    term->quasiquote->variable = var;

    // Error return case for excess quasi-quotation marks
    if (backtick_count > quote_level)
      return 0;
    else
      return 1;
  }
  // Quote
  else {
    term->type = QUOTE;
    term->quote = malloc(sizeof(*term->quote));
    return alma_quote_init(term->quote, quote_level+1, ast);
  }
}

// Recursively constructs ALMA FOL function representation of AST argument into ALMA function pointer
static int alma_function_init(alma_function *func, int quote_level, mpc_ast_t *ast) {
  // Case for function containing no terms
  if (ast->children_num == 0) {
    func->name = malloc(strlen(ast->contents)+1);
    strcpy(func->name, ast->contents);
    func->term_count = 0;
    func->terms = NULL;
    return 1;
  }
  // Otherwise, terms exist and should be populated in ALMA instance
  else {
    func->name = malloc(strlen(ast->children[0]->contents)+1);
    strcpy(func->name, ast->children[0]->contents);
    func->term_count = 0;
    func->terms = NULL; // Needed for realloc behavior

    mpc_ast_t *termlist = ast->children[2];
    // Case for single term in listofterms
    if (termlist->children_num == 0 || strstr(termlist->tag, "|term") != NULL) {
      func->term_count = 1;
      func->terms = malloc(sizeof(*func->terms));
      return alma_term_init(func->terms, quote_level, termlist);
    }
    // Case for listofterms with multiple terms
    else {
      func->term_count = (termlist->children_num+1)/2;
      func->terms = malloc(sizeof(*func->terms) * func->term_count);
      int ret = 1;
      for (int i = 0; i < func->term_count; i++) {
        ret = alma_term_init(func->terms+i, quote_level, termlist->children[i*2]) && ret;
      }
      return ret;
    }
  }
}

static int alma_tree_init(alma_node *alma_tree, int quote_level, mpc_ast_t *ast);

// Recursively constructs ALMA FOL function representation of AST argument into ALMA quote pointer
static int alma_quote_init(alma_quote *quote, int quote_level, mpc_ast_t *ast) {
  quote->type = SENTENCE;
  quote->sentence = malloc(sizeof(*quote->sentence));
  return alma_tree_init(quote->sentence, quote_level, ast->children[2]);
}

// Constructs ALMA FOL representation of a predicate:
// an ALMA node whose union is used to hold an ALMA function that describes the predicate
static int alma_predicate_init(alma_node *node, int quote_level, mpc_ast_t *ast) {
  node->type = PREDICATE;
  node->predicate = malloc(sizeof(*node->predicate));
  return alma_function_init(node->predicate, quote_level, ast);
}


// Contents should contain one of not/or/and/if
// Fif/bif still match strstr
static alma_operator op_from_contents(char *contents) {
  alma_operator result = NOT;
  if (strstr(contents, "or") != NULL)
    result = OR;
  else if (strstr(contents, "and") != NULL)
    result = AND;
  else if (strstr(contents, "if") != NULL)
    result = IF;
  return result;
}

// Given an MPC AST pointer, constructs an ALMA tree to a FOL representation of the AST
static int alma_tree_init(alma_node *alma_tree, int quote_level, mpc_ast_t *ast) {
  // Match tag containing literal as function
  if (strstr(ast->tag, "literal") != NULL) {
    return alma_predicate_init(alma_tree, quote_level, ast);
  }
  // Match nested pieces of formula/fformula/bformula/conjform/fformconc rules, recursively operate on
  // Dependent on only these productions containing string formula within an almaformula tree
  else if (strstr(ast->tag, "formula") != NULL || strstr(ast->tag, "conjform") != NULL
          || strstr(ast->tag, "fformconc") != NULL) {
    // Case for formula producing just a literal
    if (strstr(ast->children[0]->tag, "literal") != NULL) {
      return alma_predicate_init(alma_tree, quote_level, ast->children[0]);
    }
    // Otherwise, formula derives to FOL contents
    else {
      alma_tree->type = FOL;
      alma_tree->fol = malloc(sizeof(*alma_tree->fol));

      alma_tree->fol->op = op_from_contents(ast->children[0]->contents);
      alma_tree->fol->tag = NONE;

      // Set arg1 based on children
      alma_tree->fol->arg1 = malloc(sizeof(*alma_tree->fol->arg1));
      int ret = alma_tree_init(alma_tree->fol->arg1, quote_level, ast->children[1]);

      if (strstr(ast->tag, "fformula") != NULL)
        alma_tree->fol->tag = FIF;
      else if (strstr(ast->tag, "bformula") != NULL)
        alma_tree->fol->tag = BIF;

      // Set arg2 if operation is binary or/and/if
      if (alma_tree->fol->op != NOT) {
        alma_tree->fol->arg2 = malloc(sizeof(*alma_tree->fol->arg2));
        ret = alma_tree_init(alma_tree->fol->arg2, quote_level, ast->children[3]) && ret;
      }
      else {
        alma_tree->fol->arg2 = NULL;
      }

      return ret;
    }
  }
  return 0;
}

// Top-level, given an AST for entire ALMA file parse
// alma_trees must be freed by caller
static void generate_alma_trees(mpc_ast_t *ast, alma_node **alma_trees, int *size, alma_node **error_trees, int *error_size) {
  // Expects almaformula production to be children of top level of AST
  // If the grammar changes so top-level rule doesn't lead to almaformula, this must be changed
  *size = 0;
  // Only count children that contain an almaformula production
  for (int i = 0; i < ast->children_num; i++) {
    if (strstr(ast->children[i]->tag, "almaformula") != NULL)
      (*size)++;
  }
  *alma_trees = malloc(sizeof(**alma_trees) * *size);

  int index = 0;
  *error_size = 0;
  for (int i = 0; i < ast->children_num; i++) {
    if (strstr(ast->children[i]->tag, "almaformula") != NULL) {
      int ret = alma_tree_init(*alma_trees + index, 0, ast->children[i]->children[0]);
      // Error return due to excess quasi-quotation, print message about
      if (!ret) {
        (*error_size)++;
        *error_trees = realloc(*error_trees, sizeof(**error_trees) * *error_size);
        (*error_trees)[*error_size-1] = (*alma_trees)[index];
        (*alma_trees)[index].fol = NULL;
      }
      index++;
    }
  }

  // If any trees moved to error_trees, adjust alma_trees to remove NULLs
  if (*error_size > 0) {
    alma_node *no_error = malloc(sizeof(*no_error) * (*size - *error_size));
    index = 0;
    for (int i = 0; i < *size && index < *size - *error_size; i++) {
      if ((*alma_trees)[i].fol != NULL) {
        no_error[index] = (*alma_trees)[i];
        index++;
      }
    }
    free(*alma_trees);
    *alma_trees = no_error;
    *size = *size - *error_size;
  }
}

// Boolean return based on success of parsing
int fol_from_source(char *source, int file_src, int *formula_count, alma_node **formulas, int *error_count, alma_node **errors) {
  mpc_ast_t *ast;
  if (file_src ? parse_file(source, &ast) : parse_string(source, &ast)) {
    // Obtain ALMA tree representations from MPC's AST
    generate_alma_trees(ast, formulas, formula_count, errors, error_count);
    mpc_ast_delete(ast);
    return 1;
  }
  return 0;
}


void free_function(alma_function *func) {
  if (func == NULL)
    return;

  free(func->name);
  for (int i = 0; i < func->term_count; i++)
    free_term(func->terms+i);
  free(func->terms);
  free(func);
}

// If freeself is false, does NOT free the top-level alma_node
static void free_node(alma_node *node, int freeself) {
  if (node == NULL)
    return;

  if (node->type == FOL && node->fol != NULL) {
    free_node(node->fol->arg1, 1);
    free_node(node->fol->arg2, 1);
    free(node->fol);
  }
  else if (node->type == PREDICATE && node->predicate != NULL) {
    free_function(node->predicate);
  }

  if (freeself)
    free(node);
}

void free_quote(alma_quote *quote) {
  if (quote == NULL)
    return;

  if (quote->type == SENTENCE)
    free_node(quote->sentence, 1);
  else
    free_clause(quote->clause_quote);
  free(quote);
}

// Does not free alloc for term pointer itself, due to how terms are allocated in alma_function
void free_term(alma_term *term) {
  if (term->type == VARIABLE) {
    free(term->variable->name);
    free(term->variable);
  }
  else if (term->type == FUNCTION) {
    free_function(term->function);
  }
  else if (term->type == QUOTE) {
    free_quote(term->quote);
  }
  else {
    free(term->quasiquote->variable->name);
    free(term->quasiquote->variable);
    free(term->quasiquote);
  }
}

// Frees an alma_node EXCEPT for top-level
// If the entire node should be freed, call free_node with freeself true instead!
void free_alma_tree(alma_node *node) {
  free_node(node, 0);
}

// Space for copy must be allocated before call
void copy_alma_var(alma_variable *original, alma_variable *copy) {
  copy->name = malloc(strlen(original->name)+1);
  strcpy(copy->name, original->name);
  copy->id = original->id;
}

// Space for copy must be allocated before call
void copy_alma_function(alma_function *original, alma_function *copy) {
  copy->name = malloc(strlen(original->name)+1);
  strcpy(copy->name, original->name);
  copy->term_count = original->term_count;
  if (original->terms == NULL)
    copy->terms = NULL;
  else {
    copy->terms = malloc(sizeof(*copy->terms) * copy->term_count);
    for (int i = 0; i < copy->term_count; i++) {
      copy_alma_term(original->terms+i, copy->terms+i);
    }
  }
}

void copy_alma_quote(alma_quote *original, alma_quote *copy) {
  copy->type = original->type;
  if (copy->type == SENTENCE) {
    copy->sentence = malloc(sizeof(*copy->sentence));
    copy_alma_tree(original->sentence, copy->sentence);
  }
  else {
    copy->clause_quote = malloc(sizeof(*copy->clause_quote));
    copy_clause_structure(original->clause_quote, copy->clause_quote);
  }
}

void copy_alma_quasiquote(alma_quasiquote *original, alma_quasiquote *copy) {
  copy->backtick_count = original->backtick_count;
  copy->variable = malloc(sizeof(*copy->variable));
  copy_alma_var(original->variable, copy->variable);
}

// Space for copy must be allocated before call
void copy_alma_term(alma_term *original, alma_term *copy) {
  copy->type = original->type;
  if (original->type == VARIABLE) {
    copy->variable = malloc(sizeof(*copy->variable));
    copy_alma_var(original->variable, copy->variable);
  }
  else if (original->type == FUNCTION) {
    copy->function = malloc(sizeof(*copy->function));
    copy_alma_function(original->function, copy->function);
  }
  else if (original->type == QUOTE) {
    copy->quote = malloc(sizeof(*copy->quote));
    copy_alma_quote(original->quote, copy->quote);
  }
  else {
    copy->quasiquote = malloc(sizeof(*copy->quasiquote));
    copy_alma_quasiquote(original->quasiquote, copy->quasiquote);
  }
}

// Space for copy must be allocated before call
// If original is null and space is allocated for copy, probably will have memory issues
// So may make sense to just crash on null dereference instead of checking that
// TODO: Error return for failure?
void copy_alma_tree(alma_node *original, alma_node *copy) {
  copy->type = original->type;
  // FOL case
  if (original->type == FOL) {
    if (original->fol != NULL) {
      copy->fol = malloc(sizeof(*copy->fol));
      copy->fol->op = original->fol->op;

      if (original->fol->arg1 != NULL) {
        copy->fol->arg1 = malloc(sizeof(*copy->fol->arg1));
        copy_alma_tree(original->fol->arg1, copy->fol->arg1);
      }
      else
        copy->fol->arg1 = NULL;

      if (original->fol->arg2 != NULL) {
        copy->fol->arg2 = malloc(sizeof(*copy->fol->arg2));
        copy_alma_tree(original->fol->arg2, copy->fol->arg2);
      }
      else
        copy->fol->arg2 = NULL;

      copy->fol->tag = original->fol->tag;
    }
    else
      copy->fol = NULL;
  }
  // Function case
  else {
    if (original->predicate != NULL) {
      copy->predicate = malloc(sizeof(*copy->predicate));
      copy_alma_function(original->predicate, copy->predicate);
    }
    else
      copy->predicate = NULL;
  }
}

// Recursively converts all quote alma_nodes to clauses within alma_node
// Currently does so only if each quoted expression equivalent to single CNF clause
static void quote_convert(alma_node *node) {
  if (node->type == FOL) {
    quote_convert(node->fol->arg1);
    if (node->fol->arg2 != NULL)
      quote_convert(node->fol->arg2);
  }
  else {
    quote_convert_func(node->predicate);
  }
}

void quote_convert_func(alma_function *func) {
  for (int i = 0; i < func->term_count; i++) {
    if (func->terms[i].type == FUNCTION)
      quote_convert_func(func->terms[i].function);
    else if (func->terms[i].type == QUOTE) {
      alma_quote *quote = func->terms[i].quote;

      if (quote->type == SENTENCE) {
        quote_convert(quote->sentence);

        alma_node *copy = malloc(sizeof(*copy));
        copy_alma_tree(quote->sentence, copy);
        make_cnf(copy);

        // If result has an AND at the top-level, several clauses result, so abort
        if (copy->type == FOL && copy->fol->op == AND)
          free_node(copy, 1);
        // Otherwise, adjust quote to new type, and construct clause
        else {
          free_node(quote->sentence, 1);
          quote->type = CLAUSE;

          clause *c = malloc(sizeof(*c));
          c->pos_count = c->neg_count = 0;
          c->pos_lits = c->neg_lits = NULL;
          c->parent_set_count = c->children_count = 0;
          c->parents = NULL;
          c->children = NULL;
          c->tag = NONE;
          c->fif = NULL;
          make_clause(copy, c);
          free_node(copy, 1);
          quote->clause_quote = c;
        }
      }
    }
  }
}

// Constructs ALMA FOL operator (i.e. AND/OR/NOT/IF) from arguments
static void alma_fol_init(alma_node *node, alma_operator op, alma_node *arg1, alma_node *arg2, if_tag tag) {
  node->type = FOL;
  node->fol = malloc(sizeof(*node->fol));
  node->fol->op = op;
  node->fol->arg1 = arg1;
  node->fol->arg2 = arg2;
  node->fol->tag = tag;
}

// Recursively replaces all occurrences of IF(A,B) in node to OR(NOT(A),B)
static void eliminate_conditionals(alma_node *node) {
  if (node != NULL && node->type == FOL) {
    if (node->fol->op == IF) {
      alma_node *new_negation = malloc(sizeof(*new_negation));
      alma_fol_init(new_negation, NOT, node->fol->arg1, NULL, NONE);

      node->fol->op = OR;
      node->fol->arg1 = new_negation;
      eliminate_conditionals(new_negation->fol->arg1);
    }
    else {
      eliminate_conditionals(node->fol->arg1);
    }
    eliminate_conditionals(node->fol->arg2);
  }
}

// Recursively moves FOL negation inward by applying De Morgan's rules
// Doesn't handle IF case; must call after eliminate_conditionals
// If a FOL operator of IF is encountered, returns immediately
static void negation_inwards(alma_node *node) {
  if (node != NULL && node->type == FOL) {
    if (node->fol->op == NOT) {
      alma_node *notarg = node->fol->arg1;
      if (notarg->type == FOL) {
        switch (notarg->fol->op) {
          case AND:
          case OR: {
            alma_operator op = notarg->fol->op;
            // New nodes for result of De Morgan's
            alma_node *negated_arg1 = malloc(sizeof(*negated_arg1));
            alma_fol_init(negated_arg1, NOT, notarg->fol->arg1, NULL, NONE);
            alma_node *negated_arg2 = malloc(sizeof(*negated_arg2));
            alma_fol_init(negated_arg2, NOT, notarg->fol->arg2, NULL, NONE);
            // Free unused AND
            notarg->fol->arg1 = NULL;
            notarg->fol->arg2 = NULL;
            free_node(notarg, 1);
            // Adjust AND node to be OR, or OR node to be AND, instead of NOT
            node->fol->op = op == AND ? OR : AND;
            node->fol->arg1 = negated_arg1;
            node->fol->arg2 = negated_arg2;
            break;
          }
          case IF:
            return;
          case NOT: {
            free(node->fol);
            // Removal of double negation
            if (notarg->fol->arg1->type == FOL) {
              node->fol = notarg->fol->arg1->fol;
              notarg->fol->arg1->fol = NULL;
            }
            else {
              node->type = PREDICATE;
              node->predicate = notarg->fol->arg1->predicate;
              notarg->fol->arg1->predicate = NULL;
            }
            free_node(notarg, 1);
            // Recurse on current node
            negation_inwards(node);
            return;
          }
        }
      }
    }
    negation_inwards(node->fol->arg1);
    negation_inwards(node->fol->arg2);
  }
}

// Recursively does distribution of OR over AND so that node becomes conjunction of disjuncts
// If argument is in negation normal form, converts to CNF from there
// Does not modify anything contained inside of a NOT FOL node
static void dist_or_over_and(alma_node *node) {
  if (node != NULL && node->type == FOL) {
    if (node->fol->op == OR) {
      if (node->fol->arg1->type == FOL && node->fol->arg1->fol->op == AND) {
        // WLOG, (P /\ Q) \/ R

        // Create (P \/ R)
        alma_node *arg1_or = malloc(sizeof(*arg1_or));
        alma_fol_init(arg1_or, OR, node->fol->arg1->fol->arg1, node->fol->arg2, NONE);

        // Create (Q \/ R)
        alma_node *arg2_copy = malloc(sizeof(*arg2_copy));
        copy_alma_tree(node->fol->arg2, arg2_copy);
        alma_node *arg2_or = malloc(sizeof(*arg2_or));
        alma_fol_init(arg2_or, OR, node->fol->arg1->fol->arg2, arg2_copy, NONE);

        // Free old conjunction
        node->fol->arg1->fol->arg1 = NULL;
        node->fol->arg1->fol->arg2 = NULL;
        free_node(node->fol->arg1, 1);

        // Adjust node to conjunction
        node->fol->op = AND;
        node->fol->arg1 = arg1_or;
        node->fol->arg2 = arg2_or;
      }
      // If both are AND, after distributing arg2 over arg1, the conjunction of arg2
      // will appear lower in the tree, and dealt with in a later call.
      // Thus, the case below can safely be mutually exclusive.
      else if (node->fol->arg2->type == FOL && node->fol->arg2->fol->op == AND) {
        // WLOG, P \/ (Q /\ R)

        // Create (P \/ Q)
        alma_node *arg1_or = malloc(sizeof(*arg1_or));
        alma_fol_init(arg1_or, OR, node->fol->arg1, node->fol->arg2->fol->arg1, NONE);

        // Create (P \/ R)
        alma_node *arg1_copy = malloc(sizeof(*arg1_copy));
        copy_alma_tree(node->fol->arg1, arg1_copy);
        alma_node *arg2_or = malloc(sizeof(*arg2_or));
        alma_fol_init(arg2_or, OR, arg1_copy, node->fol->arg2->fol->arg2, NONE);

        // Free old conjunction
        node->fol->arg2->fol->arg1 = NULL;
        node->fol->arg2->fol->arg2 = NULL;
        free_node(node->fol->arg2, 1);

        // Adjust node to conjunction
        node->fol->op = AND;
        node->fol->arg1 = arg1_or;
        node->fol->arg2 = arg2_or;
      }
    }
    dist_or_over_and(node->fol->arg1);
    dist_or_over_and(node->fol->arg2);
  }
}

// Converts node from general FOL into CNF
void make_cnf(alma_node *node) {
  eliminate_conditionals(node);
  negation_inwards(node);
  dist_or_over_and(node);
}
