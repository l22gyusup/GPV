//------------------------------------------------------------------------------
// File        : test_fft_dma.c
// Description : CSV-driven tests for the fft_dma reference model
// Author      : Gyusup LEE <gyu2910@waric.co.kr>
// Created     : 2026-08-20
// Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
//------------------------------------------------------------------------------

#include "fft16.h"
#include "fft_dma.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_VECTOR_PATH  "vectors/fft_dma_vectors.csv"
#define MEMORY_BYTES         (64 * 1024)
#define NAME_MAX_LEN         64
#define LINE_MAX_LEN         512
#define FIXED_TOL            96
#define INITIAL_CAP          16

typedef struct {
    int             id;
    char            name[NAME_MAX_LEN];
    fft_dma_mode_t  mode;
    uint32_t        num_ffts;
    uint64_t        src_addr;
    uint64_t        dst_addr;
    cplx_t         *in;
    cplx_t         *expected;
    size_t          n_samples;
} dma_case_t;

static int16_t float_to_q214(double v) {
    double scaled  = v * (double)Q214_SCALE;
    double rounded = scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5;
    long   r       = (long)rounded;
    if (r >  32767) return  32767;
    if (r < -32768) return -32768;
    return (int16_t)r;
}

static int find_or_append_case(dma_case_t **arr,
                               size_t      *count,
                               size_t      *cap,
                               int          id) {
    for (size_t i = 0; i < *count; i++) {
        if ((*arr)[i].id == id) {
            return (int)i;
        }
    }
    if (*count == *cap) {
        size_t new_cap = (*cap == 0) ? INITIAL_CAP : (*cap * 2);
        dma_case_t *tmp = realloc(*arr, new_cap * sizeof(**arr));
        if (tmp == NULL) return -1;
        *arr = tmp;
        *cap = new_cap;
    }
    dma_case_t *slot = &(*arr)[*count];
    memset(slot, 0, sizeof(*slot));
    slot->id = id;
    (*count)++;
    return (int)(*count - 1);
}

static int load_cases(const char *path, dma_case_t **out_cases) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "load_cases: cannot open '%s'\n", path);
        return -1;
    }

    dma_case_t *arr   = NULL;
    size_t      count = 0;
    size_t      cap   = 0;

    char line[LINE_MAX_LEN];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        int      id, mode, sample_idx;
        char     name[NAME_MAX_LEN];
        uint32_t num_ffts;
        uint64_t src_addr, dst_addr;
        float    in_re, in_im, exp_re, exp_im;

        int n = sscanf(line,
                       "%d,%63[^,],%d,%" SCNu32 ",%" SCNu64 ",%" SCNu64
                       ",%d,%f,%f,%f,%f",
                       &id, name, &mode, &num_ffts,
                       &src_addr, &dst_addr, &sample_idx,
                       &in_re, &in_im, &exp_re, &exp_im);
        if (n != 11) {
            fprintf(stderr, "load_cases: malformed row: %s", line);
            free(arr);
            fclose(fp);
            return -1;
        }

        int slot = find_or_append_case(&arr, &count, &cap, id);
        if (slot < 0) {
            fprintf(stderr, "load_cases: allocation failure\n");
            free(arr);
            fclose(fp);
            return -1;
        }
        dma_case_t *c = &arr[slot];
        if (c->n_samples == 0) {
            snprintf(c->name, NAME_MAX_LEN, "%s", name);
            c->mode      = (fft_dma_mode_t)mode;
            c->num_ffts  = num_ffts;
            c->src_addr  = src_addr;
            c->dst_addr  = dst_addr;
            c->n_samples = (size_t)FFT_POINTS * num_ffts;
            c->in       = calloc(c->n_samples, sizeof(cplx_t));
            c->expected = calloc(c->n_samples, sizeof(cplx_t));
            if (c->in == NULL || c->expected == NULL) {
                fprintf(stderr, "load_cases: alloc failure for case %d\n", id);
                free(arr);
                fclose(fp);
                return -1;
            }
        }

        if ((size_t)sample_idx >= c->n_samples) {
            fprintf(stderr,
                    "load_cases: sample_idx %d out of range for case %d\n",
                    sample_idx, id);
            free(arr);
            fclose(fp);
            return -1;
        }
        c->in[sample_idx].re       = in_re;
        c->in[sample_idx].im       = in_im;
        c->expected[sample_idx].re = exp_re;
        c->expected[sample_idx].im = exp_im;
    }

    fclose(fp);
    *out_cases = arr;
    return (int)count;
}

static void free_cases(dma_case_t *arr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(arr[i].in);
        free(arr[i].expected);
    }
    free(arr);
}

