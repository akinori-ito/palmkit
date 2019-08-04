CC=gcc
CFLAGS=-g -I../include -Wall
LIBS=-L../lib -lslm -lm

sample1: sample1.o
	$(CC) $(CFLAGS) sample1.o -o sample1 $(LIBS)

clean:
	$(RM) *~ *.o sample1
