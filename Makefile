# Makefile for palmkit 1.0

all:
	cd src; make

depend:
	cd src; make depend

install:
	cd src; make install

clean:
	cd src; make clean
	rm -f bin/* lib/* include/*
