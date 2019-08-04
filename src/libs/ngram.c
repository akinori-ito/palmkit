#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include "libs.h"
#include "io.h"
#include "misc.h"
#include "hash.h"
#include "vocab.h"
#include "ngram.h"
#include "net.h"
#include "ngramd.h"

#define SLM_SOCK(ng) ((ng)->socket)
#define IS_DISTANT_BIGRAM(ng) ((ng)->ngram_len == 2 && (ng)->context_len > 1)

static SLMNgram*
format_error(int n)
{
    fprintf(stderr,"SLMReadARPA ERROR: LM format error while reading %d-gram\n",n);
    return NULL;
}

static int
SLMNgramNodeCompare(const void *n1, const void *n2)
{
    return ((SLMNgramNode*)n1)->id - ((SLMNgramNode*)n2)->id;
}

static int
SLMNgramLeafCompare(const void *n1, const void *n2)
{
    return ((SLMNgramLeaf*)n1)->id - ((SLMNgramLeaf*)n2)->id;
}

static void
putNgramNodeData(SLMNgram *ng, SLMNgramNode *nn, int nelem, SLMWordID *idarray, int level, int n, int pos)
{
    SLMNgramNode ref,*nd;
    int i;

    ref.id = idarray[level-1];
    nd = bsearch(&ref,nn,nelem,sizeof(SLMNgramNode),SLMNgramNodeCompare);
    if (nd == NULL) {
	fprintf(stderr,"SLMReadLM ERROR: Inconsistent n-gram found: ");
	for (i = 0; i < n; i++)
	    fprintf(stderr,SLMWordID_FMT " ",idarray[i]);
	fprintf(stderr,"\n");
	/*	exit(2); */
	return;
    }
    if (level == n-1) {
	/* register new node */
	if (nd->nextpos < 0)
	    nd->nextpos = pos;
	nd->nelem++;
    }
    else {
	putNgramNodeData(ng,&ng->node[level][nd->nextpos],nd->nelem,
			 idarray,level+1,n,pos);
    }
}

static SLMNgram *
SLMNewLM()
{
    SLMNgram *ng;

    ng = New(SLMNgram);
    memset(ng,0,sizeof(SLMNgram));

    ng->type = (SLM_WordNgram|SLM_ONE_UNK|SLM_UNK_IN_TRAIN|SLM_WORD_VOCAB_OPEN);
    ng->first_id = 0;
    ng->weight = 1.0;
    ng->next_lm = NULL;
    ng->delegate = NULL;
    ng->delimiter = SLM_DEFAULT_DELIMITER;
    ng->socket = -1;

    return ng;
}

