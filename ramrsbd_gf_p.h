/*
 * Utilities for polynomials built out of Galois-field elements
 *
 * Copyright (c) 2024, The littlefs authors.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef RAMRSBD_GF_P_H
#define RAMRSBD_GF_P_H

#include "lfs.h"
#include "lfs_util.h"
#include "ramrsbd_gf.h"

#ifdef __cplusplus
extern "C"
{
#endif


// Evaluate a polynomial at x
uint8_t ramrsbd_gf_p_eval(
        const uint8_t *p, lfs_size_t p_size,
        uint8_t x);

// Multiply a polynomial by a constant c
void ramrsbd_gf_p_scale(
        uint8_t *p, lfs_size_t p_size,
        uint8_t c);

// Xor two polynomials together, this is equivalent to both addition and
// subtraction in a Galois-field
void ramrsbd_gf_p_xor(
        uint8_t *a, lfs_size_t a_size,
        const uint8_t *b, lfs_size_t b_size);

// Xor two polynomials together after scaling b by a constant c
void ramrsbd_gf_p_xors(
        uint8_t *a, lfs_size_t a_size,
        uint8_t c,
        const uint8_t *b, lfs_size_t b_size);

// Multiply two polynomials together
void ramrsbd_gf_p_mul(
        uint8_t *a, lfs_size_t a_size,
        const uint8_t *b, lfs_size_t b_size);

// Find both the quotient and remainder after division
void ramrsbd_gf_p_divmod(
        uint8_t *a, lfs_size_t a_size,
        const uint8_t *b, lfs_size_t b_size);

// Find both the quotient and remainder after division, assuming b has
// an implicit leading 1
void ramrsbd_gf_p_divmod1(
        uint8_t *a, lfs_size_t a_size,
        const uint8_t *b, lfs_size_t b_size);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

