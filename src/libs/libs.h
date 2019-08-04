#ifndef LIBS_H
#define LIBS_H
#include "intsize.h"

#ifdef DEBUG_MALLOC
#include <gc.h>
#define Malloc(size) GC_debug_malloc(size,__FILE__,__LINE__)
#define Free(ptr)   GC_debug_free(ptr)
#define Realloc(ptr,size) GC_debug_realloc(ptr,size,__FILE__,__LINE__)
#else
void* SLM_my_malloc(size_t,char*,int);
void* SLM_my_realloc(void*,size_t,char*,int);
void SLM_my_free(void*,char*,int);
#define Malloc(size) SLM_my_malloc(size,__FILE__,__LINE__)
#define Realloc(ptr,size) SLM_my_realloc(ptr,size,__FILE__,__LINE__)
#define Free(size) SLM_my_free(size,__FILE__,__LINE__)
#endif

#define New(type)	((type*)Malloc(sizeof(type)))
#define NewAtom(type)	((type*)Malloc(sizeof(type)))
#define New_N(type,n)	((type*)Malloc((n)*sizeof(type)))
#define NewAtom_N(type,n)	((type*)Malloc((n)*sizeof(type)))
#define New_Reuse(type,ptr,n)   ((type*)Realloc((ptr),(n)*sizeof(type)))

#define GC_strdup(s) strdup(s)

#endif
