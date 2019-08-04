#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include "libs.h"
#include "io.h"
#include "misc.h"
#include "vocab.h"
#include "ngram.h"
#include "context.h"

/* #define MEM_DEBUG */

int ngram_len = 3;
int ascii_input;
int verbosity = 2;
int one_unk = 0;           /* one unk model */
int no_unk_in_train = 0;   /* no unk in the training text */
int open_class = 0;        /* treat unknown class */
int distance = 0;

unsigned long total_count;
int fof_size = 10;
unsigned long **fof;
int *gt_discount_range;
float **gt_discount_coef;
float *linear_discount_coef;
float *abs_discount;
double discount_tweak = 1.0;

SLMHashTable *vocab_ht;
SLMHashTable *ccs_ht;
SLMHashTable *class_ht;

char **vocab_tbl;
char **class_tbl;
int max_vocab;
int max_class;

int vocab_type = (SLM_WordNgram|SLM_ONE_UNK|SLM_UNK_IN_TRAIN|SLM_WORD_VOCAB_OPEN);
int first_id,first_class_id;

typedef struct {
  SLMWordID class_id;
  uint4 count;
  float prob;
} ClassCount;

ClassCount *class_count;
uint4 *class_freq;

int *cutoffs = NULL;
char delimiter = '+';

#define ALPHA_FLOOR 1.0e-10

#define d2l(x) SLMd2l(x)

void usage();

typedef struct ngram_node {
  SLMWordID id;                   /* this node's id */
  SLMWordID nelem;           /* # of children under this node */
  SLMWordID nelem_wo_cutoff; /* # of children (without cutoff) */
  uint4 count;                     /* n-gram count of this node */
  float alpha;                    /* alpha value, calculated after
				     the entire tree is read    */
  union {
    struct ngram_node *node;    /* child nodes */
    struct ngram_leaf *leaf;    /* child leaves */
  } z;
  float prob;                     /* prob of this node */
} NgramNode;

typedef struct ngram_leaf {
  SLMWordID id;                   /* this leaf's id */
  uint4 count;                     /* n-gram count if this leaf */
  float prob;                     /* prob of this leaf */
} NgramLeaf;

void pushNgramLeaf(NgramNode *na, SLMWordID *ng, uint4 count, int ngram_len);
void pushNgramNode(NgramNode *na, SLMWordID *ng, uint4 count, int level, int ngram_len);
void fixNgramNode(NgramNode *,int);
void fixNgramLeaf(NgramNode *);

NgramNode *SLMReadIDNgram(FILEHANDLE inf, int ngram_len, int ascii_input);

typedef struct {
  void (*prepare_proc)();
  void (*disc_proc)(NgramNode*, int);
  int need_fof;
} SLMDiscountProc;

void calc_GT_disc_range();
void prepare_GT_disc();
void prepare_linear_disc();
void prepare_absolute_disc();

void calc_WB_discount(NgramNode *nga, int level);
void calc_LI_discount(NgramNode *nga, int level);
void calc_AB_discount(NgramNode *nga, int level);
void calc_GT_discount(NgramNode *nga, int level);

SLMDiscountProc discount_proc = { NULL, calc_WB_discount, 0 };

NgramNode *Unigrams;
NgramNode **TmpNgramNode;
NgramLeaf *TmpNgramLeaf;
int N_Tmp;

unsigned long *ngcount;

/*
 * Structure of NgramNode and NgramLeaf (in case of 4-gram)
 *
 * unigram: NgramNode
 *   nelem  // # of unigram
 *   z.node[i].id      // id of i-th unigram
 *   z.node[i].count   // count of i-th unigram
 *   z.node[i].z.node  // next node: initialized by TmpNgramNode[0]
 *   z.node[i].z.node[j].id     // id of bigram (i,j)
 *   z.node[i].z.node[j].count  // count
 *   z.node[i].z.node[j].z.node // Next Node: initialized by TmpNgramNode[1]
 *   z.node[i].z.node[j].z.node[k].id      // id of trigram (i,j,k)
 *   z.node[i].z.node[j].z.node[k].count   // count
 *   z.node[i].z.node[j].z.node[k].z.leaf  // Next leaf: initialized by TmpNgramLeaf
 *   z.node[i].z.node[j].z.node[k].z.leaf[l].id    // id of 4gram (i,j,k,l)
 *   etc.
 */

#ifdef MEM_DEBUG
int ngram_mem = 0;
#endif

static char version_msg[SLM_BINLM_HEADER_SIZE_V2] = SLM_BINLM_HEADER_MSG_V2;

void
setNewNode(NgramNode *nd,int level)
{
  nd->z.node = TmpNgramNode[level];
  nd->nelem = 0;
  /*    memset(nd->z.node,0,N_Tmp*sizeof(NgramNode)); */
}

void
setNewLeaf(NgramNode *nd)
{
  nd->z.leaf = TmpNgramLeaf;
  nd->nelem = 0;
  /*    memset(nd->z.leaf,0,N_Tmp*sizeof(NgramLeaf)); */
}

#if 1 /* debug*/
void assertNode(NgramNode *nd, NgramNode *t, int level)
{
  if (nd != t) {
    fprintf(stderr,"level=%d\n",level);
    fprintf(stderr,"z.node=%lx\n",(ptr_int)nd);
    for (int ii = 0; ii < ngram_len-2; ii++) {
      fprintf(stderr,"TmpNgramNode[%d]=%lx\n",ii,TmpNgramNode[ii]);
    }
    abort();
  }
}

void assertLeaf(NgramLeaf *nd, NgramLeaf *t, int level)
{
  if (nd != t) {
    fprintf(stderr,"level=%d\n",level);
    fprintf(stderr,"z.leaf=%lx\n",(ptr_int)nd);
    fprintf(stderr,"TmpNgramLeaf=%lx\n",(ptr_int)TmpNgramLeaf);
    for (int ii = 0; ii < ngram_len-2; ii++) {
      fprintf(stderr,"TmpNgramNode[%d]=%lx\n",ii,(ptr_int)TmpNgramNode[ii]);
    }
    abort();
  }
}

#endif

/* level はbigramノードなら1, trigramノードなら2 */
void
pushNgramNode(NgramNode *na, SLMWordID *ng, uint4 count, int level, int ngram_len)
{
  if (na->nelem > 0 &&
      na->z.node[na->nelem-1].id == ng[0]) {
    na->z.node[na->nelem-1].count += count;
  }
  else {
    if (level < ngram_len-2) {
      if (na->nelem > 0) {
	assertNode(na->z.node[na->nelem-1].z.node, TmpNgramNode[level-1],level);
	fixNgramNode(&na->z.node[na->nelem-1],level);
      }
      else { /* na->nelem == 0 */
	setNewNode(na->z.node,level-1);
      }
    }
    else {
      if (na->nelem > 0) {
 	assertLeaf(na->z.node[na->nelem-1].z.leaf, TmpNgramLeaf,level);
	fixNgramLeaf(&na->z.node[na->nelem-1]);
      }
       else {
	setNewLeaf(na->z.node);
       }
    }
    na->z.node[na->nelem].count = count;
    na->z.node[na->nelem].id = ng[0];
    na->z.node[na->nelem].nelem = 0;
    na->z.node[na->nelem].nelem_wo_cutoff = 0;
    if (level == ngram_len-2)
      setNewLeaf(&na->z.node[na->nelem]);
    else
      setNewNode(&na->z.node[na->nelem],level-1);
    na->nelem++;
    na->nelem_wo_cutoff++;
  }
  if (level == ngram_len-2) {
    pushNgramLeaf(&na->z.node[na->nelem-1],ng+1,count,ngram_len);
  }
  else {
    pushNgramNode(&na->z.node[na->nelem-1],ng+1,count,level+1,ngram_len);
  }
}

