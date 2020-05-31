#ifndef RESOLUTION_H
#define RESOLUTION_H
#include "alma_formula.h"
#include "tommy.h"
#include "clause.h"


typedef struct res_task {
  clause *x;
  clause *y;
  alma_function *pos; // Positive literal from x
  alma_function *neg; // Negative literal from y
} res_task;

typedef struct res_task_pri {
    res_task *res_task;
    double priority;
} res_task_pri;


#endif
