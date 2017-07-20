CC=gcc
CFLAGS=-Wall -Wextra -std=gnu99 -O2 -fno-toplevel-reorder
TARGET=nvram

all: ${TARGET}

run: ${TARGET}
	./${TARGET}

clean:
	rm -fv ${TARGET} *.o *.blk

