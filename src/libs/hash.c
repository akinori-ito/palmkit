#include <stdio.h>
#include "misc.h"
#include "hash.h"
#include "io.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define OCCUPATION_RATIO_LIMIT 0.6

static int
next_prime(int n)
{
    int i,max;
    if (n%2 == 0) n++;
    max = sqrt((double)n);
 redo:
    for (i = 3; i < max; i += 2) {
	if (n%i == 0) {
	    n += 2;
	    goto redo;
	}
    }
    return n;
}

SLMHashTable *
SLMHashCreate(int size, SLMHashFuncT hashfunc, SLMCompFuncT compar)
{
    SLMHashTable *ht = New(SLMHashTable);
    ht->size = next_prime(size);
    ht->nelem = 0;
    ht->hashfunc = hashfunc;
    ht->compar = compar;
    ht->body = New_N(SLMHashTableElement, ht->size);
    memset(ht->body,0,sizeof(SLMHashTableElement)*ht->size);
    ht->key_incr = 7; /* lucky number */
    return ht;
}

void
SLMHashDestroy(SLMHashTable *ht)
{
    Free(ht->body);
    Free(ht);
}

#if 0
int debug_dump = 1;
#endif

SLMHashTableElement *
SLMHashSearch(SLMHashTable *ht, void *key)
{
    int h,h0;
    h = h0 = ht->hashfunc(key)%ht->size;
    do {
#if 0
if (debug_dump){
  printf("searching %s: ",key); fflush(stdout);
  printf("h=%d, ",h); fflush(stdout);
  printf("keyptr=%s\n",ht->body[h].keyptr); fflush(stdout);
}
#endif
	if (ht->body[h].keyptr == NULL) {
	    /* is empty; search failed */
	    return NULL;
	}
	else if (ht->compar(ht->body[h].keyptr,key) == 0) {
	    /* BINGO */
	    return &ht->body[h];
	}
	h = (h+ht->key_incr)%ht->size; /* lucky number */
    } while (h != h0);
    return NULL;
}

#ifdef DEBUG
static void
dumphash(SLMHashTable *ht, char *file)
{
    FILEHANDLE f = z_open(file,"w");
    int i;
    z_printf(f,"size = %d\n",ht->size);
    z_printf(f,"elem = %d\n",ht->nelem);
    for (i = 0; i < ht->size; i++) {
	if (ht->body[i].keyptr) {
	    z_printf(f,"%d: key=%s value=%d\n",i,
		    (char*)ht->body[i].keyptr,
		    (int)ht->body[i].valueptr);
	}
	else {
	    z_printf(f,"%d: key=NULL value=%d\n",i,
		    (int)ht->body[i].valueptr);
	}
    }
    z_close(f);
}

static int dumpcount = 0;
#endif

static void
rehash_if_needed(SLMHashTable *ht)
{
    SLMHashTableElement *oldbody;
    int oldsize,i;
    int old_nelem;
#ifdef DEBUG
    char buf[100];
#endif
    if ((double)ht->nelem/ht->size < OCCUPATION_RATIO_LIMIT)
	return;
#ifdef DEBUG
    sprintf(buf,"zzz.before.%d",dumpcount);
    dumphash(ht,buf);
#endif
    
    oldbody = ht->body;
    oldsize = ht->size;
    old_nelem = ht->nelem;
    ht->size = next_prime(oldsize*2);
    ht->body = New_N(SLMHashTableElement,ht->size);
    memset(ht->body,0,sizeof(SLMHashTableElement)*ht->size);
    ht->nelem = 0;
    for (i = 0; i < oldsize; i++) {
	if (oldbody[i].keyptr) {
	    SLMHashInsert(ht,oldbody[i].keyptr,oldbody[i].valueptr);
	}
    }
    Free(oldbody);
#ifdef DEBUG
    sprintf(buf,"zzz.after.%d",dumpcount);
    dumphash(ht,buf);
    dumpcount++;
#endif
}
    

void
SLMHashInsert(SLMHashTable *ht, void *key, void *value)
{
    int h,h0;
    h = h0 = ht->hashfunc(key)%ht->size;
    do {
	if (ht->body[h].keyptr == NULL) {
	    /* is empty; store there */
	    ht->body[h].keyptr = key;
	    ht->body[h].valueptr = value;
	    ht->nelem++;
#if 0
fprintf(stderr,"%s 0x%x %d %d\n",(char*)key,(unsigned)key,(int)value,h);
	    //if (strncmp(key,"<UNK>+44/16/6",12) == 0)
#endif
	    rehash_if_needed(ht);
	    return;
	}
	else if (ht->compar(ht->body[h].keyptr,key) == 0) {
	    /* replace value */
	    /* do not free the original ht->body[h].value; let GC do it. */
#if 1 //def DEBUG
	    fprintf(stderr,"Warning: (%s,%d) -> (%s,%d)\n",
		    (char*)ht->body[h].keyptr, (int)ht->body[h].valueptr,
		    (char*)key, (int)value);
#endif
	    ht->body[h].valueptr = value;
	    return;
	}
	h = (h+ht->key_incr)%ht->size; /* lucky number */
    } while (h != h0);
    /* not reached */
    fprintf(stderr,"Panic: SLMHashInsert() failed!\n");
    return;
}

u_ptr_int
SLMHash4String(void* x)
{
    unsigned char *s = (unsigned char*)x;
    u_ptr_int h,i;

    h = 0;
    for (i = 0; s[i] != '\0'; i++) {
	h = (h>>(sizeof(h)*8-6))+(h<<6);
	h += s[i];
    }
    return h;
}

ptr_int SLMIntHashSearch(SLMHashTable *ht, char *key)
{
    SLMHashTableElement *ht_elem;
    ht_elem = SLMHashSearch(ht,key);
    if (ht_elem == NULL) {
#if 0
/* begin debug */
	fprintf(stderr,"SLMIntHashSearch(\"%s\") = nil\n",key);
/* end */
#endif
	return 0;
    }
    return (ptr_int)ht_elem->valueptr;
}

