CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

zxcc: $(OBJS)
		$(CC) -o zxcc $(OBJS) $(LDFLAGS)

$(OBJS): zxcc.h

test: zxcc
		./test.sh

clean:
		rm -f zxcc *.o *~ tmp*

.PHONY: test clean