static SLMNgram *
SLMReadLM_arpa(char *filename,int verbosity)
{
    FILEHANDLE f = z_open(filename,"r");
    char buf[256],buf2[256],*a;
    SLMNgram *ng;
    int4 ngram_size[MAX_GRAM];  
    int i,j,m,n;
    int ngram_len;
    int distance = 0;
    int word_num;
    SLMNgramNode **nodes;
    SLMNgramLeaf *leaves;
    float prob,alpha;
    char **vocab;
    SLMHashTable *vocab_ht;
    SLMWordID idarray[MAX_GRAM];

    ng = SLMNewLM();
    ng->filename = strdup(filename);

    while ((a = z_gets(buf,256,f)) != NULL) {
	if (strncmp(buf,"\\distance=",10) == 0)
	    sscanf(buf+10,"%d",&distance);
	if (strncmp(buf,"\\data\\",6) == 0)
	    break;
    }
    if (a == NULL) {
	fprintf(stderr,"SLMReadLM ERROR: no \\data\\ found\n");
	return NULL;
    }
    ngram_len = 0;
    while ((a = z_gets(buf,256,f)) != NULL) {
	if (strncmp(buf,"ngram ",6) == 0) {
	    sscanf(buf+6,"%d=%d",&n,&m);
	    if (n < 1 || n > MAX_GRAM) {
		fprintf(stderr,"SLMReadLM ERROR: Can't handle this LM's n-gram length; limit = %d\n",
			MAX_GRAM);
		return NULL;
	    }
	    ngram_size[n-1] = m;
	    if (n > ngram_len)
		ngram_len = n;
	}
	else if (buf[0] == '\\')
	    break;
    }
    if (a == NULL) {
	fprintf(stderr,"SLMReadLM ERROR: file ended while scanning \"ngram n=size\" section\n");
	return NULL;
    }
    /* check for distant-bigram */
    if (distance > 0 && ngram_len != 2) {
      fprintf(stderr,"SLMReadLM ERROR: distance=%d and n=%d: ngram size must be 2\n",distance,ngram_len);
      return NULL;
    }
    nodes = New_N(SLMNgramNode*,ngram_len-1);
    for (i = 0; i < ngram_len-1; i++)
	nodes[i] = New_N(SLMNgramNode,ngram_size[i]+1);
    leaves = New_N(SLMNgramLeaf,ngram_size[ngram_len-1]);
    vocab = New_N(char*,ngram_size[0]+1);
    vocab_ht = SLMHashCreateSI(ngram_size[0]*3/2);

    ng->ngram_len = ngram_len;
    if (distance == 0)
      ng->context_len = ngram_len-1;
    else
      ng->context_len = distance+1;
    ng->n_unigram = ngram_size[0];
    ng->node = nodes;
    ng->leaf = leaves;
    ng->vocab = vocab;
    ng->vocab_ht = vocab_ht;
#ifdef NG_CACHE
    ng->hist = New_N(SLMNgramSearchHist,ngram_len-1);
    for (i = 0; i < ngram_len-1; i++) {
	ng->hist[i].id = SLM_NONWORD;
	ng->hist[i].node = NULL;
    }
#endif

    /* ngram start */
    n = 1;
    word_num = 0;
    do {
	sprintf(buf2,"\\%d-grams:",n);
	if (strncmp(buf,buf2,strlen(buf2)) != 0) {
	    fprintf(stderr,"SLMReadLM ERROR: \"\\%d-grams:\" not found\n",n);
	    z_close(f);
	    return NULL;
	}
	if (verbosity > 0) {
	    fputs(buf2+1,stderr);
	    fflush(stderr);
	}
	for (i = 0; i < ngram_size[n-1]; i++) {
	    if (verbosity > 0 && i%20000 == 0) {
		fputc('.',stderr);
		fflush(stderr);
	    }
	    if (n < ngram_len) {
	        if (z_getfloat(f,&prob) != 0) {
		    z_close(f);
		    return format_error(n);
		}
		if (n == 1) {
		    z_getstr(f,buf2,256);
		    if (word_num == 0) {
			if (strcmp(buf2,"<UNK>") != 0) {
			    vocab[word_num] = strdup("<UNK>");
			    vocab[++word_num] = strdup(buf2);
			    SLMIntHashInsert(vocab_ht,vocab[word_num],word_num);
			    SLM_Set_WORD_VOCAB(ng->type,SLM_WORD_VOCAB_CLOSED);
			    ng->first_id = 1;
			    if (verbosity > 1) {
				fprintf(stderr,"<UNK> not found: vocab_type is set to %s\n",
					vocab_type_name(ng->type));
			    }
			}
			else
			    vocab[word_num] = strdup(buf2);
		    }
		    else {
			vocab[word_num] = strdup(buf2);
			SLMIntHashInsert(vocab_ht,vocab[word_num],word_num);
		    }
		    z_getfloat(f,&alpha);
		    nodes[0][i].id = word_num;
		    nodes[0][i].prob = pow(10.0,prob);
		    nodes[0][i].alpha = pow(10.0,alpha);
		    nodes[0][i].nextpos = -1;
		    nodes[0][i].nelem = 0;
		    word_num++;
		}
		else { /* 1 < n < ngram_len */
		    for (j = 0; j < n; j++) {
			z_getstr(f,buf2,256);
			idarray[j] = SLMIntHashSearch(vocab_ht,buf2);
		    }
		    z_getfloat(f,&alpha);
		    nodes[n-1][i].id = idarray[n-1];
		    nodes[n-1][i].prob = pow(10.0,prob);
		    nodes[n-1][i].alpha = pow(10.0,alpha);
		    nodes[n-1][i].nextpos = -1;
		    nodes[n-1][i].nelem = 0;
		    putNgramNodeData(ng,ng->node[0],ng->n_unigram,idarray,1,n,i);
		}
	    }
	    else { /* n == ngram_len */
	        if (z_getfloat(f,&prob) != 0) {
		    z_close(f);
		    return format_error(n);
		}
		for (j = 0; j < n; j++) {
		    z_getstr(f,buf2,256);
		    idarray[j] = SLMIntHashSearch(vocab_ht,buf2);
		}
		leaves[i].id = idarray[n-1];
		leaves[i].prob = pow(10.0,prob);
		putNgramNodeData(ng,ng->node[0],ng->n_unigram,idarray,1,n,i);
	    }
	}
	
	while ((a = z_gets(buf,256,f)) != NULL) {
	    if (buf[0] == '\\')
		break;
	}
	if (a == NULL) {
	    fprintf(stderr,"SLMReadLM ERROR: file ended while scanning \"\\%d-grams:\" or \"\\end\\\"\n",n+1);
	    z_close(f);
	    return NULL;
	}
	n++;
    } while (strncmp(buf,"\\end\\",5) != 0);
    if (verbosity > 0)
	fputc('\n',stderr);

    /* read additional class info */
    while (z_gets(buf,256,f) != NULL) {
	if (strncmp(buf,"\\class\\",7) == 0) {
	    if (verbosity > 1)
		fprintf(stderr,"Reading additional class ngram info...\n");
	    SLM_Set_NgramType(ng->type,SLM_ClassNgram);
	    break;
	}
    }
    if (SLM_NgramType(ng->type) == SLM_WordNgram) {
        z_close(f);
	return ng;
    }
    ng->class_ht = ng->vocab_ht;
    ng->class_sym= ng->vocab;
    ng->first_class_id = ng->first_id;
    ng->first_id = 0;
    if (SLM_WORD_VOCAB(ng->type) == SLM_WORD_VOCAB_OPEN)
	SLM_Set_CLASS_VOCAB(ng->type,SLM_CLASS_VOCAB_OPEN);
    else
	SLM_Set_CLASS_VOCAB(ng->type,SLM_CLASS_VOCAB_CLOSED);
    SLM_Set_WORD_VOCAB(ng->type,SLM_WORD_VOCAB_OPEN);
    SLM_Set_N_UNK(ng->type,SLM_CLASS_UNK);
	
    z_getint(f,&ng->n_word);
    ng->vocab_ht = SLMHashCreateSI(ng->n_word*3/2);
    ng->vocab = New_N(char*,ng->n_word);
    ng->c_uniprob = New_N(float,ng->n_word);
    ng->class_id = New_N(SLMWordID,ng->n_word);
    for (i = 0; i < ng->n_word; i++) {
	z_getstr(f,buf,256);
	z_getstr(f,buf2,256);
	z_getfloat(f,&prob);
	if (i == 0 && strcmp(buf2,"<UNK>") != 0) {
	    /* first word is not UNK */
	    SLM_Set_N_UNK(ng->type,SLM_NO_UNK);
	    SLM_Set_WORD_VOCAB(ng->type,SLM_WORD_VOCAB_CLOSED);
	    ng->first_id = 1;
	    ng->vocab[0] = strdup("<UNK>");
	    i++;
	    if (verbosity > 2) {
		fprintf(stderr,"\n<UNK> not found: vocab_type is set to <%s>\n",
			vocab_type_name(ng->type));
	    }
	}
	ng->class_id[i] = SLMIntHashSearch(ng->class_ht,buf);
	ng->vocab[i] = strdup(buf2);
	SLMIntHashInsert(ng->vocab_ht,ng->vocab[i],i);
	ng->c_uniprob[i] = pow(10.0,prob);
    }
    z_close(f); /* 13,Jul,2005 kato */
    return ng;
}

