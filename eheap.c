/*========================================================================
 *  eheap.c – Minimal standalone heap manager 
 *            32-bit
 *======================================================================*/
#include "eheap.h"
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "py/runtime.h"


#ifndef EHEAP_ALIGN
#define EHEAP_ALIGN 4               /* default byte alignment */
#endif

#define ALIGN_MASK (EHEAP_ALIGN - 1)
#define ALIGN_UP(x) (((x) + ALIGN_MASK) & ~ALIGN_MASK)

/*--------------------------------------------------  block header layout */
typedef struct block_link {
    struct block_link *next;
    size_t             size;     /* MSB=1 表示“已分配” */
} block_t;

#define USED_MASK     ( (size_t)1 << (sizeof(size_t)*8 - 1) )
#define IS_USED(b)    ( (b)->size &  USED_MASK )
#define MARK_USED(b)  ( (b)->size |= USED_MASK )
#define MARK_FREE(b)  ( (b)->size &= ~USED_MASK )
#define BLOCK_SIZE(b) ( (b)->size & ~USED_MASK )

#define HDR_SIZE   ALIGN_UP(sizeof(block_t))
#define MIN_SPLIT  ( HDR_SIZE << 1 )

/*--------------------------------------------------  heap globals */
static uint8_t *heap_base  = NULL;
static uint8_t *heap_end   = NULL;
static size_t   heap_total = 0;

static block_t  start_node;         /* dummy head (always free) */
static block_t  end_marker;         /* dummy tail node storage */
static block_t *end_node = &end_marker; /* pointer to tail marker */

static size_t free_bytes = 0;
static size_t min_free  = 0;
static bool   initialized = false;

/* --------------------------------------------------------------------- */
/* Helper function to validate a block pointer is valid */
static bool is_valid_block(block_t *blk) {
    uint8_t *ptr = (uint8_t*)blk;
    return ptr >= heap_base && ptr < heap_end && 
           (((uintptr_t)ptr - (uintptr_t)heap_base) % EHEAP_ALIGN) == 0;
}
/* Safe version of insert_free with better bounds checking */
static void insert_free(block_t *blk){
    if(!(valid_block(blk) && !IS_USED(blk))){
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("insert free broken"));
        return;
    }

    block_t *it = &start_node;
    /* address-ordered insertion */
    while (it->next < blk && it->next != tail_node) {
        it = it->next;
        if (!is_valid_block(it)) {
            mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("insert free broken"));
            return;
        }
    }

    /* merge forward - check if next block is within heap and is free */
    if (it->next != end_node && 
        ((uint8_t*)blk + BLOCK_SIZE(blk)) == (uint8_t*)it->next && 
        !IS_USED(it->next)) {
        /* Safe from integer overflow due to sizes being limited by heap_total */
        blk->size += BLOCK_SIZE(it->next);
        blk->next = it->next->next;
    } else {
        blk->next = it->next;
    }

    /* merge backward - check if prev block is within heap and is free */
    if (it != &start_node && 
        ((uint8_t*)it + BLOCK_SIZE(it)) == (uint8_t*)blk && 
        !IS_USED(it)) {
        it->size += BLOCK_SIZE(blk);
        it->next = blk->next;
    } else {
        it->next = blk;
    }
}

static void heap_init_once(void){

    if (initialized) return;
    
    block_t *first = (block_t*)heap_base;
    first->size = heap_total - HDR_SIZE; /* exclude end marker */
    MARK_FREE(first);
    first->next = end_node;

    start_node.next = first;
    start_node.size = 0; /* never used */

    /* Initialize end marker node */
    end_node->next = NULL;
    end_node->size = 0;
    MARK_USED(end_node); /* Mark as used to prevent merging */

    free_bytes = heap_total - HDR_SIZE;
    min_free = free_bytes;
    initialized = true;
}

