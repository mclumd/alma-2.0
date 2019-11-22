#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "alma_command.h"
#include "alma_kb.h"

#define LINELEN 1000

// Initialize global variable (declared in alma_formula header) to count up variable IDs
long long variable_id_count = 0;

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
      fflush(stdout);

      if (fgets(line, LINELEN, stdin) != NULL) {
        int len = strlen(line);
        line[len-1] = '\0';
        //printf("Command '%s' received\n", line);

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
        else if ((pos = strstr(line, "del ")) != NULL && pos == line) {
          char *assertion = malloc(len - 4);
          strncpy(assertion, line+4, len-4);
          kb_remove(alma_kb, assertion);
          free(assertion);
        }
        else if ((pos = strstr(line, "obs ")) != NULL && pos == line) {
          char *assertion = malloc(len - 4);
          strncpy(assertion, line+4, len-4);
          kb_observe(alma_kb, assertion);
          free(assertion);
        }
        else if ((pos = strstr(line, "bs ")) != NULL && pos == line) {
          char *assertion = malloc(len - 3);
          strncpy(assertion, line+3, len-3);
          kb_backsearch(alma_kb, assertion);
          free(assertion);
        }
        else {
          printf("Command '%s' not recognized\n", line);
        }
      }
    }
  }

  return 0;
}
