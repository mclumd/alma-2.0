 #include <stdlib.h>
 #include "res_task_heap.h"
 #include "alma_kb.h"
 void res_task_heap_init(struct res_task_heap *heap, int size) {
  tommy_array_init(&heap->data);
  heap->max_size = 1 << TOMMY_ARRAY_BIT;
  heap->count = 0;
  tommy_array_grow(&heap->data, size);
}
 res_task_pri *res_task_heap_item(struct res_task_heap *heap, int idx) {
    return (res_task_pri *) tommy_array_get(&heap->data, idx);
}
 int res_task_heap_push(struct res_task_heap *heap, res_task_pri *value) {
  tommy_size_t index, parent;
  if (heap->count >= heap->max_size) {
    heap->max_size *= 2;
    tommy_array_grow(&heap->data, heap->max_size);
    heap->max_size = tommy_array_size(&heap->data);
  } else {
    for(index = heap->count++; index; index = parent) {
      parent = (index - 1) / 2;
      if ((( (* (res_task_pri *) tommy_array_get(&heap->data, parent)).priority) <= ( (*value).priority))) break;
      tommy_array_set(&heap->data, index, tommy_array_get(&heap->data, parent));
      ;
    }
    tommy_array_set(&heap->data, index, value);
    ;
  }
  return 0;
}
 res_task_pri *res_task_heap_pop(struct res_task_heap *heap)
{
  tommy_size_t index, swap, other;
  res_task_pri *result;
  result = (res_task_pri *) tommy_array_get(&heap->data, 0);
  res_task_pri *temp = (res_task_pri *) tommy_array_get(&heap->data, --(heap->count));
  index = 0;
  while (1)
    {
      swap = index * 2 + 1;
      if (swap >= heap->count) break;
      other = swap + 1;
      if ((other < heap->count) && (( (*res_task_heap_item(heap,other)).priority) <= ( (*res_task_heap_item(heap, swap)).priority))) swap = other;
      if ((( (*temp).priority) <= ( (*res_task_heap_item(heap, swap)).priority))) break;
      tommy_array_set(&heap->data, index, res_task_heap_item(heap, swap));
      ;
      index = swap;
    }
  tommy_array_set(&heap->data, index, temp);
  ;
  return result;
}
 void res_task_heap_emerge(struct res_task_heap *heap, size_t index)
{
 size_t parent;
 res_task_pri *temp = (res_task_pri *) tommy_array_get(&heap->data, index);
 for(; index; index = parent)
 {
  parent = (index - 1) / 2;
  if ((( (*res_task_heap_item(heap, parent)).priority) <= ( (*temp).priority))) break;
  tommy_array_set(&heap->data, index, tommy_array_get(&heap->data, parent));
  ;
 }
 tommy_array_set(&heap->data, index, temp);
 ;
}
 void res_task_heap_heapify(struct res_task_heap *heap)
{
 unsigned item, index, swap, other;
 res_task_pri *temp;
 if (heap->count < 2) return;
 item = (heap->count / 2) - 1;
 while (1) {
   temp = tommy_array_get(&heap->data, item);
   index = item;
   while (1)
  {
   swap = index * 2 + 1;
   if (swap >= heap->count) break;
   other = swap + 1;
   if ((other < heap->count) && (( (*res_task_heap_item(heap, other)).priority) <= ( (*res_task_heap_item(heap, swap)).priority))) swap = other;
   if ((( (*temp).priority) <= ( (*res_task_heap_item(heap, swap)).priority))) break;
   tommy_array_set(&heap->data, index, tommy_array_get(&heap->data, swap));
   ;
   index = swap;
  }
  if (index != item)
  {
    tommy_array_set(&heap->data, index, temp);
    ;
  }
  if (!item) return;
  --item;
 }
}
int res_task_eq(res_task a, res_task b) {
  return a.x == b.x || a.y == b.x;
}
int res_task_pri_eq( res_task_pri a, res_task_pri b) {
  return res_task_eq(*(a.res_task), *(b.res_task));
}
int res_task_pri_clause_cont( res_task_pri a, clause *c) {
  return ( (a.res_task)->x == c || (a.res_task)->y == c);
}
 int res_task_heap_delete(struct res_task_heap *heap, res_task_pri *value) {
  res_task_pri *element;
  for ( int i=0; i < heap->count; i++) {
    element = res_task_heap_item(heap, i);
    if (res_task_pri_eq(*element, *value)) {
      element->priority = -1;
      res_task_heap_heapify(heap);
      res_task_heap_pop(heap);
      return 1;
    }
    return 0;
  }
}
 int res_task_heap_clausal_delete(struct res_task_heap *heap, clause *c) {
  res_task_pri *element;
  int num_matches = 0;
  for ( int i=0; i < heap->count; i++) {
    element = res_task_heap_item(heap, i);
    if (res_task_pri_clause_cont(*element, c)) {
      element->priority = -1;
      num_matches++;
    }
  }
  res_task_heap_heapify(heap);
  for (int j=0; j < num_matches; j++) {
      element = res_task_heap_pop(heap);
      free(element);
  }
  return num_matches;
}
 void res_task_heap_destroy(struct res_task_heap *heap) {
        res_task_pri *current_item;
   res_task *current_res_task;
   for (int i = 0; i < tommy_array_size(&heap->data); i++) {
     current_item = res_task_heap_item(heap, i);
     if (i < heap->count) {
       current_res_task = current_item->res_task;
     }
   }
   tommy_array_done(&heap->data);
}
