#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <slm.h>

char *inw[] = {
  "私","の","もの"
};

int
main(int argc, char *argv[])
{
  SLMNgram *ng;
  SLMWordID ids[3];
  SLMBOStatus st;
  int i;

  if (argc < 2) {
    fprintf(stderr,"usage: %s file.arpa\n",argv[0]);
    exit(1);
  }
  ng = SLMReadLM(argv[1],SLM_LM_ARPA,1);
  for (i = 0; i < 3; i++)
    ids[i] = SLMWord2ID(ng,inw[i]);

  printf("P(%s[%d]|%s[%d],%s[%d])=%f\n",
	 inw[2],ids[2],inw[0],ids[0],inw[1],ids[1],
	 SLMGetBOProb(ng,3,ids,&st));
  return 0;
}
	 
