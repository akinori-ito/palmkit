# Generated automatically from Makefile.in by configure.
CC=gcc
SLMOBJS=hash.o io.o misc.o ngram.o ngramcount.o vocab.o context.o 
INCLUDES=-I. -I./libs
CFLAGS=-g -Wall  -DHAVE_ZLIB_H=1 -DSTDC_HEADERS=1 -DRETSIGTYPE=void -DHAVE_VPRINTF=1 -DHAVE_STRCHR=1 -DHAVE_STRSPN=1 -DENABLE_LONGID=1 
SLM_HEADERS=intsize.h libs.h io.h hash.h ngram.h context.h
TARGETS=slm.h libslm.a

all: $(TARGETS)

libslm.a: $(SLMOBJS)
	ar r libslm.a $(SLMOBJS)
	ranlib libslm.a

slm.h: $(SLM_HEADERS)
	./mkslm_h  -DHAVE_ZLIB_H=1 -DSTDC_HEADERS=1 -DRETSIGTYPE=void -DHAVE_VPRINTF=1 -DHAVE_STRCHR=1 -DHAVE_STRSPN=1 -DENABLE_LONGID=1  $(SLM_HEADERS) > slm.h

clean:
	rm -f $(TARGETS) *.o *~ *.bak intsize.h checkversize int_autom.c float_autom.c

depend:
	makedepend $(INCLUDES) *.c *.h

libs.h: intsize.h

intsize.h: checkversize
	./checkversize > intsize.h

checkversize: checkversize.c
	$(CC) $(CFLAGS) -o checkversize checkversize.c

hash.o: hash.c hash.h libs.h
io.o: io.c io.h libs.h misc.h int_autom.c float_autom.c
misc.o: misc.c misc.h
ngram.o: ngram.c libs.h io.h misc.h hash.h vocab.h ngram.h net.h ngramd.h
ngramcount.o: ngramcount.c libs.h io.h misc.h hash.h vocab.h ngram.h
vocab.o: vocab.c vocab.h
context.o: context.h misc.h

int_autom.c: int_autom.x
	perl proc_autom.pl int_autom.x > int_autom.c

float_autom.c: float_autom.x
	perl proc_autom.pl float_autom.x > float_autom.c
