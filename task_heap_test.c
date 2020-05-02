#include "res_task_heap.h"



int main(int argc, char *argv[]) {
  struct res_task_heap pq;
  res_task faux_task;
  res_task_pri tasks[10];
  
  faux_task.x = NULL;
  faux_task.y = NULL;
  faux_task.pos = NULL; // Positive literal from x
  faux_task.neg = NULL; // Negative literal from y

  for (int i=0; i < 10; i++) {
    tasks[i].res_task = &faux_task;
    tasks[i].priority = i;
  }

  const int perm[10] = {6, 3, 8, 0, 7, 4, 9, 5, 2, 1};
  res_task_pri *popped_task;
  res_task_heap_init(&pq, 100);
  for (int i=0; i < 10; i++)
      res_task_heap_push(&pq, &tasks[perm[i]]);
  for (int i=0; i < 10; i++) {
      popped_task = res_task_heap_pop(&pq);
      printf("Got prioirty %d\n", popped_task->priority);
  }
}

