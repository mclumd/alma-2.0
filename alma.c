#include <stdio.h>
#include <string.h>
#include "alma_kb.h"

#define LINELEN 1000

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

  int run = 0;
  if (argc > 2 && strcmp(argv[2], "run") == 0)
    run = 1;

  kb *alma_kb;
  kb_init(&alma_kb, argv[1]);

  kb_print(alma_kb);

  if (run) {
    while (!alma_kb->idling) {
      kb_step(alma_kb);
      kb_print(alma_kb);
    }
    kb_halt(alma_kb);
  }
  else {
    char line[LINELEN];

    while (1) {
      printf("alma: ");

      if (fgets(line, LINELEN, stdin) != NULL) {
        int len = strlen(line);
        line[len-1] = '\0';

        char *pos;
        if (strcmp(line, "step") == 0) {
          kb_step(alma_kb);
        }
        else if (strcmp(line, "print") == 0) {
          kb_print(alma_kb);
        }
        else if (strcmp(line, "halt") == 0) {
          kb_halt(alma_kb);
          break;
        }
        else if ((pos = strstr(line, "add ")) != NULL && pos == line) {
          char *assertion = malloc(len - 4);
          strncpy(assertion, line+4, len-4);
          kb_assert(alma_kb, assertion);
          free(assertion);
        }
      }
    }
  }

  return 0;
}