int4
SLMd2l(double x)
{
    double scale = (double)0x7fffffffL/1000;
    double z = x*scale;
    int4 r;
    if (z > 0x7fffffff)
	r = 0x80000000;
    else
	r = z;
    return r;
}

double
SLMl2d(int4 x)
{
    double scale = (double)1000/0x7fffffffL;
    return (double)x*scale;
}

#define l2d(x) SLMl2d(x)

static SLMNgram *
SLMReadLM_binary(char *filename,int verbosity)
{
    FILEHANDLE f = z_open(filename,"r");
    char buf[SLM_BINLM_HEADER_SIZE];
    SLMNgram *ng;
    int4 ngram_size[MAX_GRAM];  
    int i,j,m,n;
    int ngram_len;
    SLMNgramNode **nodes;
    SLMNgramLeaf *leaves;
    float prob,alpha;
    char **vocab;
    SLMHashTable *vocab_ht;
    SLMWordID idarray[MAX_GRAM];
    int4 l;
    uint2 s;
    uint2 distance = 0;

    ng = SLMNewLM();
    ng->filename = strdup(filename);

    z_read(buf,sizeof(char),SLM_BINLM_HEADER_SIZE_V2,f); /* read header */
    if (strcmp(buf,SLM_BINLM_HEADER_MSG_V1) == 0) {
	/* version 1 header */
	z_read(buf,sizeof(char),SLM_BINLM_HEADER_SIZE_V1-SLM_BINLM_HEADER_SIZE_V2,f);
    }
    else if (strcmp(buf,SLM_BINLM_HEADER_MSG_V2) == 0) {
	/* version 2 header */
	z_read(&s,sizeof(uint2),1,f);
	distance = ntohs(s);
    }
    else {
	fprintf(stderr,"SLMReadLM: Can't handle this version: %s\n",buf);
	z_close(f);
	return NULL;
    }

    z_read(&s,sizeof(short),1,f); /* ngram length */
    ngram_len = ntohs(s);
    if (distance > 0 && ngram_len != 2) {
      fprintf(stderr,"SLMReadLM ERROR: distance=%d and n=%d: ngram size must be 2\n",distance,ngram_len);
      z_close(f);
      return NULL;
    }
    /* number of ngram */
    for (i = 0; i < ngram_len; i++) {
	z_read(&l,sizeof(int4),1,f);
	ngram_size[i] = ntohl(l);
    }
    nodes = New_N(SLMNgramNode*,ngram_len-1);
    for (i = 0; i < ngram_len-1; i++)
	nodes[i] = New_N(SLMNgramNode,ngram_size[i]+1);
    leaves = New_N(SLMNgramLeaf,ngram_size[ngram_len-1]);
    vocab = New_N(char*,ngram_size[0]+1);
    vocab_ht = SLMHashCreateSI(ngram_size[0]*3/2);

    ng->ngram_len = ngram_len;
    if (distance == 0)
	ng->context_len = ngram_len-1;
    else
	ng->context_len = distance+1;
    ng->n_unigram = ngram_size[0];
    ng->node = nodes;
    ng->leaf = leaves;
    ng->vocab = vocab;
    ng->vocab_ht = vocab_ht;
#ifdef NG_CACHE
    ng->hist = New_N(SLMNgramSearchHist,ngram_len-1);
    for (i = 0; i < ngram_len-1; i++) {
	ng->hist[i].id = SLM_NONWORD;
	ng->hist[i].node = NULL;
    }
#endif
    /* read vocab */
    n = read_WordID(f,0);
    for (i = 1; i <= n; i++) {
	m = read_ushort(f,0);
	vocab[i] = New_N(char,m+1);
	z_read(vocab[i],sizeof(char),m,f);
        vocab[i][m] = '\0';
	SLMIntHashInsert(vocab_ht,vocab[i],i);
    }

    /* ngram start */
    n = 1;
    do {
	if (verbosity > 0) {
	    fprintf(stderr,"%d-grams",n);
	    fflush(stderr);
	}
	for (i = 0; i < ngram_size[n-1]; i++) {
	    if (verbosity > 0 && i%20000 == 0) {
		fputc('.',stderr);
		fflush(stderr);
	    }
	    if (n < ngram_len) {
		z_read(&l,sizeof(int4),1,f);
		prob = l2d(ntohl(l));
		for (j = 0; j < n; j++) {
		    idarray[j] = read_WordID(f,0);
		}
		z_read(&l,sizeof(int4),1,f);
		alpha = l2d(ntohl(l));
		nodes[n-1][i].id = idarray[n-1];
		nodes[n-1][i].prob = pow(10.0,prob);
		nodes[n-1][i].alpha = pow(10.0,alpha);
		nodes[n-1][i].nextpos = -1;
		nodes[n-1][i].nelem = 0;
		if (n > 1)
		    putNgramNodeData(ng,ng->node[0],ng->n_unigram,idarray,1,n,i);
	    }
	    else { /* n == ngram_len */
		z_read(&l,sizeof(int4),1,f);
		prob = l2d(ntohl(l));
		for (j = 0; j < n; j++) {
		    idarray[j] = read_WordID(f,0);
		}
		leaves[i].id = idarray[n-1];
		leaves[i].prob = pow(10.0,prob);
		putNgramNodeData(ng,ng->node[0],ng->n_unigram,idarray,1,n,i);
	    }
	}
	n++;
    } while (n <= ngram_len);
    if (verbosity > 0)
	fputc('\n',stderr);

    /* read additional class info */
    if (z_read(buf,sizeof(char),16,f) == 16) {
	if (verbosity > 1)
	    fprintf(stderr,"Reading additional class ngram info...\n");
	SLM_Set_NgramType(ng->type,SLM_ClassNgram);

    }
    if (SLM_NgramType(ng->type) == SLM_WordNgram) {
        z_close(f);
	return ng;
    }
    ng->class_ht = ng->vocab_ht;
    ng->class_sym = ng->vocab;
    ng->first_class_id = ng->first_id;

    if (SLM_WORD_VOCAB(ng->type) == SLM_WORD_VOCAB_OPEN)
	SLM_Set_CLASS_VOCAB(ng->type,SLM_CLASS_VOCAB_OPEN);
    else
	SLM_Set_CLASS_VOCAB(ng->type,SLM_CLASS_VOCAB_CLOSED);
    SLM_Set_WORD_VOCAB(ng->type,SLM_WORD_VOCAB_OPEN);
    SLM_Set_N_UNK(ng->type,SLM_CLASS_UNK);

    z_read(&l,sizeof(int4),1,f);
    ng->n_word = ntohl(l);
    z_read(&s,sizeof(short),1,f);
    ng->first_id = ntohs(s);
    if (ng->first_id != 0) {
	/* first word is not UNK */
	SLM_Set_N_UNK(ng->type,SLM_NO_UNK);
	SLM_Set_WORD_VOCAB(ng->type,SLM_WORD_VOCAB_CLOSED);
	ng->vocab[0] = strdup("<UNK>");
	if (verbosity > 2) {
	    fprintf(stderr,"\nvocab_type is set to <%s>\n",
		    vocab_type_name(ng->type));
	}
    }
    ng->vocab_ht = SLMHashCreateSI(ng->n_word*3/2);
    ng->vocab = New_N(char*,ng->n_word);
    ng->c_uniprob = New_N(float,ng->n_word);
    ng->class_id = New_N(SLMWordID,ng->n_word);

    for (i = ng->first_id; i <= ng->n_word; i++) {
	ng->class_id[i] = read_WordID(f,0);
	z_read(&l,sizeof(int4),1,f);
	ng->c_uniprob[i] = pow(10.0,l2d(ntohl(l)));
	if (i != 0) {
	    z_read(&s,sizeof(short),1,f);
	    j = ntohs(s);
	    ng->vocab[i] = New_N(char,j+1);
	    z_read(ng->vocab[i],sizeof(char),j,f);
	    SLMIntHashInsert(ng->vocab_ht,ng->vocab[i],i);
	}
    }
    z_close(f); /* 13,Jul,2005 kato */
    return ng;
}

