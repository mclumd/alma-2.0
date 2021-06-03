 #ifndef RES_TASK_HEAP_H
 #define RES_TASK_HEAP_H
 #include "resolution.h"
 #include "tommy.h"
typedef struct res_task_heap
{
  tommy_array data;
  size_t count;
  tommy_size_t max_size;
} res_task_heap;
 void res_task_heap_init(struct res_task_heap *heap, int size);
 res_task_pri *res_task_heap_item(struct res_task_heap *heap, int idx);
 int res_task_heap_push(struct res_task_heap *heap, res_task_pri *value);
 res_task_pri *res_task_heap_pop(struct res_task_heap *heap);
 void res_task_heap_emerge(struct res_task_heap *heap, size_t index);
 void res_task_heap_heapify(struct res_task_heap *heap);
 int res_task_heap_delete(struct res_task_heap *heap, res_task_pri *value);
 int res_task_heap_clausal_delete(struct res_task_heap *heap, clause *c);
 void res_task_heap_destroy(struct res_task_heap *heap);
 #endif
