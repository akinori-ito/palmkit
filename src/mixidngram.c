/*
 * mixidngram: n-gram count level mixture
 * by Akinori Ito (aito@eie.yz.yamagata-u.ac.jp)
 *
 */
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "slm.h"

#define ST_NORMAL  0
#define ST_MIN     1
#define ST_EOF     2

int ngram_len = 3;

void
usage()
{
  fprintf(stderr,"usage: mixidngram [options] weight-1 file-1 ... weight-n file-n\n");
  fprintf(stderr,"option: -ascii_input, -ascii_output, -n <n> \n");
  exit(1);
}

int
main(int argc, char *argv[])
{
  int i,j;
  char **ngfiles;
  double *ngweights;
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
      /* argv[i] should be a mixture weight, and argv[i+1]
	 should be a filename */
      if (i+1 >= argc) {
	fprintf(stderr,"The last filename is missing\n");
	exit(1);
      }
      i++;
      n_ng++;
    }
  }
  if (n_ng == 0) {
    fprintf(stderr,"No model is specified\n");
    exit(1);
  }

  ngfiles = New_N(char*,n_ng);
  ngweights = New_N(double,n_ng);

  for (i = 1, j = 0; i < argc; i++) {
    if (argv[i][0] == '-') {
      /* skip options */
      if (argv[i][1] == 'n') i++;
      continue;
    }
    ngweights[j] = atof(argv[i]);
    ngfiles[j] = argv[i+1];
    j++;
    i++;
  }
  SLMMixIDNgram(ngram_len,ngfiles,ngweights,n_ng,FILEIO_stdout(),ascii_in,ascii_out);
  return 0;
}
