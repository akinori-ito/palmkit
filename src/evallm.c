#include <math.h>
#include <ctype.h>
#include "misc.h"
#include <stdlib.h>
#include "slm.h"
#include "vocab.h"

/* #define DEBUG 1 */

char *arpalm_filename;
char *binlm_filename;
char *ccs_filename;
char *eval_file;
char *probs_file;
char *ann_file;

int include_unks;
SLMHashTable *context_cues;

static void
usage()
{
    fprintf(stderr,"usage: evallm -arpa .arpa\n");
    fprintf(stderr,"              [(-context|-ccs) .ccs]\n");
    fprintf(stderr,"              [-text .text]\n");
    fprintf(stderr,"              [-probs .probs]\n");
    fprintf(stderr,"              [-include_unks]\n");
    exit(1);
}


int
is_unk(SLMNgram *ng, SLMWordID id)
{
    if (id == 0) return 1;
    if (strncmp(SLMID2Word(ng,id),"<UNK>",5) == 0)
	return 1;
    return 0;
}

void
print_annotate(FILEHANDLE annf, SLMNgram *ng, SLMWordID *idarray,
	       int idarray_len, int ng_len,
	       double p, SLMBOStatus *status)
{
  int i;
  char hbuf[MAX_GRAM+1];

  z_printf(annf,"P( %s[%d] ",
	   SLMID2Word(ng,idarray[idarray_len-1]),
	   idarray[idarray_len-1]);
  if (ng_len > 1) {
    z_printf(annf,"|");
    if (SLMContextLength(ng) > 1 && SLMNgramLength(ng) == 2) {
      /* distant bigram */
      z_printf(annf," %s[%d] ",
	       SLMID2Word(ng,idarray[idarray_len-ng_len]),
	       idarray[idarray_len-ng_len]);
      for (i = idarray_len-ng_len+1; i < idarray_len-1; i++)
	z_printf(annf," * ");
    }
    else {
      for (i = idarray_len-ng_len; i < idarray_len-1; i++)
	z_printf(annf," %s[%d] ",
		 SLMID2Word(ng,idarray[i]),
		 idarray[i]);
    }
  }
  if (SLM_NgramType(ng->type) == SLM_ClassNgram)
    z_printf(annf,")\t= %g * %g\t= %g\t",
	     status->ng_prob, status->ug_prob,p);
  else
    z_printf(annf,")\t= %g\t",p);
  SLMBOStatusString(status,hbuf);
  z_printf(annf,"%s\n",hbuf);
}
    
void
calc_perplexity(SLMNgram *ng, char *file, char *probs_file, char *ann_file)
{
    SLMWordID *idarray;
    int i,n,ng_len,idarray_len;
    char buf[256];
    double p,ll;
    FILEHANDLE f,logf = NULL,annf = NULL;
    int n_unk = 0, excluded_ngram = 0, excluded_ccs = 0, id;
    SLMBOStatus status;
    int *hit;
    SLMHashTable *unk_table;
    SLMHashTableElement *hash_elem;

    idarray_len = SLMContextLength(ng)+1;
    idarray = New_N(SLMWordID, idarray_len);
    hit = New_N(int,idarray_len);
    memset(idarray,0,sizeof(SLMWordID)*idarray_len);
    memset(hit,0,sizeof(int)*idarray_len);
    unk_table = SLMHashCreateSI(SLMVocabSize(ng)/10);

    f = z_open(file,"r");

    if (probs_file)
	logf = z_open(probs_file,"w");
    if (ann_file)
	annf = z_open(ann_file,"w");
    n = 0;
    ll = 0.0;
    while (z_getstr(f,buf,256) == 0) {
	n++;
#if 0
	if (n%200 == 0) {
	  fprintf(stderr,".");
	  if (n%10000 == 0)
	    fprintf(stderr,"\n");
	  fflush(stderr);
	}
#endif
	for (i = 0; i < idarray_len-1; i++) 
	    idarray[i] = idarray[i+1];
	idarray[idarray_len-1] = id = SLMWord2ID(ng,buf);
	if (is_unk(ng,id)) {
	    /* UNK */
	    n_unk++;
	    hash_elem = SLMHashSearch(unk_table,(void*)buf);
	    if (hash_elem == NULL)
		SLMIntHashInsert(unk_table,strdup(buf),1);
	    if (!include_unks) {
		excluded_ngram++;
		continue;
	    }
	}
	else if (context_cues != NULL &&
		 SLMHashSearch(context_cues,(void*)(ptr_int)id) != NULL) {
	    /* context cues */
	    excluded_ccs++;
	    continue;
	}
	if (n < idarray_len)
	    ng_len = n;
	else
	    ng_len = idarray_len;
	p = SLMGetBOProb(ng,ng_len,idarray+idarray_len-ng_len,&status);
	if (annf) {
	    print_annotate(annf,ng,idarray,idarray_len,
			   ng_len,p,&status);
	}
	for (i = SLMNgramLength(ng)-1; i >= SLMNgramLength(ng)-ng_len; i--) {
	    if (status.hit[i] == SLM_STAT_HIT) {
		hit[i]++;
		break;
	    }
	}
	ll += log(p);
	if (logf)
	    z_printf(logf,"%g\n",p);
    }
    n = n-excluded_ngram-excluded_ccs;
    printf("Number of word = %d %s\n",n,
	   include_unks?"":"(excluding OOVs)");
    printf("%d OOVs (%4.2f%%) of %d words and %d context cues are excluded from PP calculation.\n",
	   excluded_ngram,(double)excluded_ngram/(n+excluded_ngram)*100,
	   unk_table->nelem,
	   excluded_ccs);
    printf("Total log prob = %g\n",ll);
    printf("Entropy = %g (bit)\n",-ll/n/log(2));
    printf("Perplexity = %g\n",exp(-ll/n));
    if (include_unks && unk_table->nelem > 0)
	printf("Adjusted Perplexity = %g\n",
	       exp((-ll+n_unk*log(unk_table->nelem))/n));
    for (i = SLMNgramLength(ng)-1; i >= 0; i--) {
	printf("Number of %d-grams hit = %d (%.2f%%)\n",
	       i+1,hit[i],(double)hit[i]/n*100);
    }
    if (logf)
	z_close(logf);
    if (annf)
	z_close(annf);
    z_close(f);
    Free(idarray);
    Free(hit);
    SLMHashDestroy(unk_table);
}

