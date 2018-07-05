#include "mpc/mpc.h"

// Attempts to open filename and parse contents according to ALMA language
// Boolean return for success or failure of parse
// If the parse was successful, AST is a valid pointer to the result
int alma_parse(char *filename, mpc_ast_t **ast) {
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

  mpc_result_t r;
  int result = mpc_parse_contents(filename, Alma, &r);
  mpc_cleanup(16, Alma, Almaformula, Formula, FFormula, BFormula, Conjform,
    Literal, Neglit, Poslit, Listofterms, Term, Predname, Constant,
    Funcname, Variable, Prologconst);

  if (result) {
    *ast = r.output;
    return 1;
  }
  else {
    mpc_err_print(r.error);
    mpc_err_delete(r.error);
    return 0;
  }
}
