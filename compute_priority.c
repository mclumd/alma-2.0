#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <float.h>

#include "compute_priority.h"
#include "alma_kb.h"
#include "alma_term_search.h"


//#include "priority_function.h"


double compute_priority(kb *knowledge_base, res_task *t) {
    /* The priority of a funciton will be the minimum of the priorities of any associated term in the task. */
  char *subject;
  double subject_priority;
  double priority = DBL_MAX;


  /* For each subject, go through the positive and negative literals, and look for any terms that match */
  for (int subj_idx = 0; subj_idx < knowledge_base->num_subjects; subj_idx++) {
      subject = tommy_array_get(knowledge_base->subject_list, subj_idx);
      if ( (knowledge_base->subject_priorities[subj_idx] < priority) &&
           ( alma_function_search(t->pos, subject) || alma_function_search(t->neg, subject)) ) {
          priority = knowledge_base->subject_priorities[subj_idx];
        }
    }
  return priority;
}

double old_compute_priority(kb *knowledge_base, res_task *t) {
      // Set the priority.  For now, let's do this in a binary way:  if one of the terms in either c_lit or other_lit
      // is "location", "canDo" or "empty" then prioirty is 0; otherwise it will be 1.


  
      double priority = 1;
      for (int i=0; i < t->pos->term_count; i++) {
	if ( (strncmp(t->pos->terms[i].variable->name, "cabinet", 7) == 0) && (i < t->pos->term_count-1) &&
	     (strcmp(t->pos->terms[i+1].variable->name, "location") == 0) ) {
	  priority = 0; break;
	}
	if (strcmp(t->pos->terms[i].variable->name, "canDo") == 0) {
	  priority = 0; break;
	}
	if (strcmp(t->pos->terms[i].variable->name, "empty") == 0) {
	  priority = 0; break;
	}
	if (strcmp(t->pos->terms[i].variable->name, "action") == 0) {
	  priority = 0; break;
	}
	if (strcmp(t->pos->terms[i].variable->name, "doing") == 0) {
	  priority = 0; break;
	}
	if (strcmp(t->pos->terms[i].variable->name, "done") == 0) {
	  priority = 0; break;
	}
      }
      return priority;
}


// Declaration of C++ function
double rl_priority(kb *knowledge_base, res_task *t);


int is_number(char *s) {
  if (*s == 0) return 1;
  else return (isdigit(*s) && is_number(++s));
}

/* Most basic priority possible.  Assigns high priority to resolutions
   involving *instantiated* sentences Now(t) where t is a constant.
   These must be resolved immediately because they are immediately deleted from the knowledge base.  */  

double base_priority(kb *knowledge_base, res_task *t) {
  //double priority = 1.0;
  // int idx;
  if ( (strncmp(t->pos->name, "now", 3) == 0 ) && is_number(t->pos->terms[0].variable->name))
    return 0.0;
  else
    //return 1.0;
    return (double) rand() / (double) RAND_MAX;
}


double zero_priority(kb *knowledge_base, res_task *t) {
    //double j = rl_priority(knowledge_base, t);
    return (double) 0;
}
