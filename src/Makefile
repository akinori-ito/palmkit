# Generated automatically from Makefile.in by configure.
CC=gcc
INCLUDES=-I. -I./libs
DEFS= -DHAVE_ZLIB_H=1 -DSTDC_HEADERS=1 -DRETSIGTYPE=void -DHAVE_VPRINTF=1 -DHAVE_STRCHR=1 -DHAVE_STRSPN=1 -DENABLE_LONGID=1 
CFLAGS=-g -Wall $(INCLUDES) $(DEFS)
LDFLAGS=-Llibs -lslm -lm -ldmalloc
SLMOBJS=hash.o inthash.o io.o misc.o
TARGETS=text2wfreq wfreq2vocab text2idngram idngram2lm evallm \
	mergeidngram mixidngram mergeidwfreq mixidwfreq ctext2idngram \
	interpolate ngram2mgram reverseidngram changeidvocab \
	initehmm learnehmm viterbiehmm 
LIB=libs/libslm.a
SCRIPTS=ctext2class text2class text2idtext reverseword

all: $(LIB) $(TARGETS)

$(LIB):
	cd libs; make

text2wfreq: text2wfreq.o libs/libslm.a
	$(CC) $(CFLAGS) -o text2wfreq text2wfreq.o $(LDFLAGS)

wfreq2vocab: wfreq2vocab.o libs/libslm.a
	$(CC) $(CFLAGS) -o wfreq2vocab wfreq2vocab.o $(LDFLAGS)

text2idngram: text2idngram.o idngram.o remove_tmp.o libs/libslm.a
	$(CC) $(CFLAGS) -o text2idngram text2idngram.o idngram.o remove_tmp.o $(LDFLAGS)

idngram2lm: idngram2lm.o libs/libslm.a
	$(CC) $(CFLAGS) -o idngram2lm idngram2lm.o $(LDFLAGS)

evallm: evallm.o libs/libslm.a
	$(CC) $(CFLAGS) -o evallm evallm.o $(LDFLAGS)

mergeidngram: mergeidngram.o libs/libslm.a
	$(CC) $(CFLAGS) -o mergeidngram mergeidngram.o $(LDFLAGS)

mixidngram: mixidngram.o libs/libslm.a
	$(CC) $(CFLAGS) -o mixidngram mixidngram.o $(LDFLAGS)

ctext2idngram: ctext2idngram.o idngram.o libs/libslm.a
	$(CC) $(CFLAGS) -o ctext2idngram ctext2idngram.o idngram.o $(LDFLAGS)

interpolate: interpolate.o libs/libslm.a
	$(CC) $(CFLAGS) -o interpolate interpolate.o $(LDFLAGS)

mergeidwfreq: mergeidwfreq.o libs/libslm.a
	$(CC) $(CFLAGS) -o mergeidwfreq mergeidwfreq.o $(LDFLAGS)

mixidwfreq: mixidwfreq.o libs/libslm.a
	$(CC) $(CFLAGS) -o mixidwfreq mixidwfreq.o $(LDFLAGS)

ngram2mgram: ngram2mgram.o libs/libslm.a
	$(CC) $(CFLAGS) -o ngram2mgram ngram2mgram.o $(LDFLAGS)

reverseidngram: reverseidngram.o idngram.o remove_tmp.o libs/libslm.a
	$(CC) $(CFLAGS) -o reverseidngram reverseidngram.o idngram.o remove_tmp.o  $(LDFLAGS)

changeidvocab: changeidvocab.o idngram.o remove_tmp.o libs/libslm.a
	$(CC) $(CFLAGS) -o changeidvocab changeidvocab.o idngram.o remove_tmp.o $(LDFLAGS)

initehmm: initehmm.o ehmm.o libs/libslm.a
	$(CC) $(CFLAGS) -o initehmm initehmm.o ehmm.o $(LDFLAGS)

learnehmm: learnehmm.o ehmm.o forback.o libs/libslm.a
	$(CC) $(CFLAGS) -o learnehmm learnehmm.o ehmm.o forback.o $(LDFLAGS)

viterbiehmm: viterbiehmm.o ehmm.o forback.o libs/libslm.a
	$(CC) $(CFLAGS) -o viterbiehmm viterbiehmm.o ehmm.o forback.o $(LDFLAGS)

ngramd: ngramd.o libs/libslm.a
	$(CC) $(CFLAGS) -o ngramd ngramd.o $(LDFLAGS)

install: $(TARGETS) $(LIB)
	-mkdir ../bin
	-mkdir ../lib
	-mkdir ../include
	cp $(TARGETS) $(SCRIPTS) ../bin
	cp $(LIB) ../lib
	cp libs/slm.h ../include

clean:
	rm -f *.o $(TARGETS) *~ *.bak
	cd libs; make clean
	rm -f bin/*

depend:
	makedepend $(INCLUDES) *.c *.h
	cd libs; make depend

