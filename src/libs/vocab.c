#include <stdlib.h>
#include "vocab.h"
#include <string.h>

SLMHashTable * 
read_vocab(char *vocabfile, char **vocab_tbl, int max_vocab)
{
    SLMHashTable *ht;
    FILEHANDLE inf;
    char buf[256];
    int n = 1;
    char *v;

    inf = z_open(vocabfile,"r");
    if (vocab_tbl) {
	vocab_tbl[0] = "<UNK>";
	ht = SLMHashCreateSI(max_vocab);
    }
    else {
#ifdef ENABLE_LONGID
	ht = SLMHashCreateSI(TEMP_MAX_VOCAB);
#else
	ht = SLMHashCreateSI(MAX_VOCAB);
#endif
    }

    while (z_getstr(inf,buf,256) == 0) {
	if (!strncmp(buf,"##",2)) {
	    /* comment */
	    /* skip until the end of line */
	    while (z_getc(inf) != '\n')
		;
	    continue;
	}
	v = strdup(buf);
	SLMIntHashInsert(ht,v,n);
	n++;
	if (vocab_tbl) {
	    vocab_tbl[n-1] = v;
	    if (n == max_vocab) {
		fprintf(stderr,"Vocabulary too large!\n");
		exit(1);
	    }
	}
    }
    z_close(inf);
    return ht;
}

int
add_vocab(SLMHashTable *vocab_ht, char *word, char **vocab_tbl, int max_vocab)
{
    int id = vocab_ht->nelem;
    SLMIntHashInsert(vocab_ht,word,id+1);
    if (vocab_tbl != NULL) {
	if (id+1 >= max_vocab) {
	    fprintf(stderr,"Vocabulary too large!\n");
	    exit(1);
	}
	vocab_tbl[id+1] = word;
    }
    return id+1;
}

void
add_class_unk(SLMHashTable *vocab_ht, SLMHashTable *class_ht,
	      char **vocab_tbl, char **class_tbl, int max_vocab)
{
    int i;
    char buf[256];
    for (i = 0; i <= class_ht->nelem; i++) {
	sprintf(buf,"<UNK>+%s",class_tbl[i]);
	add_vocab(vocab_ht,strdup(buf),vocab_tbl,max_vocab);
    }
}

char*
vocab_type_name(int type)
{
    static char buf[256];
    buf[0] = '\0';
    switch(SLM_NgramType(type)) {
    case SLM_WordNgram:
	strcat(buf,"WordModel,");
	break;
    case SLM_ClassNgram:
	strcat(buf,"ClassModel,");
	break;
    }
    switch(SLM_N_UNK(type)|SLM_UNK_TRAIN(type)) {
    case SLM_NO_UNK|SLM_NO_UNK_IN_TRAIN:
	strcat(buf,"have no <UNK>,");
	break;
    case SLM_ONE_UNK|SLM_UNK_IN_TRAIN:
	strcat(buf,"have <UNK>,");
	break;
    case SLM_ONE_UNK|SLM_NO_UNK_IN_TRAIN:
	strcat(buf,"have <UNK>,no <UNK> in training text,");
	break;
    case SLM_CLASS_UNK|SLM_UNK_IN_TRAIN:
	strcat(buf,"have class <UNK>,");
	break;
    }
    if (SLM_WORD_VOCAB(type)) 
	strcat(buf,"open vocab");
    else
	strcat(buf,"closed vocab,");
    if (SLM_NgramType(type)==SLM_ClassNgram) {
	if (SLM_CLASS_VOCAB(type) == SLM_CLASS_VOCAB_OPEN) 
	    strcat(buf,",open class");
	else
	    strcat(buf,",closed class");
    }
    return buf;
}

int
count_vocab(char *filename)
{
    char buf[1024];
    int n = 0;
    FILEHANDLE f = z_open(filename,"r");
    
    while (z_gets(buf,1024,f) != NULL)
	n++;
    z_close(f);
    return n+2;
}
