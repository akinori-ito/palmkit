#include "hash.h"
#include "io.h"

#ifdef ENABLE_LONGID
#define MAX_VOCAB 4294967294
#define TEMP_MAX_VOCAB 1000000
#else
#define MAX_VOCAB 65000
#endif

/*                                    |class |# of UNK|    UNK in   |  word  | class  */
/* vocab_type                         |model |        |training text| vocab  | vocab  */
/* -----------------------------------+------+--------+-------------+--------+------- */
/* VOCAB_CLOSED                       |  NO  |    0   |      NO     | CLOSED | NONE   */
/* VOCAB_OPEN                         |  NO  |    1   |     YES     | OPEN   | NONE   */
/* VOCAB_OPEN_NOUNK                   |  NO  |    1   |      NO     | OPEN   | NONE   */
/* VOCAB_CLOSED_CLASS_CLOSED          | YES  |    0   |      NO     | CLOSED | CLOSED */
/* VOCAB_OPEN_CLASS_CLOSED            | YES  | #class |     YES     | OPEN   | CLOSED */
/* VOCAB_OPEN_CLASS_OPEN              | YES  |#class+1|     YES     | OPEN   | OPEN   */
/* VOCAB_OPEN_CLASS_ONEUNK            | YES  |    1   |     YES     | OPEN   | OPEN   */


#define SLM_NgramType_MASK       0x0001
#define SLM_WordNgram            0x0000
#define SLM_ClassNgram           0x0001

#define SLM_NgramType(x) ((x)&SLM_NgramType_MASK)
#define SLM_Set_NgramType(x,v) ((x)=(((x)&~SLM_NgramType_MASK)|(v)))

#define SLM_N_UNK_MASK           0x0006
#define SLM_NO_UNK               0x0000
#define SLM_ONE_UNK              0x0002
#define SLM_CLASS_UNK            0x0004
#define SLM_CLASSONE_UNK         0x0006

#define SLM_N_UNK(x) ((x)&SLM_N_UNK_MASK)
#define SLM_Set_N_UNK(x,v) ((x)=(((x)&~SLM_N_UNK_MASK)|(v)))

#define SLM_UNK_IN_TRAIN_MASK    0x0008
#define SLM_NO_UNK_IN_TRAIN      0x0000
#define SLM_UNK_IN_TRAIN         0x0008

#define SLM_UNK_TRAIN(x) ((x)&SLM_UNK_IN_TRAIN_MASK)
#define SLM_Set_UNK_TRAIN(x,v) ((x)=(((x)&~SLM_UNK_IN_TRAIN_MASK)|(v)))

#define SLM_WORD_VOCAB_MASK      0x0010
#define SLM_WORD_VOCAB_CLOSED    0x0000
#define SLM_WORD_VOCAB_OPEN      0x0010

#define SLM_WORD_VOCAB(x) ((x)&SLM_WORD_VOCAB_MASK)
#define SLM_Set_WORD_VOCAB(x,v) ((x)=(((x)&~SLM_WORD_VOCAB_MASK)|(v)))

#define SLM_CLASS_VOCAB_MASK     0x0060
#define SLM_CLASS_VOCAB_NONE     0x0000
#define SLM_CLASS_VOCAB_CLOSED   0x0020
#define SLM_CLASS_VOCAB_OPEN     0x0040

#define SLM_CLASS_VOCAB(x) ((x)&SLM_CLASS_VOCAB_MASK)
#define SLM_Set_CLASS_VOCAB(x,v) ((x)=(((x)&~SLM_CLASS_VOCAB_MASK)|(v)))

#ifdef ENABLE_REMOTE_MODEL
#define SLM_REMOTE_MODEL         0x1000
#endif

SLMHashTable * read_vocab(char *vocabfile, char **vocab_tbl, int max_vocab);
int add_vocab(SLMHashTable *vocab_ht, char *word, char **vocab_tbl, int max_vocab);
void add_class_unk(SLMHashTable *vocab_ht, SLMHashTable *class_ht,char **vocab_tbl, char **class_tbl, int max_vocab);

char *vocab_type_name(int type);
int count_vocab(char *filename);
