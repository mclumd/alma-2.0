/*
 * Conquest of Levidon
 * Copyright (C) 2016  Martin Kunev <martinkunev@gmail.com>
 *
 * This file is part of Conquest of Levidon.
 *
 * Conquest of Levidon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 3 of the License.
 *
 * Conquest of Levidon is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Conquest of Levidon.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Modified by jdbrody@gmail.com for use as a resolution task priority
 * queue in alma.  Also adding delete method.  Since we want this to
 * facilitate a queue that can replace the least-important task, we
 * also want to keep a dual heap which keeps track of the indices of
 * elements as a max-heap.
 */

// The arguments with which the macro functions are called are guaranteed to produce no side effects.

//#include "alma_kb.h"
//#define heap_name res_task_heap




// Returns whether a is on top of b in the heap.
#if !defined(heap_above)
# define heap_above(a, b) (( (a).priority) <= ( (b).priority))
#endif

// Called after an element changes its index in the heap (including initial push).
#if !defined(heap_update)
# define heap_update(heap, index)
#endif

#define NAME_CAT_EXPAND(a, b) a ## b
#define NAME_CAT(a, b) NAME_CAT_EXPAND(a, b)
#define NAME(suffix) NAME_CAT(heap_name, suffix)

#define STRING_EXPAND(string) #string
#define STRING(string) STRING_EXPAND(string)

#if defined(HEADER) || defined(SOURCE)
# define STATIC
#else
# define STATIC static
#endif

#if !defined(SOURCE)
#define GUARD0 #ifndef RES_TASK_HEAP_H
#define GUARD1 #define RES_TASK_HEAP_H
#define GUARD2 #endif
#define INCLUDE0 #include "resolution.h"
#define INCLUDE1 #include "tommy.h"  
GUARD0
GUARD1
INCLUDE0
INCLUDE1

#ifdef USE_DUAL_HEAP
#include "index_heap.h"
#endif


typedef struct heap_name
{
  //heap_type *data; // Array with the elements.
  tommy_array data;
  size_t count; // Number of elements actually in the heap.
  tommy_size_t max_size;
#ifdef USE_DUAL_HEAP
  int max_idx; // Index of maximal element
  int max_val; // Max priority
  int min_val; // Not necessary, but easier
  index_heap *max_heap;   // Keep a heap of indices
#endif
} heap_name;
STATIC void NAME(_init)(struct heap_name *heap, int size);
STATIC heap_type *NAME(_item)(struct heap_name *heap, int idx);
STATIC int NAME(_push)(struct heap_name *heap, heap_type *value);
STATIC heap_type *NAME(_pop)(struct heap_name *heap);
STATIC void NAME(_emerge)(struct heap_name *heap, size_t index);
STATIC void NAME(_heapify)(struct heap_name *heap);
STATIC int NAME(_delete)(struct heap_name *heap, heap_type *value);
STATIC int NAME(_clausal_delete)(struct heap_name *heap, clause *c);
STATIC void NAME(_destroy)(struct heap_name *heap);
GUARD2
#endif

#if !defined(HEADER)

#if defined(SOURCE)

# define INCLUDE0 #include <stdlib.h>
# define INCLUDE1 #include STRING(HEADER_NAME)
# define INCLUDE2 #include "tommy.h"
# define INCLUDE3 #include "alma_kb.h"
INCLUDE0
INCLUDE1
INCLUDE3
#else
# include <stdlib.h>
#endif

#define heap_type res_task_pri
STATIC void NAME(_init)(struct heap_name *heap, int size) {
  /* For now, just use regular calloc for the data.  Eventually make this a tommy array. */
  //heap->max_size = size;   // Max_size ignored while using tommy arrays
  //heap->data = calloc(sizeof(res_task_pri), heap->max_size);
  tommy_array_init(&heap->data);
  //heap->max_size = tommy_array_size(&heap->data);
  heap->max_size = 1 << TOMMY_ARRAY_BIT;
  heap->count = 0;
  tommy_array_grow(&heap->data, size);
  /*
  for (int i=0; i < heap->max_size; i++) {
    heap->data[i].res_task = NULL;
    heap->data[i].priority = -1;
  } */

#ifdef USE_DUAL_HEAP
  heap->max_idx = 0;
  heap->max_val = -1;
  heap->min_val = INT_MAX;
  index_heap_init(heap->max_heap, size);   // Keep a heap of indices
#endif
}

STATIC heap_type *NAME(_item)(struct heap_name *heap, int idx) {
    return (heap_type *) tommy_array_get(&heap->data, idx);
    //return heap->data[idx];
}

