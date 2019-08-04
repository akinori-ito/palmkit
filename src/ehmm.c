#include <stdlib.h>
#include <sys/types.h>
#include <math.h>
#include "slm.h"
#include "ehmm.h"

long double llog10(long double x)
{
    if (x < FLOOR_VALUE)
	return LOG_FLOOR_VALUE;
    return (long double)log10((double)x);
}

SLMEHMMState *SLMAllocEHMMState(int n_state,int n_vocab)
{
    int i;
    long double *sprobs, *eprobs;
    SLMEHMMState *State;

    State = New_N(SLMEHMMState, n_state);
    sprobs = New_N(long double,n_state*n_state);
    eprobs = New_N(long double,n_state*n_vocab);
    for (i = 0; i < n_state; i++) {
	State[i].trans_probs = &sprobs[i*n_state];
	State[i].emit_probs = &eprobs[i*n_vocab];
    }
    return State;
}

void
SLMDumpEHMM(FILEHANDLE f, SLMEHMMState *st, int n_state, int n_vocab)
{
    int i,j;

    z_printf(f,"%d %d\n",n_state, n_vocab);
    for (i = 0; i < n_state; i++,st++) {
	for (j = 0; j < n_state; j++) {
	    z_printf(f,"%Lg ",llog10(st->trans_probs[j]));
	}
	z_printf(f,"\n");
	for (j = 0; j < n_vocab; j++) {
	    z_printf(f,"%Lg ",llog10(st->emit_probs[j]));
	}
	z_printf(f,"\n");
    }
}

static void z_getnum(char *file, FILEHANDLE f, void *ptr, int (*in_rout)(FILEHANDLE,void*))
{
    if (in_rout(f,ptr) < 0) {
	fprintf(stderr,"File format error in %s\n",file);
	exit(1);
    }
}

#define GETINT(file,f,p) z_getnum(file,f,(void*)p,(int(*)(FILEHANDLE,void*))z_getint)
#define GETFLOAT(file,f,p) z_getnum(file,f,(void*)p,(int(*)(FILEHANDLE,void*))z_getdouble)

SLMEHMMState *
SLMReadEHMM(char *file, int *n_state, int *n_vocab)
{
    FILEHANDLE f;
    int i,j;
    SLMEHMMState *st;
    double r;

    f = z_open(file,"r");
    GETINT(file,f,n_state);
    GETINT(file,f,n_vocab);
    st = SLMAllocEHMMState(*n_state, *n_vocab);
    for (i = 0; i < *n_state; i++) {
	for (j = 0; j < *n_state; j++) {
	    GETFLOAT(file,f,&r);
	    st[i].trans_probs[j] = pow(10.0,r);
	}
	for (j = 0; j < *n_vocab; j++) {
	    GETFLOAT(file,f,&r);
	    st[i].emit_probs[j] = pow(10.0,r);
	}
    }
    z_close(f);
    return st;
}
