#ifndef alma_formula_h
#define alma_formula_h

#include "mpc/mpc.h"

typedef enum node_type { FOL, FUNCTION }
typedef enum alma_operator {NOT, OR, AND, IF} alma_operator;
typedef enum if_tag {FIF, BIF} if_tag;

typedef union alma_node {
  alma_fol *fol;
  alma_function *function;
} alma_node;

typedef struct alma_fol {
  alma_operator op; // Which arguments are used is implicit in operator
  node_type arg1_t;
  node_type arg2_t;
  alma_node *arg1; // Holds antecedent when op is IF
  alma_node *arg2; // Holds consequent when op is IF
  if_tag *tag;
} alma_fol;

// Temporary way of storing functions in ALMA sentences
// Eventually will change to format more removed from mpc_ast format
typedef struct alma_function {
  mpc_ast_t *contents;
} alma_function;

void alma_function_init(alma_function f, mpc_ast_t *ast);
void alma_fol_init(alma_fol fol, alma_operator op, node_type arg1_t, node_type arg2_t, alma_node *arg1, alma_node *arg2, if_tag *tag);

void generate_alma_trees(mpc_ast_t *tree, alma_node *alma_trees, size_t size);
void single_alma_tree(mpc_ast_t *almaformula, alma_node *alma_tree);

#endif
