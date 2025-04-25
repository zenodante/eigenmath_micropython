/*======================================================================== 
 *  eheap.c – Improved standalone heap manager 
 *            32-bit, aligned, safe operations
 *======================================================================*/

 #include "eheap.h"
 #include <stdint.h>
 #include <string.h>
 #include <stdbool.h>
 #include "py/runtime.h"
 
 #ifndef EHEAP_ALIGN
 #define EHEAP_ALIGN 4               /* default byte alignment (power of two) */
 #endif
 
 #define ALIGN_MASK    (EHEAP_ALIGN - 1)
 #define ALIGN_UP(x)   (((x) + ALIGN_MASK) & ~ALIGN_MASK)
 
 /*--------------------------------------------------  block header layout */
 typedef struct block_link {
     struct block_link *next;
     size_t             size;     /* MSB=1 => allocated, lower bits => block size */
 } block_t;
 
 #define USED_MASK     ((size_t)1 << (sizeof(size_t)*8 - 1))
 #define IS_USED(b)    (((b)->size) &  USED_MASK)
 #define MARK_USED(b)  ((b)->size |= USED_MASK)
 #define MARK_FREE(b)  ((b)->size &= ~USED_MASK)
 #define BLOCK_SIZE(b) ((b)->size & ~USED_MASK)
 
 #define HDR_SIZE     ALIGN_UP(sizeof(block_t))
 #define MIN_SPLIT    (HDR_SIZE * 2)
 
 /*--------------------------------------------------  heap globals */
 static uint8_t *heap_base  = NULL;
 static uint8_t *heap_end   = NULL;
 static size_t   heap_total = 0;
 
 static block_t  start_node;         /* dummy head */
 static block_t  end_marker;         /* tail sentinel storage */
 static block_t *end_node = &end_marker;
 
 static size_t free_bytes = 0;
 static size_t min_free   = 0;
 static bool   initialized = false;
 
 /*-------------------------------------------------------------------*/
 static bool is_valid_block(block_t *blk) {
     uint8_t *ptr = (uint8_t*)blk;
     if (ptr < heap_base || ptr >= heap_end) return false;
     return (((uintptr_t)ptr - (uintptr_t)heap_base) & ALIGN_MASK) == 0;
 }
 
