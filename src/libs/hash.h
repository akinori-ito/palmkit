#ifndef SLM_HASH_H
#define SLM_HASH_H

/*
 * A Hash Table using closed hash technique.
 *
 * by A. Ito
 * August 30, 2000
 */
#include "libs.h"
#include <string.h>

typedef struct {
    void *keyptr;
    void *valueptr;
} SLMHashTableElement;

typedef u_ptr_int (*SLMHashFuncT)(void*);
typedef int (*SLMCompFuncT)(void*,void*);

typedef struct {
    int size;
    int nelem;
    short key_incr;
    SLMHashFuncT hashfunc;
    SLMCompFuncT compar;
    SLMHashTableElement *body;
} SLMHashTable;

SLMHashTable *SLMHashCreate(int size, SLMHashFuncT hashfunc, SLMCompFuncT comp);
void SLMHashDestroy(SLMHashTable *ht);
SLMHashTableElement *SLMHashSearch(SLMHashTable *hash, void *key);
void SLMHashInsert(SLMHashTable *hash, void *key, void *value);

u_ptr_int SLMHash4String(void*);

#define SLMHashCreateSI(size) SLMHashCreate(size,SLMHash4String,(SLMCompFuncT)strcmp)

ptr_int SLMIntHashSearch(SLMHashTable *ht, char *key);

#define SLMIntHashInsert(ht, key,value) SLMHashInsert(ht,key,(void*)(ptr_int)value)

#endif
