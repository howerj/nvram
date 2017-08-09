CC=gcc
CFLAGS=-Wall -Wextra -std=gnu99 -O2 -fno-toplevel-reorder
TARGET=nvram

.PHONY: all run edit clean

all: ${TARGET}

run: ${TARGET}
	./${TARGET}

XML: XML.txz
	tar -Jxf $<

doxygen: Doxyfile nvram.c
	doxygen $<

edit: editor.pl doxygen XML
	./$<

clean:
	rm -fv ${TARGET} *.o *.blk

