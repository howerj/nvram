CC=gcc
CFLAGS=-Wall -Wextra -std=gnu99 -O2 -fno-toplevel-reorder
TARGET=nvram
.PHONY: all run edit clean

ifeq ($(OS),Windows_NT)
DF=
EXE=.exe
.PHONY: nvram
nvram: nvram.exe
else # assume unixen
DF=./
EXE=
endif

all: ${TARGET}

run: ${TARGET}${EXE}
	${DF}${TARGET}${EXE}

${TARGET}${EXE}: nvram.c
	${CC} ${CFLAGS} $< -o $@

XML: 
	tar -Jxf XML.txz

doxygen: Doxyfile nvram.c
	doxygen $<

edit: editor.pl doxygen XML
	${DF}$<

clean:
	rm -fv ${TARGET}${EXE} *.o *.blk