/* Insert and coalesce a free block (address-ordered), with overflow guards */  
static void insert_free(block_t *blk) {  
    if (!is_valid_block(blk) || IS_USED(blk)) {  
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("insert_free: invalid or used block"));  
        return;  
    }  
    size_t blk_sz = BLOCK_SIZE(blk);  
    /* guard pointer addition overflow */  
    if (blk_sz > (size_t)(heap_end - (uint8_t*)blk)) {  
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("insert_free: block size overflow"));  
        return;  
    }  
    uint8_t *blk_end = (uint8_t*)blk + blk_sz;  
  
    block_t *prev = &start_node;  
    /* find insertion point */  
    while (prev->next < blk && prev->next != end_node) {  
        prev = prev->next;  
        if (!is_valid_block(prev)) {  
            mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("insert_free: corrupted free list"));  
            return;  
        }  
    }  
  
    /* forward merge */  
    if (prev->next != end_node) {  
        block_t *fwd = prev->next;  
        if (!IS_USED(fwd) &&  
            (uint8_t*)fwd == blk_end) {  
            /* fuse sizes */  
            size_t total = blk_sz + BLOCK_SIZE(fwd);  
            if (total < blk_sz) { /* overflow? */  
                mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("insert_free: combine overflow"));  
                return;  
            }  
            blk->size = total;  
            blk->next = fwd->next;  
            blk_sz = total;  /* update for potential backward merge */  
        } else {  
            blk->next = fwd;  
        }  
    } else {  
        blk->next = end_node;  
    }  
  
    /* backward merge */  
    if (prev != &start_node && !IS_USED(prev)) {  
        uint8_t *prev_end = (uint8_t*)prev + BLOCK_SIZE(prev);  
        if (prev_end == (uint8_t*)blk) {  
            size_t total = BLOCK_SIZE(prev) + blk_sz;  
            if (total < BLOCK_SIZE(prev)) { /* overflow? */  
                mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("insert_free: combine overflow"));  
                return;  
            }  
            prev->size = total;  
            prev->next = blk->next;  
            return;  
        }  
    }  
    prev->next = blk;  
}  
 
 static void heap_init_once(void) {
     if (initialized) return;
 
     /* single free block covers [heap_base .. heap_end) excluding end_node */
     block_t *first = (block_t*)heap_base;
     first->size = (heap_total - HDR_SIZE);
     MARK_FREE(first);
     first->next = end_node;
 
     start_node.next = first;
     start_node.size = 0;
 
     /* initialize end marker */
     end_node->next = NULL;
     end_node->size = 0;
     MARK_USED(end_node);
 
     free_bytes = BLOCK_SIZE(first);
     min_free   = free_bytes;
     initialized = true;
 }
 
 void eheap_init(void *buf, size_t bytes) {
     if (!buf || bytes <= HDR_SIZE*2 + ALIGN_MASK) {
         mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("eheap_init: invalid region"));
         return;
     }
 
     /* align base upward */
     uintptr_t start = ALIGN_UP((uintptr_t)buf);
     size_t   loss  = start - (uintptr_t)buf;
     bytes = (bytes > loss) ? bytes - loss : 0;
     bytes = (bytes / EHEAP_ALIGN) * EHEAP_ALIGN;
     if (bytes <= HDR_SIZE*2) {
         mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("eheap_init: too small after align"));
         return;
     }
 
     heap_base  = (uint8_t*)start;
     heap_total = bytes;
 
     /* reserve tail for end_node */
     size_t res = ALIGN_UP(sizeof(block_t));
     heap_total -= res;
     end_node = (block_t*)(heap_base + heap_total);
 
     heap_end = heap_base + heap_total;
 
     initialized = false;
     heap_init_once();
 }
 
 void* e_malloc(size_t size) {
     if (size == 0 || !initialized) return NULL;
 
     /* check overflow */
     if (size > SIZE_MAX - HDR_SIZE) return NULL;
     size_t needed = ALIGN_UP(size + HDR_SIZE);
 
     block_t *prev = &start_node;
     block_t *cur  = start_node.next;
 
     while (cur != end_node) {
         if (!is_valid_block(cur)) {
             mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("e_malloc: corrupted heap"));
             return NULL;
         }
         if (!IS_USED(cur) && BLOCK_SIZE(cur) >= needed) {
             size_t remain = BLOCK_SIZE(cur) - needed;
             if (remain >= MIN_SPLIT) {
                 /* split */
                 block_t *split = (block_t*)((uint8_t*)cur + needed);
                 split->size = remain;
                 MARK_FREE(split);
                 split->next = cur->next;
 
                 cur->size = needed;
                 prev->next = split;
             } else {
                 /* use entire */
                 prev->next = cur->next;
                 needed = BLOCK_SIZE(cur);
             }
             MARK_USED(cur);
 
             free_bytes -= needed;
             if (free_bytes < min_free) min_free = free_bytes;
             return (uint8_t*)cur + HDR_SIZE;
         }
         prev = cur;
         cur  = cur->next;
     }
     return NULL;
 }
 
 void e_free(void *ptr) {
     if (!ptr || !initialized) return;
 
     uint8_t *p = (uint8_t*)ptr;
     if (p < heap_base + HDR_SIZE || p >= heap_end) {
         mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("e_free: invalid ptr"));
         return;
     }
     if (((uintptr_t)p - HDR_SIZE) & ALIGN_MASK) {
         mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("e_free: unaligned ptr"));
         return;
     }
 
     block_t *blk = (block_t*)(p - HDR_SIZE);
     if (!IS_USED(blk)) {
         mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("e_free: double free"));
         return;
     }
 
     size_t sz = BLOCK_SIZE(blk);
     if (sz == 0 || (uint8_t*)blk + sz > heap_end) {
         mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("e_free: bad size"));
         return;
     }
 
     MARK_FREE(blk);
     free_bytes += sz;
     insert_free(blk);
 }
 
 void* e_realloc(void *ptr, size_t new_size) {
     if (!ptr) return e_malloc(new_size);
     if (new_size == 0) { e_free(ptr); return NULL; }
     if (!initialized) return NULL;
 
     uint8_t *p = (uint8_t*)ptr;
     if (p < heap_base + HDR_SIZE || p >= heap_end) {
         mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("e_realloc: invalid ptr"));
         return NULL;
     }
 
     block_t *blk = (block_t*)(p - HDR_SIZE);
     if (!IS_USED(blk)) {
         mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("e_realloc: block not used"));
         return NULL;
     }
 
     size_t curr = BLOCK_SIZE(blk) - HDR_SIZE;
     if (new_size <= curr) return ptr;
 
     /* try expand into next free block */
     uint8_t *next_addr = (uint8_t*)blk + BLOCK_SIZE(blk);
     if (next_addr + HDR_SIZE <= heap_end) {
         block_t *next = (block_t*)next_addr;
         if (is_valid_block(next) && !IS_USED(next)) {
             size_t combined = BLOCK_SIZE(blk) + BLOCK_SIZE(next);
             size_t need = ALIGN_UP(new_size + HDR_SIZE);
             if (combined >= need) {
                 /* remove next from free list */
                 block_t *prev = &start_node;
                 while (prev->next != next && prev->next != end_node) {
                     prev = prev->next;
                 }
                 if (prev->next == next) {
                     prev->next = next->next;
                     /* compute new free_bytes: remove next size */
                     free_bytes -= BLOCK_SIZE(next);
 
                     /* update blk size */
                     blk->size = (blk->size & USED_MASK) | need;
                     size_t leftover = combined - need;
                     if (leftover >= MIN_SPLIT) {
                         block_t *split = (block_t*)((uint8_t*)blk + need);
                         split->size = leftover;
                         MARK_FREE(split);
                         insert_free(split);
                     } else {
                         /* absorb all */
                         blk->size = (blk->size & USED_MASK) | combined;
                     }
                     if (free_bytes < min_free) min_free = free_bytes;
                     return ptr;
                 }
             }
         }
     }
 
     /* fallback: alloc-copy-free */
     void *nptr = e_malloc(new_size);
     if (nptr) {
         memcpy(nptr, ptr, curr);
         e_free(ptr);
     }
     return nptr;
 }
 
 size_t e_heap_free(void)     { return free_bytes; }
 size_t e_heap_min_free(void) { return min_free;   }
 
 int e_heap_fragmentation(void) {
     if (!initialized || free_bytes == 0) return 0;
     size_t largest = 0;
     for (block_t *b = start_node.next; b != end_node; b = b->next) {
         if (!IS_USED(b) && BLOCK_SIZE(b) > largest) {
             largest = BLOCK_SIZE(b);
         }
     }
     if (largest == 0) return 100;
     return (int)(100 - (largest * 100) / free_bytes);
 }
 
 bool e_heap_validate(void) {
     if (!initialized) return false;
     size_t counted = 0;
     for (block_t *b = start_node.next; b != end_node; b = b->next) {
         if (!is_valid_block(b)) return false;
         if ((uint8_t*)b + BLOCK_SIZE(b) > heap_end) return false;
         if (!IS_USED(b)) {
             counted += BLOCK_SIZE(b);
             /* ensure no adjacent free blocks */
             block_t *n = b->next;
             if (n != end_node && !IS_USED(n) &&
                 (uint8_t*)b + BLOCK_SIZE(b) == (uint8_t*)n) {
                 return false;
             }
         }
     }
     return (counted == free_bytes);
 }
 