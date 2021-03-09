/**
 *  heap.c
 *  
 *  DESCRIPTION: See heap.h for deatails
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <string.h>

#include <libdxwifi/details/heap.h>
#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>


static inline size_t parent(size_t i)               { return ((i-1)/2);     }
static inline size_t left(size_t i)                 { return (2*i + 1);     }
static inline size_t right(size_t i)                { return (2*i + 2);     }


static void swap(uint8_t* a, uint8_t* b, size_t sz) {
    uint8_t temp[sz];
    memcpy(temp, a, sz);
    memcpy(a, b, sz);
    memcpy(b, temp, sz);
}


// TODO Make this non-recursive or verify that the recursion gets compiled out
static void heapify(binary_heap* heap, comparator compare, size_t i, size_t sz) {
    size_t l = left(i);
    size_t r = right(i);
    size_t new_index = i;

    if(l < heap->count && compare(offset(heap->tree, l, sz), offset(heap->tree, i, sz))) {
        new_index = l;
    }
    if(r < heap->count && compare(offset(heap->tree, r, sz), offset(heap->tree, new_index, sz))) {
        new_index = r;
    }
    if(new_index != i) {
        swap(offset(heap->tree, i, sz), offset(heap->tree, new_index, sz), sz);
        heapify(heap, compare, new_index, sz);
    }
}


void init_heap(binary_heap* heap, size_t capacity, size_t step_size, comparator compare) {
    debug_assert(heap);

    heap->count         = 0; 
    heap->step_size     = step_size;
    heap->capacity      = capacity;
    heap->compare       = compare;

    heap->tree = calloc(capacity, step_size);
    assert_M(heap->tree, "Failed to allocated heap with capacity: %ld", capacity);
}


void teardown_heap(binary_heap* heap) {
    free(heap->tree);
    heap->tree      = NULL;
    heap->count     = 0;
    heap->step_size = 0;
    heap->capacity  = 0;
    heap->compare   = NULL;
}


void heap_push(binary_heap* heap, const void* data) {
    debug_assert(heap && data);

    size_t sz = heap->step_size;
    if(heap->count < heap->capacity) {
        size_t i = heap->count++;

        memcpy(offset(heap->tree, i, sz), data, sz);

        // Sift from the bottom up
        while(i != 0 && heap->compare(offset(heap->tree, i, sz), offset(heap->tree, parent(i), sz))) {
            swap(offset(heap->tree, i, sz), offset(heap->tree, parent(i), sz), sz);
            i = parent(i);
        }
    }
    else {
        log_error("Binary Heap is full");
    }
}


bool heap_pop(binary_heap* heap, void* out) {
    debug_assert(heap && out);

    if (heap->count > 0) {

        size_t sz = heap->step_size;

        memcpy(out, heap->tree, sz);

        memcpy(heap->tree, offset(heap->tree, --heap->count, sz), sz);

        heapify(heap, heap->compare, 0, sz);

        return true;
    }
    return false;
}


void heap_sort(void* data, size_t count, size_t step_size, comparator compare) {
    debug_assert(data);

    binary_heap heap = {
        .tree       = (uint8_t*)data,
        .count      = count,
        .capacity   = count,
        .step_size  = step_size,
        .compare    = compare
    };

    for(int i = (count / 2) - 1; i >= 0; --i) {
        heapify(&heap, compare, i, step_size);
    }

    while(--heap.count > 0) {
        swap(data, offset(data, heap.count, step_size), step_size);

        heapify(&heap, compare, 0, step_size);
    }
}
