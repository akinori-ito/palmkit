/*
 * reverseidngram:
 * by Akinori Ito (aito@eie.yz.yamagata-u.ac.jp)
 *
 */
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "slm.h"
#include "idngram.h"
#include "remove_tmp.h"

SLMIDNgramBuffer *NGBuf;
int ascii_input, ascii_output;
char *tmpdir = "/usr/tmp";

void usage()
{
    printf("Usage : reverseidngram \n");
    printf("                    [ -n 3 ]\n");
    printf("                    [ -ascii_input ]\n");
    printf("                    [ -ascii_output ]\n");
    printf("                    [ -buffer 100 ]\n");
    printf("                    [ -verbosity 2 ]\n");
    printf("                    [.idngram1] [.idngram2]\n");
    exit(1);
}
    
void 
reverseidngram(FILEHANDLE inf, FILEHANDLE outf)
{
  int i;
  int processed_ngram = 0;
  FILEHANDLE df;
  SLMNgramCount *nc;
  char buf2[256];

  NGBuf->ngram_len = ngram_len;
  NGBuf->elem_size = ngram_len+2;
  NGBuf->elem_byte = sizeof(SLMWordID)*NGBuf->elem_size;
  NGBuf->real_size = NGBuf->size*1024*1024/NGBuf->elem_byte;
  NGBuf->buffer = New_N(SLMWordID,NGBuf->real_size);

  nc = SLMNewNgramCount(ngram_len);

  if (verbosity > 1) {
    fprintf(stderr,"20,000 n-grams processed for each \".\", 1,000,000 for each line.\n");
  }

  while (SLMReadNgramCount(ngram_len, inf, nc, ascii_input)) {
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
    for (i = 0; i < ngram_len; i++)
      NGBuf->buffer[NGBuf->pos+i] = nc->word_id[ngram_len-i-1];
    NGBuf->buffer[NGBuf->pos+ngram_len] = ((nc->count >> 16)&0xffff);
    NGBuf->buffer[NGBuf->pos+ngram_len+1] = (nc->count & 0xffff);
    NGBuf->pos += NGBuf->elem_size;
    if (NGBuf->pos > NGBuf->real_size-NGBuf->elem_size) {
      sprintf(buf2,"%s/revidn%d.%d.idngram.gz",
	      tmpdir,getpid(),merge_list_num);
      RemoveAfter(buf2);
      push_merge_list(buf2);
      df = z_open(buf2,"w");
      dumpidngram(NGBuf,df,0);
      z_close(df);
      if (verbosity > 1) {
	fprintf(stderr,"\nsorted N-gram written into %s\n",buf2);
      }
      NGBuf->pos = 0;
    }
  }
  if (verbosity > 1)
    fprintf(stderr,"%d N-grams has been processed.\n",processed_ngram);
  if (merge_list_num > 0) {
    if (verbosity > 1)
      fprintf(stderr,"\nmerging ngrams...\n");
    sprintf(buf2,"%s/revidn%d.%d.idngram.gz",
	    tmpdir,getpid(),merge_list_num);
    RemoveAfter(buf2);
    push_merge_list(buf2);
    df = z_open(buf2,"w");
    dumpidngram(NGBuf,df,0);
    z_close(df);
    SLMMergeIDNgram(ngram_len,merge_list,merge_list_num,outf,0,ascii_output);
    for (i = 0; i < merge_list_num; i++) {
      sprintf(buf2,"%s/revidn%d.%d.idngram.gz",
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
    char *infile = NULL, *outfile = NULL;
    int i;
    FILEHANDLE inf,outf;

    NGBuf = New(SLMIDNgramBuffer);
    NGBuf->size = 100;  /* MB */

    verbosity = 2;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i],"-n"))
	    ngram_len = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-ascii_input"))
	    ascii_input = 1;
	else if (!strcmp(argv[i],"-ascii_output"))
	    ascii_output = 1;
	else if (!strcmp(argv[i],"-temp"))
	    tmpdir = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-buffer"))
	    NGBuf->size = atoi(nextarg(argc,argv,i++));
	else if (!strcmp(argv[i],"-verbosity") || !strcmp(argv[i],"-v"))
	    verbosity = atoi(nextarg(argc,argv,i++));
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
	fprintf(stderr,"Temp directory:  %s\n",tmpdir);
	fprintf(stderr,"N                %d\n",ngram_len);
    }
    TrapAllSignals();
    reverseidngram(inf,outf);

    z_close(inf);
    z_close(outf);
    return 0;
}
