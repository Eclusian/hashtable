
CC=gcc
CFLAGS=-Iinclude -Wall -Werror -Wextra $(USERFLAGS)

SRC_DIR=src/
INCLUDE_DIR=include/
TEST_DIR=test/
BUILD_DIR=build/
INSTALL_LIB_DIR=/usr/lib/
INSTALL_INCLUDE_DIR=/usr/include/

VPATH=$(SRC_DIR):$(INCLUDE_DIR):$(TEST_DIR):$(BUILD_DIR)

INSTALL_FILES=$(INSTALL_LIB_DIR)libhashtable.a $(INSTALL_INCLUDE_DIR)hashtable.h

TESTSRCS=$(shell ls test | grep '.c$$')
TMP=$(TESTSRCS:.c= )
TESTTARGETS=$(foreach item,$(TMP),build/$(item))

.PHONY: all
.PHONY: install
.PHONY: install_libs
.PHONY: install_includes
.PHONY: uninstall
.PHONY: clean

all: libhashtable.a $(TESTTARGETS)

install: install_libs install_includes

install_libs: libhashtable.a
	cp -t $(INSTALL_LIB_DIR) $^

install_includes: hashtable.h
	cp -t $(INSTALL_INCLUDE_DIR) $^

uninstall:
	rm -f $(INSTALL_FILES)

build/libhashtable.a: build/hashtable.o
	mkdir -p build
	ar ruv $@ $^
	ranlib $@

build/hashtable.o: hashtable.c hashtable.h
	mkdir -p build
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm -rf build/*

# Tests

$(TESTTARGETS): build/%: %.c build/libhashtable.a
	mkdir -p build
	$(CC) $(CFLAGS) $< -Lbuild -lhashtable -o $@

