#ifndef alma_parser_h
#define alma_parser_h

#include "mpc/mpc.h"

void parse_init(void);
int alma_parse(char *filename, mpc_ast_t **ast);
void parse_cleanup(void);

#endif
