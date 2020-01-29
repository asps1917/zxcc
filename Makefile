CFLAGS=-std=c11 -g -static

zxcc: zxcc.c

test: zxcc
		./test.sh

clean:
		rm -f zxcc *.o *~ tmp*

.PHONY: test clean