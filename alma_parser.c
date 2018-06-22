#include <stdio.h>
#include <stdlib.h>
#include "mpc/mpc.h"
#include "alma_formula.h"

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
    " listofterms  : <term> ',' <listofterms> | <term> ;         "
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
    for (int i = 0; i < formula_count; i++) {
      alma_print(formulas[i]);
      printf("Rewritten without conditionals:\n");
      eliminate_conditionals(formulas + i);
      alma_print(formulas[i]);
      printf("Rewritten with negation moved inwards:\n");
      negation_inwards(formulas + i);
      alma_print(formulas[i]);
      printf("\n");
    }

    mpc_ast_delete_selective(r.output);
    // Must free these AFTER delete_selective, as that checks
    for (int i = 0; i < formula_count; i++) {
      free_alma_tree(formulas + i);
    }
    free(formulas);
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
