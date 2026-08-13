//------------------------------------------------------------------------------
// File        : fft16.c
// Description : 16-point radix-2 DIT FFT implementation (float + Q2.14)
// Author      : Gyusup LEE <gyu2910@waric.co.kr>
// Created     : 2026-08-13
// Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
//------------------------------------------------------------------------------

#include "fft16.h"

// Twiddle factors W16^k = exp(-j * 2*pi * k / 16), k = 0..7 (floating-point).
static const cplx_t W16[8] = {
    { 1.00000000f,  0.00000000f},
    { 0.92387953f, -0.38268343f},
    { 0.70710678f, -0.70710678f},
    { 0.38268343f, -0.92387953f},
    { 0.00000000f, -1.00000000f},
    {-0.38268343f, -0.92387953f},
    {-0.70710678f, -0.70710678f},
    {-0.92387953f, -0.38268343f},
};

// Same twiddles in Q1.15 (raw / 32768 = value). +1.0 is approximated as 32767
// (0x7FFF) to keep the representable range symmetric.
static const cplx_fx_t W16_FX[8] = {
    { 32767,       0},
    { 30274,  -12539},
    { 23170,  -23170},
    { 12539,  -30274},
    {     0,  -32767},
    {-12539,  -30274},
    {-23170,  -23170},
    {-30274,  -12539},
};

// Bit-reversal permutation table for 4-bit indices (N=16).
static const uint8_t BIT_REV_4[FFT_POINTS] = {
    0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
};

void fft16_forward(const cplx_t in[FFT_POINTS], cplx_t out[FFT_POINTS]) {
    for (int i = 0; i < FFT_POINTS; i++) {
        out[BIT_REV_4[i]] = in[i];
    }

    for (int s = 1; s <= 4; s++) {
        int m            = 1 << s;
        int half_m       = m >> 1;
        int twiddle_step = FFT_POINTS / m;

        for (int k = 0; k < FFT_POINTS; k += m) {
            for (int j = 0; j < half_m; j++) {
                int idx_a = k + j;
                int idx_b = k + j + half_m;
                cplx_t w  = W16[j * twiddle_step];
                cplx_t a  = out[idx_a];
                cplx_t b  = out[idx_b];

                cplx_t wb = {
                    .re = w.re * b.re - w.im * b.im,
                    .im = w.re * b.im + w.im * b.re,
                };

                out[idx_a].re = a.re + wb.re;
                out[idx_a].im = a.im + wb.im;
                out[idx_b].re = a.re - wb.re;
                out[idx_b].im = a.im - wb.im;
            }
        }
    }
}

// Assumes arithmetic right shift on signed integers (guaranteed on gcc/clang;
// implementation-defined by C standard).
void fft16_forward_fx(const cplx_fx_t in[FFT_POINTS], cplx_fx_t out[FFT_POINTS]) {
    for (int i = 0; i < FFT_POINTS; i++) {
        out[BIT_REV_4[i]] = in[i];
    }

    for (int s = 1; s <= 4; s++) {
        int m            = 1 << s;
        int half_m       = m >> 1;
        int twiddle_step = FFT_POINTS / m;

        for (int k = 0; k < FFT_POINTS; k += m) {
            for (int j = 0; j < half_m; j++) {
                int idx_a    = k + j;
                int idx_b    = k + j + half_m;
                cplx_fx_t w  = W16_FX[j * twiddle_step];
                cplx_fx_t a  = out[idx_a];
                cplx_fx_t b  = out[idx_b];

                // W (Q1.15) * B (Q2.14) = Q3.29 in int32; >> 15 returns to Q3.14.
                int32_t wb_re = ((int32_t)w.re * b.re - (int32_t)w.im * b.im) >> 15;
                int32_t wb_im = ((int32_t)w.re * b.im + (int32_t)w.im * b.re) >> 15;

                // Sum/diff in int32 to avoid overflow, then >> 1 for block scaling.
                int32_t sum_re  = (int32_t)a.re + wb_re;
                int32_t sum_im  = (int32_t)a.im + wb_im;
                int32_t diff_re = (int32_t)a.re - wb_re;
                int32_t diff_im = (int32_t)a.im - wb_im;

                out[idx_a].re = (int16_t)(sum_re  >> 1);
                out[idx_a].im = (int16_t)(sum_im  >> 1);
                out[idx_b].re = (int16_t)(diff_re >> 1);
                out[idx_b].im = (int16_t)(diff_im >> 1);
            }
        }
    }
}
