#include "idngram.h"
#include "remove_tmp.h"
#include <signal.h>

int hash_size = 200000;
char *tmpdir = "/usr/tmp";

SLMIDNgramBuffer *NGBuf;
int ascii_input,ascii_output;

void usage()
{
    printf("Usage : changeidvocab -from_vocab .vocab\n");
    printf("                      -to_vocab .vocab\n");
    printf("                    [ -n 3 ]\n");
    printf("                    [ -ascii_input ]\n");
    printf("                    [ -ascii_output ]\n");
    printf("                    [ -hash 200000 ]\n");
    printf("                    [ -buffer 100 ]\n");
    printf("                    [ -verbosity 2 ]\n");
    printf("                    [ -temp /usr/tmp ]\n");
    printf("                    [ .idngram] [.idngram]\n");
    exit(1);
}
    
char**
read_vocab_tbl(char *vocabfile, int* nword)
{
    char word[256];
    int recordsize = 10000;
    int i,j,newrec;
    char **w,**o;
    FILEHANDLE inf;

    inf = z_open(vocabfile,"r");
    w = New_N(char*,recordsize);
    i = 1;
    w[0] = "<UNK>";
    while (z_getstr(inf,word,256) == 0) {
	if (i == recordsize) {
	    newrec = recordsize*2;
	    o = w;
	    w = New_N(char*,newrec);
	    for (j = 0; j < recordsize; j++) {
		w[j] = o[j];
	    }
	    recordsize = newrec;
	    free(o);
	}
	w[i] = strdup(word);
	i++;
    }
    *nword = i;
    return w;
}

void 
changeidngramvocab(char *vocabfile1, char *vocabfile2, FILEHANDLE inf, FILEHANDLE outf)
{
    SLMHashTable *vocab_ht2;
    char **vocab_tbl1;
    SLMNgramCount *ngcount;
    SLMHashTable *ngram_ht;
    SLMHashTableElement *ht_elem;
    char buf2[256];
    int i;
    int processed_ngram = 0;
    FILEHANDLE df;
    int vocab_size1;

    vocab_tbl1 = read_vocab_tbl(vocabfile1,&vocab_size1);
    vocab_ht2 = read_vocab(vocabfile2,NULL,0);

    ngcount = SLMNewNgramCount(ngram_len);

    ngram_ht = SLMHashCreate(hash_size,ngram_hashfunc, ngram_comp);
    NGBuf->ngram_len = ngram_len;
    NGBuf->elem_size = ngram_len+2;
    NGBuf->elem_byte = sizeof(SLMWordID)*NGBuf->elem_size;
    NGBuf->real_size = NGBuf->size*1024*1024/NGBuf->elem_byte;
    NGBuf->buffer = New_N(SLMWordID,NGBuf->real_size);

    if (verbosity > 1) {
	fprintf(stderr,"20,000 n-grams processed for each \".\", 1,000,000 for each line.\n");
    }

    while (SLMReadNgramCount(ngram_len,inf,ngcount,ascii_input) != NULL) {
	processed_ngram++;
	if (verbosity > 0) {
	    if (processed_ngram % 20000 == 0) {
		fprintf(stderr,".");
		if (processed_ngram % 1000000 == 0) {
		    fprintf(stderr,"\n");
		}
		fflush(stderr);
	    }
	}
	for (i = 0; i < ngram_len; i++) {
	  ngcount->word_id[i] = SLMIntHashSearch(vocab_ht2,vocab_tbl1[ngcount->word_id[i]]);
	}
	ht_elem = SLMHashSearch(ngram_ht, ngcount->word_id);
	if (ht_elem == NULL) {
	    /* newly discovered n-gram */
	    for (i = 0; i < ngram_len; i++)
		NGBuf->buffer[NGBuf->pos+i] = ngcount->word_id[i];
	    /* set count to one */
	    NGBuf->buffer[NGBuf->pos+ngram_len] = ((ngcount->count>>16)&0xffff) ;
	    NGBuf->buffer[NGBuf->pos+ngram_len+1] = (ngcount->count&0xffff);

	    SLMHashInsert(ngram_ht,
			  (void*)&NGBuf->buffer[NGBuf->pos],
			  (void*)&NGBuf->buffer[NGBuf->pos]);

	    NGBuf->pos += NGBuf->elem_size;
	    if (NGBuf->pos > NGBuf->real_size-NGBuf->elem_size) {
		sprintf(buf2,"%s/text2idn%d.%d.idngram.gz",
			tmpdir,getpid(),merge_list_num);
		RemoveAfter(buf2);
		push_merge_list(buf2);
		df = z_open(buf2,"w");
		dumpidngram(NGBuf,df,0);
		z_close(df);
		if (verbosity > 1) {
		    fprintf(stderr,"\nsorted N-gram written into %s\n",buf2);
		}
		SLMHashDestroy(ngram_ht);
		ngram_ht = SLMHashCreate(hash_size,
					 ngram_hashfunc,
					 ngram_comp);
		NGBuf->pos = 0;
	    }
	}
	else {
	    incr_ngram(NGBuf,(SLMWordID*)ht_elem->valueptr,ngcount->count);
	}
    }
    if (verbosity > 1)
	fprintf(stderr,"%d N-grams has been processed.\n",processed_ngram);
    if (merge_list_num > 0) {
	if (verbosity > 1)
	    fprintf(stderr,"\nmerging ngrams...\n");
	sprintf(buf2,"%s/text2idn%d.%d.idngram.gz",
		tmpdir,getpid(),merge_list_num);
	RemoveAfter(buf2);
	push_merge_list(buf2);
	df = z_open(buf2,"w");
	dumpidngram(NGBuf,df,0);
	z_close(df);
	SLMMergeIDNgram(ngram_len,merge_list,merge_list_num,outf,0,ascii_output);
	for (i = 0; i < merge_list_num; i++) {
	    sprintf(buf2,"%s/text2idn%d.%d.idngram.gz",
		    tmpdir,getpid(),i);
	    unlink(buf2);
	}
    }
    else {
	if (verbosity > 1)
	    fprintf(stderr,"\ndumping ngrams...\n");
	dumpidngram(NGBuf,outf,ascii_output);
    }
}

