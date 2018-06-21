#include <string.h>
#include <stdio.h>
#include "mpc/mpc.h"
#include "alma_formula.h"

// TODO: Longer term, check for error codes of library functions used

void alma_function_init(alma_node *node, mpc_ast_t *ast) {
  node->type = FUNCTION;
  node->data.function = malloc(sizeof(alma_function));
  node->data.function->contents = ast;
}

void alma_fol_init(alma_node *node, alma_operator op, alma_node *arg1, alma_node *arg2, if_tag *tag) {
  node->type = FOL;
  node->data.fol = malloc(sizeof(alma_fol));
  node->data.fol->op = op;
  node->data.fol->arg1 = arg1;
  node->data.fol->arg2 = arg2;
  node->data.fol->tag = tag;
}

alma_operator op_from_contents(char *contents) {
  // Probably should use something better than default NOT
  alma_operator result = NOT;
  if (strstr(contents, "or") != NULL)
    result = OR;
  else if (strstr(contents, "and") != NULL)
    result = AND;
  else if (strstr(contents, "if") != NULL)
    result = IF;
  return result;
}

// Note: expects alma_tree to be allocated by caller of function
static void single_alma_tree(mpc_ast_t *almaformula, alma_node *alma_tree) {
  // Match tag containing poslit as function
  if (strstr(almaformula->tag, "poslit") != NULL) {
    alma_function_init(alma_tree, almaformula);
  }
  // Match nested pieces of formula/fformula/bformula/conjform rules, recursively operate on
  // Probably needs regular expressions to avoid problems with rules having string "formula"
  else if (strstr(almaformula->tag, "formula") != NULL || strstr(almaformula->tag, "conjform") != NULL) {
    // Case for formula producing just a positive literal
    if (strstr(almaformula->children[0]->tag, "poslit") != NULL) {
      alma_function_init(alma_tree, almaformula->children[0]);
    }
    // Otherwise, formula derives to FOL contents
    // TODO: Refactor with alma_fol_init
    else {
      alma_tree->type = FOL;
      alma_tree->data.fol = malloc(sizeof(alma_fol));
      alma_fol *fol = alma_tree->data.fol;

      fol->op = op_from_contents(almaformula->children[0]->contents);
      fol->tag = NULL;

      // Set arg1 based on children
      fol->arg1 = malloc(sizeof(alma_node));
      single_alma_tree(almaformula->children[1], fol->arg1);

      if (strstr(almaformula->tag, "fformula") != NULL) {
        // Set arg2 for fformula conclusion
        fol->arg2 = malloc(sizeof(alma_node));
        single_alma_tree(almaformula->children[4], fol->arg2);
        fol->tag = malloc(sizeof(if_tag));
        *fol->tag = FIF;
      }
      else {
        // Set arg2 if operation is binary or/and/if
        switch (fol->op) {
          case OR:
          case AND:
          case IF:
            fol->arg2 = malloc(sizeof(alma_node));
            single_alma_tree(almaformula->children[3], fol->arg2);
            break;
          case NOT:
            fol->arg2 = NULL;
            break;
        }
        if (strstr(almaformula->tag, "bformula") != NULL) {
          fol->tag = malloc(sizeof(if_tag));
          *fol->tag = BIF;
        }
      }
    }
  }
}

// Top-level, given an AST for entire ALMA file parse
void generate_alma_trees(mpc_ast_t *tree, alma_node **alma_trees, size_t *size) {
  // Expects almaformula production to be children of top level of AST
  // If the grammar changes such that top-level rule doesn't lead to almaformula,
  // this will have to be changed.
  *size = 0;
  for (int i = 0; i < tree->children_num; i++) {
    if (strstr(tree->children[i]->tag, "almaformula") != NULL)
      (*size)++;
  }
  *alma_trees = malloc(sizeof(alma_node) * *size);

  int index = 0;
  for (int i = 0; i < tree->children_num; i++) {
    if (strstr(tree->children[i]->tag, "almaformula") != NULL) {
      single_alma_tree(tree->children[i]->children[0], *alma_trees + index);
      index++;
    }
  }
}


static void free_node(alma_node *node, int freeself) {
  if (node == NULL)
    return;

  if (node->type == FOL && node->data.fol != NULL) {
    free_node(node->data.fol->arg1, 1);
    free_node(node->data.fol->arg2, 1);
    if (node->data.fol->tag != NULL)
      free(node->data.fol->tag);
    free(node->data.fol);
  }
  else if (node->type == FUNCTION && node->data.function != NULL) {
    mpc_ast_delete(node->data.function->contents);
    free(node->data.function);
  }

  if (freeself)
    free(node);
}

void free_alma_tree(alma_node *node) {
  free_node(node, 0);
}


