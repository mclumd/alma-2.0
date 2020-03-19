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
 * Modified by jdbrody@gmail.com for use as a resolution task priority queue in alma.  Also adding delete method.
 */

// The arguments with which the macro functions are called are guaranteed to produce no side effects.

//#include "alma_kb.h"
#define heap_name res_task_heap




// Returns whether a is on top of b in the heap.
#if !defined(heap_above)
# define heap_above(a, b) ((a.priority) <= (b.priority))
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
# define INCLUDE0 #include "resolution.h"
GUARD0
GUARD1
INCLUDE0

typedef struct res_task_pri {
    res_task *res_task;
    int priority;
} res_task_pri;

#define heap_type res_task_pri



typedef struct heap_name
{
  heap_type *data; // Array with the elements.
  size_t count; // Number of elements actually in the heap.
  int max_size;   
} heap_name;
STATIC void NAME(_init)(struct heap_name *heap);
STATIC void NAME(_push)(struct heap_name *heap, heap_type value);
STATIC heap_type NAME(_pop)(struct heap_name *heap);
STATIC void NAME(_emerge)(struct heap_name *heap, size_t index);
STATIC void NAME(_heapify)(struct heap_name *heap);
STATIC int NAME(_delete)(struct heap_name *heap, heap_type value);
STATIC void NAME(_destroy)(struct heap_name *heap);
GUARD2
#endif

#if !defined(HEADER)

#if defined(SOURCE)

# define INCLUDE0 #include <stdlib.h>
# define INCLUDE1 #include STRING(HEADER_NAME)
# define INCLUDE2 #include "tommy.h"
INCLUDE0
INCLUDE1
#else
# include <stdlib.h>
#endif

#define heap_type res_task_pri
STATIC void NAME(_init)(struct heap_name *heap) {
  /* For now, just use regular calloc for the data.  Eventually make this a tommy array. */
  heap->max_size = 200;
  heap->data = calloc(sizeof(res_task_pri), heap->max_size);
  for (int i=0; i < heap->max_size; i++) {
    heap->data[i].res_task = NULL;
    heap->data[i].priority = -1;
  }
  heap->count = 0;
}

// Push element to the heap.
STATIC void NAME(_push)(struct heap_name *heap, heap_type value)
{
	size_t index, parent;

	// Find out where to put the element and put it.
	for(index = heap->count++; index; index = parent)
	{
		parent = (index - 1) / 2;
		if (heap_above(heap->data[parent], value)) break;
		heap->data[index] = heap->data[parent];
		heap_update(heap, index);
	}
	heap->data[index] = value;
	heap_update(heap, index);
}

// Removes the biggest element from the heap.
STATIC heap_type NAME(_pop)(struct heap_name *heap)
{
	size_t index, swap, other;
    heap_type result;
    result = heap->data[0];
	// Remove the biggest element.
	heap_type temp = heap->data[--heap->count];

	// Reorder the elements.
	index = 0;
	while (1)
	{
		// Find which child to swap with.
		swap = index * 2 + 1;
		if (swap >= heap->count) break; // If there are no children, the heap is reordered.
		other = swap + 1;
		if ((other < heap->count) && heap_above(heap->data[other], heap->data[swap])) swap = other;
		if (heap_above(temp, heap->data[swap])) break; // If the bigger child is less than or equal to its parent, the heap is reordered.

		heap->data[index] = heap->data[swap];
		heap_update(heap, index);
		index = swap;
	}
	heap->data[index] = temp;
	heap_update(heap, index);
	return result;
}

// Move an element closer to the front of the heap.
STATIC void NAME(_emerge)(struct heap_name *heap, size_t index) // TODO ? rename to heap_sift_up
{
	size_t parent;

	heap_type temp = heap->data[index];

	for(; index; index = parent)
	{
		parent = (index - 1) / 2;
		if (heap_above(heap->data[parent], temp)) break;
		heap->data[index] = heap->data[parent];
		heap_update(heap, index);
	}
	heap->data[index] = temp;
	heap_update(heap, index);
}

// Heapifies a non-empty array.
STATIC void NAME(_heapify)(struct heap_name *heap)
{
	unsigned item, index, swap, other;
	heap_type temp;

	if (heap->count < 2) return;

	// Move each non-leaf element down in its subtree until it satisfies the heap property.
	item = (heap->count / 2) - 1;
	while (1)
	{
		// Find the position of the current element in its subtree.
		temp = heap->data[item];
		index = item;
		while (1)
		{
			// Find the child to swap with.
			swap = index * 2 + 1;
			if (swap >= heap->count) break; // If there are no children, the element is placed properly.
			other = swap + 1;
			if ((other < heap->count) && heap_above(heap->data[other], heap->data[swap])) swap = other;
			if (heap_above(temp, heap->data[swap])) break; // If the bigger child is less than or equal to the parent, the element is placed properly.

			heap->data[index] = heap->data[swap];
			heap_update(heap, index);
			index = swap;
		}
		if (index != item)
		{
			heap->data[index] = temp;
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


// Deletes an element.  Returns 1 if value is found and 0 otherwise.
// The search for the element is based on res_task equality
STATIC int NAME(_delete)(struct heap_name *heap, heap_type value) {
  for ( int i=0; i < heap->count; i++) if (  res_task_pri_eq(heap->data[i], value)) {
	heap->data[i].priority = -1;
	NAME(_heapify)(heap);
	NAME(_pop)(heap);
	return 1;
      }
  return 0;
}

STATIC void NAME(_destroy)(struct heap_name *heap) {
  for (int i = 0; i < heap->count; i++) free(heap->data[i].res_task);
  free(heap);
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