void
calc_Gf(SLMNgram *ng, char *file, double t1, double t2, char *ann_file)
{
    SLMWordID *idarray;
    int i,n,ng_len,maxw;
    char buf[256];
    double p,ll;
    FILEHANDLE f,annf = NULL;
    int excluded_ngram = 0, excluded_ccs = 0, id, counter_id = 0;
    SLMBOStatus status;
    int *hit;
    double s,s2,stddev,ave,gf;

    idarray = New_N(SLMWordID, SLMNgramLength(ng));
    hit = New_N(int,SLMNgramLength(ng));
    memset(idarray,0,sizeof(SLMWordID)*SLMNgramLength(ng));
    memset(hit,0,sizeof(int)*SLMNgramLength(ng));

    if (ann_file)
	annf = z_open(ann_file,"w");

    /* first pass */
    n = 0;
    s = 0.0;
    s2 = 0.0;
    f = z_open(file,"r");

    while (z_getstr(f,buf,256) == 0) {
	n++;
	for (i = 0; i < SLMNgramLength(ng)-1; i++) 
	    idarray[i] = idarray[i+1];
	idarray[SLMNgramLength(ng)-1] = id = SLMWord2ID(ng,buf);
	if (!include_unks && is_unk(ng,id)) {
	    /* UNK */
	    excluded_ngram++;
	    continue;
	}
	else if (context_cues != NULL &&
		 SLMHashSearch(context_cues,(void*)(ptr_int)id) != NULL) {
	    /* context cues */
	    excluded_ccs++;
	    continue;
	}
	if (n < SLMNgramLength(ng))
	    ng_len = n;
	else
	    ng_len = SLMNgramLength(ng);
	p = log(SLMGetBOProb(ng,ng_len,idarray+SLMNgramLength(ng)-ng_len,&status));
	s += p;
	s2 += p*p;
    }
    n = n-excluded_ngram-excluded_ccs;
    ave = s/n;
    stddev = sqrt(s2/n-ave*ave);
    ll = s;
    z_close(f);

    /* second pass */
    gf = 0.0;
    f = z_open(file,"r");
    n = 0;

    while (z_getstr(f,buf,256) == 0) {
	n++;
	for (i = 0; i < SLMNgramLength(ng)-1; i++) 
	    idarray[i] = idarray[i+1];
	id = SLMWord2ID(ng,buf);
	if (!include_unks && is_unk(ng,id)) {
	    /* UNK */
	    continue;
	}
	else if (context_cues != NULL &&
		 SLMHashSearch(context_cues,(void*)(ptr_int)id) != NULL) {
	    /* context cues */
	    continue;
	}
	if (n < SLMNgramLength(ng))
	    ng_len = n;
	else
	    ng_len = SLMNgramLength(ng);
	if (SLM_NgramType(ng->type) == SLM_WordNgram)
	    maxw = ng->n_unigram;
	else
	    maxw = ng->n_word;
        s = 0.0;  /* prob of the right word */
        s2 = 0.0; /* maximum counter prob */
	for (i = ng->first_id; i < maxw; i++) {
	    idarray[SLMNgramLength(ng)-1] = i;
	    p = SLMGetBOProb(ng,ng_len,idarray+SLMNgramLength(ng)-ng_len,&status);
	    if (i == id)
		s = p;
	    else if (p > s2) {
		s2 = p;
		counter_id = i;
	    }
	}
	idarray[SLMNgramLength(ng)-1] = id;
	if (annf) {
	    z_printf(annf,"%s(%d) %g %s(%d) %g\n",
		     SLMID2Word(ng,id),id,s,
		     SLMID2Word(ng,counter_id),
		     counter_id,s2);
	}
	p = (log(s)-log(s2))/stddev;
	if (p > t1) p = t1;
	else if (p < t2) p = t2;
	gf += p;
	if (n % 100 == 0) {
	    putchar('.');
	    fflush(stdout);
	}
    }
    z_close(f);
    n = n-excluded_ngram-excluded_ccs;
    gf /= n;

    printf("\nNumber of word = %d\n",n);
    printf("%d OOVs (%4.2f%%) and %d context cues are excluded from PP calculation.\n",
	   excluded_ngram,(double)excluded_ngram/n*100,excluded_ccs);
    printf("Total log prob = %g\n",ll);
    ll /= n;
    printf("Entropy = %g (bit)\n",-ll/log(2));
    printf("Perplexity = %g\n",exp(-ll));
    printf("Upper limit = %f\n",t1);
    printf("Lower limit = %f\n",t2);
    printf("G(f) = %f\n",gf);
    if (annf)
	z_close(annf);
    Free(idarray);
}