static SLMNgram *
check_ngram_filename(SLMNgram *ng, char *filename)
{
    while (ng != NULL) {
	if (strcmp(ng->filename, filename) == 0)
	    return ng;
	ng = ng->next_lm;
    }
    return NULL;
}

static SLMNgram *
SLMReadLM0(char *filename,int format,int verbosity)
{
    if (format == SLM_LM_ARPA)
	return SLMReadLM_arpa(filename,verbosity);
    else if (format == SLM_LM_BINARY)
	return SLMReadLM_binary(filename,verbosity);
    else {
	fprintf(stderr,"SLMReadLM Error: unsupported format %d\n",format);
	return NULL;
    }
}

static SLMNgram *
create_delegate(SLMNgram *ng, int len)
{
    SLMNgram *ng2;

    if (IS_DISTANT_BIGRAM(ng)) {
	fprintf(stderr,"SLMReadLM Error: Can't create delegate for a distant bigram\n");
	return NULL;
    }
    if (len > ng->ngram_len) {
	fprintf(stderr,"SLMReadLM Error: You can't specify longer n-gram length (%d) than the original model length (%d)\n",len,ng->ngram_len);
	return NULL;
    }
    ng2 = SLMNewLM();
    memcpy(ng2,ng,sizeof(SLMNgram));
    ng2->context_len = len-1;
    ng2->delegate = ng;
    ng2->next_lm = ng;
    return ng2;
}