void
pushNgramLeaf(NgramNode *na, SLMWordID *ng, uint4 count, int ngram_len)
{
  na->nelem_wo_cutoff++;
  if (cutoffs != NULL && cutoffs[ngram_len-1] >= count)
    return;
  na->z.leaf[na->nelem].count = count;
  na->z.leaf[na->nelem].id = *ng;
  na->nelem++;
}

/* free unused memory */
void
fixNgramNode(NgramNode *nd,int level)
{
  NgramNode *nn;
  int i,newsize;

  newsize = nd->nelem;
  if (newsize == 0) {
    /* This node has no children because of the cutoff */
    nd->z.node = NULL;
    return;
  }
  if (level == ngram_len-1) {
    assertLeaf(nd->z.leaf, TmpNgramLeaf, level);
    fixNgramLeaf(nd);
    return;
  }
  assertNode(nd->z.node, TmpNgramNode[level-1], level);
  nn = nd->z.node;
  nd->z.node = New_N(NgramNode,newsize);
  for (i = 0; i < newsize; i++) {
    nd->z.node[i] = nn[i];
  }
  fixNgramNode(&nd->z.node[newsize-1],level+1);
#ifdef MEM_DEBUG
  ngram_mem += newsize*sizeof(NgramNode);
  printf("\n%fMB used\n",(double)ngram_mem/(1024*1024));
#endif
}

void
fixNgramLeaf(NgramNode *na)
{
  NgramLeaf *nn;
  int i,newsize;

  assertLeaf(na->z.leaf, TmpNgramLeaf, -1);
  newsize = na->nelem;
  nn = na->z.leaf;
  na->z.leaf = New_N(NgramLeaf,newsize);
  for (i = 0; i < newsize; i++) {
    na->z.leaf[i] = nn[i];
  }
#ifdef MEM_DEBUG
  ngram_mem += newsize*sizeof(NgramLeaf);
#endif
}

static SLMWordID prev_id[MAX_GRAM];
static uint4 prev_count[MAX_GRAM];

void
count_fof(SLMWordID *ids, uint4 count, int len)
{
  int i;
  int differ = 0;
  for (i = 0; i < len; i++) {
    if (ids != NULL) {
      if (!differ && prev_id[i] == ids[i])
	prev_count[i] += count;
      else {
	if (prev_count[i] > 0 && prev_count[i] < fof_size)
	  fof[i][prev_count[i]-1]++;
	prev_count[i] = count;
	prev_id[i] = ids[i];
	differ = 1;
      }
    }
    else {
      /* flush */
      if (prev_count[i] > 0 && prev_count[i] < fof_size)
	fof[i][prev_count[i]-1]++;
      prev_count[i] = 0;
      prev_id[i] = max_vocab; /* not a word */
    }
  }
}


NgramNode *
SLMReadIDNgram(FILEHANDLE inf, int ngram_len, int ascii_input)
{
  SLMNgramCount nc;
  NgramNode *unigram = New(NgramNode);
  int i,n = 0,n_unigram;
  unsigned long p_count;
  NgramNode *u,*prev_u;

  /* prepare ID n-gram info for unigram */
  if (class_ht != NULL)
    n_unigram = class_ht->nelem+1;
  else
    n_unigram = vocab_ht->nelem+1;

  unigram->z.node = New_N(NgramNode,n_unigram);
  unigram->nelem = n_unigram;
  for (i = 0; i < n_unigram; i++) {
    u = &unigram->z.node[i];
    u->id = i;
    u->count = 0;
    u->nelem = 0;
  }

  /* prepare ID n-gram for reading */
  nc.word_id = New_N(SLMWordID,ngram_len);

  /* prepare for FOF count */
  if (fof != NULL) {
    for (i = 0; i < ngram_len; i++) {
      prev_id[i] = max_vocab; /* not a word */
      prev_count[i] = 0;
    }
  }
  total_count = 0;
  prev_u = NULL;
  if (ngram_len == 2)
    setNewLeaf(&unigram->z.node[0]);
  else
    setNewNode(&unigram->z.node[0],0);
  while (SLMReadNgramCount(ngram_len,inf,&nc,ascii_input) != NULL) {
    /* UNK check */
    if (SLM_UNK_TRAIN(vocab_type) == SLM_NO_UNK_IN_TRAIN) {
      for (i = 0; i < ngram_len; i++) {
	if (nc.word_id[i] == 0) {
	  fprintf(stderr,"Unknown word(s) found while vocab type is %s!\n",
		  vocab_type_name(vocab_type));
	  exit(1);
	}
      }
    }
    if (fof != NULL)
      count_fof(nc.word_id, nc.count, ngram_len);
    /* input check */
    for (i = 0; i < ngram_len; i++) {
      if (nc.word_id[i] > n_unigram) {
	fprintf(stderr,"Illegal word number found (corrupt idngram file?)\n");
	exit(1);
      }
    }
	
    /* set unigram */
    u = &unigram->z.node[nc.word_id[0]];
    if (prev_u != u) {
      if (ngram_len == 2) {
	if (prev_u != NULL)
	  fixNgramLeaf(prev_u);
	setNewLeaf(u);
      }
      else {
	if (prev_u != NULL)
	  fixNgramNode(prev_u,1);
	setNewNode(u,0);
      }
    }
    prev_u = u;
    p_count = u->count;
    u->count += nc.count;
    if (p_count >= u->count) {
      /* count overflow */
      fprintf(stderr,"idngram2lm ERROR: n-gram count overflow (unigram count %lu+%lu)\n",p_count,nc.count);
      exit(1);
    }
    if (ngram_len == 2) {
      pushNgramLeaf(u,nc.word_id+1,nc.count,ngram_len);
    }
    else {
      pushNgramNode(u,nc.word_id+1,nc.count,1,ngram_len);
    }
    p_count = total_count;
    total_count += nc.count;
    if (total_count <= p_count) {
      /* count overflow */
      fprintf(stderr,"idngram2lm ERROR: n-gram count overflow (total count %lu+%lu)\n",p_count,nc.count);
      exit(1);
    }
	    
    n++;
    if (verbosity > 1) {
      if (n % 20000 == 0) {
	fputc('.',stderr);
	fflush(stderr);
	if (n % 1000000 == 0)
	  fputc('\n',stderr);
      }
    }
  }
  if (ngram_len == 2)
    fixNgramLeaf(prev_u);
  else
    fixNgramNode(prev_u,1);
  if (fof != NULL)
    count_fof(NULL,0,ngram_len);   /* flush */
  z_close(inf);
  return unigram;
}