int
get_next_token(char **buf, char *token)
{
    int i = 0;
    while (**buf && isspace((int)**buf))
	(*buf)++;
    while (**buf && !isspace((int)**buf))
	token[i++] = *((*buf)++);
    token[i] = '\0';
    return (i != 0);
}

void
int_usage()
{
    printf("COMMANDS:\n");
    printf("perplexity or pp: Calculate perplexity.\n");
    printf("usage: (pp | perplexity)   -text .text\n");
    printf("                         [ -include_unks ]\n");
    printf("                         [ -probs .fprobs ]\n");
    printf("\n");
    printf("quit: Terminate evallm.\n");
    printf("usage: quit\n");
}

void
validate(SLMNgram *ng, int len, char **context)
{
    SLMWordID idarray[MAX_GRAM];
    int i,n;
    double s = 0.0,p;
    SLMBOStatus status;
#ifdef DEBUG
    FILE *lg = fopen("zzzlog","w");
    char hbuf[MAX_GRAM];
#endif

    for (i = 0; i < len; i++)
      idarray[i] = SLMWord2ID(ng,context[i]);
    if (SLM_NgramType(ng->type) == SLM_WordNgram)
	n = ng->first_id+ng->n_unigram;
    else
	n = ng->first_id+ng->n_word;
    for (i = ng->first_id; i < n; i++) {
	idarray[SLMContextLength(ng)] = i;
	p = SLMGetBOProb(ng,SLMContextLength(ng)+1,idarray,&status);
	s += p;
#ifdef DEBUG
	SLMBOStatusString(&status,hbuf);
	fprintf(lg,"%d %s %g %s",i,SLMID2Word(ng,i),p,hbuf);
	if (SLM_NgramType(ng->type) == SLM_ClassNgram) {
	    fprintf(lg," %d %g %g\n",ng->class_id[i],status.ng_prob,status.ug_prob);
	}
	fprintf(lg,"\n");
#endif
    }
    printf("SUM_w P( w |");
    for (i = 0; i < len; i++)
	printf(" %s",SLMID2Word(ng,idarray[i]));
    printf(") = %g\n",s);
#ifdef DEBUG
    fclose(lg);
#endif
}

void
calc_one(SLMNgram *ng, int len, char **context)
{
    SLMWordID idarray[MAX_GRAM];
    int i;
    double p;
    SLMBOStatus status;
    FILEHANDLE h = FILEIO_stdout();

    for (i = 0; i < len; i++)
      idarray[i] = SLMWord2ID(ng,context[i]);
    p = SLMGetBOProb(ng,len,idarray,&status);
    print_annotate(h,ng,idarray,len,ng->ngram_len,p,&status);
    z_close(h);
}

