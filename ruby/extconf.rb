#!/usr/local/bin/ruby

require "mkmf"

$CFLAGS = "-I../include"
#$LDFLAGS = "-L../lib"
$LOCAL_LIBS = "../lib/libslm.a"
create_makefile("palmkit")
