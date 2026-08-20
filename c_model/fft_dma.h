//------------------------------------------------------------------------------
// File        : fft_dma.h
// Description : DMA-driven batched 16-point FFT reference model
// Author      : Gyusup LEE <gyu2910@waric.co.kr>
// Created     : 2026-08-20
// Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
//------------------------------------------------------------------------------

#ifndef FFT_DMA_H_
#define FFT_DMA_H_

#include <stddef.h>
#include <stdint.h>

#include "fft16.h"

typedef enum {
    MODE_FFT        = 0,
    MODE_READ_ONLY  = 1,
    MODE_WRITE_ONLY = 2,
} fft_dma_mode_t;

typedef struct {
    uint8_t *bytes;
    size_t   size_bytes;
} memory_t;

#define SAMPLE_BYTES     4
#define FFT_CHUNK_BYTES  (FFT_POINTS * SAMPLE_BYTES)

// Little-endian pack / unpack of a Q2.14 complex sample to / from `mem`.
void      write_sample(memory_t *mem, uint64_t addr, cplx_fx_t s);
cplx_fx_t read_sample(const memory_t *mem, uint64_t addr);

// Top-level DMA reference model. Byte-addressed memory access; alignment
// requirements per the RTL spec (Section 5.4) are the caller's responsibility.
void fft_dma(memory_t       *mem,
             uint64_t        src_addr,
             uint64_t        dst_addr,
             uint32_t        num_ffts,
             fft_dma_mode_t  mode);

#endif  // FFT_DMA_H_
