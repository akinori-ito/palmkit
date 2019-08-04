#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "misc.h"

char *nextarg(int argc, char *argv[], int i)
{
    if (i+1 >= argc) {
	fprintf(stderr,"ERROR: can't get arg next to %s\n",argv[i]);
	exit(1);
    }
    return argv[i+1];
}

int4
read_long(FILEHANDLE f, int ascii_input)
{
    int4 x;
    if (ascii_input) {
	if (z_getlong(f,&x) != 0)
	    return -1;
	return x;
    }
    else {
	if (z_read(&x,sizeof(int4),1,f) != 1)
	    return -1;
	return ntohl(x);
    }
}

uint4
read_ulong(FILEHANDLE f, int ascii_input)
{
    uint4 x;
    if (ascii_input) {
	if (z_getulong(f,&x) != 0)
	    return 0xffffffff;
	return x;
    }
    else {
	if (z_read(&x,sizeof(uint4),1,f) != 1)
	    return 0xffffffff;
	return ntohl(x);
    }
}

uint2
read_ushort(FILEHANDLE f, int ascii_input)
{
    uint2 x;
    if (ascii_input) {
	if (z_getushort(f,&x) != 0)
	    return 0xffff;
	return x;
    }
    else {
	if (z_read(&x,sizeof(uint2),1,f) != 1)
	    return 0xffff;
	return ntohs(x);
    }
}

void
write_long(FILEHANDLE f, int4 x, int ascii_output)
{
    int4 xx;
    if (ascii_output) {
	z_printf(f,"%ld ",x);
    }
    else {
	xx = htonl(x);
	z_write(&xx,sizeof(int4),1,f);
    }
}

void
write_ushort(FILEHANDLE f, uint2 x, int ascii_output)
{
    uint2 xx;
    if (ascii_output) {
	z_printf(f,"%hu ",x);
    }
    else {
	xx = htons(x);
	z_write(&xx,sizeof(uint2),1,f);
    }
}

#ifdef MY_MALLOC_DEBUG
#define ADDITIONAL_MEM sizeof(unsigned long)
static unsigned long totalmem = 0;
#else
#define ADDITIONAL_MEM 0
#endif

void *
SLM_my_malloc(size_t size, char *file, int line)
{
  void *p = malloc(size+ADDITIONAL_MEM);
  if (p == NULL) {
    fprintf(stderr,"malloc: no more memory at file %s, line %d (size %d)\n",
	    file,line,size);
#ifdef MY_MALLOC_DEBUG
    fprintf(stderr,"\tallocated memory = %lu\n",totalmem);
#endif
    exit(1);
  }
#ifdef MY_MALLOC_DEBUG
  totalmem += size;
  *(unsigned long*)p = size;
#endif
#ifdef DDEBUG
  fprintf(stderr,"allocating %d bytes; file %s, line %d; total mem=%d\n",size,file, line, totalmem);
#endif
  return p+ADDITIONAL_MEM;
}

void *
SLM_my_realloc(void *ptr, size_t size, char *file, int line)
{
#ifdef MY_MALLOC_DEBUG
  unsigned long osize;
#endif
  void *p = realloc(ptr,size+ADDITIONAL_MEM);
#ifdef MY_MALLOC_DEBUG
  ptr -= ADDITIONAL_MEM;
  osize = *(long*)ptr;
#endif
  if (p == NULL) {
    fprintf(stderr,"realloc: no more memory at file %s, line %d\n",
	    file,line);
#ifdef MY_MALLOC_DEBUG
    fprintf(stderr,"allocated memory = %lu\n",totalmem);
#endif
    exit(1);
  }
#ifdef MY_MALLOC_DEBUG
  totalmem += size-osize;
  *(unsigned long*)p = size;
#endif
  return p+ADDITIONAL_MEM;
}

void
SLM_my_free(void *ptr, char *file, int line)
{
#ifdef MY_MALLOC_DEBUG
  unsigned long size;
  if (ptr == NULL)
    return;
  ptr -= ADDITIONAL_MEM;
  size = *(unsigned long*)ptr;
  if (size == 0 || size > 0x80000000) {
    /* curious */
    fprintf(stderr,"free: curious size (%lu) at file %s, line %d\n",
	    size,file,line);
  }
  totalmem -= size;
#endif
  free(ptr);
}