void
calc_ML_probs_for_leaf(NgramNode *nga, unsigned long parent_count)
{
  int i;
  for (i = 0; i < nga->nelem; i++) {
    nga->z.leaf[i].prob = (double)nga->z.leaf[i].count/parent_count;
  }
}

void
calc_ML_probs_for_node(NgramNode *nga, unsigned long parent_count, int level)
{
  int i;
  double p;
  for (i = 0; i < nga->nelem; i++) {
    p = (double)nga->z.node[i].count/parent_count;
    nga->z.node[i].prob = p;
    if (p <= 0 || p > 1) {
      printf("i=%d level=%d count=%lu parent=%lu prob=%f\n",
	     i,level,nga->z.node[i].count,parent_count,
	     p);
    }
    if (level == ngram_len-1)
      calc_ML_probs_for_leaf(&nga->z.node[i],
			     nga->z.node[i].count);
    else
      calc_ML_probs_for_node(&nga->z.node[i],
			     nga->z.node[i].count,
			     level+1);
  }
}

static int
NgramNode_comp(const void *n1, const void *n2)
{
  return ((NgramNode*)n1)->id - ((NgramNode*)n2)->id;
}

NgramNode *
searchNgramWithinNode(NgramNode *nga,SLMWordID id)
{
  NgramNode key;

  key.id = id;
  return (NgramNode*)bsearch((void*)&key,(void*)nga->z.node,
			     nga->nelem,sizeof(NgramNode),
			     NgramNode_comp);
}

static int
NgramLeaf_comp(const void *n1, const void *n2)
{
  return ((NgramLeaf*)n1)->id - ((NgramLeaf*)n2)->id;
}

NgramLeaf *
searchNgramWithinLeaf(NgramNode *nga,SLMWordID id)
{
  NgramLeaf key;

  key.id = id;
  return (NgramLeaf*)bsearch((void*)&key,(void*)nga->z.leaf,
			     nga->nelem,sizeof(NgramLeaf),
			     NgramLeaf_comp);
}

NgramNode *
searchContext(NgramNode *nda, SLMWordID *idarray, int len)
{
  NgramNode *nd;

  nd = searchNgramWithinNode(nda,idarray[0]);
  if (nd == NULL) {
    /* not found... something's wrong */
    return NULL;
  }
  if (len == 1)
    return nd;
  else
    return searchContext(nd,idarray+1,len-1);
}

double
getNgramRawProb(SLMWordID *idarray, int level)
{
  NgramNode *nd = Unigrams;
  NgramLeaf *nl;
  int i = 0,j;

  do {
    if (i == ngram_len-1) {
      /* is leaf */
      nl = searchNgramWithinLeaf(nd,idarray[i]);
      if (nl == NULL) {
	fprintf(stderr,"WARNING: getNgramRawProb: Can't find n-gram for ");
	for (j = 0; j < level; j++)
	  fprintf(stderr,SLMWordID_FMT " ",idarray[j]);
	fprintf(stderr,"\n");
	return -1;
      }
      return nl->prob;
    }
    else {
      /* is node */
      nd = searchNgramWithinNode(nd,idarray[i]);
      if (nd == NULL) {
	fprintf(stderr,"WARNING: getNgramRawProb: Can't find n-gram for ");
	for (j = 0; j < level; j++)
	  fprintf(stderr,SLMWordID_FMT " ",idarray[j]);
	fprintf(stderr,"\n");
	return -1;
      }
      i++;
    }
  } while (i < level);
  return nd->prob;
}
	

void
calc_WB_discount(NgramNode *nga, int level)
{
  int i,j;
  for (i = 0; i < nga->nelem; i++) {
    NgramNode *nd = &nga->z.node[i];
    double disc;
    int R;

    if (level == ngram_len-1) {
      R = nd->nelem_wo_cutoff;
      disc = (double)nd->count/(R+nd->count)*discount_tweak;
      for (j = 0; j < nd->nelem; j++) {
	NgramLeaf *nl = &nd->z.leaf[j];
	nl->prob *= disc;
      }
    }
    else {
      R = nd->nelem_wo_cutoff;
      disc = (double)nd->count/(R+nd->count)*discount_tweak;
      for (j = 0; j < nd->nelem; j++) {
	NgramNode *nl = &nd->z.node[j];
	nl->prob *= disc;
      }
      calc_WB_discount(nd,level+1);
    }
  }
}

void
prepare_linear_disc()
{
  int i;

  linear_discount_coef = New_N(float,ngram_len);
  if (verbosity > 1)
    fprintf(stderr,"Linear back-off discount coef.:\n");
  for (i = 0; i < ngram_len; i++) {
    linear_discount_coef[i] = 1.0 - (double)fof[i][0]/total_count;
    if (verbosity > 1)
      fprintf(stderr,"%6.3f ",linear_discount_coef[i]);
  }
  if (verbosity > 1)
    fprintf(stderr,"\n");
}

void
calc_LI_discount(NgramNode *nga, int level)
{
  int i,j;
  for (i = 0; i < nga->nelem; i++) {
    NgramNode *nd = &nga->z.node[i];
    double disc;
    unsigned long R;

    disc = linear_discount_coef[level]*discount_tweak;
    R = nd->nelem;
    if (level == ngram_len-1) {
      for (j = 0; j < R; j++) {
	NgramLeaf *nl = &nd->z.leaf[j];
	nl->prob *= disc;
      }
    }
    else {
      for (j = 0; j < R; j++) {
	NgramNode *nl = &nd->z.node[j];
	nl->prob *= disc;
      }
      calc_LI_discount(nd,level+1);
    }
  }
}

void
prepare_absolute_disc()
{
  int i;
  abs_discount = New_N(float,ngram_len);
  if (verbosity > 1)
    fprintf(stderr,"Absolute back-off subtraction mass:\n  ");
  for (i = 0; i < ngram_len; i++) {
    if (fof[i][0] == 0 && fof[i][1] == 0) {
      /* Can't discount */
      abs_discount[i] = 0.0;
      if (verbosity > 1)
	fprintf(stderr,"N/A    ");
    }
    else {
      abs_discount[i] = (double)fof[i][0]/(fof[i][0]+2*fof[i][1]);
      if (verbosity > 1)
	fprintf(stderr,"%6.3f ",abs_discount[i]);
    }
  }
  if (verbosity > 1)
    fprintf(stderr,"\n");
}

void
calc_AB_discount(NgramNode *nga, int level)
{
  int i,j;
  for (i = 0; i < nga->nelem; i++) {
    NgramNode *nd = &nga->z.node[i];
    double disc,b;
    unsigned long R;

    b = abs_discount[level];
    R = nd->nelem;
    if (level == ngram_len-1) {
      for (j = 0; j < R; j++) {
	NgramLeaf *nl = &nd->z.leaf[j];
	disc = ((double)nl->count - b)/nl->count*discount_tweak;
	nl->prob *= disc;
      }
    }
    else {
      for (j = 0; j < R; j++) {
	NgramNode *nl = &nd->z.node[j];
	disc = ((double)nl->count - b)/nl->count*discount_tweak;
	nl->prob *= disc;
      }
      calc_AB_discount(nd,level+1);
    }
  }
}

