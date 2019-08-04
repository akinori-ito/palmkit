/*
 * Ergodic HMM
 */

#ifndef EHMM_H
#define EHMM_H
#include "io.h"

typedef struct {
    long double *trans_probs;
    long double *emit_probs;
} SLMEHMMState;

#define FLOOR_VALUE      (1e-50)
#define LOG_FLOOR_VALUE  (-50)
#define MAX_WORDS_IN_A_LINE 2048
#define START_WORD "<s>"
#define END_WORD   "</s>"
#define WORD_LEN 256

SLMEHMMState *SLMAllocEHMMState(int, int);
void SLMDumpEHMM(FILEHANDLE f, SLMEHMMState *st, int n_state, int n_vocab);
SLMEHMMState *SLMReadEHMM(char *file, int *n_state, int *n_vocab);
long double llog10(long double x);

extern long double *ForwardProbs[MAX_WORDS_IN_A_LINE];
extern long double *BackwardProbs[MAX_WORDS_IN_A_LINE];
void InitFB(int N_state);
void CalcForward(SLMWordID *sent, int len, SLMEHMMState *State, int N_state);
void CalcBackward(SLMWordID *sent, int len, SLMEHMMState *State, int N_state);

#endif
