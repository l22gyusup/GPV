#------------------------------------------------------------------------------
# File        : Makefile
# Description : Build, generate vectors, and run the fft16 / fft_dma tests.
# Author      : Gyusup LEE <gyu2910@waric.co.kr>
# Created     : 2026-08-13
# Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
#------------------------------------------------------------------------------

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11 -O2
LDFLAGS  := -lm
PYTHON   := python3

FFT16_BIN    := test_fft16
FFT16_SRCS   := fft16.c vector_io.c test_fft16.c
FFT16_HDRS   := fft16.h vector_io.h
FFT16_GEN    := vectors/gen_vectors.py
FFT16_VEC    := vectors/fft16_vectors.csv

DMA_BIN      := test_fft_dma
DMA_SRCS     := fft16.c fft_dma.c test_fft_dma.c
DMA_HDRS     := fft16.h fft_dma.h
DMA_GEN      := vectors/gen_dma_vectors.py
DMA_VEC      := vectors/fft_dma_vectors.csv

.PHONY: all gen test clean

all: $(FFT16_BIN) $(DMA_BIN)

$(FFT16_BIN): $(FFT16_SRCS) $(FFT16_HDRS)
	$(CC) $(CFLAGS) $(FFT16_SRCS) $(LDFLAGS) -o $@

$(DMA_BIN): $(DMA_SRCS) $(DMA_HDRS)
	$(CC) $(CFLAGS) $(DMA_SRCS) $(LDFLAGS) -o $@

$(FFT16_VEC): $(FFT16_GEN)
	$(PYTHON) $(FFT16_GEN)

$(DMA_VEC): $(DMA_GEN)
	$(PYTHON) $(DMA_GEN)

gen: $(FFT16_VEC) $(DMA_VEC)

test: $(FFT16_BIN) $(DMA_BIN) $(FFT16_VEC) $(DMA_VEC)
	./$(FFT16_BIN) $(FFT16_VEC)
	@echo
	./$(DMA_BIN) $(DMA_VEC)

clean:
	rm -f $(FFT16_BIN) $(DMA_BIN)
