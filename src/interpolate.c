#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "libs.h"
#include "misc.h"
#include "io.h"

#define N_PROBS 6000000;
#define STOP_RATIO 0.999

#define TEST_ALL   0
#define TEST_FIRST 1
#define TEST_LAST  2
#define TEST_CV    3

float **pstream;  /* probability stream */
int max_probs = N_PROBS;
int n_probs;
int n_stream;
int test_mode = TEST_ALL;
int test_num;
int *tags;
int n_tags;
char **captions;

char *in_lambdas_file = NULL;
char *out_lambdas_file = NULL;
char *probs_file = NULL;
float **in_lambdas;
char *change_lambda;
double stop_ratio = STOP_RATIO;
char *captions_file = NULL;
char *tags_file = NULL;

double **probs_tmp;
double *probs_all;
int *n_tmp;

FILEHANDLE out_file;

void
read_probs(float *probs, char *file)
{
    FILEHANDLE f = z_open(file,"r");
    int i = 0;
    
    while (z_getfloat(f,&probs[i]) == 0) {
	i++;
	if (i == max_probs) {
	    fprintf(stderr,"interpolate : too many probabilities\n");
	    exit(1);
	}
    }
    if (n_probs == 0)
	n_probs = i;
    else if (n_probs != i) {
	fprintf(stderr,"interpolate : number of probabilities inconsistent: %d vs %d (in file %s)\n",
		n_probs,i,file);
	exit(1);
    }
    z_close(f);
}

void
read_tags(char *file)
{
    FILEHANDLE f = z_open(file,"r");
    int i = 0;

    while (z_getint(f,&tags[i]) == 0) {
	if (tags[i]+1 > n_tags)
	    n_tags = tags[i]+1;
	i++;
	if (i == max_probs) {
	    fprintf(stderr,"interpolate : too many tags\n");
	    exit(1);
	}
    }
    if (i != n_probs) {
	fprintf(stderr,"interpolate : number of tags (%d) differs from number of probs (%d)\n",
		i,n_probs);
	exit(1);
    }
    z_close(f);
}

void
read_in_lambdas(char *file)
{
    FILEHANDLE f = z_open(file,"r");
    int i,j;

    for (j = 0; j < n_tags; j++) {
	i = 0;
	while (z_getfloat(f,&in_lambdas[j][i]) == 0) {
	    i++;
	    if (i == n_stream)
		break;
	}
	if (n_stream != i) {
	    fprintf(stderr,"interpolate : number of in_lambdas inconsistent: %d vs %d (in file %s)\n",
		    n_stream,i,file);
	    exit(1);
	}
    }
    z_close(f);
}

void
read_captions(char *file)
{
    FILEHANDLE f = z_open(file,"r");
    int i = 0;
    char buf[256];

    captions = New_N(char*,n_tags);
    while (z_getstr(f,buf,256) == 0) {
	captions[i] = strdup(buf);
	i++;
	if (i == n_tags)
	    break;
    }
    if (i != n_tags) {
	fprintf(stderr,"interpolate : number of captions (%d) differs from number of tags(%d)\n",
		i, n_tags);
	exit(1);
    }
}

double
calc_pp(int beg, int end)
{
    int i,j,t;
    double p;

    /* initialization */
    for (i = 0; i < n_tags; i++) {
	for (j = 0; j < n_stream; j++)
	    probs_tmp[i][j] = 0;
	probs_all[i] = 0;
	n_tmp[i] = 0;
    }
    for (i = beg; i <= end; i++) {
	p = 0;
	if (n_tags == 1)
	    t = 0;
	else
	    t = tags[i];
	for (j = 0; j < n_stream; j++)
	    p += in_lambdas[t][j]*pstream[j][i];
	for (j = 0; j < n_stream; j++)
	    probs_tmp[t][j] += in_lambdas[t][j]*pstream[j][i]/p;
	probs_all[t] += log(p);
	n_tmp[t]++;
    }
    p = 0;
    for (t = 0; t < n_tags; t++) {
	if (captions)
	    printf("%-20s\tweights: ",captions[t]);
	else
	    printf("\tTAG %d\tweights: ",t);
	for (j = 0; j < n_stream; j++) {
	    printf("%5.3f ",in_lambdas[t][j]);
	}
	printf("  (%d items) --> PP = ",n_tmp[t]);
	if (n_tmp[t] > 0) {
	    printf("%f\n",exp(-probs_all[t]/n_tmp[t]));
	    p += probs_all[t];
	}
	else
	    printf("N/A\n");
    }
    return exp(-p/(end-beg+1));
}


