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

// Initialize global variable (declared in alma_formula header) to count up variable IDs
long long variable_id_count = 0;

FILE *almalog = NULL;

void print_usage() {
  fprintf(stderr, "Usage: alma.x [options]\n"
	  "Options:\n"
	  "  -P                  Assign different priorities for resolution of tasks with astep (default is False)\n"
	  "  -R <sfile>          Track resolution steps, using <sfile> for subject names\n"
	  "  -M <ofile>          Output tracked resolution matrix to <ofile>; assumes -R\n"
	  "  -T <horizon>          Track resolution steps, timestep horizon; assumes -R\n"
	  "  -H <size>           Max size for resolution heap (default 10000)"
	  "  -r                  Run continuosly (default is False)\n"
	  "  -v                  Verbose mode (default is False)\n"
	  "  -f <filename>       Initial input file (required)\n"
	  "  -a <name>           Agent name\n"
	  "  -h                  Print this help message\n\n");
  return;
}

void parse_subjects_file(tommy_array *subj_list, int *num_subjects, char *subjects_file) {
  FILE *sf;
  sf = fopen(subjects_file, "r");
  if (sf == NULL) {
    fprintf(stderr, "Couldn't open subjects file %s\n", subjects_file);
    exit(1);
  }
  char subj_line[1024];
  int subj_len;
  char *subj_copy;
  tommy_array_init(subj_list);
  *num_subjects = 0;
  while (fgets(subj_line, 1024, sf) != NULL) {
    subj_len = strlen(subj_line) + 1; // Add one for terminating \0
    subj_copy = malloc(subj_len * sizeof(char));
    strncpy(subj_copy, subj_line, subj_len);
    subj_copy[subj_len-2] = '\0';
    tommy_array_insert(subj_list, subj_copy);
    (*num_subjects)++;
  }
  fclose(sf);
}

void write_resolution_matrix(kb *alma_kb, int num_subjects, int resolutions_horizon, char *filename) {
  FILE *fp;
  fp = fopen(filename, "wt");
  if (fp == NULL) {
    fprintf(stderr, "Couldn't open file %s for writing.\n", filename);
    exit(1);
  }
  /* For now, just write a text file. */
  for(int subj = 0; subj < num_subjects; subj++) {
    for(int t=0; t < resolutions_horizon; t++) {
      fprintf(fp, "%d ", alma_kb->resolution_choices[subj][t]);
    }
    fprintf(fp, "\n");
  }
  fclose(fp);
}
  
int main(int argc, char **argv) {
  int run = 0;
  int verbose = 0;
  int differential_priorities = 0;
  int tracking_resolutions = 0;
  int resolutions_horizon = -1;
  int num_subjects;
  int res_heap_size = 10000;
  tommy_array subjects_list;
  char *file = NULL;
  char *agent = NULL;
  char *subjects_file = NULL;
  char *resolutions_file = NULL;

  //int index;
  int c;
  while ((c = getopt (argc, argv, "PH:rvf:a:hR:M:T:")) != -1)
    switch (c) {
    case 'P':
      differential_priorities = 1;
      break;
    case 'H':
      res_heap_size = atoi(optarg);
      break;
    case 'h':
      print_usage();
      break;
    case 'r':
      run = 1;
      break;
    case 'v':
      verbose = 1;
      break;
    case 'f':
      file = optarg;
      break;
    case 'a':
      agent = optarg;
      break;
    case 'R':
      tracking_resolutions = 1;
      subjects_file = optarg;
      break;
    case 'M':
      resolutions_file = optarg;
      break;
    case 'T':
      resolutions_horizon = atoi(optarg);
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
    printf("Please run with an ALMA file argument.\n\n");
    print_usage();
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

  almalog = fopen(logname, "w");
  free(logname);

  kb *alma_kb;
  kb_init(&alma_kb, file, agent, verbose, differential_priorities, res_heap_size);
  kb_print(alma_kb);

  if (tracking_resolutions) {
    alma_kb->tracking_resolutions = 1;
    parse_subjects_file(alma_kb->subject_list, &(alma_kb->num_subjects), subjects_file);
    init_resolution_choices(&(alma_kb->resolution_choices), alma_kb->num_subjects, resolutions_horizon);
  }
  
  if (run) {
    while (!alma_kb->idling) {
      kb_step(alma_kb, 0);
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
          kb_step(alma_kb, 0);
        } else if (strcmp(line, "astep") == 0) {
	        kb_step(alma_kb, 1);
	    } else if (strcmp(line, "print") == 0) {
          kb_print(alma_kb);
        }
        else if (strcmp(line, "halt") == 0) {
            if (tracking_resolutions) {
                write_resolution_matrix(alma_kb, alma_kb->num_subjects, resolutions_horizon, resolutions_file);
            }
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


