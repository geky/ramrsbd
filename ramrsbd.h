/*
 * An example of Reed-Solomon BCH error-correcting block device in RAM
 *
 * Copyright (c) 2024, The littlefs authors.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef RAMRSBD_H
#define RAMRSBD_H

#include "lfs.h"
#include "lfs_util.h"

#ifdef __cplusplus
extern "C"
{
#endif


// Block device specific tracing
#ifndef RAMRSBD_TRACE
#ifdef RAMRSBD_YES_TRACE
#define RAMRSBD_TRACE(...) LFS_TRACE(__VA_ARGS__)
#else
#define RAMRSBD_TRACE(...)
#endif
#endif

// rambd config
struct ramrsbd_config {
    // Size of a codeword in bytes.
    //
    // Limited to at most 255 bytes (non-zero elements in GF(256)).
    //
    // Note code_size = read_size and prog_size + ecc_size.
    lfs_size_t code_size;

    // Size of the error-correcting code in bytes.
    //
    // ramrsbd can reliably correct up to floor(ecc_size/2) byte errors.
    //
    // Note code_size = read_size and prog_size + ecc_size.
    lfs_size_t ecc_size;

    // Size of an erase operation in bytes.
    //
    // Must be a multiple of code_size.
    lfs_size_t erase_size;

    // Number of erase blocks on the device.
    lfs_size_t erase_count;

    // Number of byte errors to try to correct.
    //
    // There is a tradeoff here. Every byte error you try to correct is two
    // fewer byte errors you can detect reliably. That being said, recovering
    // from errors is usually more useful.
    //
    // By default, when zero, tries to correct as many errors as possible.
    // -1 disables error correction and errors on any errors.
    lfs_ssize_t error_correction;

    // Optional precomputed generator polynomial.
    //
    // See the rs-poly.py script to help generate this.
    //
    // By default p is computed as needed for the configured ecc_size.
    // Must be ecc_size.
    const uint8_t *p;

    // Optional statically allocated buffer for the block device.
    void *buffer;

    // Optional statically allocated codeword buffer.
    //
    // Must be code_size.
    void *code_buffer;

    // Optional statically allocated generator polynomial buffer.
    //
    // Not needed if a precomputed p is provided.
    //
    // Must be ecc_size.
    void *p_buffer;

    // Optional statically allocated syndrome buffer.
    //
    // Must be ecc_size.
    void *s_buffer;

    // Optional statically allocated error-locator polynomial buffer.
    //
    // Must be ecc_size.
    void *λ_buffer;

    // Optional statically allocated error-evaluator polynomial buffer.
    //
    // Must be ecc_size.
    void *ω_buffer;
};

// rambd state
typedef struct ramrsbd {
    uint8_t *buffer;
    const struct ramrsbd_config *cfg;

    // various buffers for internal math

    // codeword buffer C(x)
    uint8_t *c; // code_size
    // generator polynomial P(x), with implied leading 1
    uint8_t *p; // ecc_size
    // syndrome polynomial S(x)
    uint8_t *s; // ecc_size
    // error-locator polynomial Λ(x)
    uint8_t *λ; // ecc_size
    // error-evaluator polynomial Ω(x)
    uint8_t *ω; // ecc_size
} ramrsbd_t;


// Create a RAM block device
int ramrsbd_create(const struct lfs_config *cfg,
        const struct ramrsbd_config *bdcfg);

// Clean up memory associated with block device
int ramrsbd_destroy(const struct lfs_config *cfg);

// Read a block
int ramrsbd_read(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size);

// Program a block
//
// The block must have previously been erased.
int ramrsbd_prog(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size);

// Erase a block
//
// A block must be erased before being programmed. The
// state of an erased block is undefined.
int ramrsbd_erase(const struct lfs_config *cfg, lfs_block_t block);

// Sync the block device
int ramrsbd_sync(const struct lfs_config *cfg);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
