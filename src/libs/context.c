#include "misc.h"
#include "context.h"

static u_ptr_int
ccs_hashfunc(void *key)
{
    return (u_ptr_int)key;
}

static int
ccs_compare(void *x1, void *x2)
{
    return (ptr_int)x1 - (ptr_int)x2;
}

SLMHashTable *
readContextCues(SLMNgram *ng, char *file)
{
    SLMHashTable *ht;
    FILEHANDLE f;
    char buf[256];
    int id;

    ht = SLMHashCreate(10,ccs_hashfunc,ccs_compare);
    f = z_open(file,"r");
    while (z_getstr(f,buf,256) == 0) {
	id = SLMWord2ID(ng,buf);
	SLMHashInsert(ht,(void*)(ptr_int)id,(void*)(ptr_int)1);
    }
    z_close(f);
    return ht;
}