static int check_fft(const dma_case_t *c, const memory_t *mem) {
    int fails = 0;
    for (size_t n = 0; n < c->n_samples; n++) {
        cplx_fx_t got = read_sample(mem, c->dst_addr + n * SAMPLE_BYTES);
        int16_t exp_re = float_to_q214(c->expected[n].re / (double)FFT_POINTS);
        int16_t exp_im = float_to_q214(c->expected[n].im / (double)FFT_POINTS);
        int dre = abs((int)got.re - (int)exp_re);
        int dim = abs((int)got.im - (int)exp_im);
        if (dre > FIXED_TOL || dim > FIXED_TOL) {
            if (fails < 5) {
                printf("  [%s] sample %zu: got (%6d, %6d), "
                       "exp (%6d, %6d), diff (%d, %d)\n",
                       c->name, n, got.re, got.im, exp_re, exp_im, dre, dim);
            }
            fails++;
        }
    }
    return fails;
}

static int check_readonly(const dma_case_t *c, const memory_t *mem) {
    int fails = 0;
    for (size_t n = 0; n < c->n_samples; n++) {
        cplx_fx_t got = read_sample(mem, c->dst_addr + n * SAMPLE_BYTES);
        if (got.re != 0 || got.im != 0) {
            if (fails < 5) {
                printf("  [%s] sample %zu: got (%d, %d), expected untouched (0, 0)\n",
                       c->name, n, got.re, got.im);
            }
            fails++;
        }
    }
    return fails;
}

static int check_writeonly(const dma_case_t *c, const memory_t *mem) {
    int fails = 0;
    for (size_t n = 0; n < c->n_samples; n++) {
        cplx_fx_t got = read_sample(mem, c->dst_addr + n * SAMPLE_BYTES);
        int16_t exp_re = float_to_q214(c->expected[n].re);
        int16_t exp_im = float_to_q214(c->expected[n].im);
        if (got.re != exp_re || got.im != exp_im) {
            if (fails < 5) {
                printf("  [%s] sample %zu: got (%6d, %6d), exp (%6d, %6d)\n",
                       c->name, n, got.re, got.im, exp_re, exp_im);
            }
            fails++;
        }
    }
    return fails;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : DEFAULT_VECTOR_PATH;

    dma_case_t *cases  = NULL;
    int         ncases = load_cases(path, &cases);
    if (ncases < 0) {
        fprintf(stderr, "Failed to load vectors from '%s'\n", path);
        return 2;
    }

    memory_t mem;
    mem.bytes      = calloc(MEMORY_BYTES, 1);
    mem.size_bytes = MEMORY_BYTES;
    if (mem.bytes == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free_cases(cases, (size_t)ncases);
        return 2;
    }

    printf("Loaded %d test case(s) from %s\n", ncases, path);
    printf("Running fft_dma against golden vectors...\n");

    int failed   = 0;
    int pass_fft = 0, tot_fft = 0;
    int pass_ro  = 0, tot_ro  = 0;
    int pass_wo  = 0, tot_wo  = 0;

    for (int i = 0; i < ncases; i++) {
        dma_case_t *c = &cases[i];
        memset(mem.bytes, 0, mem.size_bytes);

        if (c->mode == MODE_FFT || c->mode == MODE_READ_ONLY) {
            for (size_t n = 0; n < c->n_samples; n++) {
                cplx_fx_t s;
                s.re = float_to_q214(c->in[n].re);
                s.im = float_to_q214(c->in[n].im);
                write_sample(&mem, c->src_addr + n * SAMPLE_BYTES, s);
            }
        }

        fft_dma(&mem, c->src_addr, c->dst_addr, c->num_ffts, c->mode);

        int bad = 0;
        switch (c->mode) {
            case MODE_FFT:
                bad = check_fft(c, &mem);
                tot_fft++;
                if (bad == 0) pass_fft++;
                break;
            case MODE_READ_ONLY:
                bad = check_readonly(c, &mem);
                tot_ro++;
                if (bad == 0) pass_ro++;
                break;
            case MODE_WRITE_ONLY:
                bad = check_writeonly(c, &mem);
                tot_wo++;
                if (bad == 0) pass_wo++;
                break;
        }

        if (bad != 0) {
            printf("  FAIL: %s (mode=%d, %d sample mismatch(es))\n",
                   c->name, (int)c->mode, bad);
            failed++;
        }
    }

    free(mem.bytes);
    free_cases(cases, (size_t)ncases);

    printf("---------------------------------\n");
    printf("MODE_FFT       : %d / %d PASS\n", pass_fft, tot_fft);
    printf("MODE_READ_ONLY : %d / %d PASS\n", pass_ro,  tot_ro);
    printf("MODE_WRITE_ONLY: %d / %d PASS\n", pass_wo,  tot_wo);

    return failed == 0 ? 0 : 1;
}
