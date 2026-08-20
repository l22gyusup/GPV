//------------------------------------------------------------------------------
// File        : fft_dma.c
// Description : DMA-driven batched 16-point FFT reference model
// Author      : Gyusup LEE <gyu2910@waric.co.kr>
// Created     : 2026-08-20
// Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
//------------------------------------------------------------------------------

#include "fft_dma.h"

cplx_fx_t read_sample(const memory_t *mem, uint64_t addr) {
    uint16_t re_u = (uint16_t)mem->bytes[addr + 0]
                  | ((uint16_t)mem->bytes[addr + 1] << 8);
    uint16_t im_u = (uint16_t)mem->bytes[addr + 2]
                  | ((uint16_t)mem->bytes[addr + 3] << 8);
    cplx_fx_t s;
    s.re = (int16_t)re_u;
    s.im = (int16_t)im_u;
    return s;
}

void write_sample(memory_t *mem, uint64_t addr, cplx_fx_t s) {
    uint16_t re_u = (uint16_t)s.re;
    uint16_t im_u = (uint16_t)s.im;
    mem->bytes[addr + 0] = (uint8_t)(re_u & 0xFF);
    mem->bytes[addr + 1] = (uint8_t)((re_u >> 8) & 0xFF);
    mem->bytes[addr + 2] = (uint8_t)(im_u & 0xFF);
    mem->bytes[addr + 3] = (uint8_t)((im_u >> 8) & 0xFF);
}

static void write_counter(memory_t *mem, uint64_t addr, uint32_t n) {
    mem->bytes[addr + 0] = (uint8_t)(n & 0xFF);
    mem->bytes[addr + 1] = (uint8_t)((n >> 8) & 0xFF);
    mem->bytes[addr + 2] = (uint8_t)((n >> 16) & 0xFF);
    mem->bytes[addr + 3] = (uint8_t)((n >> 24) & 0xFF);
}

void fft_dma(memory_t       *mem,
             uint64_t        src_addr,
             uint64_t        dst_addr,
             uint32_t        num_ffts,
             fft_dma_mode_t  mode) {
    if (mode == MODE_READ_ONLY) {
        return;
    }

    if (mode == MODE_WRITE_ONLY) {
        size_t total_samples = (size_t)num_ffts * FFT_POINTS;
        for (size_t n = 0; n < total_samples; n++) {
            write_counter(mem, dst_addr + (uint64_t)n * SAMPLE_BYTES,
                          (uint32_t)n);
        }
        return;
    }

    for (uint32_t k = 0; k < num_ffts; k++) {
        cplx_fx_t in[FFT_POINTS];
        cplx_fx_t out[FFT_POINTS];
        uint64_t src_base = src_addr + (uint64_t)k * FFT_CHUNK_BYTES;
        uint64_t dst_base = dst_addr + (uint64_t)k * FFT_CHUNK_BYTES;

        for (int n = 0; n < FFT_POINTS; n++) {
            in[n] = read_sample(mem, src_base + (uint64_t)n * SAMPLE_BYTES);
        }
        fft16_forward_fx(in, out);
        for (int n = 0; n < FFT_POINTS; n++) {
            write_sample(mem, dst_base + (uint64_t)n * SAMPLE_BYTES, out[n]);
        }
    }
}
