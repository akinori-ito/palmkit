#include <sys/types.h>
#include <netinet/in.h>
#include "idngram.h"
#include <string.h>

int hash_size = 200000;
char *tmpdir = "/usr/tmp";
char delimiter = '+';
char **vocab_tbl;
SLMWordID *word_class_id;
char **class_tbl;
int max_vocab;
int max_class;
int one_unk = 0;

SLMIDNgramBuffer *NGBuf;
int ascii_output;
char *idwfreq_file;

void usage()
{
    printf("Usage : ctext2idngram  -vocab .vocab\n");
    printf("                       -class .cls\n");
    printf("                       -idwfreq .idwfreq\n");
    printf("                     [ -d delimit-char(default: %c) ]\n",delimiter);
    printf("                     [ -n 3 ]\n");
    printf("                     [ -max_class %d\n",max_class);
    printf("                     [ -write_ascii ]\n");
    printf("                     [ -hash %d ]\n",hash_size);
    printf("                     [ -buffer 100 ]\n");
    printf("                     [ -verbosity 2 ]\n");
    printf("                     [ .text] [.idngram]\n");
    exit(1);
}

void
dump_idwfreq(uint4 *word_freq, int nword)
{
    FILEHANDLE f = z_open(idwfreq_file,"w");
    int i;
    uint4 x;
    SLMWordID s;
    if (ascii_output) {
	z_printf(f,"%d\n",nword);
	for (i = 0; i < nword; i++)
	    z_printf(f,"%d %ld\n",word_class_id[i],word_freq[i]);
    }
    else {
	x = htonl((uint4)nword);
	z_write(&x,sizeof(uint4),1,f);
	for (i = 0; i < nword; i++) {
	    s = SLMhtonID((unsigned short)word_class_id[i]);
	    z_write(&s,sizeof(unsigned short),1,f);
	    x = htonl(word_freq[i]);
	    z_write(&x,sizeof(uint4),1,f);
	}
    }
    z_close(f);
}

static char*
class_part_of_word(char *v)
{
  char *p;
  for (p = v+strlen(v)-1; p >= v; p--) {
    if (*p == delimiter) {
      break;
    }
  }
  return p+1;
}

void 
ctext2idngram(char *vocabfile, char *classfile, FILEHANDLE inf, FILEHANDLE outf)
{
    SLMHashTable *vocab_ht;
    SLMHashTable *class_ht;
    SLMWordTuple tuple;
    SLMHashTable *ngram_ht;
    SLMHashTableElement *ht_elem;
    int i,total_words = 0;
    char buf[256],buf2[256];
    int class_id, word_id;
    int processed_ngram = 0;
    uint4 *word_freq;

    vocab_ht = read_vocab(vocabfile,vocab_tbl,max_vocab*2);
    class_tbl = New_N(char*,max_class);
    class_ht = read_vocab(classfile,class_tbl,max_class*2);
    tuple = SLMNewWordTuple(ngram_len);
    if (!one_unk) {
	add_class_unk(vocab_ht,class_ht,vocab_tbl,class_tbl,max_vocab);
	if (verbosity > 1)
	    fprintf(stderr,"Appending %d class UNKs. Vocabulary size is now %d\n",
		    class_ht->nelem+1,vocab_ht->nelem+1);
    }
    word_freq = New_N(uint4,vocab_ht->nelem+1);
    word_class_id = New_N(SLMWordID,vocab_ht->nelem+1);
    memset(word_class_id,0,sizeof(SLMWordID)*(vocab_ht->nelem+1));

    ngram_ht = SLMHashCreate(hash_size,ngram_hashfunc, ngram_comp);
    NGBuf->ngram_len = ngram_len;
    NGBuf->elem_size = ngram_len+2;
    NGBuf->elem_byte = sizeof(SLMWordID)*NGBuf->elem_size;
    NGBuf->real_size = NGBuf->size*1024*1024/NGBuf->elem_byte;
    NGBuf->buffer = New_N(SLMWordID,NGBuf->real_size);

    if (verbosity > 1) {
	fprintf(stderr,"20,000 n-grams processed for each \".\", 1,000,000 for each line.\n");
    }

    /* create word_id -> class_id mapping */
    for (i = 1; i <= vocab_ht->nelem; i++) {
	class_id = SLMIntHashSearch(class_ht,
				    class_part_of_word(vocab_tbl[i]));
	word_class_id[i] = class_id;
    }

    /* begin processing */
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
	word_id = SLMIntHashSearch(vocab_ht,buf);
	if (word_id == 0 && !one_unk) {
	    /* the word is unknown, but the class is known */
	    sprintf(buf,"<UNK>+%s",class_part_of_word(buf));
	    word_id = SLMIntHashSearch(vocab_ht,buf);
	}	    
	class_id = word_class_id[word_id];
	for (i = 0; i < ngram_len-1; i++)
	    tuple[i] = tuple[i+1];
	tuple[ngram_len-1] = class_id;
	word_freq[word_id]++;
	if (total_words < ngram_len)
	    continue;
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
 	        FILEHANDLE tmp_dump;
		sprintf(buf2,"%s/text2idn%d.%d.idngram.gz",
			tmpdir,getpid(),merge_list_num);
		push_merge_list(buf2);
                tmp_dump = z_open(buf2,"w");
		dumpidngram(NGBuf,tmp_dump,0);
                z_close(tmp_dump);
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
        FILEHANDLE tmp_dump;
	if (verbosity > 1)
	    fprintf(stderr,"\nmerging ngrams...\n");
	sprintf(buf2,"%s/text2idn%d.%d.idngram.gz",
		tmpdir,getpid(),merge_list_num);
	push_merge_list(buf2);
	tmp_dump = z_open(buf2,"w");
	dumpidngram(NGBuf,tmp_dump,0);
	z_close(tmp_dump);
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
    dump_idwfreq(word_freq,vocab_ht->nelem+1);
}

int
main(int argc, char *argv[])
{
    char *vocabfile = NULL;
    char *classfile = NULL;
    char *infile = NULL, *outfile = NULL;
    int i;
    FILEHANDLE inf,outf;

    NGBuf = New(SLMIDNgramBuffer);
    NGBuf->size = 100;  /* MB */

    verbosity = 2;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i],"-vocab"))
	    vocabfile = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-class"))
	    classfile = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-idwfreq"))
	    idwfreq_file = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-n"))
	    ngram_len = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-write_ascii"))
	    ascii_output = 1;
	else if (!strcmp(argv[i],"-one_unk"))
	    one_unk = 1;
	else if (!strcmp(argv[i],"-temp"))
	    tmpdir = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-hash"))
	    hash_size = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-max_class"))
	    max_class = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-buffer"))
	    NGBuf->size = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-verbosity") || !strcmp(argv[i],"-v"))
	    verbosity = atoi(nextarg(argc,argv,i++));
	else if (argv[i][0] == '-')
	    usage();
	else
	    break;
    }

    if (vocabfile == NULL) {
	fprintf(stderr, "text2idngram: vocabulary file missing\n");
	usage();
    }
    if (classfile == NULL) {
	fprintf(stderr, "text2idngram: class file missing\n");
	usage();
    }
    if (idwfreq_file == NULL) {
	fprintf(stderr, "text2idngram: ID class frequency file missing\n");
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
    }
    max_class = count_vocab(classfile);
    max_vocab = count_vocab(vocabfile)+max_class;
    vocab_tbl = New_N(char*,max_vocab);

    ctext2idngram(vocabfile,classfile,inf,outf);

    z_close(inf);
    z_close(outf);
    return 0;
}