// Push element to the heap.
STATIC int NAME(_push)(struct heap_name *heap, heap_type *value) {
  tommy_size_t index, parent;
  if (heap->count >= heap->max_size) {
    heap->max_size *= 2;
    tommy_array_grow(&heap->data, heap->max_size);
    heap->max_size = tommy_array_size(&heap->data);

#ifdef USE_DUAL_HEAP
    /* If we're using a dual heap, we'll replace the maximum valued
       item with the new value and swim.  Otherwise we simply don't
       allow pushes to full heaps.  */
    
    if (value.priority < heap->max_val) {

      
      /* We want to get rid of the largest element and add this
	 one.  By definition, the largest element has no
	 children, so we want to "swim" from this node.  Since
	 this was a heap, any heap violations will be localized and swimming will produce a valid min-heap.
	 
	 This code is taken from the swim code at:  https://algs4.cs.princeton.edu/24pq/
      */
      size_t k;
      index_heap_el I;
      I = index_heap_pop(heap->max_heap);
      k = I.index;

      heap_type temp;
      heap->data[k] = value;
      while (k > 1 && heap_above(heap->data[size_t(k/2)], heap->data[k])) {
	// exch(k, k/2);
	temp = heap->data[size_t(k/2)];
	heap->data[size_t(k/2)] = heap->data[k];
	heap->data[k] = temp;
	k = size_t(k/2);
      }
      /* Push k onto the index heap. */
      I.index = k;
      I.prioirty = value.priority;
      index_heap_push(heap->max_heap, I);
    }
#endif
  } else {
    // Find out where to put the element and put it in.
    for(index = heap->count++; index; index = parent) {
      parent = (index - 1) / 2;
      //if (heap_above(heap->data[parent], value)) break;
      if (heap_above( * (heap_type *) tommy_array_get(&heap->data, parent), *value)) break;
      //heap->data[index] =     heap->data[parent];
      tommy_array_set(&heap->data, index, tommy_array_get(&heap->data, parent));
      heap_update(heap, index);   
    }
    // heap->data[index] = value;
    tommy_array_set(&heap->data, index, value);
    heap_update(heap, index);
  }
  return 0;
}

// Removes the biggest element from the heap.
STATIC heap_type *NAME(_pop)(struct heap_name *heap)
{
  tommy_size_t index, swap, other;
  heap_type *result;
  //result = heap->data[0];
  result = (heap_type *) tommy_array_get(&heap->data, 0);
  // Remove the biggest element.
  //heap_type temp = heap->data[--heap->count];
  heap_type *temp = (heap_type *) tommy_array_get(&heap->data, --(heap->count));

  // Reorder the elements.
  index = 0;
  while (1)
    {
      // Find which child to swap with.
      swap = index * 2 + 1;
      if (swap >= heap->count) break; // If there are no children, the heap is reordered.
      other = swap + 1;
      if ((other < heap->count) && heap_above( *NAME(_item)(heap,other), *NAME(_item)(heap, swap))) swap = other;
      //if (heap_above(temp, heap->data[swap])) break; // If the bigger child is less than or equal to its parent, the heap is reordered.
      //heap->data[index] = heap->data[swap];
      if (heap_above(*temp, *NAME(_item)(heap, swap))) break; // If the bigger child is less than or equal to its parent, the heap is reordered.
      tommy_array_set(&heap->data, index, NAME(_item)(heap, swap));
      heap_update(heap, index);
      index = swap;
    }
  //heap->data[index] = temp;
  tommy_array_set(&heap->data, index, temp);
  heap_update(heap, index);
  return result;
}

// Move an element closer to the front of the heap.
STATIC void NAME(_emerge)(struct heap_name *heap, size_t index) // TODO ? rename to heap_sift_up
{
	size_t parent;

	//heap_type temp = heap->data[index];
	heap_type *temp = (heap_type *) tommy_array_get(&heap->data, index);

	for(; index; index = parent)
	{
		parent = (index - 1) / 2;
		if (heap_above( *NAME(_item)(heap, parent), *temp)) break;
		//heap->data[index] = heap->data[parent];
		tommy_array_set(&heap->data, index, tommy_array_get(&heap->data, parent));
		heap_update(heap, index);
	}
	//heap->data[index] = temp;
	tommy_array_set(&heap->data, index, temp);
	heap_update(heap, index);
}

