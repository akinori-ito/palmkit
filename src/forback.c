/*
 * forback.c: Forward-Backward Algorithm
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "slm.h"
#include "vocab.h"
#include "misc.h"
#include "libs.h"
#include "ehmm.h"

long double *ForwardProbs[MAX_WORDS_IN_A_LINE];
long double *BackwardProbs[MAX_WORDS_IN_A_LINE];

void
InitFB(int N_state)
{
    int i;
    for (i = 0; i < MAX_WORDS_IN_A_LINE; i++) {
	ForwardProbs[i] = New_N(long double,N_state);
	BackwardProbs[i] = New_N(long double,N_state);
    }
}

void
CalcForward(SLMWordID *sent, int len, SLMEHMMState *State, int N_state)
{
    int i,j,k;

    ForwardProbs[0][0] = 1.0;   /* Initial state is always 0 */
    for (i = 1; i < N_state; i++)
	ForwardProbs[0][i] = 0;
    for (i = 1; i <= len; i++) {
	for (j = 0; j < N_state; j++) {
	    ForwardProbs[i][j] = 0;
	    for (k = 0; k < N_state; k++) {
		ForwardProbs[i][j] += ForwardProbs[i-1][k]*
		    State[k].trans_probs[j]*
		    State[j].emit_probs[sent[i-1]];
	    }
	}
    }
}

void
CalcBackward(SLMWordID *sent, int len, SLMEHMMState *State, int N_state)
{
    int i,j,k;
    long double s;

    s = 0;
    for (i = 0; i < N_state; i++) {
	BackwardProbs[len][i] = ForwardProbs[len][i];
	s += ForwardProbs[len][i];
    }
    for (i = 0; i < N_state; i++) 
	BackwardProbs[len][i] /= s;
    for (i = len-1; i >= 0; i--) {
	for (j = 0; j < N_state; j++) {
	    BackwardProbs[i][j] = 0;
	    for (k = 0; k < N_state; k++) {
		BackwardProbs[i][j] += BackwardProbs[i+1][k]*
		    State[j].trans_probs[k]*
		    State[k].emit_probs[sent[i]];
	    }
	}
    }
}

