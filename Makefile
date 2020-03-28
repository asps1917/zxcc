CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

zxcc: $(OBJS)
		$(CC) -o zxcc $(OBJS) $(LDFLAGS)

$(OBJS): zxcc.h

test: zxcc
		./zxcc tests > tmp.s
		echo 'int char_fn() { return 257; } int static_fn() { return 5; }' | \
			gcc -xc -c -o tmp2.o -
		gcc -static -o tmp tmp.s tmp2.o
		./tmp

clean:
		rm -f zxcc *.o *~ tmp*

.PHONY: test clean