/*======================================================================*/
/*                        Public  API                                   */
/*======================================================================*/
void eheap_init(void *buf, size_t bytes){
    if (!(buf && bytes > HDR_SIZE*2 + EHEAP_ALIGN)){
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("heap init failed"));
        return;
    }

    /* Align the starting address upward */
    uintptr_t aligned_start = ALIGN_UP((uintptr_t)buf);
    size_t alignment_loss = (size_t)(aligned_start - (uintptr_t)buf);
    
    /* Apply alignment and adjust size */
    heap_base = (uint8_t*)aligned_start;
    bytes -= alignment_loss;
    bytes = (bytes / EHEAP_ALIGN) * EHEAP_ALIGN; /* Ensure size is aligned */
    
    heap_total = bytes;
    heap_end = heap_base + heap_total;
    
    /* Place end_node storage at the end of the heap, if not using external storage */
    if (end_node == &end_marker) {
        /* Use a portion of our heap for the end marker */
        size_t reserved = ALIGN_UP(sizeof(block_t));
        heap_total -= reserved;
        end_node = (block_t*)(heap_base + heap_total);
    }
    
    initialized = false;
    heap_init_once();
}

vvoid* e_malloc(size_t size)
{
    if(!size || !heap_base || !initialized) return NULL;
    
    /* Align the requested size and add header size */
    size_t total_size = ALIGN_UP(size + HDR_SIZE);
    
    /* Prevent potential overflow */
    if (total_size < size) return NULL;
    
    block_t *prev = &start_node;
    block_t *cur = start_node.next;

    while(cur != end_node)
    {
        /* Validate current block */
        if (!is_valid_block(cur)) {
            mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("heap struct broken"));
            return NULL;
        }
        
        if(!IS_USED(cur) && BLOCK_SIZE(cur) >= total_size)
        {
            size_t remain = BLOCK_SIZE(cur) - total_size;
            if(remain >= MIN_SPLIT)
            {
                /* Ensure split block would be within heap bounds */
                block_t *split = (block_t*)((uint8_t*)cur + total_size);
                if (!is_valid_block(split)) {
                    mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("heap struct broken"));
                    return NULL;
                }
                
                split->size = remain;
                MARK_FREE(split);
                split->next = cur->next;

                cur->size = total_size;
                prev->next = split;
            }
            else
            {
                /* No splitting - remove the whole block from free list */
                prev->next = cur->next;
            }
            
            MARK_USED(cur);
            free_bytes -= BLOCK_SIZE(cur);
            if(free_bytes < min_free) min_free = free_bytes;
            
            /* Return pointer to payload area */
            return (uint8_t*)cur + HDR_SIZE;
        }
        prev = cur;
        cur = cur->next;
    }
    
    return NULL; /* OOM */
}

void e_free(void *ptr)
{
    if(!ptr || !heap_base || !initialized) return;
    
    /* Check if pointer is within heap bounds */
    if ((uint8_t*)ptr < heap_base + HDR_SIZE || (uint8_t*)ptr >= heap_end) {
        assert(false && "Invalid pointer to e_free - outside heap bounds");
        return;
    }
    
    /* Check alignment */
    if (((uintptr_t)ptr - HDR_SIZE) & ALIGN_MASK) {
        assert(false && "Unaligned pointer to e_free");
        return;
    }
    
    block_t *blk = (block_t*)((uint8_t*)ptr - HDR_SIZE);
    
    /* Prevent double free */
    if (!IS_USED(blk)) {
        assert(false && "Double free detected");
        return;
    }
    
    /* Verify block header looks valid */
    size_t blk_size = BLOCK_SIZE(blk);
    if (blk_size == 0 || (uint8_t*)blk + blk_size > heap_end) {
        assert(false && "Invalid block size in e_free");
        return;
    }
    
    MARK_FREE(blk);
    free_bytes += blk_size;
    insert_free(blk);
}

