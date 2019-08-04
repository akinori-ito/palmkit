
/* ngram daemon mode definition */

/* command: client->server */

/* closes the server. no parameters. no return value.*/
#define SLM_NGD_CLOSE           0

/* requests the basic info. no parameters.      */
/* return value:                                */
/*      type      (2bytes)                      */
/*      first_id  (1bytes)                      */
/*      first class_id (1bytes)                 */
/*      ngram_len (1byte)                       */
/*      context_len (1byte)                     */
/*      vocab_size (2bytes)                     */
#define SLM_NGD_BASIC_INFO      1

/* converts a string into id. */
/* parameter: length of the string (1byte)                */
/*            the string (arbitrary size, up to 255 bytes)*/
/* return value: the id number (2bytes)                   */
#define SLM_NGD_WORD2ID         2

/* converts an id into string. */
/* parameter: the id number (2bytes)                      */
/* return value: length of the string (1byte)             */
/*            the string (arbitrary size, up to 255 bytes)*/
#define SLM_NGD_ID2WORD         3

/* calculate the n-gram probs. */
/* parameter: length of the ngram (1byte)                 */
/*            id array (2*n bytes)                        */
/* return value:                                          */
/*    the real length of the evaluated n-gram (1byte)     */
/*    the ngram probability converted in int (4bytes)     */
/*    the class->word prob in int (4bytes)                */
/*    hit info (1*n bytes)                                */
#define SLM_NGD_PROB            4