int
main(int argc, char *argv[])
{
    char *vocabfile1 = NULL;
    char *vocabfile2 = NULL;
    char *infile = NULL, *outfile = NULL;
    int i;
    FILEHANDLE inf,outf;

    NGBuf = New(SLMIDNgramBuffer);
    NGBuf->size = 100;  /* MB */

    verbosity = 2;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i],"-from_vocab"))
	    vocabfile1 = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-to_vocab"))
	    vocabfile2 = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-n"))
	    ngram_len = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-write_ascii") || !strcmp(argv[i],"-ascii_output"))
	    ascii_output = 1;
	else if (!strcmp(argv[i],"-ascii_input"))
	    ascii_input = 1;
	else if (!strcmp(argv[i],"-temp"))
	    tmpdir = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-hash"))
	    hash_size = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-buffer"))
	    NGBuf->size = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-verbosity") || !strcmp(argv[i],"-v"))
	    verbosity = atoi(nextarg(argc,argv,i++));
	else if (argv[i][0] == '-')
	    usage();
	else
	    break;
    }

    if (vocabfile1 == NULL || vocabfile2 == NULL) {
	fprintf(stderr, "text2idngram: vocabulary file missing\n");
	usage();
    }
    if (i < argc)
	infile = argv[i];
    if (i+1 < argc)
	outfile = argv[i+1];
    if (i+2 < argc)
	usage();

    if (infile) {
	inf = z_open(infile,"r");
	if (verbosity > 1)
	    fprintf(stderr,"Input file:      %s\n",infile);
    }
    else {
	inf = FILEIO_stdin();
	if (verbosity > 2)
	    fprintf(stderr,"Input file:      stdin\n");
    }

    if (outfile) {
	outf = z_open(outfile,"w");
	if (verbosity > 1)
	    fprintf(stderr,"Output file:     %s\n",outfile);
    }
    else {
	outf = FILEIO_stdout();
	if (verbosity > 1)
	    fprintf(stderr,"Output file:     stdout\n");
    }
    if (verbosity > 1) {
	fprintf(stderr,"N-gram Buffer:   %dMB\n",NGBuf->size);
	fprintf(stderr,"Hash table size: %d\n",hash_size);
	fprintf(stderr,"From-Vocab       %s\n",vocabfile1);
	fprintf(stderr,"To-Vocab         %s\n",vocabfile2);
	fprintf(stderr,"Temp directory:  %s\n",tmpdir);
	fprintf(stderr,"N                %d\n",ngram_len);
    }
    TrapAllSignals();
    changeidngramvocab(vocabfile1,vocabfile2,inf,outf);

    z_close(inf);
    z_close(outf);
    return 0;
}
