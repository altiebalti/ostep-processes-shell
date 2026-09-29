CC      = gcc
CFLAGS  = -Wall -Werror -g -std=gnu11

wish: wish.c
	$(CC) $(CFLAGS) -o wish wish.c

test: wish
	./test-wish.sh $(ARGS)

clean:
	rm -f wish
	rm -rf tests-out

.PHONY: test clean