void* e_realloc(void *ptr, size_t new_size)
{
    /* Handle special cases */
    if(!ptr) return e_malloc(new_size);
    if(new_size == 0) { 
        e_free(ptr); 
        return NULL; 
    }
    
    /* Check if pointer is within heap bounds */
    if ((uint8_t*)ptr < heap_base + HDR_SIZE || (uint8_t*)ptr >= heap_end) {
        assert(false && "Invalid pointer to e_realloc");
        return NULL;
    }
    
    block_t *blk = (block_t*)((uint8_t*)ptr - HDR_SIZE);
    
    /* Verify block is used and has valid size */
    if (!IS_USED(blk)) {
        assert(false && "e_realloc on freed memory");
        return NULL;
    }
    
    size_t curr_payload_size = BLOCK_SIZE(blk) - HDR_SIZE;
    
    /* If shrinking or same size, just return the same pointer */
    if(new_size <= curr_payload_size) return ptr;
    
    /* Need to grow - check if next block is free and has enough space */
    block_t *next_blk = NULL;
    
    /* Find the next physical block */
    uint8_t *next_addr = (uint8_t*)blk + BLOCK_SIZE(blk);
    if (next_addr < heap_end - HDR_SIZE) {
        next_blk = (block_t*)next_addr;
        
        /* Check if we can expand into next block */
        if (is_valid_block(next_blk) && !IS_USED(next_blk)) {
            size_t combined_size = BLOCK_SIZE(blk) + BLOCK_SIZE(next_blk);
            size_t needed_size = ALIGN_UP(new_size + HDR_SIZE);
            
            if (combined_size >= needed_size) {
                /* Find next block in free list to update links */
                block_t *prev = &start_node;
                while (prev->next != next_blk && prev->next != end_node) {
                    prev = prev->next;
                    /* Safety check to prevent infinite loops */
                    if (!is_valid_block(prev)) {
                        assert(false && "Corrupted free list in e_realloc");
                        return NULL;
                    }
                }
                
                if (prev->next == next_blk) {
                    /* Remove next block from free list */
                    prev->next = next_blk->next;
                    
                    /* Update current block size */
                    blk->size = (blk->size & USED_MASK) | combined_size;
                    
                    /* Split if enough remaining space */
                    size_t remain = combined_size - needed_size;
                    if (remain >= MIN_SPLIT) {
                        block_t *split = (block_t*)((uint8_t*)blk + needed_size);
                        split->size = remain;
                        MARK_FREE(split);
                        
                        /* Insert split into free list */
                        blk->size = (blk->size & USED_MASK) | needed_size;
                        free_bytes -= needed_size;
                        insert_free(split);
                    } else {
                        /* Use entire combined block */
                        free_bytes -= combined_size;
                    }
                    
                    if (free_bytes < min_free) min_free = free_bytes;
                    return ptr;
                }
            }
        }
    }
    
    /* Standard case: allocate new block, copy data, free old block */
    void *n = e_malloc(new_size);
    if (n) {
        /* Safe copy that won't overrun source buffer */
        memcpy(n, ptr, curr_payload_size);
        e_free(ptr);
    }
    return n;
}


size_t e_heap_free(void)     { return free_bytes; }
size_t e_heap_min_free(void) { return min_free;   }

/*---------------- diagnostics ------------------*/
/* Get fragmentation level as a percentage (0-100) */
int e_heap_fragmentation(void) {
    if (!heap_base || !initialized || free_bytes == 0) return 0;
    
    /* Find largest free block */
    size_t largest_free = 0;
    block_t *cur = start_node.next;
    
    while (cur != end_node) {
        if (!IS_USED(cur)) {
            size_t blk_size = BLOCK_SIZE(cur);
            if (blk_size > largest_free) {
                largest_free = blk_size;
            }
        }
        cur = cur->next;
    }
    
    /* Calculate fragmentation percentage */
    /* 0% = one contiguous free block, 100% = completely fragmented */
    if (largest_free == 0) return 100;
    return (int)(100 - (largest_free * 100) / free_bytes);
}

/* Optional: Add debug/validation function */
bool e_heap_validate(void) {
    if (!heap_base || !initialized) return false;
    
    size_t counted_free = 0;
    block_t *prev = &start_node;
    block_t *cur = start_node.next;
    
    while (cur != end_node) {
        /* Check block is within heap */
        if (!is_valid_block(cur)) return false;
        
        /* Check block doesn't exceed heap bounds */
        if ((uint8_t*)cur + BLOCK_SIZE(cur) > heap_end) return false;
        
        /* Count free bytes */
        if (!IS_USED(cur)) {
            counted_free += BLOCK_SIZE(cur);
            
            /* Check for adjacent free blocks that should have been merged */
            block_t *next = cur->next;
            if (next != end_node && !IS_USED(next) && 
                ((uint8_t*)cur + BLOCK_SIZE(cur) == (uint8_t*)next)) {
                return false; /* Adjacent free blocks not merged */
            }
        }
        
        prev = cur;
        cur = cur->next;
    }
    
    /* Verify free byte count matches tracked count */
    return counted_free == free_bytes;
}