void
calc_GT_disc_range(int gt_cutoff)
{
  int i,j;
  int calc_again;

  gt_discount_coef = New_N(float*,ngram_len);
 again:
  if (verbosity > 1) fprintf(stderr,"Discount coef:\n");
  for (j = 0; j < ngram_len; j++) {
    if (gt_discount_range[j] >= fof_size) {
      /* discounting range must be less than fof size */
      gt_discount_range[j] = fof_size-1;
      if (verbosity > 1)
	fprintf(stderr,"FOF size is %d; GT discount range is set to %d\n",
		fof_size, gt_discount_range[j]);
    }
    gt_discount_coef[j] = New_N(float,gt_discount_range[j]);
    if (verbosity > 1)
      fprintf(stderr,"  %d-gram: ",j+1);
    for (i = 1; i <= gt_discount_range[j]; i++) {
      double a,b,c,d,x;
      int k = gt_discount_range[j];
      a = (i+1)*fof[j][i];
      b = i*fof[j][i-1];
      c = k*fof[j][k-1];
      d = fof[j][0];
      if (b == 0 || d == 0 || 1.0-c/d == 0) {
	x = 100; /* any value more than 1 goes */
	if (verbosity > 1) fprintf(stderr,"N/A   ");
      }
      else {
	x = (a/b-c/d)/(1.0-c/d);
	if (verbosity > 1) fprintf(stderr,"%5.3f ",x);
      }
      gt_discount_coef[j][i-1] = x;
    }
    if (verbosity > 1)
      fputc('\n',stderr);
  }
  if (!gt_cutoff)
    return;

  calc_again = 0;
  for (j = 0; j < ngram_len; j++) {
    if (gt_discount_coef[j][0] >= 1.0) {
      gt_discount_range[j] = 0;
      if (verbosity > 1)
	fprintf(stderr,"GT discount is disabled for %d-gram.\n",j+1);
    }
    for (i = gt_discount_range[j]; i > 1; i--) {
      if (gt_discount_coef[j][i-1] >= 1) {
	gt_discount_range[j] = i-1;
	if (verbosity > 1)
	  fprintf(stderr,
		  "GT discount range for %d-gram is set to %d\n",
		  j+1,i-1);
	calc_again = 1;
	break;
      }
    }
  }
  if (calc_again)
    goto again;
}

void
prepare_GT_disc()
{
  calc_GT_disc_range(1);
}

void
calc_GT_discount(NgramNode *nga, int level)
{
  int i,j;
  for (i = 0; i < nga->nelem; i++) {
    NgramNode *nd = &nga->z.node[i];
    double disc;
    unsigned long R;
    unsigned long sum_count;
    double sum_prob;

    R = nd->nelem;
    if (level == ngram_len-1) {
      sum_prob = 0.0;
      sum_count = 0;
      for (j = 0; j < R; j++) {
	NgramLeaf *nl = &nd->z.leaf[j];
	if (nl->count <= gt_discount_range[level]) {
	  disc = gt_discount_coef[level][nl->count-1]*discount_tweak;
	  nl->prob *= disc;
	}
	sum_prob += nl->prob;
	sum_count += nl->count;
      }
      if (sum_prob == 1.0) {
	/* no discount mass is obtained. force discounting */
	for (j = 0; j < R; j++) {
	  NgramLeaf *nl = &nd->z.leaf[j];
	  nl->prob = (float)nl->count/(sum_count+1);
	}
      }
    }
    else {
      sum_prob = 0.0;
      sum_count = 0;
      for (j = 0; j < R; j++) {
	NgramNode *nl = &nd->z.node[j];
	if (nl->count <= gt_discount_range[level]) {
	  disc = gt_discount_coef[level][nl->count-1]*discount_tweak;
	  nl->prob *= disc;
	}
	sum_prob += nl->prob;
	sum_count += nl->count;
      }
      if (sum_prob == 1.0) {
	/* no discount mass is obtained. force discounting */
	for (j = 0; j < R; j++) {
	  NgramNode *nl = &nd->z.node[j];
	  nl->prob = (float)nl->count/(sum_count+1);
	}
      }
      calc_GT_discount(nd,level+1);
    }
  }
}


void
calc_alpha(NgramNode *nga, SLMWordID *idarray, int level)
{
  int i,j;
  for (i = 0; i < nga->nelem; i++) {
    NgramNode *nd = &nga->z.node[i];
    unsigned long R;
    double s_prob = 0.0;
    double n_prob = 0.0;

    idarray[level-1] = nd->id;

    if (level == ngram_len-1) {
      R = nd->nelem;
      for (j = 0; j < R; j++) {
	NgramLeaf *nl = &nd->z.leaf[j];
	if (nl->prob > 0) {
	  idarray[level] = nl->id;
	  s_prob += getNgramRawProb(idarray+1,level);
	  n_prob += nl->prob;
	}
      }
    }
    else {
      R = nd->nelem;
      for (j = 0; j < R; j++) {
	NgramNode *nl = &nd->z.node[j];
	if (nl->prob > 0) {
	  idarray[level] = nl->id;
	  s_prob += getNgramRawProb(idarray+1,level);
	  n_prob += nl->prob;
	}
      }
      calc_alpha(nd,idarray,level+1);
    }
    if (s_prob > 0) {
      double num,den;
      num = 1.0-n_prob;
      den = 1.0-s_prob;
      if (num < ALPHA_FLOOR) num = ALPHA_FLOOR;
      if (den < ALPHA_FLOOR) den = ALPHA_FLOOR;
      nd->alpha = num/den;
    }
    else {
      /* nd has no child node */
      /* therefore nd->alpha is never used */
      nd->alpha = 1.0;
    }
  }
}

void
count_ngram(NgramNode *nga, unsigned long *counts, int level, int count_all)
{
  int i,j;
  unsigned long R;
  for (i = 0; i < nga->nelem; i++) {
    NgramNode *nd;
    nd = &nga->z.node[i];
    if (!count_all && nd->prob <= 0) {
      if (level == 1)
	fprintf(stderr,"Warning: P(%d,%d)=%f\n",i,level,nd->prob);
      continue;
    }
    counts[level-1]++;
    if (level == ngram_len-1) {
      R = nd->nelem;
      for (j = 0; j < R; j++) {
	if (count_all || nd->z.leaf[j].prob > 0)
	  counts[level]++;
      }
    }
    else {
      count_ngram(nd,counts,level+1,count_all);
    }
  }
}

/* calculate cutoff for ngrams less than N */
void
calc_cutoff_again(NgramNode *nga, int level)
{
  int i;
  for (i = 0; i < nga->nelem; i++) {
    NgramNode *nd = &nga->z.node[i];
    if ((long long)nd->count <= (long long)cutoffs[level-1])
      nd->prob = -1;
    if (level < ngram_len-1)
      calc_cutoff_again(nd,level+1);
  }
}

