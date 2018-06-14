#include <stdio.h>
#include <stdlib.h>
#include "mpc/mpc.h"

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

  /*
  // Given file, parse assuming a single full ALMA formula per line
  if (argc > 1) {
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(argv[1], "r");
    if (fp == NULL)
      exit(EXIT_FAILURE);

    while ((read = getline(&line, &len, fp)) != -1) {
      int parse = 0;
      for (int i = 0; i < read; i++) {
        if (isalnum(line[i])) {
          parse = 1;
          break;
        }
      }

      if (parse) {
        if (mpc_parse(argv[1], line, Alma, &r)) {
          mpc_ast_print(r.output);
          mpc_ast_delete(r.output);
        }
        else {
          mpc_err_print(r.error);
          mpc_err_delete(r.error);
        }
      }
    }

    fclose(fp);
    if (line)
      free(line);
  }
  else {
    if (mpc_parse_pipe("<stdin>", stdin, Alma, &r)) {
      mpc_ast_print(r.output);
      mpc_ast_delete(r.output);
    }
    else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
  }*/

  int result;
  if (argc > 1)
    result = mpc_parse_contents(argv[1], Alma, &r);
  else
    result = mpc_parse_pipe("<stdin>", stdin, Alma, &r);
  if (result) {
    mpc_ast_print(r.output);
    mpc_ast_delete(r.output);
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
