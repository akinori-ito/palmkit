/*
 * margeidwfreq:
 * by Akinori Ito (aito@eie.yz.yamagata-u.ac.jp)
 *
 */
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "slm.h"
#include "misc.h"

#define ST_NORMAL  0
#define ST_MIN     1
#define ST_EOF     2

int ngram_len = 3;

void
usage()
{
  fprintf(stderr,"usage: mergeidwfreq [options] file-1 ... file-n\n");
  fprintf(stderr,"option: -ascii_input, -ascii_output, -n <n> \n");
  exit(1);
}

void
mergeidwfreq(int ngram_len, char **ngfiles, int n_ng,
	     FILEHANDLE outf,int ascii_in, int ascii_out)
{
    int i,j,nword = 0,nw;
    int cl,c;
    FILEHANDLE *infile;

    infile = New_N(FILEHANDLE, n_ng);
    for (i = 0; i < n_ng; i++) {
	infile[i] = z_open(ngfiles[i],"r");
	nw = read_long(infile[i],ascii_in);
	if (nword == 0)
	    nword = nw;
	else if (nw != nword) {
	    fprintf(stderr,"mergeidwfreq ERROR: vocabulary size inconsistent\n");
	    fprintf(stderr,"  vocab. size in file %s is %d, while previously observed one is %d\n",ngfiles[i],nw,nword);
	    exit(1);
	}
    }
    write_long(outf,nword,ascii_out);
    for (i = 0; i < nword; i++) {
	nw = 0;
	cl = -1;
	for (j = 0; j < n_ng; j++) {
	    c = read_ushort(infile[j],ascii_in);
	    if (cl < 0)
		cl = c;
	    else if (c != cl) {
		fprintf(stderr,"mergeidwfreq ERROR: class info inconsistent\n");
		fprintf(stderr,"  class of word %d in file %s is %d, while\n",
			i,ngfiles[j],c);
		fprintf(stderr,"previously observed one is %d\n",cl);
		exit(1);
	    }
	    nw += read_long(infile[j],ascii_in);
	}
	write_ushort(outf,cl,ascii_out);
	write_long(outf,nw,ascii_out);
	if (ascii_out)
	    fputc('\n',outf->f);
    }
    for (i = 0; i < n_ng; i++)
	z_close(infile[i]);
    z_close(outf);
}

	    

int
main(int argc, char *argv[])
{
  int i,j;
  char **ngfiles;
  int ascii_in = 0, ascii_out = 0;
  int n_ng;

  if (argc == 1) usage();
  /* First, check all arguments */
  n_ng = 0;
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      /* options */
      if (!strcmp(argv[i],"-n")) {
	if (!isdigit((int)argv[i+1][0]))
	  usage();
	ngram_len = atoi(argv[i+1]);
	i++;
	continue;
      }
      else if (!strcmp(argv[i],"-ascii_input"))
	ascii_in = 1;
      else if (!strcmp(argv[i],"-ascii_output"))
	ascii_out = 1;
    }
    else {
      /* not option */
      n_ng++;
    }
  }
  if (n_ng == 0) {
    fprintf(stderr,"No model is specified\n");
    exit(1);
  }

  ngfiles = New_N(char*,n_ng);

  for (i = 1, j = 0; i < argc; i++) {
    if (argv[i][0] == '-') {
      /* skip options */
      if (argv[i][1] == 'n') i++;
      continue;
    }
    ngfiles[j] = argv[i];
    j++;
  }
  mergeidwfreq(ngram_len,ngfiles,n_ng,FILEIO_stdout(),ascii_in,ascii_out);
  return 0;
}
