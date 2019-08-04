#ifdef ENABLE_LARGEFILE
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#endif

#include "io.h"
#include "libs.h"
#include "misc.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define IS_SPACE(x)  (isspace((int)(x))&&((x)&0x80)==0)

#ifdef HAVE_ZLIB

FILEHANDLE z_open(const char *file, const char* mode)
{
    FILEHANDLE f = New(struct filehandle);
    f->f = gzopen(file,mode);
    f->ugbuf_ptr = 0;
    return f;
}

void z_close(FILEHANDLE f)
{
    gzclose(f->f);
    Free(f);
}

void z_printf(FILEHANDLE f, char *fmt,...)
{
    va_list *ap;
    va_start(ap,fmt);
    gzprintf(f->f,fmt,ap);
    va_end(ap);
}


FILEHANDLE FILEIO_stdin()
{
    FILEHANDLE h = New(struct filehandle);
    h->f = gzdopen(fileno(stdin),"r");
    h->ugbuf_ptr = 0;
    return h;
}

FILEHANDLE FILEIO_stdout()
{
    FILEHANDLE h = New(struct filehandle);
    h->f = gzdopen(fileno(stdout),"w");
    h->ugbuf_ptr = 0;
    return h;
}

FILEHANDLE FILEIO_stderr()
{
    FILEHANDLE h = New(struct filehandle);
    h->f = gzdopen(fileno(stdout),"w");
    h->ugbuf_ptr = 0;
    return h;
}

#else /* not HAVE_ZLIB */
static char tmpbuf[256];

FILEHANDLE z_open(char *file, char *mode)
{
    int len = strlen(file);
    int i;
    FILEHANDLE fh = New(struct filehandle);
    char buf[1024];

    fh->flag = 0;
    if (len > 4 && strcmp(file+len-3,".gz") == 0) {
	fh->flag |= FH_PIPE;
	switch (*mode) {
	case 'r':
	    sprintf(tmpbuf,"gzip -dc %s",file);
	    fh->flag |= FH_READ;
	    break;
	case 'w':
	    sprintf(tmpbuf,"gzip > %s",file);
	    fh->flag |= FH_WRITE;
	    break;
	case 'a':
	    sprintf(tmpbuf,"gzip >> %s",file);
	    fh->flag |= FH_APPEND;
	    break;
	default:
	    fprintf(stderr,"z_open ERROR: %s: unknown mode\n",mode);
	    exit(1);
	}

	fh->f = popen(tmpbuf,mode);
	if (fh->f == NULL)
	    goto error;
	fh->flag |= FH_OPEN;
	return fh;
    }
    for (i = len-1; i >= 0; i--)
	if (!IS_SPACE(file[i]))
	    break;
    if (file[i] == '|') {
	/* PIPE */
	fh->flag |= FH_PIPE;
	strcpy(buf,file);
	buf[i] = '\0';
	fh->f = popen(buf,mode);
    }
    else {
	fh->flag |= FH_FILE;
	fh->f = fopen(file,mode);
    }
    fh->flag |= FH_OPEN;
    switch (*mode) {
    case 'r':
	fh->flag |= FH_READ;
	break;
    case 'w':
	fh->flag |= FH_WRITE;
	break;
    case 'a':
	fh->flag |= FH_APPEND;
	break;
    default:
	fprintf(stderr,"z_open ERROR: %s: unknown mode\n",mode);
	exit(1);
    }
    if (fh->f == NULL)
	goto error;
    return fh;
 error:
    fprintf(stderr,"z_open ERROR: Can't open %s for %s\n",file,mode);
    exit(1);
}

void z_close(FILEHANDLE fh)
{
    if (!(fh->flag & FH_OPEN))
	return;
    if (fh->flag & FH_PIPE) 
	pclose(fh->f);
    else
	fclose(fh->f);
    Free(fh);
}

FILEHANDLE FILEIO_stdin()
{
    FILEHANDLE h = New(struct filehandle);
    h->flag = FH_FILE | FH_READ;
    h->f = stdin;
    return h;
}

