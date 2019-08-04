#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <slm.h>
#include <ngramd.h>
#include <misc.h>

#undef DEBUG

#define DEFAULT_PORTNUM 9876

extern int errno;

void
usage()
{
  printf("Usage : ngramd  -P portnum\n");
  printf("                -arpa file.arpa\n");
  printf("                -binary file.binlm\n");
  exit(1);
}

void
simply_wait(int signum)
{
  int status;
  wait(&status);
  signal(SIGCHLD,simply_wait);
}

#ifndef DEBUG
#define Fwrite(ptr,nelem,elsize,fptr) fwrite(ptr,nelem,elsize,fptr) 
#else
int Fwrite(void *ptr, int nelem,int elsize,FILE *fptr) 
{
  int i,rval;
  char *p = ptr;
  rval = fwrite(ptr,nelem,elsize,fptr) ;
  fprintf(stderr,"fwrite: %d-%d values=",nelem,elsize);
  for (i = 0; i < nelem*elsize; i++)
    fprintf(stderr,"%02x ",p[i]&0xff);
  fprintf(stderr,"\n");
  return rval;
}
#endif

void
do_service(SLMNgram *ng, int sock)
{
  FILE *infile,*outfile;
  unsigned char cmd,len;
  uint2 i2;
  SLMWordID id,idarray[MAX_GRAM];
  char buf[256];
  const char *p;
  float x;
  SLMBOStatus status;
  int4 q;
  int i;
  
  infile = fdopen(sock,"r");
  outfile = fdopen(sock,"w");
  if (infile == NULL || outfile == NULL)
    exit(2);
  while (1) {
    fread(&cmd,1,1,infile);
#ifdef DEBUG
    fprintf(stderr,"cmd=%d\n",cmd);
#endif
    switch((int)cmd) {
    case SLM_NGD_CLOSE:
      exit(0);
    case SLM_NGD_BASIC_INFO:
      i2 = ng->type;
      buf[0] = ((i2 >> 8)&0xff);
      buf[1] = (i2 & 0xff);
      buf[2] = ng->first_id;
      buf[3] = ng->first_class_id;
      buf[4] = SLMNgramLength(ng);
      buf[5] = SLMContextLength(ng);
      i2 = SLMVocabSize(ng);
      buf[6] = ((i2 >> 8)&0xff);
      buf[7] = (i2 & 0xff);
      Fwrite(buf,7,1,outfile);
      break;
    case SLM_NGD_WORD2ID:
      fread(&len,1,1,infile);
      fread(buf,len,1,infile);
      buf[len] = '\0';
      id = htons(SLMWord2ID(ng,buf));
      Fwrite(&id,2,1,outfile);
      break;
    case SLM_NGD_ID2WORD:
      fread(&id,1,2,infile);
      p = SLMID2Word(ng,ntohs(id));
      len = strlen(p);
      Fwrite(&len,1,1,outfile);
      Fwrite(p,len,1,outfile);
      break;
    case SLM_NGD_PROB:
      fread(&len,1,1,infile);
      for (i = 0; i < len; i++) {
	fread(&id,1,2,infile);
	if (i < MAX_GRAM)
	  idarray[i] = ntohs(id);
#ifdef DEBUG
	fprintf(stderr,"%d: received=%d internal=%d\n",i,id,ntohs(id));
#endif
      }
      x = SLMGetBOProb(ng,len,idarray,&status);
      len = status.len;
#ifdef DEBUG
      fprintf(stderr,"%d %d %d PROB: %g\n",idarray[0],idarray[1],idarray[2],x);
#endif
      Fwrite(&len,1,1,outfile);
      q = htonl(SLMd2l(log(status.ng_prob)));
      Fwrite(&q,4,1,outfile);
      q = htonl(SLMd2l(log(status.ug_prob)));
      Fwrite(&q,4,1,outfile);
      Fwrite(status.hit,len,1,outfile);
      break;
    }
    fflush(outfile);
  }
}
  


void
provide(SLMNgram *ng, int port)
{
  int sock,newsock,len;
  struct sockaddr_in addr;
  pid_t pid;

  sock = socket(AF_INET, SOCK_STREAM, 6);  /* TCP socket */
  if (sock < 0) {
    fprintf(stderr,"Can't create socket\n");
    exit(1);
  }
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock,(struct sockaddr*)&addr,sizeof addr) < 0) {
    fprintf(stderr,"Can't bind socket: %s\n",strerror(errno));
    exit(1);
  }
  if (listen(sock,5) < 0) {
    fprintf(stderr,"Can't listen socket: %s\n",strerror(errno));
    exit(1);
  }
  signal(SIGCHLD,simply_wait);
  while (1) {
    len = sizeof addr;
    newsock = accept(sock,(struct sockaddr*)&addr,&len);
    if (newsock < 0) {
      fprintf(stderr,"Accept error: %s\n",strerror(errno));
      exit(1);
    }
    if ((pid = fork()) == 0) {
      do_service(ng,newsock);
    }
    else if (pid < 0) {
      fprintf(stderr,"Can't spawn subprocess: %s\n",strerror(errno));
      exit(1);
    }
  }
}
  

int
main(int argc, char *argv[])
{
  int i;
  int port = DEFAULT_PORTNUM;
  char *lmfile = NULL;
  int lmmode;
  SLMNgram *ng;

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i],"-P")) {
      port = atoi(nextarg(argc,argv,i++));
    }
    else if (!strcmp(argv[i],"-arpa")) {
      lmfile = nextarg(argc,argv,i++);
      lmmode = SLM_LM_ARPA;
    }
    else if (!strcmp(argv[i],"-binary")) {
      lmfile = nextarg(argc,argv,i++);
      lmmode = SLM_LM_BINARY;
    }
    else
      usage();
  }
  if (lmfile == NULL) {
    fprintf(stderr,"LM file missing; use -arpa or -binary option\n");
    exit(1);
  }
  ng = SLMReadLM(lmfile,lmmode,2);
  if (ng == NULL) {
    fprintf(stderr,"Can't read the language model %s\n",lmfile);
    exit(1);
  }
  provide(ng,port);
  return 0;
}
