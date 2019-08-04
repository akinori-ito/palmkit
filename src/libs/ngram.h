#ifndef SLM_NGRAM_H
#define SLM_NGRAM_H

#undef NG_CACHE  /* use cache to expedite n-gram search */

#define MAX_GRAM 20 /* 20-gram max */
#define SLM_DEFAULT_DELIMITER '+'

#ifdef ENABLE_LONGID
typedef uint4 SLMWordID;
#define SLMhtonID(x) htonl(x)
#define SLMntohID(x) ntohl(x)
#define SLM_NONWORD 0xffffffff   /* not a word id */
#define SLMWordID_FMT "%lu"
#else
typedef uint2 SLMWordID;
#define SLMhtonID(x) htons(x)
#define SLMntohID(x) ntohs(x)
#define SLM_NONWORD 0xffff   /* not a word id */
#define SLMWordID_FMT "%hu"
#endif
typedef SLMWordID *SLMWordTuple;


typedef struct {
    uint4 count;
    SLMWordID *word_id;
} SLMNgramCount;

/* N-gram tree leaf */
typedef struct {
    SLMWordID id;
    float prob;
} SLMNgramLeaf;

/* N-gram tree node */
/* An n-gram tree is expressed as N arrays: node[0], node[1],...
   unigram probs are stored in node[0], bigram in node[1], and so on.
   The probs for the longest N are stored in the array `leaf', not `node'.

   The members id, prob and alpha have the word ID, probability and
   back-off weight respectively. nextpos denotes the index of the first
   node/leaf in the next-level array. nelem is number of child of the node.
*/
typedef struct {
    SLMWordID id;
    float prob;
    float alpha;
    int nextpos;
    unsigned int nelem;
} SLMNgramNode;


#ifdef NG_CACHE
/* N-gram search history */
typedef struct {
    SLMWordID id;
    SLMNgramNode *node;
} SLMNgramSearchHist;
#endif

/* N-gram structure */
typedef struct SLM_Ngram {
    short type;                      /* type of N-gram */
    unsigned char first_id;        /* closed:1 open:0 */
    unsigned char first_class_id;  /* closed:1 open:0 */
    int ngram_len;                 /* unigram=1 bigram=2 trigram=3...*/
    int context_len;               /* length of the context words. Usually
				      it is ngram_len-1, but it varies
				      in the case of distant-bigram. */
    int n_unigram;                 /* # of word for word-ngram, */
                                   /* # of class for class-ngram */
    char **vocab;                  /* array of vocabulary words */
    SLMHashTable *vocab_ht;        /* hash table of vocab */
    char delimiter;                /* delimiter between word and class */
    char **class_sym;              /* array of class symbols */
    SLMHashTable *class_ht;        /* hash table of class */
    int n_word;                    /* # of word: used for class-ngram */
    float *c_uniprob;              /* array of P(w|c) for each w      */
    SLMWordID *class_id;           /* class number of a word  */
    SLMNgramNode **node;           /* N-gram tree node */
    SLMNgramLeaf *leaf;            /* N-gram tree leaf */
    /* following members are used for combined model */
    char *filename;                /* filename of LM */
    float weight;                  /* weight of this model */
    struct SLM_Ngram *next_lm;     /* next LM */
    struct SLM_Ngram *delegate;    /* If this model is a mirror of another */
                                   /* model, this member points that */
#ifdef NG_CACHE
    SLMNgramSearchHist *hist;      /* previously searched ngram */
#endif
    int socket;                    /* socket for remote model */
} SLMNgram;

typedef struct {
    unsigned char len;  /* length of the eveluated n-gram */
    char hit[MAX_GRAM]; /* hit status */
    float ng_prob;      /* P(w|w') for word n-gram, P(c|c') for class n-gram */
    float ug_prob;      /* P(w|c) for class n-gram */
} SLMBOStatus;

#define SLMNewWordTuple(len) New_N(SLMWordID,len)
#define SLMNewWordTuple_N(n,len) New_N(SLMWordID,(len)*(n))
SLMNgramCount *SLMNewNgramCount(int len);
SLMNgramCount *SLMNewNgramCount_N(int n,int len);

SLMWordTuple SLMDupTuple(SLMWordTuple t, int len);
SLMNgramCount *SLMReadNgramCount(int ngram_len, FILEHANDLE inf,SLMNgramCount *base,int ascii_in);
int SLMCompareNgramCount(int ngram_len, SLMNgramCount *p1, SLMNgramCount *p2);
void SLMPrintNgramCount(int ngram_len,SLMNgramCount *ngc, FILEHANDLE outf,int ascii_out);
void SLMMergeIDNgram(int ngram_len, char **list, int nlist, FILEHANDLE outf, int ascii_in, int ascii_out);
void SLMMixIDNgram(int ngram_len, char **list, double *weight, int nlist, FILEHANDLE outf, int ascii_in, int ascii_out);

#define SLM_LM_ARPA      0
#define SLM_LM_BINGRAM   1
#define SLM_LM_BINARY    2

SLMNgram *SLMReadLM(char *filename,int format,int verbosity);
void SLMFreeLM(SLMNgram *ng);
#ifdef USE_WHEN_APPEARED
void SLMAddLM(SLMNgram *ng, int len, double weight, int use_when_appeared, char *filename,int format,int verbosity);
#else
void SLMAddLM(SLMNgram *ng, int len, double weight, char *filename,int format,int verbosity);
#endif
SLMWordID SLMWord2ID(SLMNgram *ng, char *word);
double SLMGetBOProb(SLMNgram *ng, int len, SLMWordID *idarray, SLMBOStatus *status);
void SLMBOStatusString(SLMBOStatus *status, char *buf);
int SLMVocabSize(SLMNgram *ng);
int SLMContextLength(SLMNgram *ng);
int SLMNgramLength(SLMNgram *ng);
const char *SLMID2Word(SLMNgram *ng, SLMWordID id);
double SLMl2d(int4 x);
int4 SLMd2l(double x);

#define SLM_STAT_HIT           3
#define SLM_STAT_BO_WITH_ALPHA 1
#define SLM_STAT_BO            0

#define SLM_BINLM_HEADER_SIZE 512
#define SLM_BINLM_HEADER_SIZE_V1 512
#define SLM_BINLM_HEADER_SIZE_V2 510

#define SLM_BINLM_HEADER_MSG_V1 "Palmkit binary format v.1"
#define SLM_BINLM_HEADER_MSG_V2 "Palmkit binary format v.2"
#endif