FILEHANDLE FILEIO_stdout()
{
    FILEHANDLE h = New(struct filehandle);
    h->flag = FH_FILE | FH_WRITE;
    h->f = stdout;
    return h;
}

FILEHANDLE FILEIO_stderr()
{
    FILEHANDLE h = New(struct filehandle);
    h->flag = FH_FILE | FH_WRITE;
    h->f = stdout;
    return h;
}

void z_printf(FILEHANDLE f, char *fmt,...)
{
    va_list ap;
    va_start(ap,fmt);
    vfprintf(f->f,fmt,ap);
    va_end(ap);
}

#endif /* not HAVE_ZLIB */

#define BUFSIZE 1024
static char geti_buf[BUFSIZE];

#define ST_REJECT 255
#define ST_SKIP   128
#include "int_autom.c"
#include "float_autom.c"

/* common subroutines */
static int z_getbuf(FILEHANDLE f, unsigned char **autom)
{
    int i = 0;
    int c;
    int status = 0;
    unsigned char *a;

    while (1) {
	c = z_getc(f);
	if (c == EOF || z_eof(f) || c < 0) {
	    if (i > 0) {
		geti_buf[i] = '\0';
		return 0;
	    }
	    return EOF;
	}
	a = autom[status];
	if (a[c] == ST_REJECT) {
	    geti_buf[i] = '\0';
	    z_ungetc(c,f);
	    return 0;
	}
	else {
	    if ((a[c] & ST_SKIP) == 0) {
		geti_buf[i++] = c;
		status = a[c];
	    }
	    else
		status = (a[c] & ~ST_SKIP);
	}
    }
}

int z_getint(FILEHANDLE f, int *iptr)
{
    int r;
    if ((r = z_getbuf(f, int_autom)) != 0) 
	return r;
    *iptr = atoi(geti_buf);
    return 0;
}

int z_getlong(FILEHANDLE f, int4 *iptr)
{
    int r;
    if ((r = z_getbuf(f, int_autom)) != 0) 
	return r;
    *iptr = atol(geti_buf);
    return 0;
}

int z_getulong(FILEHANDLE f, uint4 *iptr)
{
    int r;
    if ((r = z_getbuf(f, int_autom)) != 0) 
	return r;
    *iptr = strtoul(geti_buf,NULL,10);
    return 0;
}

int z_getushort(FILEHANDLE f, unsigned short *iptr)
{
    int r;
    if ((r = z_getbuf(f, int_autom)) != 0)
	return r;
    *iptr = (unsigned short)atoi(geti_buf);
    return 0;
}

int z_getfloat(FILEHANDLE f, float *fptr)
{
    int r;
    if ((r = z_getbuf(f, float_autom)) != 0) 
	return r;
    *fptr = atof(geti_buf);
    return 0;
}

int z_getdouble(FILEHANDLE f, double *fptr)
{
    int r;
    if ((r = z_getbuf(f, float_autom)) != 0) 
	return r;
    *fptr = atof(geti_buf);
    return 0;
}

int z_getstr(FILEHANDLE f, char *buf, int limit)
{
    int i = 0;
    char c;

    if (limit == 0)
	return EOF;
    /* skip spaces */
    while (1) {
	c = z_getc(f);
	if (!IS_SPACE(c)) {
	    if (!z_eof(f))
		buf[i++] = c;
	    break;
	}
    }
    while (1) {
	c = z_getc(f);
	if (c == EOF || z_eof(f)) {
	    if (i > 0) {
		buf[i] = '\0';
		return 0;
	    }
	    return EOF;
	}
	if (!IS_SPACE(c)) {
	    buf[i++] = c;
	    if (i == limit-1) {
		/* buffer overflow */
		buf[i] = '\0';
		return 0;
	    }
	}
	else {
	    buf[i] = '\0';
	    z_ungetc(c,f);
	    return 0;
	}
    }
}