void
prepare_fof()
{
  int i,j;
  fof = New_N(unsigned long*,ngram_len);
  for (i = 0; i < ngram_len; i++) {
    fof[i] = New_N(unsigned long,fof_size);
    for (j = 0; j < fof_size; j++)
      fof[i][j] = 0;
  }
}

void
print_ngram_ent(FILEHANDLE outf, double prob, SLMWordID *idarray,
		int level, double alpha, char **tbl)
{
  int j;

  if (prob == 0)
    z_printf(outf,"-1e99");
  else
    z_printf(outf,"%.5f ",log10(prob));
  for (j = 0; j < level; j++)
    z_printf(outf,"%s ",tbl[idarray[j]]);
  if (level < ngram_len) {
    if (alpha == 0)
      z_printf(outf," -1e99\n");
    else
      z_printf(outf," %.4f\n",log10(alpha));
  }
  else
    z_printf(outf,"\n");
}    

void
dump_arpa0(FILEHANDLE outf, NgramNode *nga,
	   SLMWordID *idarray, int level, int curlevel, char **tbl)
{
  NgramNode *nd;
  NgramLeaf *nl;
  int i;

  if (level == curlevel) {
    if (level == ngram_len) {
      for (i = 0; i < nga->nelem; i++) {
	nl = &nga->z.leaf[i];
	if (nl->prob <= 0.0) continue;
	idarray[level-1] = nl->id;
	print_ngram_ent(outf,nl->prob,idarray,level,0.0,tbl);
      }
    }
    else {
      for (i = 0; i < nga->nelem; i++) {
	double alpha;
	nd = &nga->z.node[i];
	if (nd->prob <= 0.0) continue;
	idarray[level-1] = nd->id;
	if (discount_proc.disc_proc != NULL)
	  alpha = nd->alpha;
	else
	  alpha = 0; /* no discount model */
	print_ngram_ent(outf,nd->prob,idarray,level,alpha,tbl);
      }
    }
  }
  else {
    for (i = 0; i < nga->nelem; i++) {
      nd = &nga->z.node[i];
      idarray[curlevel-1] = nd->id;
      dump_arpa0(outf,nd,idarray,level,curlevel+1,tbl);
    }
  }
}
	
void
dump_arpa(FILEHANDLE outf, char **tbl)
{
  int i;
  SLMWordID *idarray = New_N(SLMWordID,ngram_len);

  for (i = 0; i < ngram_len; i++)
    ngcount[i] = 0;
  count_ngram(Unigrams,ngcount,1,0);
  if (distance > 0)
    z_printf(outf,"\\distance=%d\n",distance);
  z_printf(outf,"\\data\\\n");
  for (i = 0; i < ngram_len; i++) {
    z_printf(outf,"ngram %d=%d\n",i+1,ngcount[i]);
  }
  z_printf(outf,"\n");
  for (i = 0; i < ngram_len; i++) {
    z_printf(outf,"\n\\%d-grams:\n",i+1);
    dump_arpa0(outf,Unigrams,idarray,i+1,1,tbl);
  }
  z_printf(outf,"\n\\end\\\n");
}


void
print_b_ngram_ent(FILEHANDLE outf, double prob, SLMWordID *idarray,
		  int level, double alpha, char **tbl)
{
  int j;
  int4 p;
  SLMWordID s;

  if (prob == 0)
    p = htonl(0x80000000);
  else
    p = htonl(d2l(log10(prob)));
  z_write(&p,sizeof(int4),1,outf);
    
  for (j = 0; j < level; j++) {
    s = SLMhtonID(idarray[j]);
    z_write(&s,sizeof(SLMWordID),1,outf);
  }
  if (level < ngram_len) {
    if (alpha == 0)
      p = htonl(0x80000000L);
    else
      p = htonl(d2l(log10(alpha)));
    z_write(&p,sizeof(int4),1,outf);
  }
}    

void
dump_bin0(FILEHANDLE outf, NgramNode *nga,
	  SLMWordID *idarray, int level, int curlevel, char **tbl)
{
  NgramNode *nd;
  NgramLeaf *nl;
  int i;

  if (level == curlevel) {
    if (level == ngram_len) {
      for (i = 0; i < nga->nelem; i++) {
	nl = &nga->z.leaf[i];
	if (nl->prob <= 0.0) continue;
	idarray[level-1] = nl->id;
	print_b_ngram_ent(outf,nl->prob,idarray,level,0.0,tbl);
      }
    }
    else {
      for (i = 0; i < nga->nelem; i++) {
	double alpha;
	nd = &nga->z.node[i];
	if (nd->prob <= 0.0) continue;
	idarray[level-1] = nd->id;
	if (discount_proc.disc_proc != NULL)
	  alpha = nd->alpha;
	else
	  alpha = 0; /* no discount model */
	print_b_ngram_ent(outf,nd->prob,idarray,level,alpha,tbl);
      }
    }
  }
  else {
    for (i = 0; i < nga->nelem; i++) {
      nd = &nga->z.node[i];
      idarray[curlevel-1] = nd->id;
      dump_bin0(outf,nd,idarray,level,curlevel+1,tbl);
    }
  }
}
	
void
dump_binary(FILEHANDLE outf, char **tbl)
{
  int i;
  SLMWordID *idarray = New_N(SLMWordID,ngram_len);
  uint2 n;
  int4 l;
  SLMWordID z;

  for (i = 0; i < ngram_len; i++)
    ngcount[i] = 0;
  count_ngram(Unigrams,ngcount,1,0);
  z_write(version_msg,sizeof(char),SLM_BINLM_HEADER_SIZE_V2,outf);
  n = htons(distance);
  z_write(&n,sizeof(uint2),1,outf);
  n = htons(ngram_len);
  z_write(&n,sizeof(uint2),1,outf);
  for (i = 0; i < ngram_len; i++) {
    l = htonl(ngcount[i]);
    z_write(&l,sizeof(int4),1,outf);
  }
  if (class_ht == NULL) {
    z = SLMhtonID(vocab_ht->nelem);
    z_write(&z,sizeof(SLMWordID),1,outf);
    for (i = 1; i <= vocab_ht->nelem; i++) {
      n = htons(strlen(vocab_tbl[i]));
      z_write(&n,sizeof(uint2),1,outf);
      z_write(vocab_tbl[i],sizeof(char),strlen(vocab_tbl[i]),outf);
    }
  }
  else {
    z = SLMhtonID(class_ht->nelem);
    z_write(&z,sizeof(SLMWordID),1,outf);
    for (i = 1; i <= class_ht->nelem; i++) {
      n = htons(strlen(class_tbl[i]));
      z_write(&n,sizeof(uint2),1,outf);
      z_write(class_tbl[i],sizeof(char),strlen(class_tbl[i]),outf);
    }
  }
  for (i = 0; i < ngram_len; i++) {
    dump_bin0(outf,Unigrams,idarray,i+1,1,tbl);
  }
}


/* find a class of a word.
 * a word is composed from the spell of the word, followed by a delimiter,
 * plus the class name. (for example, BOOK+NOUN or RUN+VERB)
 */
