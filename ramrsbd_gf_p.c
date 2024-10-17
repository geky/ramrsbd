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
        a[(a_size-b_size)+i] ^= b[i];
    }
}

// Xor two polynomials together after scaling b by a constant c
void ramrsbd_gf_p_xors(
        uint8_t *a, lfs_size_t a_size,
        uint8_t c,
        const uint8_t *b, lfs_size_t b_size) {
    LFS_ASSERT(a_size >= b_size);

    // this just gets a little bit confusing since b may be smaller than a
    for (lfs_size_t i = 0; i < b_size; i++) {
        a[(a_size-b_size)+i] ^= ramrsbd_gf_mul(b[i], c);
    }
}

// Multiply two polynomials together
void ramrsbd_gf_p_mul(
        uint8_t *a, lfs_size_t a_size,
        const uint8_t *b, lfs_size_t b_size) {
    LFS_ASSERT(a_size >= b_size);

    // in place multiplication, which gets a bit confusing
    //
    // note we only write to i-(b_size-1)+j, and i-(b_size-1)+j is
    // always <= i
    //
    for (lfs_size_t i = 0; i < a_size; i++) {
        uint8_t x = a[i];
        a[i] = 0;

        for (lfs_size_t j = ((i < b_size) ? b_size-1-i : 0);
                j < b_size;
                j++) {
            a[i-(b_size-1)+j] ^= ramrsbd_gf_mul(x, b[j]);
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

// Find both the quotient and remainder after division, assuming b has
// an implicit leading 1
void ramrsbd_gf_p_divmod1(
        uint8_t *a, lfs_size_t a_size,
        const uint8_t *b, lfs_size_t b_size) {
    LFS_ASSERT(a_size >= b_size);

    // normally you would need to normalize b, but we know the leading
    // coefficient is 1

    // divide via synthetic division
    for (lfs_size_t i = 0; i < a_size-b_size; i++) {
        if (a[i] != 0) {
            for (lfs_size_t j = 0; j < b_size; j++) {
                a[i+j+1] ^= ramrsbd_gf_mul(a[i], b[j]);
            }
        }
    }
}
