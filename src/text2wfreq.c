/*
 * text -> wfreq conversion
 *
 * by A. Ito
 * 30 August, 2000
 */
#include <stdio.h>
#include <stdlib.h>
#include "hash.h"
#include "io.h"
#include "misc.h"

void usage()
{
    printf("Usage : text2wfreq [ (-h|-hash) 10000 ]\n");
    printf("                   [ (-v|-verbosity) 2 ]\n");
    printf("                   [ .text] [.wfreq]\n");
    exit(1);
}

void
text2wfreq(int hashsize, int verbosity, FILEHANDLE inf, FILEHANDLE outf)
{
    SLMHashTable *ht;
    char buffer[256];
    int i;
    SLMHashTableElement *ht_elem;

    ht = SLMHashCreateSI(hashsize);
    while (z_getstr(inf,buffer,256) == 0) {
	ht_elem = SLMHashSearch(ht,(void*)buffer);
	if (ht_elem == NULL) {
	    SLMIntHashInsert(ht,strdup(buffer),1);
	}
	else {
	    ht_elem->valueptr = (void*)((ptr_int)ht_elem->valueptr+1);
	}
    }
    for (i = 0; i < ht->size; i++) {
	if (ht->body[i].keyptr != NULL) {
	    z_printf(outf,"%s %d\n",
		    (char*)ht->body[i].keyptr,
		    (ptr_int)ht->body[i].valueptr);
	}
    }
}
	
int
main(int argc, char *argv[])
{
    int hashsize = 10000;
    int verbosity = 2;
    char *infile = NULL, *outfile = NULL;
    int i;
    FILEHANDLE inf,outf;
    
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i],"-hash") || !strcmp(argv[i],"-h"))
	    hashsize = atoi(nextarg(argc,argv,i++));
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

    if (infile)
	inf = z_open(infile,"r");
    else
	inf = FILEIO_stdin();

    if (outfile)
	outf = z_open(outfile,"w");
    else
	outf = FILEIO_stdout();

    text2wfreq(hashsize,verbosity,inf,outf);

    z_close(inf);
    z_close(outf);
    return 0;
}