SLMWordID
find_class(char *word)
{
  char *p;
  int i;

  for (p = word+strlen(word)-1; p >= word; p--)
    if (*p == delimiter)
      break;
  p++;
  i = SLMIntHashSearch(class_ht,p);
  if (i == 0) {
    fprintf(stderr,"idngram2lm WARNING: class of %s unknown\n",word);
  }
  return i;
}

void
read_class_count(char *idwfreqfile)
{
  int i;
  unsigned long n;
  SLMWordID class_id;
  FILEHANDLE f = z_open(idwfreqfile,"r");

  class_freq = New_N(int4,class_ht->nelem+1);
  class_count = New_N(ClassCount,vocab_ht->nelem+1);

  memset(class_freq,0,sizeof(int4)*class_ht->nelem+1);

  /* read number of word */
  if ((n = read_ulong(f,ascii_input)) < 0)
    goto error;
  if (n != vocab_ht->nelem+1) {
    fprintf(stderr,"idngram2lm ERROR: vocabulary size inconsistent (from vocab file: %d, from %s: %lu)\n",
	    vocab_ht->nelem+1,idwfreqfile,n);
    exit(1);
  }
  /* read word frequency */
  for (i = 0; i < n; i++) {
    if ((class_id = read_ushort(f,ascii_input)) == 0xffff)
      goto error;
    if ((class_count[i].count = read_ulong(f,ascii_input)) < 0)
      goto error;
    class_count[i].class_id = class_id;
    class_freq[class_id] += class_count[i].count;
  }
  z_close(f);
  return;
 error:
  fprintf(stderr,"idngram2lm: format error in %s\n",idwfreqfile);
  exit(1);
}

void
calc_word_uniprob()
{
  int i;
  int n_word;
  NgramNode *u;
  unsigned long n_all = 0, n_singleton = 0, n_zeroton = 0;
  double disc;

  if (class_ht != NULL)
    n_word = class_ht->nelem+1;
  else
    n_word = vocab_ht->nelem+1;

  for (i = 0; i < n_word; i++) {
    u = &Unigrams->z.node[i];
    if (u->count == 0) {
      if (verbosity > 1)
	fprintf(stderr,"idngram2lm Warning: N(%s[%d]) == 0\n",
		class_ht!=NULL?class_tbl[i]:vocab_tbl[i],
		i);
      n_zeroton++;
    }
    else if (u->count == 1)
      n_singleton++;
    n_all += u->count;
  }
  fprintf(stderr,"Total unigram count=%lu  #zeroton=%lu  #singleton=%lu\n",
	  n_all,n_zeroton,n_singleton);
  if (n_zeroton == 0) {
    /* no zeroton words; no need to do smoothing */
    return;
  }
  if (n_singleton == 0) {
    /* there's some zerotons and no singletons. therefore
       we can't calculate discount rate using the number
       of singleton. In this case, probability mass for one 
       singleton is dedicated for all zerotons. */
    disc = 1.0/n_all*discount_tweak;
  }
  else
    disc = (double)n_singleton/n_all*discount_tweak;
  for (i = 0; i < n_word; i++) {
    u = &Unigrams->z.node[i];
    if (u->count == 0)
      u->prob = disc/n_zeroton;
    else
      u->prob *= 1.0-disc;
  }
}

void
calc_class_uniprob()
{
  int i,cid;
  int n_class = class_ht->nelem+1;
  unsigned long n_word = vocab_ht->nelem+1;
  unsigned long *n_zeroton = New_N(unsigned long,n_class);
  unsigned long *n_singleton = New_N(unsigned long,n_class);
  double *disc = New_N(double,n_class);

  memset(n_zeroton,0,sizeof(unsigned long)*n_class);
  memset(n_singleton,0,sizeof(unsigned long)*n_class);
  /* estimate ML probabilities */
  for (i = first_id; i < n_word; i++) {
    cid = class_count[i].class_id;
    if (class_freq[cid] == 0)
      class_count[i].prob = 0.0;
    else
      class_count[i].prob = (double)class_count[i].count/class_freq[cid];
    switch (class_count[i].count) {
    case 0:
      /* zeroton */
      n_zeroton[cid]++;
      break;
    case 1:
      /* singleton */
      n_singleton[cid]++;
      break;
    }
  }
  for (i = first_class_id; i < n_class; i++) {
    if (class_freq[i] == 0)
      disc[i] = 0.0;
    else if (n_singleton[i] == 0 && n_zeroton[i] > 0) {
      /* there's no singleton and some zerotons */
      /* in this case, some a priori discount value */
      /* is used. */
      disc[i] = 1.0/class_freq[i]*discount_tweak;
    }
    else if (n_zeroton[i] == 0) {
      /* there's no zerotons. Therefore, there's no need */
      /* to do smoothing. */
      disc[i] = 0;
    }
    else {
      disc[i] = (double)n_singleton[i]/class_freq[i];
      if (disc[i] == 1.0) {
	/* There're only singletons! */
	disc[i] = 0.5*discount_tweak;
      }
    }
  }
  /* calculate discounted prob */
  for (i = first_id; i < n_word; i++) {
    cid = class_count[i].class_id;
    if (class_freq[cid] == 0) {
      /* if class[cid]'s frequency is 0, there's no way to */
      /* estimate the output probabilities of the words    */
      /* belongs to class[cid]. In this case, uniform      */
      /* distribution is applied upon these words.         */
      class_count[i].prob = 1.0/n_zeroton[cid];
    }
    else if (class_count[i].prob > 0.0)
      class_count[i].prob *= 1.0-disc[cid];
    else
      class_count[i].prob = disc[cid]/n_zeroton[cid];
  }
}

void
dump_class(FILEHANDLE f)
{
  int i;
  double lp;
  z_printf(f,"\\class\\\n");
  z_printf(f,"%d\n",vocab_ht->nelem+1);
  for (i = first_id; i <= vocab_ht->nelem; i++) {
    if (class_count[i].prob == 0)
      lp = -1e99;
    else
      lp = log10(class_count[i].prob); 
    z_printf(f,"%s %s %g\n",
	     class_tbl[class_count[i].class_id],
	     vocab_tbl[i],lp);
  }
}

void
dump_class_binary(FILEHANDLE f)
{
  int i;
  double lp;
  int4 l;
  SLMWordID s;

  z_write("class           ",sizeof(char),16,f);
  l = htonl(vocab_ht->nelem);
  z_write(&l,sizeof(int4),1,f);
  s = SLMhtonID(first_id);
  z_write(&s,sizeof(SLMWordID),1,f);
  for (i = first_id; i <= vocab_ht->nelem; i++) {
    if (class_count[i].prob == 0)
      lp = -1e99;
    else
      lp = log10(class_count[i].prob); 
    s = SLMhtonID(class_count[i].class_id);
    z_write(&s,sizeof(SLMWordID),1,f);
    l = htonl(d2l(lp));
    z_write(&l,sizeof(int4),1,f);
    if (i != 0) {
      s = htons(strlen(vocab_tbl[i]));
      z_write(&s,sizeof(short),1,f);
      z_write(vocab_tbl[i],sizeof(char),strlen(vocab_tbl[i]),f);
    }
  }
}


