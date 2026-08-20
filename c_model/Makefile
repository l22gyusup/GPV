#------------------------------------------------------------------------------
# File        : Makefile
# Description : Build, generate vectors, and run the fft16 golden-model tests.
# Author      : Gyusup LEE <gyu2910@waric.co.kr>
# Created     : 2026-08-13
# Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
#------------------------------------------------------------------------------

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11 -O2
LDFLAGS  := -lm
PYTHON   := python3

TEST_BIN     := test_fft16
SRCS         := fft16.c vector_io.c test_fft16.c
HDRS         := fft16.h vector_io.h
VECTOR_GEN   := vectors/gen_vectors.py
VECTOR_FILE  := vectors/fft16_vectors.csv

.PHONY: all gen test clean

all: $(TEST_BIN)

$(TEST_BIN): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $@

gen $(VECTOR_FILE): $(VECTOR_GEN)
	$(PYTHON) $(VECTOR_GEN)

test: $(TEST_BIN) $(VECTOR_FILE)
	./$(TEST_BIN) $(VECTOR_FILE)

clean:
	rm -f $(TEST_BIN)
