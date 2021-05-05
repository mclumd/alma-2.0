#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include "alma_command.h"
#include "alma_kb.h"
#include "alma_print.h"

#define LINELEN 1000

extern char logs_on;
extern char python_mode;

// Initialize global variable (declared in alma_formula header) to count up variable IDs
//long long variable_id_count = 0;

int main(int argc, char **argv) {
  int run = 0;
  int verbose = 0;
  int file_count = 0;
  char **files = NULL;
  char *agent = NULL;

  logs_on = (char) 1;
  python_mode = (char) 0;

  int c;
  while ((c = getopt(argc, argv, "rxvf:a:")) != -1)
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
      case '?':
        if (optopt == 'f')
          printf("Option -%c requires an ALMA file argument.\n", optopt);
        else if (optopt == 'a')
          printf("Option -%c requires an agent name argument.\n", optopt);
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

  kb *alma_kb;
  kb_init(&alma_kb, files, file_count, agent, NULL, NULL, verbose, NULL, logs_on);
  free(files);
  kb_print(alma_kb, NULL);


  if (run) {
    while (!alma_kb->idling) {
      kb_step(alma_kb, NULL);
      kb_print(alma_kb, NULL);
    }
    kb_halt(alma_kb);
  }
  else {
    char line[LINELEN];

    int counter = 0;

    while (1) {
      tee_alt("alma: ",alma_kb,NULL,counter);
      //      tee_alt("about to fgets...\n",NULL);
      fflush(stdout);
      if (fgets(line, LINELEN, stdin) != NULL) {
        int len = strlen(line);
        line[len-1] = '\0';
	//        tee_alt("Command '%s' received at %d.\n", NULL, line, counter);
	//	tee_alt(line);

        char *pos;
        if (strcmp(line, "step") == 0) {
	  //	  tee_alt("ALMA %d step:\n",NULL, counter);
          kb_step(alma_kb, NULL);
        }
        else if (strcmp(line, "print") == 0) {
	  //	  tee_alt("ALMA %d print:\n",NULL,counter);
          kb_print(alma_kb, NULL);
	  fflush(stdout);
        }
        else if (strcmp(line, "halt") == 0) {
	  //	  tee_alt("ALMA %d halt:\n",NULL,counter);
          kb_halt(alma_kb);
          break;
        }
        else if ((pos = strstr(line, "add ")) != NULL && pos == line) {
	  //	  tee_alt("ALMA %d add:\n",NULL,counter);
          char *assertion = malloc(len - 4);
          strncpy(assertion, line+4, len-4);
          kb_assert(alma_kb, assertion, NULL);
          free(assertion);
        }
        else if ((pos = strstr(line, "del ")) != NULL && pos == line) {
	  //	  tee_alt("ALMA %d del:\n",NULL,counter);
          char *assertion = malloc(len - 4);
          strncpy(assertion, line+4, len-4);
          kb_remove(alma_kb, assertion, NULL);
          free(assertion);
        }
        else if ((pos = strstr(line, "update ")) != NULL && pos == line) {
	  //	  tee_alt("ALMA %d update:\n",NULL,counter);
          char *assertion = malloc(len - 7);
          strncpy(assertion, line+7, len-7);
          kb_update(alma_kb, assertion, NULL);
          free(assertion);
        }
        else if ((pos = strstr(line, "obs ")) != NULL && pos == line) {
	  //	  tee_alt("ALMA %d obs:\n",NULL,counter);
          char *assertion = malloc(len - 4);
          strncpy(assertion, line+4, len-4);
          kb_observe(alma_kb, assertion, NULL);
          free(assertion);
        }
        else if ((pos = strstr(line, "bs ")) != NULL && pos == line) {
	  //	  tee_alt("ALMA %d bs:\n",NULL,counter);
          char *assertion = malloc(len - 3);
          strncpy(assertion, line+3, len-3);
          kb_backsearch(alma_kb, assertion, NULL);
          free(assertion);
        }
        else {
          tee_alt("-a: Command '%s' not recognized\n", alma_kb, NULL, line);
        }
      } else {
	tee_alt("ALMA no more input\n", alma_kb, NULL);
	fflush(stdout);
      }
      counter++;
    }
  }

  return 0;
}