double
interpolate(int learn_begin, int learn_end, int test_begin, int test_end)
{
    double ratio = 0;
    double pp,prev_pp;
    int i = 0,j,t,n;

    n = learn_end-learn_begin+1;
    for (;;) {
	pp = calc_pp(learn_begin,learn_end);
	printf("\t\t=========> TOTAL PP = %f",pp);
	if (i > 0) {
	    ratio = pp/prev_pp;
	    printf(" (down %f)",1.0-ratio);
	}
	prev_pp = pp;
	printf("\n\n");
	i++;
	if (ratio > stop_ratio)
	    break;
	for (t = 0; t < n_tags; t++) {
	    for (j = 0; j < n_stream; j++) {
		if (change_lambda[j] && n_tmp[t] > 0)
		    in_lambdas[t][j] = probs_tmp[t][j]/n_tmp[t];
	    }
	}
    }
    for (j = 0; j < n_tags; j++) {
	for (i = 0; i < n_stream; i++) {
	    z_printf(out_file,"%f ",in_lambdas[j][i]);
	}
	z_printf(out_file,"\n");
    }
    if (test_begin >= 0) {
	printf("\nNOW TESTING...\n\n");
	pp = calc_pp(test_begin,test_end);
	printf("\t\t=========> TOTAL PP = %f\n",pp);
    }
    return pp;
}

void
init_lambda()
{
    int i,j;

    if (in_lambdas_file != NULL) {
	read_in_lambdas(in_lambdas_file);
	printf("initial weights read from %s\n",in_lambdas_file);
    }
    else {
	for (j = 0; j < n_tags; j++)
	    for (i = 0; i < n_stream; i++) {
		in_lambdas[j][i] = 1.0/n_stream;
	    }
    }
}

int
main(int argc, char *argv[])
{
    int i,j;
    int half;
    double pp1,pp2;

    /* first, check number of stream */
    n_stream = 0;
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '+') {
	    n_stream++;
	    i++;
	}
	else if (!strcmp(argv[i],"-max_probs"))
	    max_probs = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-test_all"))
	    test_mode = TEST_ALL;
	else if (!strcmp(argv[i],"-test_first")) {
	    test_mode = TEST_FIRST;
	    test_num = atoi(nextarg(argc,argv,i++));
	}
	else if (!strcmp(argv[i],"-test_last")) {
	    test_mode = TEST_LAST;
	    test_num = atoi(nextarg(argc,argv,i++));
	}
	else if (!strcmp(argv[i],"-cv"))
	    test_mode = TEST_CV;
	else if (!strcmp(argv[i],"-stop_ratio"))
	    stop_ratio = atof(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-captions"))
	    captions_file = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-tags"))
	    tags_file = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-in_lambdas"))
	    in_lambdas_file = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-out_lambdas"))
	    out_lambdas_file = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-probs"))
	    probs_file = nextarg(argc,argv,i++);
    }

    pstream = New_N(float*,n_stream);
    for (i = 0; i < n_stream; i++)
	pstream[i] = New_N(float,max_probs);
    change_lambda = New_N(char,n_stream);

    /* read probability stream */
    printf("Reading the probability streams...");
    fflush(stdout);
    j = 0;
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '+') {
	    if (argv[i][1] == '-')
		change_lambda[j] = 0;
	    else
		change_lambda[j] = 1;
	    i++;
	    read_probs(pstream[j],argv[i]);
	    j++;
	}
    }
    printf("Done.\n");

    if (tags_file != NULL) {
	tags = New_N(int,max_probs);
	read_tags(tags_file);
	if (captions_file != NULL)
	    read_captions(captions_file);
    }
    else
	n_tags = 1;

    in_lambdas = New_N(float*,n_tags);
    for (i = 0; i < n_tags; i++)
	in_lambdas[i] = New_N(float,n_stream);

    probs_tmp = New_N(double*,n_tags);
    for (i = 0; i < n_tags; i++)
	probs_tmp[i] = New_N(double,n_stream);

    probs_all = New_N(double,n_tags);
    n_tmp = New_N(int,n_tags);
    init_lambda();
    if (out_lambdas_file != NULL) {
	out_file = z_open(out_lambdas_file,"w");
	printf("The final weights will be written in %s\n",out_lambdas_file);
    }
    else
	out_file = FILEIO_stdout();

    printf("%d models will be interpolated using ",n_stream);
    switch (test_mode) {
    case TEST_ALL:
	printf("%d data items\n",n_probs);
	interpolate(0,n_probs-1,-1,-1);
	break;
    case TEST_FIRST:
	printf("last %d data items\n",n_probs-test_num);
	printf("\tThe first %d items will be used for testing\n",test_num);
	interpolate(test_num,n_probs-1,0,test_num-1);
	break;
    case TEST_LAST:
	printf("first %d data items\n",n_probs-test_num);
	printf("\tThe last %d items will be used for testing\n",test_num);
	interpolate(0,test_num-1,test_num,n_probs-1);
	break;
    case TEST_CV:
	printf("cross varidation of\n");
	half = n_probs/2;
	printf("\tthe first %d data items and the last %d items\n",
	       half,n_probs-half);
	printf("Phase 1: first part for learning, last part for test\n");
	pp1 = interpolate(0,half-1,half,n_probs-1);

	init_lambda();
	printf("\n\nPhase 2: first part for learning, last part for test\n");
	pp2 = interpolate(half,n_probs-1,0,half);
	printf("\nTwo-way cross varidation:\n");
	printf("\tFirst half PP = %f\n",pp2);
	printf("\tSecond half PP = %f\n",pp1);
	printf("\t=====> TOTAL PP = %f\n",(pp1+pp2)/2);
	break;
    }
    return 0;
}
