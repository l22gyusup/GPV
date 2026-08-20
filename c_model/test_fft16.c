//------------------------------------------------------------------------------
// File        : test_fft16.c
// Description : CSV-driven tests for fft16 (float + Q2.14 fixed-point)
// Author      : Gyusup LEE <gyu2910@waric.co.kr>
// Created     : 2026-08-13
// Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
//------------------------------------------------------------------------------

#include "fft16.h"
#include "vector_io.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_VECTOR_PATH  "vectors/fft16_vectors.csv"

#define FLOAT_TOL  1e-3f
#define FIXED_TOL  96

static int16_t float_to_q214(double v) {
    double scaled  = v * (double)Q214_SCALE;
    double rounded = scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5;
    long   r       = (long)rounded;
    if (r >  32767) return  32767;
    if (r < -32768) return -32768;
    return (int16_t)r;
}

static int check_float_case(const fft16_vector_t *v,
                            const cplx_t          got[FFT_POINTS],
                            float                 tol) {
    int fail_bins = 0;
    for (int k = 0; k < FFT_POINTS; k++) {
        float dre = fabsf(got[k].re - v->expected[k].re);
        float dim = fabsf(got[k].im - v->expected[k].im);
        if (dre > tol || dim > tol) {
            printf("  [%s] bin %2d: got (% .6f, % .6f), expected (% .6f, % .6f), "
                   "diff (%.2e, %.2e)\n",
                   v->name, k, got[k].re, got[k].im,
                   v->expected[k].re, v->expected[k].im, dre, dim);
            fail_bins++;
        }
    }
    return fail_bins;
}

// For the fixed path, the DUT applies per-stage right-shift, so its output
// scale is 1 / FFT_POINTS relative to the unscaled DFT. Compare against the
// float expected value scaled by (Q214_SCALE / FFT_POINTS).
static int check_fixed_case(const fft16_vector_t *v,
                            const cplx_fx_t       got[FFT_POINTS],
                            int                   tol) {
    int fail_bins = 0;
    for (int k = 0; k < FFT_POINTS; k++) {
        int16_t exp_re = float_to_q214(v->expected[k].re / (double)FFT_POINTS);
        int16_t exp_im = float_to_q214(v->expected[k].im / (double)FFT_POINTS);
        int     dre    = abs((int)got[k].re - (int)exp_re);
        int     dim    = abs((int)got[k].im - (int)exp_im);
        if (dre > tol || dim > tol) {
            printf("  [%s] bin %2d: got (%6d, %6d), expected (%6d, %6d), "
                   "diff (%d, %d)\n",
                   v->name, k, got[k].re, got[k].im, exp_re, exp_im, dre, dim);
            fail_bins++;
        }
    }
    return fail_bins;
}

static void report(const char *domain, const char *name, int fail_bins) {
    if (fail_bins == 0) {
        return;
    }
    printf("  FAIL: %s / %s (%d bins mismatched)\n", domain, name, fail_bins);
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : DEFAULT_VECTOR_PATH;

    fft16_vector_t *vecs = NULL;
    int             ncases = load_fft16_vectors(path, &vecs);
    if (ncases < 0) {
        fprintf(stderr, "Failed to load vectors from '%s'\n", path);
        return 2;
    }

    printf("Loaded %d test case(s) from %s\n", ncases, path);
    printf("Running float and Q2.14 fixed-point FFT against golden vectors...\n");

    int failed_float = 0;
    int failed_fixed = 0;

    for (int i = 0; i < ncases; i++) {
        cplx_t out_f[FFT_POINTS];
        fft16_forward(vecs[i].in, out_f);
        int bad = check_float_case(&vecs[i], out_f, FLOAT_TOL);
        if (bad) {
            failed_float++;
            report("float", vecs[i].name, bad);
        }

        cplx_fx_t in_fx[FFT_POINTS];
        cplx_fx_t out_fx[FFT_POINTS];
        for (int n = 0; n < FFT_POINTS; n++) {
            in_fx[n].re = float_to_q214(vecs[i].in[n].re);
            in_fx[n].im = float_to_q214(vecs[i].in[n].im);
        }
        fft16_forward_fx(in_fx, out_fx);
        bad = check_fixed_case(&vecs[i], out_fx, FIXED_TOL);
        if (bad) {
            failed_fixed++;
            report("fixed", vecs[i].name, bad);
        }
    }

    free(vecs);

    printf("---------------------------------\n");
    printf("Float : %d / %d PASS\n", ncases - failed_float, ncases);
    printf("Fixed : %d / %d PASS\n", ncases - failed_fixed, ncases);

    return (failed_float == 0 && failed_fixed == 0) ? 0 : 1;
}
