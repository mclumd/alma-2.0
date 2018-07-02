#include <stdio.h>
#include <stdlib.h>
#include "mpc/mpc.h"
#include "alma_formula.h"
#include "alma_unify.h"

int main(int argc, char **argv) {

  mpc_result_t r;

  mpc_parser_t* Alma  = mpc_new("alma");
  mpc_parser_t* Almaformula  = mpc_new("almaformula");
  mpc_parser_t* Formula = mpc_new("formula");
  mpc_parser_t* FFormula = mpc_new("fformula");
  mpc_parser_t* BFormula = mpc_new("bformula");
  mpc_parser_t* Conjform = mpc_new("conjform");
  mpc_parser_t* Literal = mpc_new("literal");
  mpc_parser_t* Neglit = mpc_new("neglit");
  mpc_parser_t* Poslit = mpc_new("poslit");
  mpc_parser_t* Listofterms = mpc_new("listofterms");
  mpc_parser_t* Term = mpc_new("term");
  mpc_parser_t* Predname = mpc_new("predname");
  mpc_parser_t* Constant = mpc_new("constant");
  mpc_parser_t* Funcname = mpc_new("funcname");
  mpc_parser_t* Variable = mpc_new("variable");
  mpc_parser_t* Prologconst = mpc_new("prologconst");

  // TODO: Add to, allowing Prolog comments to be ignored, and other more general features
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                            "
    " alma         : /^/ <almaformula>* /$/ ;                    "
    " almaformula  : (<fformula> | <bformula> | <formula>) '.' ; "
    " formula      : \"and(\" <formula> ',' <formula> ')'        "
    "              | \"or(\" <formula> ','  <formula> ')'        "
    "              | \"if(\" <formula> ',' <formula> ')'         "
    "              | <literal> ;                                 "
    " fformula     : \"fif(\" <conjform> ',' \"conclusion(\"     "
    "                <poslit> ')' ')' ;                          "
    " bformula     : \"bif(\" <formula> ',' <formula> ')' ;      "
    " conjform     : \"and(\" <conjform> ',' <conjform> ')'      "
    "              | <literal> ;                                 "
    " literal      : <neglit> | <poslit> ;                       "
    " neglit       : \"not(\" <poslit> ')' ;                     "
    " poslit       : <predname> '(' <listofterms> ')'            "
    "              | <predname> ;                                "
    " listofterms  : <term> (',' <term>)* ;                      "
    " term         : <funcname> '(' <listofterms> ')'            "
    "              | <variable> | <constant> ;                   "
    " predname     : <prologconst> ;                             "
    " constant     : <prologconst> ;                             "
    " funcname     : <prologconst> ;                             "
    " variable     : /[A-Z_][a-zA-Z0-9_]*/ ;                     "
    " prologconst  : /[a-z][a-zA-Z0-9_]*/ ;                      ",
    Alma, Almaformula, Formula, FFormula, BFormula, Conjform, Literal, Neglit,
    Poslit, Listofterms, Term, Predname, Constant, Funcname, Variable,
    Prologconst, NULL);

  int result;
  if (argc > 1)
    result = mpc_parse_contents(argv[1], Alma, &r);
  else
    result = mpc_parse_pipe("<stdin>", stdin, Alma, &r);
  if (result) {
    alma_node *formulas;
    int formula_count;
    generate_alma_trees(r.output, &formulas, &formula_count);
    mpc_ast_delete(r.output);

    for (int i = 0; i < formula_count; i++) {
      //alma_print(formulas+i);
      //printf("CNF equivalent:\n");
      make_cnf(formulas+i);
      //alma_print(formulas+i);
      //printf("\n");
    }

    kb *alma_kb;
    flatten(formulas, formula_count, &alma_kb);
    for (int i = 0; i < formula_count; i++)
      free_alma_tree(formulas+i);
    free(formulas);
    //kb_print(alma_kb);

    // Unify test
    binding_list *theta;
    theta = malloc(sizeof(binding_list));
    pred_unify(alma_kb->clauses[12]->pos_lits[0], alma_kb->clauses[13]->pos_lits[0], theta);
    print_bindings(theta);
    cleanup_bindings(theta);

    free_kb(alma_kb);
  }
  else {
    mpc_err_print(r.error);
    mpc_err_delete(r.error);
  }

  mpc_cleanup(16, Alma, Almaformula, Formula, FFormula, BFormula, Conjform,
    Literal, Neglit, Poslit, Listofterms, Term, Predname, Constant,
    Funcname, Variable, Prologconst);

  return 0;
}