void
idngram2lm(char *vocabfile,char *idngramfile, char *arpafile, char *binfile,
	   char *ccsfile, char *classfile, char *idwfreqfile)
{
  FILEHANDLE idnf,lmf;

  int i,j;
  unsigned long total;
  SLMWordID *idarray;

  if (verbosity > 1) {
    fprintf(stderr,"N:               %d\n",ngram_len);
    if (distance > 0)
      fprintf(stderr,"Distance:        %d\n",distance);
    fprintf(stderr,"Cutoffs:\n");
    for (i = 1; i < ngram_len; i++) {
      if (cutoffs == NULL) 
	fprintf(stderr,"\t%d-gram: 0",i+1);
      else
	fprintf(stderr,"\t%d-gram: %d\n",i+1,cutoffs[i]);
    }
    fprintf(stderr,"\n");
    fprintf(stderr,"Reading Vocabulary...\n");
  }
  vocab_ht = read_vocab(vocabfile,vocab_tbl,max_vocab);
  if (classfile != NULL) {
    if (idwfreqfile == NULL) {
      fprintf(stderr,"idngram2lm ERROR: classfile is specified and idwfreqfile is not specified\n");
      exit(1);
    }
    if (verbosity > 1) fprintf(stderr,"Reading Class definition...\n");
    class_ht = read_vocab(classfile,class_tbl,max_vocab);
    SLM_Set_NgramType(vocab_type,SLM_ClassNgram);

    if (open_class)
      SLM_Set_CLASS_VOCAB(vocab_type,SLM_CLASS_VOCAB_OPEN);
    else
      SLM_Set_CLASS_VOCAB(vocab_type,SLM_CLASS_VOCAB_CLOSED);
    if (one_unk)
      SLM_Set_N_UNK(vocab_type,SLM_ONE_UNK);
    else
      SLM_Set_N_UNK(vocab_type,SLM_CLASS_UNK);
  }
  switch (SLM_N_UNK(vocab_type)) {
  case SLM_NO_UNK:
    first_id = 1;
    break;
  case SLM_ONE_UNK:
    first_id = 0;
    break;
  case SLM_CLASS_UNK:
    first_id = 0;
    add_class_unk(vocab_ht,class_ht,vocab_tbl,class_tbl,max_vocab);
    break;
  }
  if (idwfreqfile != NULL) {
    if (classfile == NULL) {
      fprintf(stderr,"idngram2lm ERROR: idwfreqfile is specified and classfile is not specified\n");
      exit(1);
    }
    if (verbosity > 1) fprintf(stderr,"Reading ID wfreq...\n");
    read_class_count(idwfreqfile);
  }
  if (no_unk_in_train)
    SLM_Set_UNK_TRAIN(vocab_type,SLM_NO_UNK_IN_TRAIN);
  if (SLM_NgramType(vocab_type) == SLM_ClassNgram) {
    if (SLM_CLASS_VOCAB(vocab_type) == SLM_CLASS_VOCAB_OPEN)
      first_class_id = 1;
    else
      first_class_id = 0;
  }
  if (verbosity > 1)
    fprintf(stderr,"Vocabulary type is %s\n",vocab_type_name(vocab_type));

  /* prepare FOF array, if needed */
  if (discount_proc.need_fof)
    prepare_fof();
  /* prepare temporary idngram space */
  if (class_ht != NULL)
    N_Tmp = class_ht->nelem+1;
  else
    N_Tmp = vocab_ht->nelem+1;
  if (ngram_len > 2) {
    TmpNgramNode = New_N(NgramNode*,ngram_len-2);
    for (i = 0; i < ngram_len-2; i++)
      TmpNgramNode[i] = New_N(NgramNode,N_Tmp);
  }
  TmpNgramLeaf = New_N(NgramLeaf,N_Tmp);

  /* idngram processing start */
  idnf = z_open(idngramfile,"r");
  if (verbosity > 1) fprintf(stderr,"Reading ID N-gram...\n");

  Unigrams = SLMReadIDNgram(idnf,ngram_len,ascii_input); 
  if (verbosity > 1) {
    fprintf(stderr,"\nTotal N-gram count = %lu\nCounting N-gram...\n",
	    total_count);
  }
  ngcount = New_N(unsigned long,ngram_len);
  for (i = 0; i < ngram_len; i++)
    ngcount[i] = 0;
  count_ngram(Unigrams,ngcount,1,1);
  if (verbosity > 1) {
    fprintf(stderr,"# of n-gram: ");
    for (i = 0; i < ngram_len; i++)
      fprintf(stderr,"%lu ",ngcount[i]);
    fprintf(stderr,"\nCalculating ML probabilities...\n");
  }
  total = 0;
  for (i = 0; i < Unigrams->nelem; i++)
    total += Unigrams->z.node[i].count;
  calc_ML_probs_for_node(Unigrams,total,1);
  idarray = New_N(SLMWordID,ngram_len);
  if (discount_proc.disc_proc == NULL) {
    if (verbosity > 1)
      fprintf(stderr,"No discount model: No smoothing method applied.\n");
  }
  else {
    if (discount_proc.need_fof) {
      if (verbosity > 1) {
	fprintf(stderr,"FOF results are:\n");
	for (i = 0; i < ngram_len; i++) {
	  fprintf(stderr,"  %d-gram:",i+1);
	  for (j = 0; j < fof_size; j++)
	    fprintf(stderr,"%5lu ",fof[i][j]);
	  fprintf(stderr,"\n");
	}
      }
    }
    if (verbosity > 1) fprintf(stderr,"Calculating back-off weights...\n");
    if (discount_proc.prepare_proc != NULL)
      discount_proc.prepare_proc();
    discount_proc.disc_proc(Unigrams,1);
  }
  if (cutoffs != NULL) {
    if (verbosity > 1)
      fprintf(stderr,"Calculating cut-off for shorter N-grams...\n");
    calc_cutoff_again(Unigrams,1);
  }
  if (discount_proc.disc_proc != NULL)
    calc_word_uniprob();
  if (classfile != NULL)
    calc_class_uniprob();
  calc_alpha(Unigrams,idarray,1);

  if (verbosity > 1) fprintf(stderr,"Dumping Language Model...\n");
  if (arpafile != NULL) {
    lmf = z_open(arpafile,"w");
    if (classfile != NULL) {
      dump_arpa(lmf,class_tbl);
      dump_class(lmf);
    }
    else
      dump_arpa(lmf,vocab_tbl);
  }
  else {
    lmf = z_open(binfile,"w");
    if (classfile != NULL) {
      dump_binary(lmf,class_tbl);
      dump_class_binary(lmf);
    }
    else
      dump_binary(lmf,vocab_tbl);
  }

  z_close(lmf);
} 

