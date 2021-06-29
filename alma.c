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
long long variable_id_count = 0;

FILE *almalog = NULL;

void print_usage() {
  fprintf(stderr, "Usage: alma.x [options]\n"
	  "Options:\n"
	  "  -P                  Assign different priorities for resolution of tasks with astep (default is False)\n"
	  "  -S <sfile>          Assign different priorities for resolution of tasks with astep, using priorities specified in <sfile>\n"
	  "  -R <sfile>          Track resolution steps, using <sfile> for subject names\n"
	  "  -M <ofile>          Output tracked resolution matrix to <ofile>; assumes -R\n"
	  "  -T <horizon>          Track resolution steps, timestep horizon; assumes -R\n"
	  "  -H <size>           Max size for resolution heap (default 10000)"
	  "  -r                  Run continuosly (default is False)\n"
	  "  -v                  Verbose mode (default is False)\n"
	  "  -f <filename>       Initial input file (required)\n"
	  "  -a <name>           Agent name\n"
	  "  -x                  Supress log files\n"
	  "  -h                  Print this help message\n\n");
  return;
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
    //tee("subj %d ", subj);
    for(int t=0; t < resolutions_horizon; t++) {
      //tee("t %d ", t);
      fprintf(fp, "%d ", alma_kb->resolution_choices[subj][t]);
    }
    //tee("\n");
    fprintf(fp, "\n");
  }
  fclose(fp);
}

int main(int argc, char **argv) {
  int run = 0;
  int verbose = 0;
  int differential_priorities = 0;
  int res_heap_size = 10000;
  char *file = NULL;
  char *agent = NULL;

  logs_on = (char) 1;
  python_mode = (char) 0;
  
  //int index;
  rl_init_params rip;
  rip.tracking_resolutions = 0;
  rip.subjects_file = NULL;
  rip.resolutions_file = NULL;
  rip.rl_priority_file = NULL;

  // TODO:  switch these back; just for debugging
  rip.use_lists = 1;
  rip.use_res_pre_buffer = 1;


  int c;
  while ((c = getopt (argc, argv, "PH:rxvf:a:hR:M:T:S:")) != -1)
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
    case 'x':
      logs_on = (char) 0;
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
      rip.tracking_resolutions = 1;
      rip.subjects_file = optarg;
      break;
    case 'M':
      rip.tracking_resolutions = 1;
      rip.resolutions_file = optarg;
      break;
    case 'T':
      rip.tracking_resolutions = 1;
      rip.resolutions_horizon = atoi(optarg);
      break;
    case 'S':
      rip.tracking_resolutions = 1;
      rip.rl_priority_file = optarg;
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

  if (logs_on) {
    almalog = fopen(logname, "w");
  }
  free(logname);

  kb *alma_kb;
  /* TODO:  Check this for merge issues. */
  kb_init(&alma_kb, file, agent, NULL, NULL, verbose, differential_priorities, res_heap_size, rip, NULL, logs_on);
  kb_print(alma_kb,NULL);

  if (run) {
    while (!alma_kb->idling) {
      kb_step(alma_kb, 0, NULL);
      kb_print(alma_kb, NULL);
    }
    kb_halt(alma_kb);
  } else {
    char line[LINELEN];

    int counter = 0;

    while (1) {
      tee_alt("alma: ",alma_kb,NULL,counter);
      fflush(stdout);

      if (fgets(line, LINELEN, stdin) == NULL) {
	if (rip.tracking_resolutions==1) {
	  write_resolution_matrix(alma_kb, alma_kb->num_subjects, rip.resolutions_horizon, rip.resolutions_file);
	}
	kb_halt(alma_kb);
	break;
      } else {
	int len = strlen(line);
	line[len-1] = '\0';
	tee_alt("Command '%s' received at %d.\n", alma_kb, NULL, line, counter);

	char *pos;
	if (strcmp(line, "step") == 0) {
	  tee_alt("ALMA %d step:\n",alma_kb, NULL, counter);
	  pre_res_buffer_to_heap(alma_kb, 0);
	  kb_step(alma_kb, 0, NULL);
	  //pre_res_buffer_to_heap(alma_kb, 0);
	} else if (strcmp(line, "astep") == 0) {
	  tee_alt("ALMA %d step:\n",alma_kb, NULL, counter);
	  kb_step(alma_kb, 1, NULL);
	} else if (strcmp(line, "print") == 0) {
	  kb_print(alma_kb, NULL);
	  fflush(stdout);
	} else  if (strcmp(line, "prb_print") == 0) {
	  kb_print_pre_res_buf(alma_kb, NULL);
	} else  if (strcmp(line, "prb_flush") == 0) {
	  pre_res_buffer_to_heap(alma_kb, 0);
	  tee_alt("Flushed pre resolution task buffer\n",alma_kb, NULL,counter);
	} else if (strcmp(line, "halt") == 0) {
	  if (rip.tracking_resolutions == 1) {
	    write_resolution_matrix(alma_kb, alma_kb->num_subjects, rip.resolutions_horizon, rip.resolutions_file);
	  }
	  kb_halt(alma_kb);
	  break;
	} else if ((pos = strstr(line, "add ")) != NULL && pos == line) {
	  tee_alt("ALMA %d add:\n",alma_kb, NULL,counter);
	  char *assertion = malloc(len - 4);
	  strncpy(assertion, line+4, len-4);
	  kb_assert(alma_kb, assertion, NULL);
	  free(assertion);
	} else if  ((pos = strstr(line, "load ")) != NULL && pos == line) {
	  char *filename = malloc(len - 5);
	  strncpy(filename, line+5, len-5);
	  if (!load_file(alma_kb, filename, NULL))
	    tee_alt("Error loading %s.\n", alma_kb, NULL, filename);
	  free(filename);
	} else if ((pos = strstr(line, "del ")) != NULL && pos == line) {
	  tee_alt("ALMA %d del:\n",alma_kb, NULL,counter);
	  char *assertion = malloc(len - 4);
	  strncpy(assertion, line+4, len-4);
	  kb_remove(alma_kb, assertion, NULL);
	  free(assertion);
	} else if ((pos = strstr(line, "update ")) != NULL && pos == line) {
	  tee_alt("ALMA %d update:\n",alma_kb, NULL,counter);
	  char *assertion = malloc(len - 7);
	  strncpy(assertion, line+7, len-7);
	  kb_update(alma_kb, assertion, NULL);
	  free(assertion);
	} else if ((pos = strstr(line, "obs ")) != NULL && pos == line) {
	    tee_alt("ALMA %d obs:\n",alma_kb, NULL,counter);
	    char *assertion = malloc(len - 4);
	    strncpy(assertion, line+4, len-4);
	    kb_observe(alma_kb, assertion, NULL);
	    free(assertion);
	} else if ((pos = strstr(line, "bs ")) != NULL && pos == line) {
	  tee_alt("ALMA %d bs:\n",alma_kb, NULL,counter);
	  char *assertion = malloc(len - 3);
	  strncpy(assertion, line+3, len-3);
	  kb_backsearch(alma_kb, assertion, NULL);
	  free(assertion);
	} else {
	  tee_alt("-a: Command '%s' not recognized\n", alma_kb, NULL, line);
	}
      }
      tee_alt("ALMA no more input\n", alma_kb, NULL);
      fflush(stdout);
    }
    counter++;
  }
  return 0;
}


