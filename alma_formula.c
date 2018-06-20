#include <string.h>
#include <stdio.h>
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
    // TODO: Refactor with better version of alma_fol_init
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


static void alma_print_rec(alma_node node, int indent) {
  char *spacing = malloc(sizeof(char) * indent*2 + 1);
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
    mpc_ast_print_depth(node.data.function->contents, indent, stdout);
  }
  free(spacing);
}

void alma_print(alma_node node) {
  alma_print_rec(node, 0);
}


static void free_node(alma_node *node, int freeself) {
  if (node == NULL)
    return;

  if (node->type == FOL) {
    free_node(node->data.fol->arg1, 1);
    if (node->data.fol->op != NOT) {
      free_node(node->data.fol->arg2, 1);
    }
    if (node->data.fol->tag != NULL)
      free(node->data.fol->tag);
    free(node->data.fol);
  }
  else if (node->type == FUNCTION) {
    mpc_ast_delete(node->data.function->contents);
    free(node->data.function);
  }

  if (freeself)
    free(node);
}

void free_alma_tree(alma_node *node) {
  free_node(node, 0);
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