#ifdef ENABLE_REMOTE_MODEL
/*
 * openRemoteModel() connects to the specified host
 * and returns a pointer to SLMNgram structure that points
 * the host. 
 */
static SLMNgram *
openRemoteModel(char *hostname, int portnum, int verbosity)
{
    SLMNgram *ng;
    char buf[1024];
    int sock;
    char cmd;
    unsigned char par1;
    unsigned short par2,id;
    unsigned char size;
    int i;

    sock = openSocket(hostname,portnum);
    if (sock < 0)
	return NULL;

    ng = SLMNewLM();
    sprintf(buf,"%s:%d",hostname,portnum);
    ng->filename = strdup(buf);
    ng->type = SLM_REMOTE_MODEL;
    SLM_SOCK(ng) = sock;
    /* retrieve basic info */
    cmd = SLM_NGD_BASIC_INFO;
    write(SLM_SOCK(ng),&cmd,1);
    read(SLM_SOCK(ng),&par2,2);
    ng->type |= ntohs(par2);
    read(SLM_SOCK(ng),&par1,1);
    ng->first_id = par1;
    read(SLM_SOCK(ng),&par1,1);
    ng->first_class_id = par1;
    read(SLM_SOCK(ng),&par1,1);
    ng->ngram_len = par1;
    read(SLM_SOCK(ng),&par1,1);
    ng->context_len = par1;
    read(SLM_SOCK(ng),&par2,2);
    ng->n_unigram = ng->n_word = ntohs(par2);
    /* retrieve word info */
    ng->vocab_ht = SLMHashCreateSI(ng->n_word*3/2);
    ng->vocab = New_N(char*,ng->n_word);
    ng->vocab[0] = strdup("<UNK>");
    cmd = SLM_NGD_ID2WORD;
    for (i = ng->first_id; i <= ng->n_word; i++) {
	write(SLM_SOCK(ng),&cmd,1);
	id = htons((unsigned short)i);
	write(SLM_SOCK(ng),&id,2);
	read(SLM_SOCK(ng),&size,1);
	read(SLM_SOCK(ng),buf,size);
	buf[size] = '\0';
	ng->vocab[i] = strdup(buf);
	SLMIntHashInsert(ng->vocab_ht,ng->vocab[i],i);
	if (verbosity > 1) {
	  fprintf(stderr,"%d:%s\n",i,buf);
	  if (i % 100 == 0) {
	    fprintf(stderr,".");
	    fflush(stderr);
	  }
	}
    }
    if (verbosity > 1) {
      fprintf(stderr,"Remote model read.\n");
    }
    return ng;
}
#endif

