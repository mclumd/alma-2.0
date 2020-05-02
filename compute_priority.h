#ifndef COMPUTE_PRIORITY_H
#define COMPUTE_PRIORITY_H

#include "alma_kb.h"
#include "resolution.h"

double compute_priority(kb *knowledge_base, res_task *t);
double base_priority(kb *knowledge_base, res_task *t);
double zero_priority(kb *knowledge_base, res_task *t);


#endif
