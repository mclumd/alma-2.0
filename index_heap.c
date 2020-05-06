 #include <stdlib.h>
 #include "index_heap.h"
 void index_heap_init(struct index_heap *heap, int size) {
  tommy_array_init(&heap->data);
  heap->max_size = tommy_array_size(heap->data);
  heap->count = 0;
}
 res_task_pri index_heap_item(struct index_heap *heap, int idx) {
    return * (res_task_pri *) tommy_array_get(heap->data, idx);
}
 int index_heap_push(struct index_heap *heap, res_task_pri value) {
  tommy_count_t index, parent;
  if (heap->count >= heap->max_size) {
    heap->max_size *= 2;
    tommy_array_grow(heap->data, heap->max_size);
    heap->max_size = tommay_array_size(heap->data);
  } else {
    for(index = heap->count++; index; index = parent) {
      parent = (index - 1) / 2;
      if (((h eap->data[parent].priority) <= (value.priority))) break;
      tommy_array_set(heap->data, index, tommay_array_get(heap->data, parent));
      ;
    }
    tommy_array_set(heap->data, index, value);
    ;
  }
  return 0;
}
 res_task_pri index_heap_pop(struct index_heap *heap)
{
  tommy_count_t index, swap, other;
  res_task_pri result;
  result = * (res_task_pri *) tommy_array_get(heap->data, 0);
  res_task_pri *temp = (res_task_pri *) tommy_array_get(heap->data, --(heap->count));
  index = 0;
  while (1)
    {
      swap = index * 2 + 1;
      if (swap >= heap->count) break;
      other = swap + 1;
      if ((other < heap->count) && ((tommy_array_get(heap->data,other).priority) <= (tommy_array_get(heap->data, swap).priority))) swap = other;
      if (((temp.priority) <= (tommy_array_get(heap->data, swap).priority))) break;
      tommy_array_set(heap->data, index, tommy_array_get(heap->data, swap));
      ;
      index = swap;
    }
  tommy_array_set(heap->data, index, temp);
  ;
  return result;
}
 void index_heap_emerge(struct index_heap *heap, size_t index)
{
 size_t parent;
 res_task_pri *temp = (heapt_type *) tommy_array_get(heap->data, index);
 for(; index; index = parent)
 {
  parent = (index - 1) / 2;
  if (((heap->data[parent].priority) <= (*temp.priority))) break;
  tommy_array_set(heap->data, index, tommy_array_get(heap->data, parent));
  ;
 }
 tommy_array_set(heap->data, index, temp);
 ;
}
 void index_heap_heapify(struct index_heap *heap)
{
 unsigned item, index, swap, other;
 res_task_pri *temp;
 if (heap->count < 2) return;
 item = (heap->count / 2) - 1;
 while (1)
 {
   temp = tommy_array_get(heap->data, item);
   index = item;
   while (1)
  {
   swap = index * 2 + 1;
   if (swap >= heap->count) break;
   other = swap + 1;
   if ((other < heap->count) && ((tommy_array_get(heap->data, other).priority) <= (tommy_array_get(heap->data, swap).priority))) swap = other;
   if (((temp.priority) <= (tommy_array_get(heap->data, swap).priority))) break;
   tommy_array_set(heap->data, index, tommy_array_get(heap->data, swap);
   ;
   index = swap;
  }
  if (index != item)
  {
    tommy_array_set(heap->data, index, temp);
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
 int index_heap_delete(struct index_heap *heap, res_task_pri value) {
  for ( int i=0; i < heap->count; i++) if ( res_task_pri_eq(heap->data[i], value)) {
 heap->data[i].priority = -1;
 index_heap_heapify(heap);
 index_heap_pop(heap);
 return 1;
      }
  return 0;
}
 void index_heap_destroy(struct index_heap *heap) {
  for (int i = 0; i < heap->count; i++) free(heap->data[i].res_task);
  free(heap);
}
