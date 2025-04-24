/*==========================================================================
 *  eigenmem.c  –  Dual‑ended bump allocator implementation
 *==========================================================================*/

 #include "eigenmem.h"
 #include "py/runtime.h"   // mp_raise_msg, MP_ERROR_TEXT
 
 /* Helper to align allocations to 4‑byte boundary */
 #define EM_ALIGN4(n)   (((n) + 3U) & ~3U)
 
 /* -------------------------------------------------------------------------
  *  Initialisation / teardown
  * -------------------------------------------------------------------------*/
 
 int eigen_heap_init(eigen_heap_t *h, size_t bytes)
 {
     h->arena = m_malloc(bytes);            /* allocate arena inside MicroPython GC heap */
     if (!h->arena) {
         return -1;                         /* Out‑of‑memory */
     }
 
     h->arena_size = bytes;
     h->perm_top   = 0;                     /* perm grows upward  */
     h->tmp_top    = bytes;                 /* tmp  grows downward*/
     return 0;
 }
 
 void eigen_heap_deinit(eigen_heap_t *h)
 {
     if (h->arena) {
         m_free(h->arena);
         h->arena      = NULL;
         h->arena_size = h->perm_top = h->tmp_top = 0;
     }
 }
 
 /* -------------------------------------------------------------------------
  *  Internal helper for uniform OOM handling
  * -------------------------------------------------------------------------*/
 static inline void eigenmem_oom(const char *msg)
 {
     mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT(msg));
 }
 
 /* -------------------------------------------------------------------------
  *  Allocation – persistent segment (symbol table, constants)
  * -------------------------------------------------------------------------*/
 void *em_alloc_perm(eigen_heap_t *h, size_t nbytes)
 {
     nbytes = EM_ALIGN4(nbytes);
 
     /* Collision check */
     if (h->perm_top + nbytes > h->tmp_top) {
         eigenmem_oom("perm heap full");
         return NULL;
     }
 
     void *p = h->arena + h->perm_top;
     h->perm_top += nbytes;
     return p;
 }
 
 /* -------------------------------------------------------------------------
  *  Allocation – temporary segment (AST, runtime stack)
  * -------------------------------------------------------------------------*/
 void *em_alloc_tmp(eigen_heap_t *h, size_t nbytes)
 {
     nbytes = EM_ALIGN4(nbytes);
 
     if (nbytes > h->tmp_top - h->perm_top) {
         eigenmem_oom("tmp heap full");
         return NULL;
     }
 
     h->tmp_top -= nbytes;
     return h->arena + h->tmp_top;
 }
 