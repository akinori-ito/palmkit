#include <stdlib.h>
#include <string.h>
#include "libs.h"
#include "io.h"
#include "misc.h"

struct wordfreq {
    char *word;
    int n;
};

void usage()
{
    printf("Usage : wfreq2vocab [ -top 20000 | -gt 10 ]\n");
    printf("                    [ -records 100000 ]\n");
    printf("                    [ .wfreq] [.vocab]\n");
    exit(1);
}

struct wordfreq *
read_wfreq(FILEHANDLE inf, int* nword, int* recordsize, int gt)
{
    char word[256];
    int n,i,j,newrec;
    struct wordfreq *wfreq,*ofreq;

    wfreq = New_N(struct wordfreq,*recordsize);
    i = 0;
    while (z_getstr(inf,word,256) == 0) {
	if (z_getint(inf,&n) < 0)
	    break;
	if (n <= gt)
	    continue;
	if (i == *recordsize) {
	    newrec = *recordsize*2;
	    ofreq = wfreq;
	    wfreq = New_N(struct wordfreq,newrec);
	    for (j = 0; j < *recordsize; j++)
		wfreq[j] = ofreq[j];
	    *recordsize = newrec;
	}
	wfreq[i].word = strdup(word);
	wfreq[i].n = n;
	i++;
    }
    *nword = i;
    return wfreq;
}

static int
comp_by_freq(const void *s1, const void *s2)
{
    struct wordfreq *w1,*w2;
    w1 = (struct wordfreq*)s1;
    w2 = (struct wordfreq*)s2;
    return w2->n - w1->n;
}

static int
comp_by_word(const void *s1, const void *s2)
{
    struct wordfreq *w1,*w2;
    w1 = (struct wordfreq*)s1;
    w2 = (struct wordfreq*)s2;
    return strcmp(w1->word,w2->word);
}

void
do_top(int top, int records, FILEHANDLE inf, FILEHANDLE outf)
{
    struct wordfreq *wfreq;
    int n;

    wfreq = read_wfreq(inf, &n, &records,0);
    qsort(wfreq,n,sizeof(struct wordfreq),comp_by_freq);
    if (top > n)
	top = n;
    qsort(wfreq,top,sizeof(struct wordfreq),comp_by_word);
    for (n = 0; n < top; n++) {
	z_printf(outf,"%s\n",wfreq[n].word);
    }
}
    
void
do_gt(int gt, int records, FILEHANDLE inf, FILEHANDLE outf)
{
    struct wordfreq *wfreq;
    int n,i;

    wfreq = read_wfreq(inf, &n, &records, gt);
    qsort(wfreq,n,sizeof(struct wordfreq),comp_by_word);
    for (i = 0; i < n; i++) {
	z_printf(outf,"%s\n",wfreq[i].word);
    }
}
    

int
main(int argc, char *argv[])
{
    int records = 100000;
    int top = -1;
    int gt = -1;
    char *infile = NULL, *outfile = NULL;
    int i;
    FILEHANDLE inf,outf;
    
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i],"-top")) {
	    if (gt > 0) {
		fprintf(stderr,"wfreq2vocab: Can't specify both -top and -gt\n");
		exit(1);
	    }
	    top = atoi(nextarg(argc,argv,i++));
	}
	else if (!strcmp(argv[i],"-gt")) {
	    if (top > 0) {
		fprintf(stderr,"wfreq2vocab: Can't specify both -top and -gt\n");
		exit(1);
	    }
	    gt = atoi(nextarg(argc,argv,i++));
	}
	else if (!strcmp(argv[i],"-records"))
	    records = atoi(nextarg(argc,argv,i++));
	else if (argv[i][0] == '-')
	    usage();
	else
	    break;
    }
    if (i < argc)
	infile = argv[i];
    if (i+1 < argc)
	outfile = argv[i+1];
    if (i+2 < argc)
	usage();

    if (infile)
	inf = z_open(infile,"r");
    else
	inf = FILEIO_stdin();

    if (outfile)
	outf = z_open(outfile,"w");
    else
	outf = FILEIO_stdout();

    if (gt < 0 && top < 0)
	top = 20000;
    if (top > 0)
	do_top(top,records,inf,outf);
    else
	do_gt(gt,records,inf,outf);

    z_close(inf);
    z_close(outf);
    return 0;
}
