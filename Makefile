CFLAGS=-std=c11 -g -static -fno-common
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

zxcc: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): zxcc.h

zxcc-gen2: zxcc $(SRCS) zxcc.h
	./self.sh

extern.o: tests-extern
	gcc -xc -c -o extern.o tests-extern

test: zxcc extern.o
	./zxcc tests > tmp.s
	gcc -static -o tmp tmp.s extern.o
	./tmp

test-gen2: zxcc-gen2 extern.o
	./zxcc-gen2 tests > tmp.s
	gcc -static -o tmp tmp.s extern.o
	./tmp

clean:
	rm -rf zxcc zxcc-gen* *.o *~ tmp*

.PHONY: test clean
