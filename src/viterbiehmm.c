/*
 * viterbiehmm.c: Viterbi Algorithm
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "slm.h"
#include "vocab.h"
#include "misc.h"
#include "libs.h"
#include "ehmm.h"

SLMEHMMState *State;
int N_state, N_vocab;
SLMWordID Sent[MAX_WORDS_IN_A_LINE];
SLMWordID *Choice[MAX_WORDS_IN_A_LINE];
SLMWordID Tmp[MAX_WORDS_IN_A_LINE];
SLMHashTable *vocab_ht = NULL;
char *VSent[MAX_WORDS_IN_A_LINE];

void usage()
{
    fprintf(stderr,"usage: viterbiehmm -vocab .vocab -hmm file.ehmm [file.text]\n");
    exit(1);
}

void
CalcViterbi(SLMWordID *sent, int len)
{
    int i,j,k,best_k;
    long double best_p,g;

    ForwardProbs[0][0] = 0;   /* Initial state is always 0 */
    for (i = 1; i < N_state; i++)
	ForwardProbs[0][i] = LOG_FLOOR_VALUE;
    for (i = 1; i <= len; i++) {
	for (j = 0; j < N_state; j++) {
	    best_k = 0;
	    best_p = LOG_FLOOR_VALUE;
	    for (k = 0; k < N_state; k++) {
		g = ForwardProbs[i-1][k]+
		    llog10(State[k].trans_probs[j]*
			   State[j].emit_probs[sent[i-1]]);
		if (g > best_p) {
		    best_p = g;
		    best_k = k;
		}
	    }
	    ForwardProbs[i][j] = best_p;
	    Choice[i][j] = best_k;
	}
    }
}

void
BackTrace(int len)
{
    int i,s;
    long double p;

    s = 0;
    p = ForwardProbs[len][0];
    for (i = 1; i < N_state; i++) {
	if (ForwardProbs[len][i] > p) {
	    p = ForwardProbs[len][i];
	    s = i;
	}
    }
    i = len;
    while (s > 0) {
	s = Choice[i][s];
	Tmp[i] = s;
	i--;
    }
    Tmp[0] = 0;
}

void
ViterbiEHMM1(SLMWordID *sent, char **vsent, int len)
{
    int i;
    CalcViterbi(sent,len);
    BackTrace(len);
    for (i = 0; i < len; i++) {
	printf("%s+%ld ",vsent[i],Tmp[i]);
    }
    printf("\n");
}


void
ViterbiEHMM(FILEHANDLE f)
{
    int i,linenum,wordnum,len;
    SLMWordID start_id, end_id, id;
    char word[WORD_LEN];

    start_id = SLMIntHashSearch(vocab_ht,START_WORD);
    end_id = SLMIntHashSearch(vocab_ht,END_WORD);

    linenum = 1;
    wordnum = 0;
    i = 0;
    while (z_getstr(f,word,WORD_LEN) == 0) {
	if (i == MAX_WORDS_IN_A_LINE) {
	    fprintf(stderr,"Error: Too many words in a line at %d\n",linenum);
	    exit(1);
	}
	wordnum++;
	id = SLMIntHashSearch(vocab_ht,word);
	Sent[i] = id;
	VSent[i] = strdup(word);
	i++;
	if (id == end_id) {
	    len = i;
	    ViterbiEHMM1(Sent,VSent,len);
	    for (i = 0; i < len; i++)
		Free(VSent[i]);
	    i = 0;
	    linenum++;
	}
    }
}

void
selectTopState()
{
    int i,j,best_j;
    long double p;

    for (i = 0; i < N_vocab; i++) {
	p = State[0].emit_probs[i];
	best_j = 0;
	for (j = 1; j < N_state; j++) {
	    if (State[j].emit_probs[i] > p) {
		p = State[j].emit_probs[i];
		best_j = j;
	    }
	}
	for (j = 0; j < N_state; j++) {
	    if (j != best_j)
		State[j].emit_probs[i] = 0;
	}
    }
}

int
main(int argc, char *argv[])
{
    int i;
    FILEHANDLE f = FILEIO_stdout();
    char *in_hmmfile = NULL,*vocabfile = NULL;
    int cluster = 0;

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i],"-vocab"))
	    vocabfile = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-hmm"))
	    in_hmmfile = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-cluster"))
	    cluster = 1;
	else if (argv[i][0] == '-')
	    usage();
	else
	    f = z_open(argv[i],"r");
    }
    if (vocabfile == NULL) {
	fprintf(stderr,"Error: no vocab file specified\n");
	usage();
    }
    if (in_hmmfile == NULL)
	usage();
    State = SLMReadEHMM(in_hmmfile,&N_state, &N_vocab);
    vocab_ht = read_vocab(vocabfile,NULL,0);
    InitFB(N_state);
    for (i = 0; i < MAX_WORDS_IN_A_LINE; i++)
	Choice[i] = New_N(SLMWordID, N_state);
    if (cluster)
	selectTopState();
    ViterbiEHMM(f);
    z_close(f);
    return 0;
}

