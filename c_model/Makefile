#------------------------------------------------------------------------------
# File        : Makefile
# Description : Build and run the fft16 golden-model unit tests.
# Author      : Gyusup LEE <gyu2910@waric.co.kr>
# Created     : 2026-08-13
# Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
#------------------------------------------------------------------------------

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11 -O2
LDFLAGS  := -lm

TEST_BIN := test_fft16
SRCS     := fft16.c test_fft16.c
HDRS     := fft16.h

.PHONY: all test clean

all: $(TEST_BIN)

$(TEST_BIN): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(TEST_BIN)
