#ifndef SLM_MISC_H
#define SLM_MISC_H
#include "io.h"
#include "intsize.h"

char *nextarg(int argc, char *argv[], int i);
int4 read_long(FILEHANDLE f, int ascii_input);
uint4 read_ulong(FILEHANDLE f, int ascii_input);
uint2 read_ushort(FILEHANDLE f, int ascii_input);
void write_long(FILEHANDLE f, int4 x, int ascii_output);
void write_ushort(FILEHANDLE f, uint2 x, int ascii_output);
void *SLM_my_malloc(size_t size, char *file, int line);
void *SLM_my_realloc(void *ptr, size_t size, char *file, int line);
void SLM_my_free(void *ptr, char *file, int line);

#ifdef ENABLE_LONGID
#define read_WordID(f,a) read_ulong(f,a)
#define write_WordID(f,x,a) write_long(f,x,a)
#define z_getWordID(f,x) z_getulong(f,x)
#else
#define read_WordID(f,a) read_ushort(f,a)
#define write_WordID(f,x,a) write_ushort(f,x,a)
#define z_getWordID(f,x) z_getushort(f,x)
#endif

#endif
