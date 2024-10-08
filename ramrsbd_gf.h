/*
 * Galois field utilities
 *
 * Copyright (c) 2024, The littlefs authors.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef RAMRSBD_GF_H
#define RAMRSBD_GF_H

#include "lfs_util.h"

#ifdef __cplusplus
extern "C"
{
#endif


// The irreducible polynomial that defines the field
#define RAMRSBD_GF_P 0x11d

// A generator in the field
#define RAMRSBD_GF_G 0x02


// Note addition/subtraction is just xor, we don't really need a special
// function for it

// Multiplication in the field
uint8_t ramrsbd_gf_mul(uint8_t a, uint8_t b);

// Division in the field
uint8_t ramrsbd_gf_div(uint8_t a, uint8_t b);

// Exponentiation in the field
uint8_t ramrsbd_gf_pow(uint8_t a, uint32_t e);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

