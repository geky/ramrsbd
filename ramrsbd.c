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
        bd->m = bd->cfg->code_buffer;
    } else {
        bd->m = lfs_malloc(bd->cfg->code_size);
        if (!bd->m) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // allocate generator polynomial?
    if (bd->cfg->p_buffer) {
        bd->p = bd->cfg->p_buffer;
    } else {
        bd->p = lfs_malloc(bd->cfg->ecc_size+1);
        if (!bd->p) {
            RAMRSBD_TRACE("ramrsbd_create -> %d", LFS_ERR_NOMEM);
            return LFS_ERR_NOMEM;
        }
    }

    // calculate generator polynomial
    //
    // p(x) = prod_i^ecc_size{ x - g^i }
    //
    // note this evaluates to 0 at every x=g^i for i < ecc_size
    
    // let p(x) = 1
    memset(bd->p, 0, bd->cfg->ecc_size);
    bd->p[bd->cfg->ecc_size] = 1;

    for (lfs_size_t i = 0; i < bd->cfg->ecc_size; i++) {
        // let r(x) = x + g^i
        uint8_t r[2] = {1, ramrsbd_gf_pow(RAMRSBD_GF_G, i)};

        // let p'(x) = p(x) * r(x)
        ramrsbd_gf_p_mul(
                bd->p, bd->cfg->ecc_size+1,
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
        lfs_free(bd->m);
    }
    if (!bd->cfg->p_buffer) {
        lfs_free(bd->p);
    }
    RAMRSBD_TRACE("ramrsbd_destroy -> %d", 0);
    return 0;
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
        memcpy(bd->m,
                &bd->buffer[block*bd->cfg->erase_size + off_],
                bd->cfg->code_size);

        // copy the data part of our codeword
        memcpy(buffer_, bd->m, bd->cfg->code_size-bd->cfg->ecc_size);

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
        // let m'(x) = m(x) + (m(x) % p(x))
        //
        // note this makes m'(x) divisible by p(x)
        //
        memcpy(bd->m, buffer_, bd->cfg->code_size-bd->cfg->ecc_size);
        ramrsbd_gf_p_divmod(
                bd->m, bd->cfg->code_size,
                bd->p, bd->cfg->ecc_size+1);

        // the divmod clobbers m(x), so we need to copy m(x) again
        memcpy(bd->m, buffer_, bd->cfg->code_size-bd->cfg->ecc_size);

        // program codeword
        memcpy(&bd->buffer[block*bd->cfg->erase_size + off_],
                buffer_, bd->cfg->code_size-bd->cfg->ecc_size);

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
