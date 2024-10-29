/*
 * An example of crc32 based error-correcting block device in RAM
 *
 * Copyright (c) 2024, The littlefs authors.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ramrsbd.h"

#include "ramrsbd_gf.h"
#include "ramrsbd_gf_p.h"

int ramrsbd_create(const struct lfs_config *cfg,
        const struct ramrsbd_config *bdcfg) {
    RAMRSBD_TRACE("ramrsbd_create(%p {.context=%p, "
                ".read=%p, .prog=%p, .erase=%p, .sync=%p}, "
                "%p {.code_size=%"PRIu32", "
                ".erase_size=%"PRIu32", .erase_count=%"PRIu32", "
                ".buffer=%p})",
            (void*)cfg, cfg->context,
            (void*)(uintptr_t)cfg->read, (void*)(uintptr_t)cfg->prog,
            (void*)(uintptr_t)cfg->erase, (void*)(uintptr_t)cfg->sync,
            (void*)bdcfg,
            bdcfg->code_size, bdcfg->erase_size,
            bdcfg->erase_count, bdcfg->buffer);
    ramrsbd_t *bd = cfg->context;
    bd->cfg = bdcfg;

    // The from code size to message size is a bit complicated, so let's make
    // sure things are configured correctly
    LFS_ASSERT(bd->cfg->erase_size % bd->cfg->code_size == 0);
    LFS_ASSERT(bd->cfg->ecc_size <= bd->cfg->code_size);
    LFS_ASSERT(cfg->read_size % (bd->cfg->code_size-bd->cfg->ecc_size) == 0);
    LFS_ASSERT(cfg->prog_size % (bd->cfg->code_size-bd->cfg->ecc_size) == 0);
    LFS_ASSERT(cfg->block_size
            % (bd->cfg->erase_size
                - ((bd->cfg->erase_size/bd->cfg->code_size)
                    * bd->cfg->ecc_size))
            == 0);

    // Make sure the requested error correction is possible
    LFS_ASSERT(bd->cfg->error_correction <= 0
            || (lfs_size_t)bd->cfg->error_correction <= bd->cfg->ecc_size/2);

    // allocate buffer?
    if (bd->cfg->buffer) {
        bd->buffer = bd->cfg->buffer;
    } else {
        bd->buffer = lfs_malloc(bd->cfg->erase_size * bd->cfg->erase_count);
        if (!bd->buffer) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // zero for reproducibility
    memset(bd->buffer, 0, bd->cfg->erase_size * bd->cfg->erase_count);

    // allocate codeword buffer?
    if (bd->cfg->code_buffer) {
        bd->c = (uint8_t*)bd->cfg->code_buffer;
    } else {
        bd->c = lfs_malloc(bd->cfg->code_size);
        if (!bd->c) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // allocate generator polynomial buffer?
    if (bd->cfg->p_buffer) {
        bd->p = (uint8_t*)bd->cfg->p_buffer;
    } else {
        bd->p = lfs_malloc(bd->cfg->ecc_size);
        if (!bd->p) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // allocate syndrome buffer?
    if (bd->cfg->s_buffer) {
        bd->s = (uint8_t*)bd->cfg->s_buffer;
    } else {
        bd->s = lfs_malloc(bd->cfg->ecc_size);
        if (!bd->s) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // allocate error-locator polynomial buffer?
    if (bd->cfg->λ_buffer) {
        bd->λ = (uint8_t*)bd->cfg->λ_buffer;
    } else {
        bd->λ = lfs_malloc(bd->cfg->ecc_size);
        if (!bd->s) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // allocate error-evaluator polynomial buffer?
    if (bd->cfg->ω_buffer) {
        bd->ω = (uint8_t*)bd->cfg->ω_buffer;
    } else {
        bd->ω = lfs_malloc(bd->cfg->ecc_size);
        if (!bd->s) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // calculate generator polynomial
    //
    // P(x) = prod_i^n-1 (x - g^i)
    //
    // the important property of P(x) is that it evaluates to 0
    // at every x=g^i for i < n
    //
    // normally P(x) needs n+1 terms, but the leading term is always 1,
    // so we can make it implicit
    //

    // let P(x) = 1
    memset(bd->p, 0, bd->cfg->ecc_size);
    bd->p[bd->cfg->ecc_size-1] = 1;

    for (lfs_size_t i = 0; i < bd->cfg->ecc_size; i++) {
        // let R(x) = x - g^i
        uint8_t r[2] = {1, ramrsbd_gf_pow(RAMRSBD_GF_G, i)};

        // let P(x) = P(x) * R(x)
        ramrsbd_gf_p_mul(
                bd->p, bd->cfg->ecc_size,
                r, 2);
    }

    RAMRSBD_TRACE("ramrsbd_create -> %d", 0);
    return 0;
}

int ramrsbd_destroy(const struct lfs_config *cfg) {
    RAMRSBD_TRACE("ramrsbd_destroy(%p)", (void*)cfg);
    // clean up memory
    ramrsbd_t *bd = cfg->context;
    if (!bd->cfg->buffer) {
        lfs_free(bd->buffer);
    }
    if (!bd->cfg->code_buffer) {
        lfs_free(bd->c);
    }
    if (!bd->cfg->p_buffer) {
        lfs_free(bd->p);
    }
    if (!bd->cfg->s_buffer) {
        lfs_free(bd->s);
    }
    if (!bd->cfg->λ_buffer) {
        lfs_free(bd->λ);
    }
    if (!bd->cfg->ω_buffer) {
        lfs_free(bd->ω);
    }
    RAMRSBD_TRACE("ramrsbd_destroy -> %d", 0);
    return 0;
}

// find the set of syndromes S for a codeword C(x)
//
// S_i = C(g^i)
//
// if our codeword contains no errors, these should all be zero,
// otherwise they tell us information about the errors,
// S_i = sum_j Y_j X_j^i where j is an error
//
// also returns true if zero for convenience
static bool ramrsbd_find_s(
        uint8_t *s, lfs_size_t s_size,
        const uint8_t *c, lfs_size_t c_size) {
    // calculate syndromes
    bool s_zero = true;
    for (lfs_size_t i = 0; i < s_size; i++) {
        // let S_i = C(g^i)
        s[i] = ramrsbd_gf_p_eval(
                c, c_size,
                ramrsbd_gf_pow(RAMRSBD_GF_G, s_size-1-i));

        // keep track of if we have any non-zero syndromes
        if (s[i] != 0) {
            s_zero = false;
        }
    }

    return s_zero;
}

// find the error-locator polynomial Λ(x), given a set of syndromes S,
// with C providing scratch space for interim math
//
// Λ(x) = prod_j (1 - X_j x) = 1 + sum_k=1^e Λ_k x^k
//
// where Λ(X_j^-1)=0 if j is an error and Λ(0)=1
//
// also returns the number of errors for convenience
static lfs_size_t ramrsbd_find_λ(
        uint8_t *λ, lfs_size_t λ_size,
        uint8_t *c, lfs_size_t c_size,
        const uint8_t *s, lfs_size_t s_size) {
    LFS_ASSERT(c_size == λ_size);
    LFS_ASSERT(s_size == λ_size);

    // iteratively find the error-locator using Berlekamp-Massey
    //
    // this treats Λ as an LFSR we need to solve in GF(256)
    //

    // guess an error-locator LFSR
    //
    // let e = 0    // guessed LFSR size/number of errors
    // let Λ(i) = 1 // current LFSR guess
    // let C(i) = 1 // best LFSR so far
    //
    lfs_size_t e = 0;
    memset(λ, 0, λ_size-1);
    λ[λ_size-1] = 1;
    memset(c, 0, c_size-1);
    c[c_size-1] = 1;

    // iterate through symbols
    for (lfs_size_t n = 0; n < s_size; n++) {
        // shift C(i) = C(i-1)
        memmove(c, c+1, c_size-1);
        c[c_size-1] = 0;

        // calculate next symbol discrepancy
        //
        // let d = S_n - Λ(n) = S_n - sum_k=1^e Λ_k S_n-k
        //
        uint8_t d = s[s_size-1-n];
        for (lfs_size_t k = 1; k <= e; k++) {
            d ^= ramrsbd_gf_mul(
                    λ[λ_size-1-k],
                    s[s_size-1-(n-k)]);
        }

        // found discrepancy?
        if (d != 0) {
            // let Λ(i) = Λ(i) - d C(i)
            ramrsbd_gf_p_xors(
                    λ, λ_size,
                    d,
                    c, c_size);

            // not enough errors for discrepancy?
            if (n >= 2*e) {
                // update the number of errors
                e = n+1 - e;

                // save best LFSR for later
                //
                // this should be C(i) = d^-1 Λ(i), but before we
                // modified Λ(i) = Λ(i) - d C(i), fortunately we can just
                // undo the modification to avoid needing another buffer:
                //
                // let C(i) = d^-1 (Λ(i) + d C(i))
                //          = C(i) + d^-1 Λ(i)
                //
                ramrsbd_gf_p_xors(
                        c, c_size,
                        ramrsbd_gf_div(1, d),
                        λ, λ_size);
            }
        }
    }

    return e;
}

// find the error-evaluator polynomial Ω(x), given a syndrome
// polynomial S(x) and an error-locator polynomial Λ(x)
//
// Ω(x) = S(x) Λ(x) mod x^n
//
// this indirectly gives us our error-magnitudes Y_j for a given X_j,
// Ω(X_j^-1) = Y_j X_j Λ'(X_j^-1), if j is an error
//
static void ramrsbd_find_ω(
        uint8_t *ω, lfs_size_t ω_size,
        const uint8_t *s, lfs_size_t s_size,
        const uint8_t *λ, lfs_size_t λ_size) {
    LFS_ASSERT(ω_size == s_size);
    LFS_ASSERT(ω_size == λ_size);

    // let Ω(x) = S(x) Λ(x) mod x^n
    //
    // note that the mod is really just truncating the array, which
    // ramrsbd_gf_p_mul does implicitly if the array is too small
    //
    memcpy(ω, s, s_size);
    ramrsbd_gf_p_mul(
            ω, ω_size,
            λ, λ_size);
}

int ramrsbd_read(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size) {
    RAMRSBD_TRACE("ramrsbd_read(%p, "
                "0x%"PRIx32", %"PRIu32", %p, %"PRIu32")",
            (void*)cfg, block, off, buffer, size);
    ramrsbd_t *bd = cfg->context;

    // check if read is valid
    LFS_ASSERT(block < cfg->block_count);
    LFS_ASSERT(off  % cfg->read_size == 0);
    LFS_ASSERT(size % cfg->read_size == 0);
    LFS_ASSERT(off+size <= cfg->block_size);

    // work on one codeword at a time
    uint8_t *buffer_ = buffer;
    while (size > 0) {
        // map off to codeword space
        lfs_off_t off_
                = (off / (bd->cfg->code_size-bd->cfg->ecc_size))
                * bd->cfg->code_size;

        // read codeword
        memcpy(bd->c,
                &bd->buffer[block*bd->cfg->erase_size + off_],
                bd->cfg->code_size);

        // calculate syndromes
        bool s_zero = ramrsbd_find_s(
                bd->s, bd->cfg->ecc_size,
                bd->c, bd->cfg->code_size);

        // non-zero syndromes? errors are present, attempt to correct
        if (!s_zero) {
            // find the error-locator polynomial Λ(x)
            lfs_size_t n = ramrsbd_find_λ(
                    bd->λ, bd->cfg->ecc_size,
                    // use Ω(x) as scratch space
                    bd->ω, bd->cfg->ecc_size,
                    bd->s, bd->cfg->ecc_size);

            // too many errors?
            if (n > bd->cfg->ecc_size/2
                    || (bd->cfg->error_correction
                        && (lfs_ssize_t)n > bd->cfg->error_correction)) {
                LFS_WARN("Found uncorrectable ramrsbd errors "
                        "0x%"PRIx32".%"PRIx32" %"PRIu32" "
                        "(%"PRId32" > %"PRId32")",
                        block, off_,
                        bd->cfg->code_size - bd->cfg->ecc_size,
                        n,
                        (bd->cfg->error_correction)
                            ? bd->cfg->error_correction
                            : (lfs_ssize_t)(bd->cfg->ecc_size/2));
                return LFS_ERR_CORRUPT;
            }

            // find the error evaluator polynomial Ω(x)
            ramrsbd_find_ω(
                    bd->ω, bd->cfg->ecc_size,
                    bd->s, bd->cfg->ecc_size,
                    bd->λ, bd->cfg->ecc_size);

            // brute force search for error locations, this is any
            // location X_j=g^j where X_j^-1 is a root of our
            // error-locator, Λ(X_j^-1) = 0
            for (lfs_size_t j = 0; j < bd->cfg->code_size; j++) {
                // map the error location to the multiplicative ring
                //
                // let X_j = g^j
                //
                uint8_t x_j = ramrsbd_gf_pow(
                        RAMRSBD_GF_G,
                        bd->cfg->code_size-1-j);
                uint8_t x_j_ = ramrsbd_gf_div(1, x_j);

                // is X_j a root of our error-locator?
                //
                // does Λ(X_j^-1) = 0?
                //
                if (ramrsbd_gf_p_eval(
                            bd->λ, bd->cfg->ecc_size,
                            x_j_)
                        != 0) {
                    continue;
                }

                // found an error location, now find its magnitude
                //
                //                Ω(X_j^-1)
                // let Y_j = X_j ----------
                //               Λ'(X_j^-1)
                //
                uint8_t y_j = ramrsbd_gf_mul(
                        x_j,
                        ramrsbd_gf_div(
                            ramrsbd_gf_p_eval(
                                bd->ω, bd->cfg->ecc_size,
                                x_j_),
                            ramrsbd_gf_p_deval(
                                bd->λ, bd->cfg->ecc_size,
                                x_j_)));

                // found error location and magnitude, now we can fix it!
                bd->c[j] ^= y_j;
            }

            // calculate syndromes again to make sure we found all errors
            bool s_zero = ramrsbd_find_s(
                    bd->s, bd->cfg->ecc_size,
                    bd->c, bd->cfg->code_size);
            if (!s_zero) {
                LFS_WARN("Found uncorrectable ramrsbd errors "
                        "0x%"PRIx32".%"PRIx32" %"PRIu32" "
                        "(s != 0)",
                        block, off_,
                        bd->cfg->code_size - bd->cfg->ecc_size);
                return LFS_ERR_CORRUPT;
            }

            LFS_DEBUG("Found %"PRId32" correctable ramcrc32bd errors "
                    "0x%"PRIx32".%"PRIx32" %"PRIu32,
                    n,
                    block, off_,
                    bd->cfg->code_size - bd->cfg->ecc_size);
        }

        // copy the data part of our codeword
        memcpy(buffer_, bd->c, bd->cfg->code_size-bd->cfg->ecc_size);

        off += bd->cfg->code_size-bd->cfg->ecc_size;
        buffer_ += bd->cfg->code_size-bd->cfg->ecc_size;
        size -= bd->cfg->code_size-bd->cfg->ecc_size;
    }

    RAMRSBD_TRACE("ramrsbd_read -> %d", 0);
    return 0;
}

int ramrsbd_prog(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size) {
    RAMRSBD_TRACE("ramrsbd_prog(%p, "
                "0x%"PRIx32", %"PRIu32", %p, %"PRIu32")",
            (void*)cfg, block, off, buffer, size);
    ramrsbd_t *bd = cfg->context;

    // check if prog is valid
    LFS_ASSERT(block < cfg->block_count);
    LFS_ASSERT(off  % cfg->prog_size == 0);
    LFS_ASSERT(size % cfg->prog_size == 0);
    LFS_ASSERT(off+size <= cfg->block_size);

    // work on one codeword at a time
    const uint8_t *buffer_ = buffer;
    while (size > 0) {
        // map off to codeword space
        lfs_off_t off_
                = (off / (bd->cfg->code_size-bd->cfg->ecc_size))
                * bd->cfg->code_size;

        // calculate ecc of size n
        //
        // let C(x) = M(x) x^n + (M(x) x^n mod P(x))
        //
        // note this makes C(x) divisible by P(x)
        //
        memset(bd->c, 0, bd->cfg->code_size);
        memcpy(bd->c, buffer_, bd->cfg->code_size-bd->cfg->ecc_size);
        ramrsbd_gf_p_divmod1(
                bd->c, bd->cfg->code_size,
                bd->p, bd->cfg->ecc_size);

        // the divmod clobbers M(x), so we need to copy M(x) again
        memcpy(bd->c, buffer_, bd->cfg->code_size-bd->cfg->ecc_size);

        // program our codeword
        memcpy(&bd->buffer[block*bd->cfg->erase_size + off_],
                bd->c,
                bd->cfg->code_size);

        off += bd->cfg->code_size-bd->cfg->ecc_size;
        buffer_ += bd->cfg->code_size-bd->cfg->ecc_size;
        size -= bd->cfg->code_size-bd->cfg->ecc_size;
    }

    RAMRSBD_TRACE("ramrsbd_prog -> %d", 0);
    return 0;
}

int ramrsbd_erase(const struct lfs_config *cfg, lfs_block_t block) {
    RAMRSBD_TRACE("ramrsbd_erase(%p, 0x%"PRIx32" (%"PRIu32"))",
            (void*)cfg, block, ((ramrsbd_t*)cfg->context)->cfg->erase_size);

    // check if erase is valid
    LFS_ASSERT(block < cfg->block_count);

    // erase is a noop
    (void)cfg;
    (void)block;

    RAMRSBD_TRACE("ramrsbd_erase -> %d", 0);
    return 0;
}

int ramrsbd_sync(const struct lfs_config *cfg) {
    RAMRSBD_TRACE("ramrsbd_sync(%p)", (void*)cfg);

    // sync is a noop
    (void)cfg;

    RAMRSBD_TRACE("ramrsbd_sync -> %d", 0);
    return 0;
}
