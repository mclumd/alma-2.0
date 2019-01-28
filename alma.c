#include <stdio.h>
#include "alma_parser.h"
#include "alma_formula.h"
#include "alma_kb.h"
#include "alma_unify.h"

// Initialize global variable (declared in alma_formula header) to count up vairable IDs
long long variable_id_count = 0;

// ALMA currently:
// 1. parses an input file into an MPC AST
// 2. obtains a FOL representation from the AST
// 3. converts this general FOL into CNF
// 4. converts CNF into a collection of clauses for the KB
// 5. constructs KB with list of clauses, hashmaps + linked lists indexing predicates
// 6. runs forward chaining, which loops through executing resolution tasks, adding to KB, obtaining new tasks
int main(int argc, char **argv) {
  if (argc <= 1) {
    printf("Please run with an input file argument.\n");
    return 0;
  }

  parse_init();

  alma_node *formulas;
  int formula_count;
  if (formulas_from_source(argv[1], 1, &formula_count, &formulas)) {

    kb *alma_kb;
    kb_init(formulas, formula_count, &alma_kb);
    tommy_array now1;
    tommy_array_init(&now1);
    assert_formula("now(1).", &now1);
    add_clause(alma_kb, tommy_array_get(&now1, 0));
    tommy_array_done(&now1);
    kb_print(alma_kb);

    forward_chain(alma_kb);

    free_kb(alma_kb);

    parse_cleanup();
  }

  return 0;
}
