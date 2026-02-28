#include "kernel.h"

extern u8 _end;

#define MEMORY_END  0x2000000
#define HEAP_MAGIC  0xDEADBEEF
#define ALIGNMENT   8

static void heap_dump(void);

typedef struct block {
    u32 magic;
    usize size;
    struct block* next;
    int free;
} block_t;

static block_t* heap_start = NULL;

/* align helper */
static inline usize align_up(usize size) {
    return (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
}

void mem_init(void) {
    u32 heap_addr = (u32)&_end;

    /* ensure heap start is aligned */
    heap_addr = align_up(heap_addr);

    heap_start = (block_t*)heap_addr;

    heap_start->magic = HEAP_MAGIC;
    heap_start->size =
        MEMORY_END - heap_addr - sizeof(block_t);
    heap_start->next = NULL;
    heap_start->free = 1;

    vga_write("heap initialized\n", COLOUR_LIGHT_GREEN);

    char buf[64];
    snprintf(buf, sizeof(buf),
             "kernel end: %x\nheap start: %x\n",
             (u32)&_end, heap_addr);
    vga_write(buf, COLOUR_YELLOW);
}

void* kmalloc(usize size) {
    if (size == 0 || heap_start == NULL)
        return NULL;

    size = align_up(size);

    block_t* current = heap_start;

    while (current) {

        if (current->magic != HEAP_MAGIC) {
            vga_write("kmalloc: heap corruption\n", COLOUR_RED);
            heap_dump();
            return NULL;
        }

        if (current->free && current->size >= size) {

            /* split block if enough space */
            if (current->size >= size + sizeof(block_t) + ALIGNMENT) {

                block_t* new_block =
                    (block_t*)((u8*)(current + 1) + size);

                new_block->magic = HEAP_MAGIC;
                new_block->size =
                    current->size - size - sizeof(block_t);
                new_block->free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            current->free = 0;

            char buf[64];
            snprintf(buf, sizeof(buf),
                     "kmalloc: %u bytes at %x\n",
                     (u32)size, (u32)(current + 1));
            vga_write(buf, COLOUR_LIGHT_GREEN);

            return (void*)(current + 1);
        }

        current = current->next;
    }

    vga_write("kmalloc: out of memory\n", COLOUR_LIGHT_RED);
    heap_dump();
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;

    block_t* block = ((block_t*)ptr) - 1;

    if (block->magic != HEAP_MAGIC) {
        vga_write("kfree: invalid pointer\n", COLOUR_RED);
        return;
    }

    block->free = 1;

    char buf[64];
    snprintf(buf, sizeof(buf),
             "kfree: %x\n", (u32)ptr);
    vga_write(buf, COLOUR_LIGHT_BLUE);

    /* coalesce forward */
    if (block->next && block->next->free) {
        block->size += sizeof(block_t) + block->next->size;
        block->next = block->next->next;
    }
}

static void heap_dump(void) {
    vga_write("=== HEAP DUMP ===\n", COLOUR_YELLOW);

    block_t* current = heap_start;
    int i = 0;

    while (current) {

        if (current->magic != HEAP_MAGIC) {
            vga_write("HEAP CORRUPTION DETECTED\n", COLOUR_RED);
            return;
        }

        char buf[64];
        snprintf(buf, sizeof(buf),
                 "block %d: addr=%x size=%u free=%d\n",
                 i, (u32)current, (u32)current->size, current->free);
        vga_write(buf, COLOUR_LIGHT_GRAY);

        current = current->next;
        i++;
    }
}
