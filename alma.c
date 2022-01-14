#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include "alma.h"
#include "alma_command.h"
#include "alma_print.h"

#define LINELEN 1000

extern char logs_on;
extern char python_mode;

// Initialize global variable (declared in alma_formula header) to count up variable IDs

int main(int argc, char **argv) {
  int run = 0;
  int verbose = 0;
  int file_count = 0;
  char **files = NULL;
  char *agent = NULL;
  int max_depth = 1000;

  logs_on = 1;
  python_mode = 0;

  int c;
  while ((c = getopt(argc, argv, "rxvf:a:n:")) != -1)
    switch (c) {
      case 'r':
        run = 1;
        break;
      case 'v':
        verbose = 1;
        break;
      case 'f':
        file_count++;
        files = realloc(files, sizeof(*files) * file_count);
        files[file_count-1] = optarg;
        break;
      case 'x':
        logs_on = (char) 0;
        break;
      case 'a':
        agent = optarg;
        break;
      case 'n':
        max_depth = atoi(optarg);
        break;
      case '?':
        if (optopt == 'f')
          printf("Option -%c requires an ALMA file argument.\n", optopt);
        else if (optopt == 'a')
          printf("Option -%c requires an agent name argument.\n", optopt);
        else if (optopt == 'n')
          printf("Option -%c requires an integer max nesting dapth.\n", optopt);
        else if (isprint (optopt))
          printf("Unknown option `-%c'.\n", optopt);
        else
          printf("Unknown option character `\\x%x'.\n", optopt);
        return 1;
      default:
        return 0;
    }

  if (files == NULL) {
    printf("Please run with at least one ALMA file argument using the \"-f\" option.\n");
    return 0;
  }

  alma *reasoner = malloc(sizeof(*reasoner));
  alma_init(reasoner, files, file_count, agent, NULL, NULL, verbose, NULL, logs_on, max_depth);
  free(files);
  alma_print(reasoner, NULL);

  if (run) {
    while (!reasoner->idling) {
      alma_step(reasoner, NULL);
      alma_print(reasoner, NULL);
    }
    alma_halt(reasoner);
  }
  else {
    char line[LINELEN];

    int counter = 0;

    kb_logger logger;
    logger.log = reasoner->almalog;
    logger.buf = NULL;

    while (1) {
      tee_alt("alma: ", &logger, counter);
      fflush(stdout);
      if (fgets(line, LINELEN, stdin) != NULL) {
        int len = strlen(line);
        line[len-1] = '\0';

        char *pos;
        if (strcmp(line, "step") == 0) {
          alma_step(reasoner, NULL);
        }
        else if (strcmp(line, "print") == 0) {
          alma_print(reasoner, NULL);
          fflush(stdout);
        }
        else if (strcmp(line, "halt") == 0) {
          alma_halt(reasoner);
          break;
        }
        else if ((pos = strstr(line, "add ")) != NULL && pos == line) {
          char *assertion = malloc(len - 4);
          strncpy(assertion, line+4, len-4);
          alma_assert(reasoner, assertion, NULL);
          free(assertion);
        }
        else if ((pos = strstr(line, "del ")) != NULL && pos == line) {
          char *assertion = malloc(len - 4);
          strncpy(assertion, line+4, len-4);
          alma_remove(reasoner, assertion, NULL);
          free(assertion);
        }
        else if ((pos = strstr(line, "update ")) != NULL && pos == line) {
          char *assertion = malloc(len - 7);
          strncpy(assertion, line+7, len-7);
          alma_update(reasoner, assertion, NULL);
          free(assertion);
        }
        else if ((pos = strstr(line, "obs ")) != NULL && pos == line) {
          char *assertion = malloc(len - 4);
          strncpy(assertion, line+4, len-4);
          alma_observe(reasoner, assertion, NULL);
          free(assertion);
        }
        else if ((pos = strstr(line, "bs ")) != NULL && pos == line) {
          char *assertion = malloc(len - 3);
          strncpy(assertion, line+3, len-3);
          alma_backsearch(reasoner, assertion, NULL);
          free(assertion);
        }
        else {
          tee_alt("-a: Command '%s' not recognized\n", &logger, line);
        }
      } else {
          tee_alt("ALMA no more input\n", &logger);
          fflush(stdout);
      }
      counter++;
    }
  }

  return 0;
}
