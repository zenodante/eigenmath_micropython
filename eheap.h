/*========================================================================
 *  eheap.h – Minimal standalone heap manager (Freertos heap_4 style)
 *  ---------------------------------------------------------------------
 *  API:
 *      void   eheap_init(void *buffer, size_t size);
 *      void*  e_malloc(size_t bytes);
 *      void   e_free(void *ptr);
 *      void*  e_realloc(void *ptr, size_t new_size);
 *      size_t e_heap_free(void);
 *      size_t e_heap_min_free(void);
 *======================================================================*/

 #ifndef EHEAP_H
 #define EHEAP_H
 
 #include <stddef.h>
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 void   eheap_init(void *buffer, size_t size);
 void*  e_malloc(size_t size);
 void   e_free(void *ptr);
 void*  e_realloc(void *ptr, size_t new_size);
 size_t e_heap_free(void);
 size_t e_heap_min_free(void);
 int e_heap_fragmentation(void);
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* EHEAP_H */