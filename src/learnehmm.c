/*
 * learnehmm.c: Learn Ergodic HMM
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "slm.h"
#include "vocab.h"
#include "misc.h"
#include "libs.h"
#include "ehmm.h"


SLMEHMMState *State,*TmpState, *TmpState2;
long double *TmpDenom_a, *TmpDenom_b;
int N_state, N_vocab;
SLMWordID Sent[MAX_WORDS_IN_A_LINE];

double TotalLikelihood;

void usage()
{
    fprintf(stderr,"usage: learnehmm [-n 10] -vocab .vocab -n #_of_iter infile.text[.gz] [infile] [outfile]\n");
    exit(1);
}

void
ClearEHMM()
{
    int i,j;
    for (i = 0; i < N_state; i++) {
	for (j = 0; j < N_state; j++) {
	    TmpState[i].trans_probs[j] = 0;
	}
	for (j = 0; j < N_vocab; j++) {
	    TmpState[i].emit_probs[j] = 0;
	}
	TmpDenom_a[i] = 0;
	TmpDenom_b[i] = 0;
    }
}

void
ClearEHMM2()
{
    int i,j;
    for (i = 0; i < N_state; i++) {
	for (j = 0; j < N_state; j++) {
	    TmpState2[i].trans_probs[j] = 0;
	}
	for (j = 0; j < N_vocab; j++) {
	    TmpState2[i].emit_probs[j] = 0;
	}
    }
}


void
SLMLearnEHMM1(SLMWordID *sent, int len)
{
    int i,j,k;
    long double g;

    CalcForward(sent,len,State,N_state);
    CalcBackward(sent,len,State,N_state);
    for (i = 1; i <= len; i++) {
	for (j = 0; j < N_state; j++) {
	    for (k = 0; k < N_state; k++) {
		g = ForwardProbs[i-1][j]*State[j].trans_probs[k]*
		    State[k].emit_probs[sent[i-1]]*BackwardProbs[i][j];

		TmpState[j].trans_probs[k] += g;
		TmpDenom_a[j] += g;
		TmpState[k].emit_probs[sent[i-1]] += g;
		TmpDenom_b[k] += g;
	    }
	}
    }
    g = 0;
    for (i = 0; i < N_state; i++) {
	g += ForwardProbs[len-1][i];
    }
    TotalLikelihood += llog10(g);
}

void
UpdateEHMM()
{
    int i,j;
    double r;

    for (i = 0; i < N_state; i++) {
	for (j = 0; j < N_state; j++) {
	    if (TmpDenom_a[i] == 0) {
		fprintf(stderr,"Warning: state %d divide by 0\n",i);
		r = 0;
	    }
	    else
		r = TmpState[i].trans_probs[j]/TmpDenom_a[i];
	    if (r < FLOOR_VALUE)
		r = FLOOR_VALUE;
	    TmpState2[i].trans_probs[j] += r;
	}
	for (j = 0; j < N_vocab; j++) {
	    if (TmpDenom_b[i] == 0) {
		fprintf(stderr,"Warning: state %d divide by 0\n",i);
		r = 0;
	    }
	    else
		r = TmpState[i].emit_probs[j]/TmpDenom_b[i];
	    if (r < FLOOR_VALUE)
		r = FLOOR_VALUE;
	    TmpState2[i].emit_probs[j] += r;
	}
    }
}

void
checkEHMM()
{
    int i,j;
    long double s,p;
    for (i = 0; i < N_state; i++) {
	s = 0;
	for (j = 0; j < N_state; j++) {
	    p = State[i].trans_probs[j];
	    if (p < 0 || 1 < p)
		fprintf(stderr,"Warning: trans prob from %d to %d = %Lg\n",
			i,j,p);
	    s += p;
	}
	if (s < 0.9999 || 1.0001 < s) {
	    fprintf(stderr,"Warning: trans prob of %d = %Lg\n",i,s);
	}
	s = 0;
	for (j = 0; j < N_vocab; j++) {
	    p = State[i].emit_probs[j];
	    if (p < 0 || 1 < p)
		fprintf(stderr,"Warning: emit prob from %d word %d = %Lg\n",
			i,j,p);
	    s += p;
	}
	if (s < 0.9999 || 1.0001 < s) {
	    fprintf(stderr,"Warning: emit prob of %d = %Lg\n",i,s);
	}
    }
}

void
UpdateEHMM2(int linenum)
{
    int i,j;
    for (i = 0; i < N_state; i++) {
	for (j = 0; j < N_state; j++) {
	    State[i].trans_probs[j] = TmpState2[i].trans_probs[j]/linenum;
	}
	for (j = 0; j < N_vocab; j++) {
	    State[i].emit_probs[j] = TmpState2[i].emit_probs[j]/linenum;
	}
    }
}

void
SLMLearnEHMM(char *textfile, SLMHashTable *vocab_ht)
{
    FILEHANDLE f;
    int i,linenum,wordnum;
    SLMWordID start_id, end_id, id;
    char word[WORD_LEN];

    f = z_open(textfile,"r");
    start_id = SLMIntHashSearch(vocab_ht,START_WORD);
    end_id = SLMIntHashSearch(vocab_ht,END_WORD);

    linenum = 1;
    wordnum = 0;
    i = 0;
    TotalLikelihood = 0;
    ClearEHMM2();
    while (z_getstr(f,word,WORD_LEN) == 0) {
	if (i == MAX_WORDS_IN_A_LINE) {
	    fprintf(stderr,"Error: Too many words in a line at %d\n",linenum);
	    exit(1);
	}
	wordnum++;
	id = SLMIntHashSearch(vocab_ht,word);
	Sent[i++] = id;
	if (id == end_id) {
	    ClearEHMM();
	    SLMLearnEHMM1(Sent,i);
	    UpdateEHMM();
	    i = 0;
	    linenum++;
	    if (linenum % 1000 == 0) {
		fprintf(stderr,".");
		fflush(stderr);
	    }
	}
    }
    z_close(f);
    UpdateEHMM2(linenum-1);
    fprintf(stderr,"\nTotal Likelihood = %g Perplexity = %g #word=%d\n",
	    TotalLikelihood,
	    pow(10.0,-TotalLikelihood/wordnum), wordnum);
/*    checkEHMM(); */
}

