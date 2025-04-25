
#ifndef EIGENMATH_H
#define EIGENMATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


#define STRBUFLEN 1000
#define BUCKETSIZE 100
#define MAXDIM 24

//extern struct atom *mem ;
//extern struct atom **stack ; // 
//extern struct atom **symtab ; // symbol table
//extern struct atom **binding ;
//extern struct atom **usrfunc ;
//extern char *strbuf ;

//extern uint32_t STACKSIZE ; // evaluation stack
//extern uint32_t MAXATOMS ; // 10,240 atoms



extern void eigenmath_init(uint8_t *pHeap,size_t heapSize);
extern void run(char *buf);
#ifdef __cplusplus
}
#endif

#endif /* EHEAP_H */