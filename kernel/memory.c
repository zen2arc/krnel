#include "kernel.h"

#define MEMORY_START 0x100000  /* starts at 1MB */
#define MEMORY_END   0x2000000 /* 32MB for now, dont need more its already snappy enough */

typedef struct block {
    usize size;
    struct block* next;
    int free;
} block_t;

static block_t* heap_start = NULL;

void mem_init(void) {
    heap_start = (block_t*)MEMORY_START;
    heap_start->size = MEMORY_END - MEMORY_START - sizeof(block_t);
    heap_start->next = NULL;
    heap_start->free = 1;
}

void* kmalloc(usize size) {
    if (size == 0 || heap_start == NULL) {
        return NULL;
    }

    block_t* current = heap_start;
    while (current != NULL) {
        if (current->free && current->size >= size) {
            current->free = 0;
            return (void*)(current + 1);
        }
        current = current->next;
    }

    return NULL;
}

void kfree(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    block_t* block = ((block_t*)ptr) - 1;
    block->free = 1;
}
