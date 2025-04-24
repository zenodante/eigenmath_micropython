/*==========================================================================
 *  eigenmem.h  –  Dual‑ended bump allocator for EigenMath MicroPython port
 *  -----------------------------------------------------------------------
 *  Copyright (c) 2025  Your Name
 *  Licensed under MIT / 0BSD or your project license.
 *
 *  Overview
 *  --------
 *  The allocator carves a **single contiguous arena** inside the MicroPython
 *  GC heap and manages two growing segments:
 *
 *      perm_top   ↑                (long‑lived symbols, constants)
 *      ───────────┼─────────────── arena ───────────┼───   bytes
 *                  \                             /
 *                   \                           /
 *                    tmp_top  ↓  (ephemeral AST / stack)
 *
 *  Invariants: 0 ≤ perm_top ≤ tmp_top ≤ arena_size.
 *  `em_begin_run()` simply resets `tmp_top` to `arena_size`, instantly
 *  discarding all temporary allocations between runs while preserving the
 *  symbol table and other persistent objects.
 *==========================================================================*/

 #ifndef EIGENMEM_H
 #define EIGENMEM_H
 
 #include <stddef.h>
 #include <stdint.h>
 #include "py/obj.h"      /* m_malloc / m_free */
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /* ----------------------------------------------------------
  *  Heap descriptor
  * --------------------------------------------------------*/
 
 typedef struct {
     uint8_t *arena;      /* base pointer into MicroPython GC heap        */
     size_t   arena_size; /* total bytes in arena                         */
 
     size_t   perm_top;   /* offset of first free byte in perm segment    */
     size_t   tmp_top;    /* offset of first free byte below tmp segment  */
 } eigen_heap_t;
 
 /* ----------------------------------------------------------
  *  API
  * --------------------------------------------------------*/
 
 /* Initialise arena. Returns 0 on success, −1 on OOM. */
 int  eigen_heap_init   (eigen_heap_t *h, size_t bytes);
 
 /* Free arena memory – call from __del__. */
 void eigen_heap_deinit (eigen_heap_t *h);
 
 /* Allocate **persistent** memory (symbol table, constants). */
 void *em_alloc_perm (eigen_heap_t *h, size_t nbytes);
 
 /* Allocate **temporary** memory (AST nodes, runtime stack). */
 void *em_alloc_tmp  (eigen_heap_t *h, size_t nbytes);
 
 /* Discard all temporary allocations – call at start of each run(). */
 static inline void em_begin_run(eigen_heap_t *h) {
     h->tmp_top = h->arena_size;  /* snap back tmp pointer */
 }
 
 /* Hard reset: clear everything, ready to rebuild symbol table. */
 static inline void em_hard_reset(eigen_heap_t *h) {
     h->perm_top = 0;
     h->tmp_top  = h->arena_size;
 }
 
 /* Helpers --------------------------------------------------*/
 #define EM_ALIGN4(n) (((n) + 3U) & ~3U)
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* EIGENMEM_H */
 