/*
 * SLMReadLM() invokes SLMReadLM0() to read an LM. If filename is
 * "lmfile1.arpa[;length]*weight,lmfile2.arpa[;length]*weight,..." 
 * then all LMs are read and combined with the specified weight.
 */
SLMNgram *
SLMReadLM(char *filename,int format,int verbosity)
{
    SLMNgram *ng = NULL;
    char buf1[256],buf2[256];
    char *p,*q;
    double w;
    int len;

#ifdef ENABLE_REMOTE_MODEL
    if ((p = strchr(filename,':')) != NULL) {
      /* the filename is hostname:portnum */
      strncpy(buf1,filename,p-filename);
      buf1[p-filename] = '\0';
      return openRemoteModel(buf1,atoi(p+1),verbosity);
    }
#endif
    q = buf1;
    for (p = filename; *p; p++) {
	if (*p == '*' || *p == ';'
	    ) {
	    *q = '\0';
	    if (*buf1 == '\0') {
		/* no filename is specified */
		fprintf(stderr,"SLMReadLM: %s: no filename part\n",filename);
		exit(1);
	    }
	    len = 0; /* no length specified */
	    w = 1.0; /* no weight specified */
	    if (*p == ';') {
		/* length follows */
		q = buf2;
		p++;
		while (*p && *p != '*' && *p != ',')
		    *(q++) = *(p++);
		*q = '\0';
		len = atoi(buf2);
	    }
	    if (*p == '*') {
		q = buf2;
		p++;
		while (*p && *p != ',')
		    *(q++) = *(p++);
		*q = '\0';
		w = atof(buf2);
	    }
	    if (verbosity > 1) {
		fprintf(stderr,"Reading LM file %s \n",buf1);
		if (len > 0)
		    fprintf(stderr,"length=%d ",len);
		if (w != 1.0)
		    fprintf(stderr,"weight=%f",w);
		fprintf(stderr,"\n");
	    }
	    if (ng == NULL) {
		ng = SLMReadLM0(buf1,format,verbosity);
		if (len > 0 && ng->ngram_len != len) {
		    ng->weight = 0.0;
		    ng = create_delegate(ng,len);
		    if (ng == NULL) {
			/* error */
			return ng;
		    }
		}
		ng->weight = w;
	    }
	    else {
		SLMAddLM(ng,len,w,buf1,format,verbosity);
	    }
	    q = buf1;
	}
	else {
	    *(q++) = *p;
	}
    }
    if (ng == NULL) {
	*q = '\0';
	if (verbosity > 1) {
	    fprintf(stderr,"Reading LM file %s\n",buf1);
	}
	ng = SLMReadLM0(buf1,format,verbosity);
    }
    return ng;
}

void
SLMAddLM(SLMNgram *ng, int len, double weight, char *filename,int format,int verbosity)
{
    SLMNgram *next_ng;

    if (ng == NULL) {
	fprintf(stderr,"SLMAddLM Warning: base LM == NULL\n");
	return;
    }
    next_ng = check_ngram_filename(ng,filename);
    if (next_ng != NULL) {
	SLMNgram *ng2 = create_delegate(next_ng,len);
	if (ng2 == NULL) {
	    /* error */
	    return;
	}
	ng2->weight = weight;
	ng2->next_lm = ng->next_lm;
	ng->next_lm = ng2;
	return;
    }
    next_ng = SLMReadLM0(filename,format,verbosity);
    if (len > 0 && next_ng->ngram_len != len) {
	SLMNgram *ng2 = create_delegate(next_ng,len);
	if (ng2 == NULL) {
	    /* error */
	    return;
	}
	next_ng->weight = 0.0;
	next_ng = ng2;
    }
    next_ng->weight = weight;
    next_ng->next_lm = ng->next_lm;
    ng->next_lm = next_ng;
}

void
SLMFreeLM(SLMNgram *ng)
{
    int i;

#ifdef ENABLE_REMOTE_MODEL
    if (ng->type & SLM_REMOTE_MODEL) {
	Free(ng->filename);
	close(SLM_SOCK(ng));
	return;
    }
#endif
    if (ng->next_lm != NULL)
	SLMFreeLM(ng->next_lm);
    if (ng->delegate == NULL) {
	for (i = 0; i < ng->ngram_len-1; i++)
	    Free(ng->node[i]);
	Free(ng->node);
	Free(ng->leaf);
	Free(ng->vocab);
	Free(ng->filename);
#ifdef NG_CACHE
	Free(ng->hist);
#endif
	SLMHashDestroy(ng->vocab_ht);
	if (SLM_NgramType(ng->type) == SLM_ClassNgram) {
	    Free(ng->class_sym);
	    SLMHashDestroy(ng->class_ht);
	}
    }
    Free(ng);
}