// Heapifies a non-empty array.
STATIC void NAME(_heapify)(struct heap_name *heap)
{
	unsigned item, index, swap, other;
	heap_type *temp;

	if (heap->count < 2) return;

	// Move each non-leaf element down in its subtree until it satisfies the heap property.
	item = (heap->count / 2) - 1;
	while (1) {
	  // Find the position of the current element in its subtree.
	  //temp = heap->data[item];
	  temp = tommy_array_get(&heap->data, item);
	  index = item;
	  while (1)
		{
			// Find the child to swap with.
			swap = index * 2 + 1;
			if (swap >= heap->count) break; // If there are no children, the element is placed properly.
			other = swap + 1;
			//if ((other < heap->count) && heap_above(heap->data[other], heap->data[swap])) swap = other;
			if ((other < heap->count) && heap_above( *NAME(_item)(heap, other), *NAME(_item)(heap, swap))) swap = other;
			if (heap_above(*temp, *NAME(_item)(heap, swap))) break; // If the bigger child is less than or equal to the parent, the element is placed properly.

			//heap->data[index] = heap->data[swap];
			tommy_array_set(&heap->data, index, tommy_array_get(&heap->data, swap));
			heap_update(heap, index);
			index = swap;
		}
		if (index != item)
		{
		  //heap->data[index] = temp;
		  tommy_array_set(&heap->data, index, temp);
		  heap_update(heap, index);
		}

		if (!item) return;
		--item;
	}
}

// This somewhat counterintuitive definiton derives from the way remove_res_tasks needs it.
// TODO:  Make this more robust and intuitive.  This will probably require definiing a host of
// recursive equality functions, starting with clause equality.
int res_task_eq(res_task a, res_task b) {
  return a.x == b.x || a.y == b.x;
}

int res_task_pri_eq( res_task_pri a, res_task_pri b) {
  return res_task_eq(*(a.res_task), *(b.res_task));
}

// Returns true iff one of the clauses in the res_task corresponding to a is c
int res_task_pri_clause_cont( res_task_pri a, clause *c) {
  return ( (a.res_task)->x == c  || (a.res_task)->y == c);
}

// Deletes an element.  Returns 1 if value is found and 0 otherwise.
// The search for the element is based on res_task equality and is slow.
STATIC int NAME(_delete)(struct heap_name *heap, heap_type *value) {
  heap_type *element;
  for ( int i=0; i < heap->count; i++) {
    element = NAME(_item)(heap, i);
    if (res_task_pri_eq(*element, *value)) {
      element->priority = -1;
      NAME(_heapify)(heap);
      NAME(_pop)(heap);
      return 1;
    }
    return 0;
  }
}

// Delete any heap element that contains the given clause (a prelude to clause deletion).
// This destroys any pretense that this is type-neutral; probably the quickest thing.
STATIC int NAME(_clausal_delete)(struct heap_name *heap, clause *c) {
  heap_type *element;
  int num_matches = 0;
  for ( int i=0; i < heap->count; i++) {
    element = NAME(_item)(heap, i);
    if (res_task_pri_clause_cont(*element, c)) {
      element->priority = -1;
      num_matches++;
    }
  }
  NAME(_heapify)(heap);
  for (int j=0; j < num_matches; j++) {
      element = NAME(_pop)(heap);
      free(element);
  }
  return num_matches;
}

STATIC void NAME(_destroy)(struct heap_name *heap) {
        res_task_pri *current_item;
   res_task *current_res_task;
   //for (int i = 0; i < heap->count; i++) {
   for (int i = 0; i < tommy_array_size(&heap->data); i++) {
     current_item = res_task_heap_item(heap, i);
     if (i < heap->count) {
       //fprintf(stderr, "free RTHD0");
       current_res_task = current_item->res_task;
       //free_clause(current_res_task->x);
       //free_clause(current_res_task->y);
       // Not explicitly free pos and neg; guess they're part of x and y and
       // thus handled above.
       //free(current_res_task);
     }
     //fprintf(stderr, "free RTHD");
     // if (current_item) free(current_item);  // Free current item if not NULL; this may not work.
   }
   tommy_array_done(&heap->data);
   /*   TODO:  Why do we need the condititional?
   if (heap->count > 0) {
     tommy_array_done(&heap->data);
     } */
}

#endif /* !defined(HEADER) */

#undef STATIC

#undef STRING
#undef STRING_EXPAND

#undef NAME
#undef NAME_CAT
#undef NAME_CAT_EXPAND

#undef heap_update
#undef heap_above
#undef heap_type
#undef heap_name
