/*
 * initehmm.c: Initialize Ergodic HMM
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "slm.h"
#include "vocab.h"
#include "misc.h"
#include "libs.h"
#include "ehmm.h"

SLMEHMMState *State;
int *Wfreq;

void usage()
{
    fprintf(stderr,"usage: initehmm [options] -vocab .vocab -n #_of_state [outfile]\n");
    fprintf(stderr,"options:\n");
    fprintf(stderr,"\t-wfreq file.wfreq\n");
    exit(1);
}

int 
read_wfreq(char *wfreqfile, SLMHashTable *vocab_ht)
{
    FILEHANDLE f;
    char word[256];
    int freq,id;
    int i,total = 0;

    Wfreq = New_N(int,vocab_ht->nelem+1);
    for (i = 0; i <= vocab_ht->nelem; i++)
	Wfreq[i] = 0;
    f = z_open(wfreqfile,"r");
    while (z_getstr(f,word,256) == 0) {
	if (z_getint(f,&freq) < 0)
	    break;
	id = SLMIntHashSearch(vocab_ht, word);
	Wfreq[id] += freq;
	total += freq;
    }
    z_close(f);
    return total;
}

double
nrand()
{
    double n = 0;
    int i;
    for (i = 0; i < 6; i++)
	n += drand48();
    return n/6+0.5;
}

void
generate_ehmm(FILEHANDLE f, int n_state, int n_vocab, int total)
{
    int i,j;
    double r,s;
    SLMEHMMState *st = State;

    for (i = 0; i < n_state; i++,st++) {
	s = 0;
	for (j = 0; j < n_state; j++) {
	    r = drand48();
	    st->trans_probs[j] = r;
	    s += r;
	}
	for (j = 0; j < n_state; j++) {
	    st->trans_probs[j] /= s;
	}
	s = 0;
	for (j = 0; j < n_vocab; j++) {
	    if (total)
		r = Wfreq[j]*nrand();
	    else
		r = drand48();
	    s += r;
	    st->emit_probs[j] = r;
	}
	for (j = 0; j < n_vocab; j++) {
	    st->emit_probs[j] /= s;
	}
    }
    SLMDumpEHMM(f,State,n_state,n_vocab);
}

int
main(int argc, char *argv[])
{
    SLMHashTable *vocab_ht = NULL;
    int n_state = 0;
    int i,total = 0;
    FILEHANDLE f = FILEIO_stdout();
    char *wfreqfile = NULL;

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i],"-vocab"))
	    vocab_ht = read_vocab(nextarg(argc,argv,i++),NULL,0);
	else if (!strcmp(argv[i],"-n"))
	    n_state = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-wfreq"))
	    wfreqfile = nextarg(argc,argv,i++);
	else if (argv[i][0] == '-')
	    usage();
	else
	    f = z_open(argv[i],"w");
    }
    if (vocab_ht == NULL) {
	fprintf(stderr,"Error: no vocab file specified\n");
	usage();
    }
    if (n_state == 0) {
	fprintf(stderr,"Error: number of state unspecified\n");
	usage();
    }
    State = SLMAllocEHMMState(n_state,vocab_ht->nelem+1);
    if (wfreqfile)
	total = read_wfreq(wfreqfile,vocab_ht);
    srand48(time(0));
    generate_ehmm(f,n_state,vocab_ht->nelem+1,total);
    z_close(f);
    return 0;
}
    
