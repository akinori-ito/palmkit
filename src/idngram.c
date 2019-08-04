#include "idngram.h"

char **merge_list;
int merge_list_num;
int merge_list_size;
int ngram_len = 3;

int verbosity;

u_ptr_int
ngram_hashfunc(void *ngptr)
{
    SLMWordTuple tuple = ngptr;
    int shift_bit = sizeof(unsigned int)*8/ngram_len;
    u_ptr_int h = 0;
    int i;

    for (i = 0; i < ngram_len; i++) {
	h <<= shift_bit;
	h += tuple[i];
    }
    return h;
}

int
ngram_comp(void *p1,void *p2)
{
    int d,i;
    SLMWordTuple t1,t2;
    t1 = (SLMWordTuple)p1;
    t2 = (SLMWordTuple)p2;
    for (i = 0; i < ngram_len; i++) {
	d = t1[i] - t2[i];
	if (d != 0)
	    return d;
    }
    return 0;
}

void
push_merge_list(char *tmpfile)
{
    int i,osize;
    char **olist;

    if (merge_list == NULL) {
	merge_list_size = 64;
	merge_list = New_N(char*,merge_list_size);
    }
    merge_list[merge_list_num++] = GC_strdup(tmpfile);
    if (merge_list_num == merge_list_size) {
	/* enlarge */
	osize = merge_list_size;
	merge_list_size *= 2;
	olist = merge_list;
	merge_list = New_N(char*,merge_list_size);
	for (i = 0; i < merge_list_num; i++)
	    merge_list[i] = olist[i];
	Free(olist);
    }
}


void
dumpidngram(SLMIDNgramBuffer *buffer, FILEHANDLE outf, int ascii_out)
{
    int i,j;
    SLMNgramCount nc;
    nc.word_id = New_N(SLMWordID,ngram_len);

    if (verbosity > 1) {
	fprintf(stderr,"\nSorting N-gram...");
	fflush(stderr);
    }
    qsort(buffer->buffer,
	  buffer->pos/buffer->elem_size,
	  buffer->elem_byte,
	  (int(*)(const void*,const void*))ngram_comp);
    if (verbosity > 1) {
	fprintf(stderr,"done.\n");
    }
    for (i = 0; i < buffer->pos; i += buffer->elem_size) {
	for (j = 0; j < buffer->ngram_len; j++) {
	    nc.word_id[j] = buffer->buffer[i+j];
	}
	nc.count = (buffer->buffer[i+buffer->elem_size-2]<<16)+
	    buffer->buffer[i+buffer->elem_size-1];
	SLMPrintNgramCount(buffer->ngram_len,&nc, outf, ascii_out);
    }
}

void
incr_ngram(SLMIDNgramBuffer *buffer, SLMWordID *ptr, uint4 incr)
{
    uint4 lower;
    lower = ptr[buffer->elem_size-1] + (incr & 0xffff);
    ptr[buffer->elem_size-1] = (lower & 0xffff);
    lower >>= 16;
    incr >>= 16;
    ptr[buffer->elem_size-2] += ((incr+lower) & 0xffff);
}
    
