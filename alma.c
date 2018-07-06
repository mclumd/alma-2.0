#include <stdio.h>
#include "mpc/mpc.h"
#include "alma_parser.h"
#include "alma_formula.h"
#include "alma_unify.h"

// ALMA currently:
// 1. parses an input file into an mpc_ast_t
// 2. obtains a FOL representation from the AST
// 3. converts this general FOL into CNF
// 4. flattens CNF into a collection of clauses for the KB
// 5. demos unification based on hardcoded pair from file
int main(int argc, char **argv) {
  if (argc <= 1) {
    printf("Please run with an input file argument.\n");
    return 0;
  }

  mpc_ast_t *alma_ast;
  if (alma_parse(argv[1], &alma_ast)) {

    // Obtain ALMA tree representations from MPC's AST
    alma_node *formulas;
    int formula_count;
    generate_alma_trees(alma_ast, &formulas, &formula_count);
    mpc_ast_delete(alma_ast);
    for (int i = 0; i < formula_count; i++) {
      alma_print(formulas+i);
    }
    printf("\n");

    // Convert general FOL formulas to CNF
    for (int i = 0; i < formula_count; i++) {
      make_cnf(formulas+i);
    }

    printf("CNF equivalents:\n");
    for (int i = 0; i < formula_count; i++) {
      alma_print(formulas+i);
    }
    printf("\n");

    // Flatten CNF list into KB of clauses
    kb *alma_kb;
    flatten(formulas, formula_count, &alma_kb);
    for (int i = 0; i < formula_count; i++)
      free_alma_tree(formulas+i);
    free(formulas);
    kb_print(alma_kb);

    // Unify test for pair from KB; currently harcoded indices for pair from formulae.pl
    binding_list *theta;
    theta = malloc(sizeof(binding_list));
    if(pred_unify(alma_kb->clauses[12]->pos_lits[0], alma_kb->clauses[13]->pos_lits[0], theta))
      print_bindings(theta);
    else
      printf("Unification failed :(\n");
    cleanup_bindings(theta);
    free(theta);

    free_kb(alma_kb);
  }

  return 0;
}
