/*
 * Utilities for polynomials built out of Galois-field elements
 *
 * Copyright (c) 2024, The littlefs authors.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ramrsbd_gf_p.h"


// Evaluate a polynomial at x
uint8_t ramrsbd_gf_p_eval(
        const uint8_t *p, lfs_size_t p_size,
        uint8_t x) {
    // evaluate using Horner's method
    uint8_t y = 0;
    for (lfs_size_t i = 0; i < p_size; i++) {
        y = ramrsbd_gf_mul(y, x) ^ p[i];
    }

    return y;
}

// Multiply a polynomial by a constant c
void ramrsbd_gf_p_scale(
        uint8_t *p, lfs_size_t p_size,
        uint8_t c) {
    for (lfs_size_t i = 0; i < p_size; i++) {
        p[i] = ramrsbd_gf_mul(p[i], c);
    }
}

// Xor two polynomials together, this is equivalent to both addition and
// subtraction in a Galois-field
void ramrsbd_gf_p_xor(
        uint8_t *a, lfs_size_t a_size,
        const uint8_t *b, lfs_size_t b_size) {
    LFS_ASSERT(a_size >= b_size);

    // this just gets a little bit confusing since b may be smaller than a
    for (lfs_size_t i = 0; i < b_size; i++) {
        a[i+(a_size-b_size)] ^= b[i];
    }
}

// Multiply two polynomials together
void ramrsbd_gf_p_mul(
        uint8_t *a, lfs_size_t a_size,
        const uint8_t *b, lfs_size_t b_size) {
    LFS_ASSERT(a_size >= b_size);

    // in place multiplication, which gets a bit confusing
    //
    // note we only write to b_size-1 + i-j, and b_size-1 + i-j
    // is always >= b_size-1 + 0-j
    //
    for (lfs_size_t i = 0; i < (a_size-b_size)+1; i++) {
        uint8_t x = a[b_size-1 + i-0];
        a[b_size-1 + i-0] = 0;

        for (lfs_size_t j = 0; j < b_size; j++) {
            a[b_size-1 + i-j] ^= ramrsbd_gf_mul(x, b[b_size-1 + 0-j]);
        }
    }
}

// Find both the quotient and remainder after division
void ramrsbd_gf_p_divmod(
        uint8_t *a, lfs_size_t a_size,
        const uint8_t *b, lfs_size_t b_size) {
    LFS_ASSERT(a_size >= b_size);

    // first find the leading coefficient to normalize b
    uint8_t c = b[0];

    // divide via synthetic division
    for (lfs_size_t i = 0; i < (a_size-b_size)+1; i++) {
        if (a[i] != 0) {
            // normalize
            a[i] = ramrsbd_gf_div(a[i], c);

            for (lfs_size_t j = 1; j < b_size; j++) {
                a[i+j] ^= ramrsbd_gf_mul(a[i], b[j]);
            }
        }
    }
}


