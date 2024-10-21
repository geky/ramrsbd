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
    if (bd->cfg->math_buffer) {
        bd->c = (uint8_t*)bd->cfg->math_buffer;
    } else {
        bd->c = lfs_malloc(bd->cfg->code_size);
        if (!bd->c) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // allocate generator polynomial?
    if (bd->cfg->math_buffer) {
        bd->p = (uint8_t*)bd->cfg->math_buffer
                + bd->cfg->code_size;
    } else {
        bd->p = lfs_malloc(bd->cfg->ecc_size);
        if (!bd->p) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // allocate syndrome buffer?
    if (bd->cfg->math_buffer) {
        bd->s = (uint8_t*)bd->cfg->math_buffer
                + bd->cfg->code_size
                + bd->cfg->ecc_size;
    } else {
        bd->s = lfs_malloc(bd->cfg->ecc_size);
        if (!bd->s) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // allocate error-locator buffer?
    if (bd->cfg->math_buffer) {
        bd->l = (uint8_t*)bd->cfg->math_buffer
                + bd->cfg->code_size
                + bd->cfg->ecc_size
                + bd->cfg->ecc_size;
    } else {
        bd->l = lfs_malloc(bd->cfg->ecc_size);
        if (!bd->s) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // allocate error-locator derivative buffer?
    if (bd->cfg->math_buffer) {
        bd->dl = (uint8_t*)bd->cfg->math_buffer
                + bd->cfg->code_size
                + bd->cfg->ecc_size
                + bd->cfg->ecc_size
                + bd->cfg->ecc_size;
    } else {
        bd->dl = lfs_malloc(bd->cfg->ecc_size);
        if (!bd->s) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // calculate generator polynomial
    //
    // P(x) = prod_i^ecc_size { x - g^i }
    //
    // the important property of P(x) is that it evaluates to 0
    // at every x=g^i for i < ecc_size
    //
    // normally P(x) needs ecc_size+1 terms, but the leading term
    // is always 1, so we can make it implicit
    //
    
    // let P(x) = 1
    memset(bd->p, 0, bd->cfg->ecc_size);
    bd->p[bd->cfg->ecc_size-1] = 1;

    for (lfs_size_t i = 0; i < bd->cfg->ecc_size; i++) {
        // let R(x) = x + g^i
        uint8_t r[2] = {1, ramrsbd_gf_pow(RAMRSBD_GF_G, i)};

        // let P'(x) = P(x) * R(x)
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
    if (!bd->cfg->math_buffer) {
        lfs_free(bd->c);
        lfs_free(bd->p);
        lfs_free(bd->s);
        lfs_free(bd->l);
        lfs_free(bd->dl);
    }
    RAMRSBD_TRACE("ramrsbd_destroy -> %d", 0);
    return 0;
}

// find a set of syndromes, S, of a codeword C(x)
//
// S_i = C(g^i)
//
// also returns true if zero for convenience
static bool ramrsbd_find_s(
        uint8_t *s, lfs_size_t s_size,
        const uint8_t *c, lfs_size_t c_size) {
    // calculate syndromes
    bool s_zero = true;
    for (lfs_size_t i = 0; i < s_size; i++) {
        // let S_i = C(g^i)
        //
        // note that because C(x) is a multiple of P(x), and P(x) is zero
        // at every x=g^i for i < ecc_size, this should also be zero
        // if no errors are present
        //
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

// find the error-locator polynomial, L(x), given a set of syndromes, S,
// T(x) provides scratch space for interim math
//
// L(x) = prod_i=0^n { 1 - X_i x }
//
// also returns the number of errors for convenience
static lfs_size_t ramrsbd_find_l(
        uint8_t *l, lfs_size_t l_size,
        uint8_t *t, lfs_size_t t_size,
        const uint8_t *s, lfs_size_t s_size) {
    LFS_ASSERT(t_size == l_size);
    LFS_ASSERT(s_size == l_size);

    // iteratively find the error-locator using Berlekamp-Massey
    //

    // guess an error-locator
    //
    // let L(x) = 1 // current guess
    // let T(x) = 1 // previous guess
    //
    memset(l, 0, l_size-1);
    l[l_size-1] = 1;
    memset(t, 0, t_size-1);
    t[t_size-1] = 1;

    // guess a number of errors
    //
    // let n = 0
    //
    lfs_size_t n = 0;

    // iterate through syndromes
    for (lfs_size_t i = 0; i < s_size; i++) {
        // shift T(x)
        //
        // let T(x) = T(x) x
        memmove(t, t+1, t_size-1);
        t[t_size-1] = 0;

        // calculate syndrome discrepancy
        //
        // let d = sum_j=0^n { L_j S_i-j }
        //
        uint8_t d = s[s_size-1-i];
        for (lfs_size_t j = 1; j <= n; j++) {
            d ^= ramrsbd_gf_mul(
                    l[l_size-1-j],
                    s[s_size-1-(i-j)]);
        }

        // found discrepancy?
        if (d != 0) {
            // let L(x) = L(x) - d T(x)
            ramrsbd_gf_p_xors(
                    l, l_size,
                    d,
                    t, t_size);

            // not enough errors for discrepancy?
            if (i >= 2*n) {
                // let T(x) = T(x) + d^-1 L(x)
                ramrsbd_gf_p_xors(
                        t, t_size,
                        ramrsbd_gf_div(1, d),
                        l, l_size);

                // update the number of errors
                n = i+1 - n;
            }
        }
    }

    return n;
}

// find the error-evaluator polynomial, E(x), given a set of
// syndromes, S, and an error-locator polynomial, L(x)
//
// E(x) = S(x) L(x) mod x^2n
//
static void ramrsbd_find_e(
        uint8_t *e, lfs_size_t e_size,
        const uint8_t *s, lfs_size_t s_size,
        const uint8_t *l, lfs_size_t l_size) {
    LFS_ASSERT(e_size == s_size);
    LFS_ASSERT(e_size == l_size);

    // allow E(x)/S(x) to overlap
    if (e != s) {
        memcpy(e, s, s_size);
    }

    // let E(x) = S(x) L(x) mod x^2n
    //
    // note that the mod is really just truncating the array, which
    // ramrsbd_gf_p_mul does implicitly if the array is too small
    //
    ramrsbd_gf_p_mul(
            e, e_size,
            l, l_size);
}

// find the formal derivative of the error-locator polynomial
//
// L(x)  = 1 + sum_i=1^n {         L_i   x^i     }
// L'(x) =     sum_i=1^n { sum^i { L_i } x^(i-1) }
//
static void ramrsbd_find_dl(
        uint8_t *dl, lfs_size_t dl_size,
        const uint8_t *l, lfs_size_t l_size) {
    LFS_ASSERT(dl_size == l_size);

    memset(dl, 0, dl_size);
    for (lfs_size_t i = 1; i < l_size; i++) {
        // the formal derivative defines each step as repeated addition,
        // but our addition is just xor, so we really just need to see
        // if this term cancels itself out
        if (i % 2 != 0) {
            dl[dl_size-1-(i-1)] = l[l_size-1-i];
        }
    }
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
            // find the error-locator polynomial, L(x)
            lfs_size_t n = ramrsbd_find_l(
                    bd->l, bd->cfg->ecc_size,
                    bd->dl, bd->cfg->ecc_size,
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

            // find the error evaluator polynomial, E(x)
            ramrsbd_find_e(
                    bd->s, bd->cfg->ecc_size,
                    bd->s, bd->cfg->ecc_size,
                    bd->l, bd->cfg->ecc_size);

            // find the formal derivative of L(x)
            ramrsbd_find_dl(
                    bd->dl, bd->cfg->ecc_size,
                    bd->l, bd->cfg->ecc_size);

            // brute force search for error locations, this is any location i
            // where g^-(code_size-1-i) is a root of our error-locator
            for (lfs_size_t i = 0; i < bd->cfg->code_size; i++) {
                // map the error location to the multiplicative ring
                //
                // let X_i = g^i
                //
                uint8_t x_i = ramrsbd_gf_pow(
                        RAMRSBD_GF_G,
                        bd->cfg->code_size-1-i);
                uint8_t x_i_ = ramrsbd_gf_div(1, x_i);

                // is X_i a root of our error-locator?
                //
                // does L(X_i^-1) = 0?
                //
                if (ramrsbd_gf_p_eval(
                            bd->l, bd->cfg->ecc_size,
                            x_i_)
                        != 0) {
                    continue;
                }

                // found an error location, now find its magnitude
                //
                //                E(X_i^-1)
                // let Y_i = X_i ----------
                //               L'(X_i^-1) 
                //
                uint8_t y_i = ramrsbd_gf_mul(
                        x_i,
                        ramrsbd_gf_div(
                            ramrsbd_gf_p_eval(
                                bd->s, bd->cfg->ecc_size,
                                x_i_),
                            ramrsbd_gf_p_eval(
                                bd->dl, bd->cfg->ecc_size,
                                x_i_)));

                // found error location and magnitude, now we can fix it!
                bd->c[i] ^= y_i;
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

        // calculate ecc
        //
        // let C(x) = M(x) x^n + (M(x) x^n % P(x))
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