static SLMNgramNode*
search_node(SLMNgram *ng, SLMNgramNode *base, int nelem, int level,
	    int len, SLMWordID *idarray, int cache_ok)
{
    SLMNgramNode ref,*nd;
    ref.id = idarray[level];
    if (level == ng->ngram_len-1) {
	return bsearch(&ref,base,nelem,sizeof(SLMNgramLeaf),
		       SLMNgramLeafCompare);
    }
    else {
#ifdef NG_CACHE
	/* check cache */
	if (cache_ok && ng->hist[level].id == idarray[level])
	    nd = ng->hist[level].node;
	else {
	    nd = bsearch(&ref,base,nelem,sizeof(SLMNgramNode),SLMNgramNodeCompare);
	    ng->hist[level].id = idarray[level];
	    ng->hist[level].node = nd;
	    cache_ok = 0;
	}
#else
	nd = bsearch(&ref,base,nelem,sizeof(SLMNgramNode),SLMNgramNodeCompare);
#endif
	    
	if (level == len-1 || nd == NULL)
	    return nd;
	else {
	    if (level == ng->ngram_len-2) {
		return search_node(ng,
				   (SLMNgramNode*)&ng->leaf[nd->nextpos],
				   nd->nelem,
				   level+1,len,idarray, cache_ok);
	    }
	    else {
		return search_node(ng,
				   &ng->node[level+1][nd->nextpos],
				   nd->nelem,
				   level+1,len,idarray, cache_ok);
	    }
	}
    }
}

static double 
SLMGetBOProb0(SLMNgram *ng, int len, SLMWordID *idarray, SLMBOStatus *status)
{
    SLMNgramNode *nn1,*nn2;
    double prob;
    int i;

    nn1 = search_node(ng,ng->node[0],ng->n_unigram,0,len,idarray,1);
    if (nn1 != NULL) {
	if (status) {
	    for (i = 0; i < len; i++)
		status->hit[i] = SLM_STAT_HIT;
	}
	prob = nn1->prob;
    }
    else {
	if (len == 1) {
	    /* unigram search failed */
	    prob = 0;
	}
	else {
	    nn2 = search_node(ng,ng->node[0],ng->n_unigram, 0,len-1,idarray,1);
	    if (nn2 != NULL) {
		if (status)
		    status->hit[len-1] = SLM_STAT_BO_WITH_ALPHA;
		prob = nn2->alpha*SLMGetBOProb0(ng,len-1,idarray+1,status);
	    }
	    else 
		prob = SLMGetBOProb0(ng,len-1,idarray+1,status);
	}
    }
    if (status) {
	status->ng_prob = prob;
	status->ug_prob = 1;
    }
    return prob;
}

SLMWordID
SLMWord2ID(SLMNgram *ng, char *word)
{
    int id;
    char *q,buf[256];

    if (ng->delegate != NULL)
	return SLMWord2ID(ng->delegate,word);
    id = SLMIntHashSearch(ng->vocab_ht,word);
    if (id == 0 && SLM_NgramType(ng->type) == SLM_ClassNgram) {
	/* if ng is class ngram, UNK is class by class */
	for (q = word+strlen(word)-1; q >= word; q--) {
	    if (*q == ng->delimiter)
		break;
	}
	q++;
	sprintf(buf,"<UNK>%c%s",ng->delimiter,q);
	id = SLMIntHashSearch(ng->vocab_ht,buf);
    }
    return id;
}

int
SLMVocabSize(SLMNgram *ng)
{
    if (SLM_NgramType(ng->type) == SLM_WordNgram)
	return ng->n_unigram;
    else
	return ng->n_word;
}


const char*
SLMID2Word(SLMNgram *ng, SLMWordID id)
{
    if (id == 0)
	return "<UNK>";
    if (id >= SLMVocabSize(ng))
	return "<ERROR>";
    return ng->vocab[id];
}

int
SLMContextLength(SLMNgram *ng)
{
    int context_len = 0;
    while (ng) {
	if (ng->weight == 0) {
	    /* dummy model */
	    ng = ng->next_lm;
	    continue;
	}
	if (ng->context_len > context_len)
	    context_len = ng->context_len;
	ng = ng->next_lm;
    }
    return context_len;
}
    
int
SLMNgramLength(SLMNgram *ng)
{
    int len = 0,x;
    while (ng) {
	if (IS_DISTANT_BIGRAM(ng))
	    x = 2;
	else
	    x = ng->context_len+1;
	if (ng->weight > 0 && x > len)
	    len = x;
	ng = ng->next_lm;
    }
    return len;
}
    