int 
main(int argc, char *argv[]) 
{ 
  char *vocabfile = NULL; 
  char *idngramfile = NULL; 
  char *arpafile = NULL;
  char *binfile = NULL;
  char *ccsfile = NULL;
  char *classfile = NULL;
  char *idwfreqfile = NULL;
  int cutoff_arg = 0;
  int i,j; 

  verbosity = 2; 
  gt_discount_range = New_N(int,ngram_len);
  gt_discount_range[0] = 1;
  for (i = 1; i < ngram_len; i++)
    gt_discount_range[i] = 7;
	
  for (i = 1; i < argc; i++) { 
    if (!strcmp(argv[i],"-vocab")) 
      vocabfile = nextarg(argc,argv,i++); 
    else if (!strcmp(argv[i],"-arpa")) 
      arpafile = nextarg(argc,argv,i++); 
    else if (!strcmp(argv[i],"-binary")) 
      binfile = nextarg(argc,argv,i++); 
    else if (!strcmp(argv[i],"-idngram")) 
      idngramfile = nextarg(argc,argv,i++); 
    else if (!strcmp(argv[i],"-context")) 
      ccsfile = nextarg(argc,argv,i++); 
    else if (!strcmp(argv[i],"-class")) 
      classfile = nextarg(argc,argv,i++); 
    else if (!strcmp(argv[i],"-idwfreq")) 
      idwfreqfile = nextarg(argc,argv,i++); 
    else if (!strcmp(argv[i],"-n")) {
      ngram_len = atoi(nextarg(argc,argv,i++)); 
      if (ngram_len < 2) {
	fprintf(stderr,"idngram2lm: N-gram length must be more than one\n");
	exit(1);
      }
    }
    else if (!strcmp(argv[i],"-ascii_input")) 
      ascii_input = 1; 
    else if (!strcmp(argv[i],"-cutoffs")) {
      cutoff_arg = i+1;
      for (i++; i < argc; i++) { 
	if (!isdigit((int)*argv[i])) {
	  i--;
	  break;
	}
      }
    }
    else if (!strcmp(argv[i],"-verbosity") || !strcmp(argv[i],"-v")) 
      verbosity = atoi(nextarg(argc,argv,i++)); 
    else if (!strcmp(argv[i],"-distance") || !strcmp(argv[i],"-d")) 
      distance = atoi(nextarg(argc,argv,i++)); 
    else if (!strcmp(argv[i],"-vocab_type")) {
      int t = atoi(nextarg(argc,argv,i++));
      switch (t) {
      case 0:
	SLM_Set_WORD_VOCAB(vocab_type,SLM_WORD_VOCAB_CLOSED);
	break;
      case 1:
	/* default */
	break;
      case 2:
	SLM_Set_UNK_TRAIN(vocab_type,SLM_UNK_IN_TRAIN);
	break;
      default:
	fprintf(stderr,"idngram2lm: %d: unknown vocabulary type\n",t);
	exit(1);
      }
    }
    else if (!strcmp(argv[i],"-one_unk"))
      one_unk = 1;
    else if (!strcmp(argv[i],"-no_unk_in_text"))
      no_unk_in_train = 1;
    else if (!strcmp(argv[i],"-open_class"))
      open_class = 1;
    else if (!strcmp(argv[i],"-witten_bell")) {
      discount_proc.prepare_proc = NULL;
      discount_proc.disc_proc = calc_WB_discount;
      discount_proc.need_fof = 0;
    }
    else if (!strcmp(argv[i],"-linear")) {
      discount_proc.prepare_proc = prepare_linear_disc;
      discount_proc.disc_proc = calc_LI_discount;
      discount_proc.need_fof = 1;
    }
    else if (!strcmp(argv[i],"-absolute")) {
      discount_proc.prepare_proc = prepare_absolute_disc;
      discount_proc.disc_proc = calc_AB_discount;
      discount_proc.need_fof = 1;
    }
    else if (!strcmp(argv[i],"-good_turing")) {
      discount_proc.prepare_proc = prepare_GT_disc;
      discount_proc.disc_proc = calc_GT_discount;
      discount_proc.need_fof = 1;
    }
    else if (!strcmp(argv[i],"-no_discount")) {
      discount_proc.prepare_proc = NULL;
      discount_proc.disc_proc = NULL;
      discount_proc.need_fof = 0;
    }
    else if (!strcmp(argv[i],"-discount_tweak"))
      discount_tweak = atof(nextarg(argc,argv,i++));
    else if (argv[i][0] == '-') 
      usage(); 
    else 
      break; 
  } 

  if (vocabfile == NULL) { 
    fprintf(stderr, "idngram2lm: vocabulary file missing\n"); 
    usage(); 
  } 
  if (arpafile == NULL && binfile == NULL) { 
    fprintf(stderr, "idngram2lm: lm file missing\n"); 
    usage(); 
  } 
  if (distance > 0 && ngram_len != 2) {
    /* if distance > 0, N should be 2 */
    fprintf(stderr,"idngram2lm: -n should be 2 when -d is specified\n");
    exit(1);
  }
  if (cutoff_arg > 0) {
    cutoffs = New_N(int,ngram_len);
    i = cutoff_arg; 
    cutoffs[0] = -1; /* no cutoff for unigram */
    for (j = 1; j < ngram_len; j++) {
      if (i+j-1 >= argc || !isdigit((int)*argv[i+j-1])) {
	fprintf(stderr,"idngram2lm: no cutoff threshold specified for %d-gram\n",j+1);
	exit(1);
      }
      cutoffs[j] = atoi(argv[i+j-1]);
      if (cutoffs[j] < cutoffs[j-1]) {
	fprintf(stderr,"idngram2lm: Can't specify smaller cutoff for longer n-gram\n\t(%d for %d-gram vs. %d for %d-gram)\n",
		cutoffs[j-1],j,cutoffs[j],j+1);
	exit(1);
      }
    }
  }

  if (classfile != NULL) {
    max_class = count_vocab(classfile);
    class_tbl = New_N(char*,max_class);
  }
  else
    max_class = 0;
  max_vocab = count_vocab(vocabfile)+max_class;
  vocab_tbl = New_N(char*,max_vocab);

  idngram2lm(vocabfile,idngramfile,arpafile,binfile,ccsfile,classfile,idwfreqfile); 

  return 0; 
} 

void usage()
{
  printf("Usage : idngram2lm    -vocab .vocab\n");
  printf("                      -idngram .idngram\n");
  printf("                      -arpa .arpa\n");
  printf("                      -binary .binlm\n");
  printf("                    [ -class .cls ]\n");
  printf("                    [ -idwfreq .idwfreq ]\n");
  printf("                    [ -n 3 ]\n");
  printf("                    [ -ascii_input ]\n");
  printf("                    [ -linear|-absolute|-good_turing|-witten_bell|-no_discount]\n");
  printf("                    [ -cutoffs bigram-cutoff ... ]\n");
  printf("                    [ -discount_tweak 1.0 ]\n");
  printf("                    [ -one_unk ]\n");
  printf("                    [ -no_unk_in_text ]\n");
  printf("                    [ -open_class ]\n");
  printf("                    [ -verbosity 2 ]\n");
  printf("                    [ -distance 0 ]\n");
  exit(1);
}
