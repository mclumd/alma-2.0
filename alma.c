#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include "alma_command.h"
#include "alma_kb.h"
#include "alma_print.h"

#define LINELEN 1000

extern char logs_on;

// Initialize global variable (declared in alma_formula header) to count up variable IDs
long long variable_id_count = 0;

FILE *almalog = NULL;

int main(int argc, char **argv) {
  int run = 0;
  int verbose = 0;
  char *file = NULL;
  char *agent = NULL;
  logs_on = (char) 1;
  
  //int index;
  int c;
  while ((c = getopt (argc, argv, "rxvf:a:")) != -1)
    switch (c) {
      case 'r':
        run = 1;
        break;
      case 'v':
        verbose = 1;
        break;
      case 'f':
        file = optarg;
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

  if (file == NULL) {
    printf("Please run with an ALMA file argument using the \"-f\" option.\n");
    return 0;
  }

  time_t rawtime;
  time(&rawtime);
  char *time = ctime(&rawtime);
  int timelen = strlen(time)-1;
  for (int i = 0; i < timelen; i++)
    if (time[i] == ' ' || time[i] == ':')
      time[i] = '-';
  int agentlen = agent != NULL ? strlen(agent) : 0;
  char *logname = malloc(4 + agentlen + 1 + timelen + 9);
  strcpy(logname, "alma");
  if (agent != NULL)
    strcpy(logname+4, agent);
  logname[4+agentlen] = '-';
  strncpy(logname+5+agentlen, time, 24);
  strcpy(logname+5+agentlen+timelen, "-log.txt");

  if (logs_on) {
    almalog = fopen(logname, "w");
  }
  free(logname);

  kb *alma_kb;
  kb_init(&alma_kb, file, agent, verbose);
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
          tee("-a: Command '%s' not recognized\n", line);
        }
      }
    }
  }

  return 0;
}
