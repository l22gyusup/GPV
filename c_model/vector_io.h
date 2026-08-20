//------------------------------------------------------------------------------
// File        : vector_io.h
// Description : CSV test-vector loader for the fft16 reference model.
// Author      : Gyusup LEE <gyu2910@waric.co.kr>
// Created     : 2026-08-20
// Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
//------------------------------------------------------------------------------

#ifndef VECTOR_IO_H_
#define VECTOR_IO_H_

#include <stddef.h>

#include "fft16.h"

#define VECTOR_NAME_MAX  64

typedef struct {
    int    id;
    char   name[VECTOR_NAME_MAX];
    cplx_t in[FFT_POINTS];
    cplx_t expected[FFT_POINTS];
} fft16_vector_t;

// Loads a CSV of the form produced by gen_vectors.py. Allocates *out_vecs
// (caller must free). Returns the number of test cases on success, or a
// negative value on error.
int load_fft16_vectors(const char *path, fft16_vector_t **out_vecs);

#endif  // VECTOR_IO_H_
