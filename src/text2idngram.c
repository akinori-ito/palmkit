#include "idngram.h"
#include "remove_tmp.h"
#include <signal.h>

int hash_size = 200000;
char *tmpdir = "/usr/tmp";

SLMIDNgramBuffer *NGBuf;
int ascii_output;
int distance = 0;

void usage()
{
    printf("Usage : text2idngram  -vocab .vocab\n");
    printf("                    [ -n 3 ]\n");
    printf("                    [ -d 0 ]\n");
    printf("                    [ -write_ascii ]\n");
    printf("                    [ -hash 200000 ]\n");
    printf("                    [ -buffer 100 ]\n");
    printf("                    [ -verbosity 2 ]\n");
    printf("                    [ -temp /usr/tmp ]\n");
    printf("                    [ .text] [.idngram]\n");
    exit(1);
}
    
void 
text2idngram(char *vocabfile, FILEHANDLE inf, FILEHANDLE outf)
{
    SLMHashTable *vocab_ht;
    SLMWordTuple tuple;
    SLMWordTuple read_tuple;
    SLMHashTable *ngram_ht;
    SLMHashTableElement *ht_elem;
    int i,total_words = 0;
    char buf[256],buf2[256];
    int processed_ngram = 0;
    FILEHANDLE df;
    int tuple_len = ngram_len+distance;

    vocab_ht = read_vocab(vocabfile,NULL,0);
    tuple = SLMNewWordTuple(ngram_len);
    read_tuple = SLMNewWordTuple(tuple_len);

    ngram_ht = SLMHashCreate(hash_size,ngram_hashfunc, ngram_comp);
    NGBuf->ngram_len = ngram_len;
    NGBuf->elem_size = ngram_len+2;
    NGBuf->elem_byte = sizeof(SLMWordID)*NGBuf->elem_size;
    NGBuf->real_size = NGBuf->size*1024*1024/NGBuf->elem_byte;
    NGBuf->buffer = New_N(SLMWordID,NGBuf->real_size);

    if (verbosity > 1) {
	fprintf(stderr,"20,000 n-grams processed for each \".\", 1,000,000 for each line.\n");
    }

    while (z_getstr(inf,buf,256) == 0) {
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
	total_words++;
	for (i = 0; i < tuple_len-1; i++)
	    read_tuple[i] = read_tuple[i+1];
	read_tuple[tuple_len-1] = SLMIntHashSearch(vocab_ht,buf);
	if (total_words < tuple_len)
	    continue;
	if (distance > 0) {
	    tuple[0] = read_tuple[0];
	    tuple[1] = read_tuple[tuple_len-1];
	}
	else {
	    for (i = 0; i < ngram_len; i++)
		tuple[i] = read_tuple[i];
	}

	ht_elem = SLMHashSearch(ngram_ht, tuple);
	if (ht_elem == NULL) {
	    /* newly discovered n-gram */
	    for (i = 0; i < ngram_len; i++)
		NGBuf->buffer[NGBuf->pos+i] = tuple[i];
	    /* set count to one */
	    NGBuf->buffer[NGBuf->pos+ngram_len] = 0;
	    NGBuf->buffer[NGBuf->pos+ngram_len+1] = 1;

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
	    incr_ngram(NGBuf,(SLMWordID*)ht_elem->valueptr,1);
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
    char *vocabfile = NULL;
    char *infile = NULL, *outfile = NULL;
    int i;
    FILEHANDLE inf,outf;

    NGBuf = New(SLMIDNgramBuffer);
    NGBuf->size = 100;  /* MB */

    verbosity = 2;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i],"-vocab"))
	    vocabfile = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-n"))
	    ngram_len = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-d"))
	    distance = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-write_ascii"))
	    ascii_output = 1;
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

    if (distance > 0) {
	/* if distance > 0, N should be 2 */
	if (ngram_len != 2) {
	    fprintf(stderr,"text2idngram: -n should be 2 when -d is specified\n");
	    exit(1);
	}
    }

    if (vocabfile == NULL) {
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
	fprintf(stderr,"Vocab            %s\n",vocabfile);
	fprintf(stderr,"Temp directory:  %s\n",tmpdir);
	fprintf(stderr,"N                %d\n",ngram_len);
	if (distance > 0) {
	    fprintf(stderr,"Distance:        %d\n",distance);
	}
    }
    TrapAllSignals();
    text2idngram(vocabfile,inf,outf);

    z_close(inf);
    z_close(outf);
    return 0;
}
