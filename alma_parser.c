#include "alma_parser.h"
#include <pthread.h>


mpc_parser_t* Alma;
mpc_parser_t* Almacomment;
mpc_parser_t* Almaformula;
mpc_parser_t* Sentence;
mpc_parser_t* Formula;
mpc_parser_t* FFormula;
mpc_parser_t* FFormConc;
mpc_parser_t* BFormula;
mpc_parser_t* Conjform;
mpc_parser_t* Literal;
mpc_parser_t* Listofterms;
mpc_parser_t* Term;
mpc_parser_t* Predname;
mpc_parser_t* Constant;
mpc_parser_t* Funcname;
mpc_parser_t* Variable;
mpc_parser_t* Prologconst;

pthread_mutex_t count_mutex;
int parser_ref_count = 0;

void parse_init(void) {
  pthread_mutex_lock(&count_mutex);
  parser_ref_count++;
  pthread_mutex_unlock(&count_mutex);

  Alma = mpc_new("alma");
  Almacomment = mpc_new("almacomment");
  Almaformula = mpc_new("almaformula");
  Sentence = mpc_new("sentence");
  Formula = mpc_new("formula");
  FFormula = mpc_new("fformula");
  FFormConc = mpc_new("fformconc");
  BFormula = mpc_new("bformula");
  Conjform = mpc_new("conjform");
  Literal = mpc_new("literal");
  Listofterms = mpc_new("listofterms");
  Term = mpc_new("term");
  Predname = mpc_new("predname");
  Constant = mpc_new("constant");
  Funcname = mpc_new("funcname");
  Variable = mpc_new("variable");
  Prologconst = mpc_new("prologconst");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                            "
    " alma         : /^/ (<almaformula> | <almacomment>)* /$/ ;  "
    " almacomment  : /%[^\n]*\n/ ;                               "
    " almaformula  : <sentence> '.' ;                            "
    " sentence     : <fformula> | <bformula> | <formula> ;       "
    " formula      : \"and(\" <formula> ',' <formula> ')'        "
    "              | \"or(\" <formula> ','  <formula> ')'        "
    "              | \"if(\" <formula> ',' <formula> ')'         "
    "              | \"not(\" <formula> ')'                      "
    "              | <literal> ;                                 "
    " fformula     : \"fif(\" <conjform> ',' <fformconc> ')' ;   "
    " fformconc    : \"and(\" <fformconc> ',' <fformconc> ')'    "
    "              | <fformula> | \"not(\" <literal> ')'         "
    "              | <literal> ;                                 "
    " bformula     : \"bif(\" <formula> ',' <formula> ')' ;      "
    " conjform     : \"and(\" <conjform> ',' <conjform> ')'      "
    "              | \"not(\" <literal> ')'                      "
    "              | <literal> ;                                 "
    " literal      : <predname> '(' <listofterms> ')'            "
    "              | <predname> ;                                "
    " listofterms  : <term> (',' <term>)* ;                      "
    " term         : \"quote\" '(' <sentence> ')'                "
    "              | <funcname> '(' <listofterms> ')'            "
    "              | <variable> | <constant> ;                   "
    " predname     : <prologconst> ;                             "
    " constant     : <prologconst> ;                             "
    " funcname     : <prologconst> ;                             "
    " variable     : '`' <variable> | /[A-Z][a-zA-Z0-9_]*/;      "
    " prologconst  : /[a-z0-9_][a-zA-Z0-9_]*/ ;                  ",
    Alma, Almacomment, Almaformula, Sentence, Formula, FFormula,
    FFormConc, BFormula, Conjform, Literal, Listofterms, Term,
    Predname, Constant, Funcname, Variable, Prologconst,
    NULL);
}

// Attempts to open filename and parse contents according to ALMA language
// Boolean return for success or failure of parse
// If the parse was successful, AST is a valid pointer to the result
int parse_file(char *filename, mpc_ast_t **ast) {
  mpc_result_t r;
  int result = mpc_parse_contents(filename, Alma, &r);

  if (result) {
    *ast = r.output;
    //mpc_ast_print(*ast);
    //exit(0);
    return 1;
  }
  else {
    mpc_err_print(r.error);
    mpc_err_delete(r.error);
    return 0;
  }
}

int parse_string(char *string, mpc_ast_t **ast) {
  mpc_result_t r;
  int result = mpc_parse("stringname", string, Alma, &r);

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

void parse_cleanup(void) {
  pthread_mutex_lock(&count_mutex);
  parser_ref_count--;
  pthread_mutex_unlock(&count_mutex);

  if (parser_ref_count == 0) {
    mpc_cleanup(17, Alma, Almacomment, Almaformula, Sentence, Formula,
      FFormula, FFormConc, BFormula, Conjform, Literal, Listofterms,
      Term, Predname, Constant, Funcname, Variable, Prologconst);
  }
}
