/**
 *  heap.h
 *  
 *  DESCRIPTION: Generic Binary heap data structure.
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */


#ifndef LIBDXWIFI_BINARY_HEAP_H
#define LIBDXWIFI_BINARY_HEAP_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>


/**
 *  Comparator is used to maintain the heaps invariants. The provided comparator
 *  for your set should maintain the algebraic properties of a partial-order
 *  relation.
 */
typedef bool (*comparator)(const uint8_t* lhs, const uint8_t* rhs);


typedef struct {
    uint8_t*    tree;       /* Heap data                                    */
    size_t      count;      /* Number of elements currently in the heap     */
    size_t      capacity;   /* Number of elements that can fit into the heap*/
    size_t      step_size;  /* Size of each element in the heap             */
    comparator  compare;    /* Ordering function                            */
} binary_heap;


/**
 *  DESCRIPTION:    Sorts the given array
 * 
 *  ARGUMENTS: 
 * 
 *      data:       pointer to array
 *  
 *      count:      Number of elements in the array
 *      
 *      step_size:  Size of each element
 * 
 *      comparator: Comparison function     
 */
void heap_sort(void* data, size_t count, size_t step_size, comparator compare);


/**
 *  DESCRIPTION:    Initializes the binary heap data structure
 * 
 *  ARGUMENTS: 
 * 
 *      heap:       pointer to the heap to be initialized
 *  
 *      capacity:   Desired capacity for the heap
 *      
 *      step_size:  Size of each element
 * 
 *      comparator: Comparison function     
 */
void init_heap(binary_heap* heap, size_t capacity, size_t step_size, comparator compare);


/**
 *  DESCRIPTION:    Tearsdown any resources associated with the heap
 * 
 *  ARGUMENTS: 
 * 
 *      heap:       pointer to the heap to be torndown
 *  
 */
void teardown_heap(binary_heap* heap);


/**
 *  DESCRIPTION:    Pushes an element onto the heap
 * 
 *  ARGUMENTS: 
 * 
 *      data:       pointer to the data to be copied
 *  
 */
void heap_push(binary_heap* heap, const void* data);


/**
 *  DESCRIPTION:    Pops an element off of the heap
 * 
 *  ARGUMENTS: 
 * 
 *      out:        pointer to an memory block that is at least as large as the
 *                  step_size
 * 
 *  RETURNS:
 *  
 *      bool:       true if the element was sucessfully popped
 *  
 */
bool heap_pop(binary_heap* heap, void* out);


#endif // LIBDXWIFI_BINARY_HEAP_H
