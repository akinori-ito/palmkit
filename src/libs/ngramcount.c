#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "libs.h"
#include "io.h"
#include "misc.h"
#include "hash.h"
#include "vocab.h"
#include "ngram.h"

#define ST_NORMAL  0
#define ST_MIN     1
#define ST_EOF     2

SLMWordTuple SLMDupTuple(SLMWordTuple t, int len)
{
    SLMWordTuple d = SLMNewWordTuple(len);
    int i;
    for (i = 0; i < len; i++) {
	d[i] = t[i];
    }
    return d;
}

SLMNgramCount*
SLMNewNgramCount(int len)
{
    SLMNgramCount *nc;
    nc = New(SLMNgramCount);
    nc->word_id = New_N(SLMWordID,len);
    return nc;
}

void
SLMFreeNgramCount(SLMNgramCount *nc)
{
    Free(nc->word_id);
    Free(nc);
}

SLMNgramCount*
SLMNewNgramCount_N(int n, int len)
{
    SLMNgramCount *nc;
    SLMWordID *wid;
    int i;

    nc = New_N(SLMNgramCount,n);
    wid = New_N(SLMWordID,len*n);
    for (i = 0; i < n; i++) {
        nc[i].word_id = &wid[i*len];
    }
    return nc;
}

void
SLMFreeNgramCount_N(SLMNgramCount *nc, int n)
{
    Free(nc[0].word_id);
    Free(nc);
}

SLMNgramCount *
SLMReadNgramCount(int ngram_len, FILEHANDLE inf,
		  SLMNgramCount *base,int ascii_in)
{
    int i;
    SLMWordID w;
    int4 c;

    if (base == NULL) {
	base = SLMNewNgramCount(ngram_len);
    }
    if (ascii_in) {
	for (i = 0; i < ngram_len; i++) {
	    if (z_getWordID(inf,&base->word_id[i]) != 0)
		return NULL;
	}
	if (z_getulong(inf,&base->count) != 0)
	    return NULL;
    }
    else {
	for (i = 0; i < ngram_len; i++) {
	    if (z_read(&w,sizeof(SLMWordID),1,inf) != 1)
		return NULL;
	    base->word_id[i] = SLMntohID(w);
	}
	if (z_read(&c,sizeof(int4),1,inf) != 1)
	    return NULL;
	base->count = ntohl(c);
    }
    return base;
}

int
SLMCompareNgramCount(int ngram_len, SLMNgramCount *p1, SLMNgramCount *p2)
{
    int d,i;
    for (i = 0; i < ngram_len; i++) {
	d = p1->word_id[i]-p2->word_id[i];
	if (d != 0)
	    return d;
    }
    return 0;
} 

static int
check_merge(int ngram_len, int n_ng, SLMNgramCount **ngs, char *status)
{
  int min_id = -1;
  int d,i,j;

  for (i = 0; i < n_ng; i++) {
    if (status[i] & ST_EOF)
      continue;
    if (min_id < 0) {
      min_id = i;
      status[i] |= ST_MIN;
      continue;
    }
    d = SLMCompareNgramCount(ngram_len,ngs[min_id],ngs[i]);
    if (d > 0) {
      /* ngs[i] is smaller than ngs[min_id] */
      for (j = 0; j < i; j++)
	status[j] &= ~ST_MIN;
      min_id = i;
      status[i] |= ST_MIN;
    }
    else if (d == 0)
      status[i] |= ST_MIN;
  }
  return min_id;
}

void
SLMPrintNgramCount(int ngram_len,SLMNgramCount *ngc, FILEHANDLE outf,int ascii_out)
{
    int j;
    if (ascii_out) {
	for (j = 0; j < ngram_len; j++)
	    z_printf(outf,SLMWordID_FMT " ",ngc->word_id[j]);
	z_printf(outf,"%lu\n",ngc->count);
    }
    else {
	SLMWordID id;
	int4 c;
	for (j = 0; j < ngram_len; j++) {
	    id = SLMhtonID(ngc->word_id[j]);
	    z_write(&id,sizeof(SLMWordID),1,outf);
	}
	c = htonl(ngc->count);
	z_write(&c,sizeof(int4),1,outf);
    }
}

void
SLMMergeIDNgram(int ngram_len, char **list, int nlist, FILEHANDLE outf, int ascii_in, int ascii_out)
{
    double *weights = NewAtom_N(double,nlist);
    int i;
    for (i = 0; i < nlist; i++)
	weights[i] = 1.0;
    SLMMixIDNgram(ngram_len,list,weights,nlist,outf,ascii_in,ascii_out);
    Free(weights);
}

void
SLMMixIDNgram(int ngram_len, char **list, double *weight,int nlist,
	      FILEHANDLE outf, int ascii_in, int ascii_out)
{
    FILEHANDLE *fhs = New_N(FILEHANDLE, nlist);
    int i,j;
    char *status = New_N(char,nlist);
    SLMNgramCount **ngcount,nc;
    double newcount;
    
    /* initialize */
    ngcount = New_N(SLMNgramCount*,nlist);
    for (i = 0; i < nlist; i++) {
	status[i] = ST_MIN;
	ngcount[i] = SLMNewNgramCount(ngram_len);
	fhs[i] = z_open(list[i],"r");
    }

    /* main loop */
    for (;;) {
	for (i = 0; i < nlist; i++) {
	    if (status[i] & ST_EOF)
		continue;
	    if (status[i] & ST_MIN) {
		if (SLMReadNgramCount(ngram_len,fhs[i],ngcount[i],ascii_in) == NULL) {
		    status[i] |= ST_EOF;
		}
		status[i] &= ~ST_MIN;
	    }
	}
	if ((j = check_merge(ngram_len,nlist,ngcount,status)) < 0) {
	    /* all streams end */
	    for (i = 0; i < nlist; i++)
		z_close(fhs[i]);
	    break;
	}
	newcount = 0;
	for (i =0; i < nlist; i++) {
	    if (status[i] & ST_MIN) {
		nc.word_id = ngcount[i]->word_id;
		newcount += ngcount[i]->count*weight[i];
	    }
	}
	nc.count = ceil(newcount);
	if (nc.count < newcount) {
	    fprintf(stderr,"Warning: n-gram count overflow (%f -> %lu)\n",newcount,nc.count);
        }
	SLMPrintNgramCount(ngram_len,&nc,outf,ascii_out);
    }
    Free(fhs);
    Free(status);
    for (i = 0; i < nlist; i++) {
	Free(ngcount[i]);
    }
    Free(ngcount);
}


