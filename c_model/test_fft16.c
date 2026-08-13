//------------------------------------------------------------------------------
// File        : test_fft16.c
// Description : Unit tests for fft16 (float + Q2.14 fixed-point)
// Author      : Gyusup LEE <gyu2910@waric.co.kr>
// Created     : 2026-08-13
// Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
//------------------------------------------------------------------------------

#include "fft16.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FLOAT_TOL   1e-4f
#define FIXED_TOL   64

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

static int16_t float_to_q214(double v) {
    double scaled  = v * (double)Q214_SCALE;
    double rounded = scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5;
    long   r       = (long)rounded;
    if (r >  32767) return  32767;
    if (r < -32768) return -32768;
    return (int16_t)r;
}

static int check_float(const char *name,
                       const cplx_t got[FFT_POINTS],
                       const cplx_t expected[FFT_POINTS],
                       float tol) {
    int fail_bins = 0;
    for (int k = 0; k < FFT_POINTS; k++) {
        float dre = fabsf(got[k].re - expected[k].re);
        float dim = fabsf(got[k].im - expected[k].im);
        if (dre > tol || dim > tol) {
            printf("  [%s] bin %2d: got (% .6f, % .6f), expected (% .6f, % .6f), "
                   "diff (%.2e, %.2e)\n",
                   name, k, got[k].re, got[k].im,
                   expected[k].re, expected[k].im, dre, dim);
            fail_bins++;
        }
    }
    return fail_bins;
}

static int check_fixed(const char *name,
                       const cplx_fx_t got[FFT_POINTS],
                       const cplx_fx_t expected[FFT_POINTS],
                       int tol) {
    int fail_bins = 0;
    for (int k = 0; k < FFT_POINTS; k++) {
        int dre = abs((int)got[k].re - (int)expected[k].re);
        int dim = abs((int)got[k].im - (int)expected[k].im);
        if (dre > tol || dim > tol) {
            printf("  [%s] bin %2d: got (%6d, %6d), expected (%6d, %6d), "
                   "diff (%d, %d)\n",
                   name, k, got[k].re, got[k].im,
                   expected[k].re, expected[k].im, dre, dim);
            fail_bins++;
        }
    }
    return fail_bins;
}

static void report(const char *name, int fail_bins) {
    printf("  test_%s: %s\n", name, fail_bins == 0 ? "PASS" : "FAIL");
}

//------------------------------------------------------------------------------
// Float tests
//------------------------------------------------------------------------------

static int test_impulse_float(void) {
    cplx_t in[FFT_POINTS]       = {0};
    cplx_t out[FFT_POINTS]      = {0};
    cplx_t expected[FFT_POINTS] = {0};

    in[0].re = 1.0f;
    for (int k = 0; k < FFT_POINTS; k++) {
        expected[k].re = 1.0f;
        expected[k].im = 0.0f;
    }

    fft16_forward(in, out);
    int fails = check_float("impulse_float", out, expected, FLOAT_TOL);
    report("impulse_float", fails);
    return fails == 0 ? 0 : 1;
}

static int test_dc_float(void) {
    cplx_t in[FFT_POINTS]       = {0};
    cplx_t out[FFT_POINTS]      = {0};
    cplx_t expected[FFT_POINTS] = {0};

    for (int n = 0; n < FFT_POINTS; n++) {
        in[n].re = 1.0f;
    }
    expected[0].re = (float)FFT_POINTS;

    fft16_forward(in, out);
    int fails = check_float("dc_float", out, expected, FLOAT_TOL);
    report("dc_float", fails);
    return fails == 0 ? 0 : 1;
}

static int test_tone_float(int bin) {
    cplx_t in[FFT_POINTS]       = {0};
    cplx_t out[FFT_POINTS]      = {0};
    cplx_t expected[FFT_POINTS] = {0};

    for (int n = 0; n < FFT_POINTS; n++) {
        double angle = 2.0 * M_PI * (double)bin * (double)n / (double)FFT_POINTS;
        in[n].re = (float)cos(angle);
        in[n].im = (float)sin(angle);
    }
    expected[bin].re = (float)FFT_POINTS;

    fft16_forward(in, out);
    char name[32];
    snprintf(name, sizeof(name), "tone_float_bin%d", bin);
    int fails = check_float(name, out, expected, FLOAT_TOL);
    report(name, fails);
    return fails == 0 ? 0 : 1;
}

//------------------------------------------------------------------------------
// Fixed-point tests (Q2.14, per-stage scaled; expected output scale = 1 / N)
//------------------------------------------------------------------------------

static int test_impulse_fixed(void) {
    cplx_fx_t in[FFT_POINTS]       = {0};
    cplx_fx_t out[FFT_POINTS]      = {0};
    cplx_fx_t expected[FFT_POINTS] = {0};

    in[0].re = (int16_t)Q214_SCALE;  // 1.0 in Q2.14
    for (int k = 0; k < FFT_POINTS; k++) {
        expected[k].re = (int16_t)(Q214_SCALE / FFT_POINTS);  // 1/16 in Q2.14
        expected[k].im = 0;
    }

    fft16_forward_fx(in, out);
    int fails = check_fixed("impulse_fixed", out, expected, FIXED_TOL);
    report("impulse_fixed", fails);
    return fails == 0 ? 0 : 1;
}

static int test_dc_fixed(void) {
    cplx_fx_t in[FFT_POINTS]       = {0};
    cplx_fx_t out[FFT_POINTS]      = {0};
    cplx_fx_t expected[FFT_POINTS] = {0};

    for (int n = 0; n < FFT_POINTS; n++) {
        in[n].re = (int16_t)Q214_SCALE;
    }
    expected[0].re = (int16_t)Q214_SCALE;  // (N * 1) / N = 1 → raw 16384

    fft16_forward_fx(in, out);
    int fails = check_fixed("dc_fixed", out, expected, FIXED_TOL);
    report("dc_fixed", fails);
    return fails == 0 ? 0 : 1;
}

static int test_tone_fixed(int bin) {
    cplx_fx_t in[FFT_POINTS]       = {0};
    cplx_fx_t out[FFT_POINTS]      = {0};
    cplx_fx_t expected[FFT_POINTS] = {0};

    for (int n = 0; n < FFT_POINTS; n++) {
        double angle = 2.0 * M_PI * (double)bin * (double)n / (double)FFT_POINTS;
        in[n].re = float_to_q214(cos(angle));
        in[n].im = float_to_q214(sin(angle));
    }
    expected[bin].re = (int16_t)Q214_SCALE;  // (N * 1) / N = 1 → raw 16384

    fft16_forward_fx(in, out);
    char name[32];
    snprintf(name, sizeof(name), "tone_fixed_bin%d", bin);
    int fails = check_fixed(name, out, expected, FIXED_TOL);
    report(name, fails);
    return fails == 0 ? 0 : 1;
}

//------------------------------------------------------------------------------
// Entry
//------------------------------------------------------------------------------

int main(void) {
    int failed_tests = 0;

    printf("--- Float FFT tests ---\n");
    failed_tests += test_impulse_float();
    failed_tests += test_dc_float();
    failed_tests += test_tone_float(1);
    failed_tests += test_tone_float(8);

    printf("--- Q2.14 fixed-point FFT tests ---\n");
    failed_tests += test_impulse_fixed();
    failed_tests += test_dc_fixed();
    failed_tests += test_tone_fixed(1);
    failed_tests += test_tone_fixed(8);

    printf("---------------------------------\n");
    if (failed_tests == 0) {
        printf("All tests PASSED.\n");
        return 0;
    }
    printf("%d test(s) FAILED.\n", failed_tests);
    return 1;
}
