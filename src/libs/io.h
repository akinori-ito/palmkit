#ifndef SLM_IO_H
#define SLM_IO_H

#include <stdio.h>
#include "intsize.h"

#ifdef HAVE_ZLIB
#include <zlib.h>

#define FH_UNGETC_BUFSIZE 16
typedef struct filehandle {
    gzFile f;
    char ungetc_buf[FH_UNGETC_BUFSIZE];
    unsigned int ugbuf_ptr;
} *FILEHANDLE;

FILEHANDLE z_open(const char* file, const char* mode);
void z_close(FILEHANDLE gzf);
#define z_write(ptr,elsize,nelem,stream) gzwrite((stream)->f,ptr,(elsize)*(nelem))
#define z_read(ptr,elsize,nelem,stream) gzwrite((stream)->f,ptr,(elsize)*(nelem))
#define z_eof(gzf) gzeof((gzf)->f)
#define z_gets(ptr, len, stream)  gzgets((stream)->f,ptr,len)
#define z_getc(gzf) (((gzf)->ugbuf_ptr > 0)?(gzf)->ungetc_buf[--(gzf)->ugbuf_ptr]:gzgetc((gzf)->f))
#define z_ungetc(c,f) (((f)->ugbuf_ptr<FH_UNGETC_BUFSIZE)?(f)->ungetc_buf[(f)->ugbuf_ptr++] = (c):(c))

#else
typedef struct filehandle {
    unsigned flag;
    FILE *f;
} *FILEHANDLE;

#define FH_FILE 0
#define FH_PIPE 1

#define FH_READ     0
#define FH_WRITE    2
#define FH_APPEND   4

#define FH_OPEN     8

FILEHANDLE z_open(char *file,char *mode);
void z_close(FILEHANDLE fh);
#define z_write(ptr,elsize,nelem,stream) fwrite(ptr,elsize,nelem,(stream)->f)
#define z_read(ptr,elsize,nelem,stream) fread(ptr,elsize,nelem,(stream)->f)
#define z_getc(stream) fgetc((stream)->f)
#define z_ungetc(c,stream) ungetc(c,(stream)->f)
#define z_eof(stream) feof((stream)->f)
#define z_gets(ptr, len, stream)  fgets(ptr,len,(stream)->f)

#endif /* not HAVE_ZLIB */

/* common subroutines */

FILEHANDLE FILEIO_stdin();
FILEHANDLE FILEIO_stdout();
FILEHANDLE FILEIO_stderr();
void z_printf(FILEHANDLE f, char *fmt,...);
int z_getint(FILEHANDLE f, int *iptr);
int z_getlong(FILEHANDLE f, int4 *iptr);
int z_getulong(FILEHANDLE f, uint4 *iptr);
int z_getushort(FILEHANDLE f, unsigned short *iptr);
int z_getfloat(FILEHANDLE f, float *iptr);
int z_getdouble(FILEHANDLE f, double *iptr);
int z_getstr(FILEHANDLE f, char *ptr, int limit);

#endif