void
interactive(SLMNgram *ng)
{
    char buf[512],buf2[256],*p;
    char *context[MAX_GRAM];

    for (;;) {
    top:
	printf("evallm : ");
	fflush(stdout);
	if (fgets(buf,512,stdin) == NULL)
	    break;
	p = buf;
	get_next_token(&p,buf2);
	if (!strcmp(buf2,"perplexity") || !strcmp(buf2,"pp")) {
	    eval_file = probs_file = ann_file = NULL;
	    include_unks = 0;
	    while (get_next_token(&p,buf2)) {
		if (!strcmp(buf2,"-text")) {
		    get_next_token(&p,buf2);
		    eval_file = GC_strdup(buf2);
		}
		else if (!strcmp(buf2,"-probs")) {
		    get_next_token(&p,buf2);
		    probs_file = GC_strdup(buf2);
		}
		else if (!strcmp(buf2,"-annotate")) {
		    get_next_token(&p,buf2);
		    ann_file = GC_strdup(buf2);
		}
		else if (!strcmp(buf2,"-include_unks"))
		    include_unks = 1;
		else {
		    printf("perplexity: unknown option %s\n\n",buf2);
		    int_usage();
		    goto top;
		}
	    }
	    if (eval_file == NULL) {
		printf("perplexity: -text missing\n");
		goto top;
	    }
	    calc_perplexity(ng,eval_file,probs_file,ann_file);
	}
	else if (!strcmp(buf2,"gf")) {
	    double t1 = 1.0, t2 = -1.0;
	    eval_file = ann_file = NULL;
	    include_unks = 0;
	    while (get_next_token(&p,buf2)) {
		if (!strcmp(buf2,"-text")) {
		    get_next_token(&p,buf2);
		    eval_file = GC_strdup(buf2);
		}
		else if (!strcmp(buf2,"-annotate")) {
		    get_next_token(&p,buf2);
		    ann_file = GC_strdup(buf2);
		}
		else if (!strcmp(buf2,"-include_unks"))
		    include_unks = 1;
		else if (!strcmp(buf2,"-upper")) {
		    get_next_token(&p,buf2);
		    t1 = atof(buf2);
		}
		else if (!strcmp(buf2,"-lower")) {
		    get_next_token(&p,buf2);
		    t2 = atof(buf2);
		}
		else {
		    printf("gf: unknown option %s\n\n",buf2);
		    int_usage();
		    goto top;
		}
	    }
	    if (eval_file == NULL) {
		printf("gf: -text missing\n");
		goto top;
	    }
	    calc_Gf(ng,eval_file,t1,t2,ann_file);
	}
	else if (!strcmp(buf2,"validate")) {
	    int i = 0,j;
	    while (get_next_token(&p,buf2)) {
		if (i >= SLMNgramLength(ng)-1) {
		    printf("context too long\n");
		    break;
		}
		context[i++] = strdup(buf2);
	    }
	    validate(ng,i,context);
	    for (j = 0; j < i; j++)
		Free(context[j]);
	}
	else if (!strcmp(buf2,"calc")) {
	    int i = 0,j;
	    while (get_next_token(&p,buf2)) {
		if (i >= SLMNgramLength(ng)) {
		    printf("n-gram too long\n");
		    break;
		}
		context[i++] = strdup(buf2);
	    }

	    calc_one(ng,i,context);
	    for (j = 0; j < i; j++)
		Free(context[j]);
	}
	else if (!strcmp(buf2,"quit")) {
	    break;
	}
	else
	    int_usage();
    }
    printf("evallm : done.\n");
}
    

void
evallm()
{
    SLMNgram *ng;

    if (arpalm_filename)
	ng = SLMReadLM(arpalm_filename,SLM_LM_ARPA,2);
    else
	ng = SLMReadLM(binlm_filename,SLM_LM_BINARY,2);
	
    if (ng == NULL) {
	fprintf(stderr,"Can't open LM file %s\n",arpalm_filename);
	exit(3);
    }
    if (ccs_filename)
	context_cues = readContextCues(ng,ccs_filename);
    if (eval_file)
	calc_perplexity(ng,eval_file,probs_file,ann_file);
    else
	interactive(ng);
}

int
main(int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i],"-arpa"))
	    arpalm_filename = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-binary"))
	    binlm_filename = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-context") ||
		 !strcmp(argv[i],"-ccs"))
	    ccs_filename = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-text"))
	    eval_file = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-probs"))
	    probs_file = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-annotate"))
	    ann_file = nextarg(argc,argv,i++);
	else if (!strcmp(argv[i],"-include_unks"))
	    include_unks = 1;
	else
	    usage();
    }
    if (arpalm_filename == NULL && binlm_filename == NULL)
        usage();
    evallm();
    return 0;
}
