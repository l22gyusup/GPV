//------------------------------------------------------------------------------
// File        : fft16.h
// Description : 16-point radix-2 DIT FFT golden reference (float + Q2.14)
// Author      : Gyusup LEE <gyu2910@waric.co.kr>
// Created     : 2026-08-13
// Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
//------------------------------------------------------------------------------

#ifndef FFT16_H_
#define FFT16_H_

#include <stdint.h>

#define FFT_POINTS  16
#define Q214_SCALE  16384

typedef struct {
    float re;
    float im;
} cplx_t;

// Q2.14 fixed-point: real_value = raw / Q214_SCALE. Range: [-2, 2 - 2^-14].
typedef struct {
    int16_t re;
    int16_t im;
} cplx_fx_t;

// Floating-point forward FFT.
// X[k] = sum_{n=0..15} x[n] * exp(-j * 2*pi * k * n / 16)
void fft16_forward(const cplx_t in[FFT_POINTS], cplx_t out[FFT_POINTS]);

// Q2.14 fixed-point forward FFT with per-stage right-shift by 1 (block scaling).
// Total output scale is 1 / FFT_POINTS relative to the unscaled DFT.
void fft16_forward_fx(const cplx_fx_t in[FFT_POINTS], cplx_fx_t out[FFT_POINTS]);

#endif  // FFT16_H_
