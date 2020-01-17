#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "alma_command.h"
#include "alma_kb.h"
#include "alma_print.h"

#define LINELEN 1000

// Initialize global variable (declared in alma_formula header) to count up variable IDs
long long variable_id_count = 0;

FILE *almalog = NULL;

int main(int argc, char **argv) {
  if (argc <= 1) {
    tee("Please run with an input file argument.\n");
    return 0;
  }

  int run = 0;
  if (argc > 2 && strcmp(argv[2], "run") == 0)
    run = 1;

  time_t rawtime;
  time(&rawtime);
  char *time = ctime(&rawtime);
  int timelen = strlen(time);
  for (int i = 0; i < timelen; i++)
    if (time[i] == ' ')
      time[i] = '-';
  char *logname = malloc(5 + timelen + 9);
  strcpy(logname, "alma-");
  strcpy(logname+5, time);
  strcpy(logname+5+timelen, "-log.txt");

  almalog = fopen(logname,"w");
  free(logname);

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
      tee("alma: ");
      fflush(stdout);

      if (fgets(line, LINELEN, stdin) != NULL) {
        int len = strlen(line);
        line[len-1] = '\0';
        //tee("Command '%s' received\n", line);

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
        else if ((pos = strstr(line, "update ")) != NULL && pos == line) {
          char *assertion = malloc(len - 7);
          strncpy(assertion, line+7, len-7);
          kb_update(alma_kb, assertion);
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
          tee("Command '%s' not recognized\n", line);
        }
      }
    }
  }

  return 0;
}
