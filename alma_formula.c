#include <string.h>
#include "mpc/mpc.h"
#include "alma_formula.h"

void alma_function_init(alma_node *node, mpc_ast_t *ast) {
  node->type = FUNCTION;
  node->data.function = malloc(sizeof(alma_function));
  node->data.function->contents = ast;
}

void alma_fol_init(alma_fol fol, alma_operator op, alma_node *arg1, alma_node *arg2, if_tag *tag) {
  fol.op = op;
  fol.arg1 = arg1;
  fol.arg2 = arg2;
  fol.tag = tag;
}

alma_operator op_from_contents(char *contents) {
  alma_operator result;
  if (strstr(contents, "not") != NULL)
    result = NOT;
  else if (strstr(contents, "or") != NULL)
    result = OR;
  else if (strstr(contents, "and") != NULL)
    result = AND;
  else if (strstr(contents, "if") != NULL)
    result = IF;
  return result;
}

// Top-level, given an AST for entire ALMA file parse
void generate_alma_trees(mpc_ast_t *tree, alma_node **alma_trees, size_t *size) {
  // Expects almaformula production to be children of top level of AST
  // If the grammar changes such that top-level rule doesn't lead to almaformula,
  // this will have to be changed.
  for (int i = 0; i < tree->children_num; i++) {
    // TODO, obtain # trees from almaformula count in children
    // call single_alma_tree on alma_trees[i] per tree
  }
  // TODO
}

// Note: expects alma_tree to be allocated by caller of function
void single_alma_tree(mpc_ast_t *almaformula, alma_node *alma_tree) {
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
    // TODO: Refactor with better version of alma_fol_init
    else {
      alma_tree->type = FOL;
      alma_tree->data.fol = malloc(sizeof(alma_fol));
      alma_fol *fol = alma_tree->data.fol;
      fol->op = op_from_contents(almaformula->children[0]->contents);
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
        // Set tag to non-null if bif/fif
        if (strstr(almaformula->tag, "formula") != NULL) {
          fol->tag = NULL;
        }
        else if (strstr(almaformula->tag, "bformula") != NULL) {
          fol->tag = malloc(sizeof(if_tag));
          *fol->tag = BIF;
        }
      }
    }
  }
}