int
main(int argc, char *argv[])
{
    SLMHashTable *vocab_ht = NULL;
    int i,n = 10;
    FILEHANDLE f = FILEIO_stdout();
    char *textfile = NULL;
    char *in_hmmfile = NULL;

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i],"-vocab"))
	    vocab_ht = read_vocab(nextarg(argc,argv,i++),NULL,0);
	else if (!strcmp(argv[i],"-n"))
	    n = atoi(nextarg(argc,argv,i++));
	else if (argv[i][0] == '-')
	    usage();
	else if (textfile == NULL)
	    textfile = argv[i];
	else if (in_hmmfile == NULL)
	    in_hmmfile = argv[i];
	else
	    f = z_open(argv[i],"w");
    }
    if (vocab_ht == NULL) {
	fprintf(stderr,"Error: no vocab file specified\n");
	usage();
    }
    if (in_hmmfile == NULL || textfile == NULL)
	usage();
    State = SLMReadEHMM(in_hmmfile,&N_state, &N_vocab);
    TmpState = SLMAllocEHMMState(N_state, N_vocab);
    TmpState2 = SLMAllocEHMMState(N_state, N_vocab);
    TmpDenom_a = New_N(long double,N_state);
    TmpDenom_b = New_N(long double,N_state);
    InitFB(N_state);
    for (i = 0; i < n; i++) {
	fprintf(stderr,"Iteration #%d\n",i);
	SLMLearnEHMM(textfile,vocab_ht);
    }
    SLMDumpEHMM(f,State,N_state,N_vocab);
    z_close(f);
    return 0;
}

