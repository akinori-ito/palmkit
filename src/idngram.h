#include <stdlib.h>
#include <unistd.h>
#include "libs.h"
#include "io.h"
#include "misc.h"
#include "vocab.h"
#include "ngram.h"


extern char **merge_list;
extern int merge_list_num;
extern int merge_list_size;

extern int hash_size;
extern char *tmpdir;

typedef struct {
    int ngram_len;
    SLMWordID *buffer;
    int size;
    int real_size;
    int pos;
    int elem_size;
    int elem_byte;
} SLMIDNgramBuffer;

extern int ngram_len;
extern int verbosity;

#define TFTREE_FILE 0
#define TFTREE_PARENT 1

u_ptr_int ngram_hashfunc(void *ngptr);
int ngram_comp(void *p1,void *p2);

void push_merge_list(char *tmpfile);
void dumpidngram(SLMIDNgramBuffer *buffer, FILEHANDLE outf, int ascii_out);
void incr_ngram(SLMIDNgramBuffer *buffer, SLMWordID *ptr, uint4 incr);
