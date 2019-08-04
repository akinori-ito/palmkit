/*
 * ngram2mgram:
 * by Akinori Ito (aito@eie.yz.yamagata-u.ac.jp)
 *
 */
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "slm.h"

int no_ngram = 0;

void
usage()
{
  fprintf(stderr,"usage: ngram2mgram -n length -m length [options] file1.idngram file2.idngram\n");
  fprintf(stderr,"option: -ascii_input, -ascii_output\n");
  exit(1);
}

void
ngram2mgram(char *infile, char *outfile, int n_len, int m_len,
	    int ascii_in, int ascii_out)
{
  FILEHANDLE inf,outf;
  SLMNgramCount *nc,*nc0;
  int i;

  if (infile == NULL)
    inf = FILEIO_stdin();
  else
    inf = z_open(infile,"r");
  if (outfile == NULL)
    outf = FILEIO_stdout();
  else
    outf = z_open(outfile,"w");
  nc0 = SLMNewNgramCount(m_len);
  nc = SLMNewNgramCount(n_len);
  for (i = 0; i < m_len; i++)
    nc0->word_id[i] = 0;
  nc0->count = 0;

  while (SLMReadNgramCount(n_len,inf,nc,ascii_in)) {
    int identical = 1;
    for (i = 0; i < m_len; i++) {
      if (nc0->word_id[i] != nc->word_id[i]) {
	identical = 0;
	break;
      }
    }
    if (identical)
      nc0->count += nc->count;
    else {
      if (nc0->count > 0) {
	SLMPrintNgramCount(m_len,nc0,outf,ascii_out);
	no_ngram++;
      }
      for (i = 0; i < m_len; i++)
	nc0->word_id[i] = nc->word_id[i];
      nc0->count = nc->count;
    }
  }
  if (nc0->count > 0) {
    SLMPrintNgramCount(m_len,nc0,outf,ascii_out);
    no_ngram++;
  }
  z_close(inf);
  z_close(outf);
}


int
main(int argc, char *argv[])
{
  int i;
  int ascii_in = 0, ascii_out = 0;
  char *infile = NULL, *outfile = NULL;
  int n_len = 0,m_len = 0;

  if (argc == 1) usage();
  /* First, check all arguments */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      /* options */
      if (!strcmp(argv[i],"-n")) {
	if (!isdigit((int)argv[i+1][0]))
	  usage();
	n_len = atoi(argv[i+1]);
	i++;
	continue;
      }
      else if (!strcmp(argv[i],"-m")) {
	if (!isdigit((int)argv[i+1][0]))
	  usage();
	m_len = atoi(argv[i+1]);
	i++;
	continue;
      }
      else if (!strcmp(argv[i],"-ascii_input"))
	ascii_in = 1;
      else if (!strcmp(argv[i],"-ascii_output"))
	ascii_out = 1;
      else
	usage();
    }
    else {
      if (infile == NULL)
	infile = argv[i];
      else if (outfile == NULL)
	outfile = argv[i];
      else
	usage();
    }
  }
  if (n_len == 0 || m_len == 0) {
    fprintf(stderr,"Error: please specify both -n and -m\n");
    usage();
  }
  if (n_len < m_len) {
    fprintf(stderr,"Error: n (%d) must be greater than or equal to m (%d)\n",
	    n_len,m_len);
    exit(1);
  }
  
  ngram2mgram(infile,outfile,n_len,m_len,ascii_in,ascii_out);

  return 0;
}