void eliminate_conditionals(alma_node *node) {
  if (node != NULL && node->type == FOL) {
    if (node->data.fol->op == IF) {
      alma_node *new_negation = malloc(sizeof(alma_node));
      alma_fol_init(new_negation, NOT, node->data.fol->arg1, NULL, NULL);

      node->data.fol->op = OR;
      node->data.fol->arg1 = new_negation;
      eliminate_conditionals(new_negation->data.fol->arg1);
    }
    else {
      eliminate_conditionals(node->data.fol->arg1);
    }
    eliminate_conditionals(node->data.fol->arg2);
  }
}

// Doesn't handle IF case; must call after eliminate_conditionals
// If a FOL operator of IF is encountered, returns immediately
void negation_inwards(alma_node *node) {
  if (node != NULL && node->type == FOL) {
    if (node->data.fol->op == NOT) {
      alma_node *notarg = node->data.fol->arg1;
      if (notarg->type == FOL) {
        // TODO: Refactor common and/or case
        switch (notarg->data.fol->op) {
          case AND: {
            // New nodes for result of DeMorgan's
            alma_node *negated_arg1 = malloc(sizeof(alma_node));
            alma_fol_init(negated_arg1, NOT, notarg->data.fol->arg1, NULL, NULL);
            alma_node *negated_arg2 = malloc(sizeof(alma_node));
            alma_fol_init(negated_arg2, NOT, notarg->data.fol->arg2, NULL, NULL);
            // Free unused AND
            notarg->data.fol->arg1 = NULL;
            notarg->data.fol->arg2 = NULL;
            free_node(notarg, 1);
            // Adjust node to be OR instead of NOT
            node->data.fol->op = OR;
            node->data.fol->arg1 = negated_arg1;
            node->data.fol->arg2 = negated_arg2;
            break;
          }
          case OR: {
            // New nodes for result of DeMorgan's
            alma_node *negated_arg1 = malloc(sizeof(alma_node));
            alma_fol_init(negated_arg1, NOT, notarg->data.fol->arg1, NULL, NULL);
            alma_node *negated_arg2 = malloc(sizeof(alma_node));
            alma_fol_init(negated_arg2, NOT, notarg->data.fol->arg2, NULL, NULL);
            // Free unused AND
            notarg->data.fol->arg1 = NULL;
            notarg->data.fol->arg2 = NULL;
            free_node(notarg, 1);
            // Adjust node to be AND instead of NOT
            node->data.fol->op = AND;
            node->data.fol->arg1 = negated_arg1;
            node->data.fol->arg2 = negated_arg2;
            break;
          }
          case IF:
            return;
          case NOT: {
            free(node->data.fol);
            // Removal of double negation
            if (notarg->data.fol->arg1->type == FOL) {
              node->data.fol = notarg->data.fol->arg1->data.fol;
            }
            else {
              node->type = FUNCTION;
              node->data.function = notarg->data.fol->arg1->data.function;
            }
            notarg->data.fol->arg1 = NULL;
            free_node(notarg, 1);
            // Recurse on current node
            negation_inwards(node);
            return;
          }
        }
      }
    }
    negation_inwards(node->data.fol->arg1);
    negation_inwards(node->data.fol->arg2);
  }
}


static void alma_print_rec(alma_node node, int indent) {
  char *spacing = malloc(sizeof(char) * indent*2 + 1);
  if (indent > 0)
    memset(spacing, ' ', indent*2);
  spacing[indent*2] = '\0';

  if (node.type == FOL) {
    char *op;
    switch (node.data.fol->op) {
      case NOT:
        op = "NOT"; break;
      case OR:
        op = "OR"; break;
      case AND:
        op = "AND"; break;
      case IF:
        op = "IF"; break;
    }

    if (node.data.fol->tag != NULL) {
      char *tag = "";
      switch (*node.data.fol->tag) {
        case FIF:
          tag = "FIF"; break;
        case BIF:
          tag = "BIF"; break;
      }
      printf("%sFOL: %s, tag: %s\n", spacing, op, tag);
    }
    else {
      printf("%sFOL: %s\n", spacing, op);
    }

    alma_print_rec(*node.data.fol->arg1, indent+1);
    if (node.data.fol->arg2 != NULL) {
      alma_print_rec(*node.data.fol->arg2, indent+1);
    }
  }
  // Currently, invoke mpc_ast printing on ast for alma_function
  else {
    printf("%sFUNCTION:\n", spacing);
    //mpc_ast_print_depth(node.data.function->contents, indent, stdout);
  }
  free(spacing);
}

void alma_print(alma_node node) {
  alma_print_rec(node, 0);
}


void mpc_ast_delete_selective(mpc_ast_t *a) {

  int i;

  if (a == NULL) { return; }

  if (strstr(a->tag, "poslit") == NULL) {
    for (i = 0; i < a->children_num; i++) {
      mpc_ast_delete_selective(a->children[i]);
    }

    free(a->children);
    free(a->tag);
    free(a->contents);
    free(a);
  }

}