double 
SLMGetBOProb(SLMNgram *ng, int len, SLMWordID *idarray, SLMBOStatus *status)
{
    SLMWordID cidarray[MAX_GRAM];
    double prob_array[MAX_GRAM];
    double weight_array[MAX_GRAM];
    int i,j;
    double prob;
    double sum_weight = 0.0;
    int reallen;
    SLMBOStatus my_stat;

    if (len > MAX_GRAM) {
	fprintf(stderr,"SLMGetBOProb: n-gram length %d too big (limit is %d\n",
		len, MAX_GRAM);
	return 0.0;
    }
#ifdef ENABLE_REMOTE_MODEL
    if (ng->type & SLM_REMOTE_MODEL) {
	char cmd = SLM_NGD_PROB;
	unsigned char len1 = len;
	SLMWordID id;
	int4 iprob;
	write(SLM_SOCK(ng),&cmd,1);
	write(SLM_SOCK(ng),&len1,1);
	for (i = 0; i < len; i++) {
	    id = SLMhtonID(idarray[i]);
	    write(SLM_SOCK(ng),&id,sizeof(SLMWordID));
	}
	read(SLM_SOCK(ng),&len1,1);
	my_stat.len = len1;
	read(SLM_SOCK(ng),&iprob,4);
	my_stat.ng_prob = exp(SLMl2d(ntohl(iprob)));
	read(SLM_SOCK(ng),&iprob,4);
	my_stat.ug_prob = exp(SLMl2d(ntohl(iprob)));
	read(SLM_SOCK(ng),my_stat.hit,len1);
	if (status) {
	    status->len = my_stat.len;
	    status->ng_prob = my_stat.ng_prob;
	    status->ug_prob = my_stat.ug_prob;
	    for (i = 0; i < my_stat.len; i++)
		status->hit[i] = my_stat.hit[i];
	}
	return my_stat.ng_prob*my_stat.ug_prob;
    }
#endif	

    j = 0;
    for (; ng != NULL; ng = ng->next_lm) {
	if (ng->weight == 0.0)
	    continue;
	/* set reallen; reallen is set to n-gram length */
	reallen = len;
	if (len > ng->context_len+1)
	    reallen = ng->context_len+1;
	my_stat.len = len;
	for (i = 0; i < reallen; i++)
	    my_stat.hit[i] = 0;
	if (SLM_NgramType(ng->type) == SLM_WordNgram) {
	    if (IS_DISTANT_BIGRAM(ng)) {
		/* distant bigram */
		if (len < ng->context_len+1) {
		    reallen = 1; /* back to unigram */
		    cidarray[len-1] = idarray[len-1];
		}
		else {
		    reallen = 2;
		    cidarray[len-2] = idarray[len-ng->context_len-1];
		    cidarray[len-1] = idarray[len-1];
		}

	    }
	    else {
		for (i = 0; i < len; i++) {
		    cidarray[i] = idarray[i];
		}
	    }
	}
	else {
	    if (IS_DISTANT_BIGRAM(ng)) {
		if (len < ng->context_len+1) {
		    reallen = 1; /* back to unigram */
		    cidarray[0] = ng->class_id[idarray[len-1]];
		}
		else {
		    reallen = 2;
		    cidarray[0] = ng->class_id[idarray[len-ng->context_len-1]];
		    cidarray[1] = ng->class_id[idarray[len-1]];
		}
	    }
	    else {
		for (i = 0; i < len; i++) {
		    cidarray[i] = ng->class_id[idarray[i]];
		}
	    }
	}
	if (ng->delegate != NULL)
	    prob_array[j] = SLMGetBOProb0(ng->delegate,reallen,cidarray+len-reallen,&my_stat);
	else
	    prob_array[j] = SLMGetBOProb0(ng,reallen,cidarray+len-reallen,&my_stat);
	if (SLM_NgramType(ng->type) == SLM_ClassNgram) {
	    my_stat.ug_prob = ng->c_uniprob[idarray[len-1]];
	    prob_array[j] *= ng->c_uniprob[idarray[len-1]];
	}

	weight_array[j] = ng->weight;
	sum_weight += ng->weight;
	j++;
    }

    /* combine probs using weight*/
    if (sum_weight < 0.99999) {
	/* Illegal weight; in this case weight is re-normalized */
	for (i = 0; i < j; i++) {
	    weight_array[i] /= sum_weight;
	}
    }
    prob = 0;
    for (i = 0; i < j; i++) {
	prob += weight_array[i]*prob_array[i];
    }

    if (status) {
	status->len = my_stat.len;
	status->ng_prob = my_stat.ng_prob;
	status->ug_prob = my_stat.ug_prob;
	for (i = 0; i < my_stat.len; i++)
	    status->hit[i] = my_stat.hit[i];
    }
    return prob;
}

void
SLMBOStatusString(SLMBOStatus *status, char *buf)
{
    int i;
    for (i = 0; i < status->len; i++) {
	if (status->hit[i] == SLM_STAT_HIT) {
	    buf[i] = 'H';
	}
	else if (status->hit[i] == SLM_STAT_BO_WITH_ALPHA)
	    buf[i] = 'b';
	else
	    buf[i] = '-';
    }
    buf[i] = '\0';
}

    
