
CC=gcc
CFLAGS=-Wall -Werror -Wextra


all: libhashtable.a

install: libhashtable.a
	cp libhashtable.a /usr/lib/libhashtable.a
	cp hashtable.h /usr/include/hashtable.h

uninstall:
	rm -f /usr/lib/libhashtable.a /usr/include/hashtable.h

libhashtable.a: hashtable.o
	ar ruv libhashtable.a hashtable.o
	ranlib libhashtable.a

hashtable.o: 
	$(CC) $(CFLAGS) hashtable.c -c -o hashtable.o

clean:
	rm -f libhashtable.a hashtable.o

