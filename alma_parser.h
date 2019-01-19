#ifndef alma_parser_h
#define alma_parser_h

#include "mpc/mpc.h"

void parse_init(void);
int parse_file(char *filename, mpc_ast_t **ast);
int parse_string(char *string, mpc_ast_t **ast);
void parse_cleanup(void);

#endif
