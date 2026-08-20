//------------------------------------------------------------------------------
// File        : vector_io.c
// Description : CSV test-vector loader for the fft16 reference model.
// Author      : Gyusup LEE <gyu2910@waric.co.kr>
// Created     : 2026-08-20
// Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
//------------------------------------------------------------------------------

#include "vector_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_BUF_MAX  512
#define INITIAL_CAP   16

static int find_or_append_case(fft16_vector_t **arr,
                               size_t          *count,
                               size_t          *cap,
                               int              id,
                               const char      *name) {
    for (size_t i = 0; i < *count; i++) {
        if ((*arr)[i].id == id) {
            return (int)i;
        }
    }

    if (*count == *cap) {
        size_t          new_cap = (*cap == 0) ? INITIAL_CAP : (*cap * 2);
        fft16_vector_t *tmp     = realloc(*arr, new_cap * sizeof(**arr));
        if (tmp == NULL) {
            return -1;
        }
        *arr = tmp;
        *cap = new_cap;
    }

    fft16_vector_t *slot = &(*arr)[*count];
    memset(slot, 0, sizeof(*slot));
    slot->id = id;
    snprintf(slot->name, VECTOR_NAME_MAX, "%s", name);
    (*count)++;
    return (int)(*count - 1);
}

int load_fft16_vectors(const char *path, fft16_vector_t **out_vecs) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "load_fft16_vectors: cannot open '%s'\n", path);
        return -1;
    }

    fft16_vector_t *arr   = NULL;
    size_t          count = 0;
    size_t          cap   = 0;

    char line[LINE_BUF_MAX];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fprintf(stderr, "load_fft16_vectors: empty file\n");
        fclose(fp);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        int   id, sample_idx;
        char  name[VECTOR_NAME_MAX];
        float in_re, in_im, out_re, out_im;

        int scanned = sscanf(line, "%d,%63[^,],%d,%f,%f,%f,%f",
                             &id, name, &sample_idx,
                             &in_re, &in_im, &out_re, &out_im);
        if (scanned != 7) {
            fprintf(stderr, "load_fft16_vectors: malformed row: %s", line);
            free(arr);
            fclose(fp);
            return -1;
        }

        if (sample_idx < 0 || sample_idx >= FFT_POINTS) {
            fprintf(stderr, "load_fft16_vectors: sample_idx %d out of range\n",
                    sample_idx);
            free(arr);
            fclose(fp);
            return -1;
        }

        int slot = find_or_append_case(&arr, &count, &cap, id, name);
        if (slot < 0) {
            fprintf(stderr, "load_fft16_vectors: allocation failure\n");
            free(arr);
            fclose(fp);
            return -1;
        }

        arr[slot].in[sample_idx].re       = in_re;
        arr[slot].in[sample_idx].im       = in_im;
        arr[slot].expected[sample_idx].re = out_re;
        arr[slot].expected[sample_idx].im = out_im;
    }

    fclose(fp);
    *out_vecs = arr;
    return (int)count;
}
