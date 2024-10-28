#!/usr/bin/env python3

import itertools as it
import functools as ft
import operator as op


GF_POW = []
GF_LOG = []

# build the GF_POW/GF_LOG tables
def build_gf_tables(p):
    global GF_POW
    global GF_LOG
    pow_table = []
    log_table = {}

    x = 1
    for i in range(256):
        pow_table.append(x)
        if x not in log_table:
            log_table[x] = i

        x <<= 1
        if x & 0x100:
            x ^= p

    GF_POW = pow_table
    GF_LOG = [log_table.get(i, 0xff) for i in range(256)]

# GF(256) operations
def gf_mul(a, b):
    if a == 0 or b == 0:
        return 0

    x = GF_LOG[a] + GF_LOG[b]
    if x > 255:
        x -= 255
    return GF_POW[x]

def gf_div(a, b):
    assert b != 0

    x = GF_LOG[a] + 255 - GF_LOG[b]
    if x > 255:
        x -= 255
    return GF_POW[x]

def gf_pow(a, e):
    if e == 0:
        return 1
    elif a == 0:
        return 0
    else:
        x = (GF_LOG[a] * e) % 255
        return GF_POW[x]


# GF(256) polynomial operations
def gf_p_eval(p, x):
    y = 0
    for p_ in p:
        y = gf_mul(y, x) ^ p_
    return y

def gf_p_scale(p, c):
    return [gf_mul(p_, c) for p_ in p]

def gf_p_xor(a, b):
    r = [0]*max(len(a), len(b))
    for i, a_ in enumerate(a):
        r[i + len(r)-len(a)] ^= a_
    for i, b_ in enumerate(b):
        r[i + len(r)-len(b)] ^= b_
    return r

def gf_p_mul(a, b):
    r = [0]*(len(a)+len(b)-1)
    for i, a_ in enumerate(a):
        for j, b_ in enumerate(b):
            r[i+j] ^= gf_mul(a_, b_)
    return r

def gf_p_divmod(a, b):
    assert len(a) >= len(b)
    r = a.copy()
    for i in range(len(a)-len(b)+1):
        if r[i] != 0:
            r[i] = gf_div(r[i], b[0])

            for j, b_ in enumerate(b[1:]):
                r[i+j] ^= gf_mul(r[i], b_)
    return r


def main(ecc_size, *,
        p=None,
        no_truncate=False):
    # first build our GF_POW/GF_LOG tables based on p
    build_gf_tables(p)

    # calculate generator polynomial
    #
    # P(x) = prod_i^n-1 (x - g^i)
    #
    # the important property of P(x) is that it evaluates to 0
    # at every x=g^i for i < n
    #
    p = ft.reduce(
        gf_p_mul,
        ([1, gf_pow(2, i)] for i in range(ecc_size)),
        [1])

    # print the generator polynomial
    print("// generator polynomial for ecc_size=%s" % ecc_size)
    print("//")
    print("// P(x) = prod_i^n-1 (x - g^i)")
    print("//")
    print("static const uint8_t RAMRSBD_P[%s] = {" % (
        len(p) if no_truncate else len(p[1:])))
    if no_truncate:
        print("    ", end='')
        print("0x%02x," % p[0])
    for j in range((len(p[1:])+8-1)//8):
        print("    ", end='')
        for i in range(8):
            if j*8+i < len(p[1:]):
                print("%s0x%02x," % (
                    " " if i != 0 else "",
                    p[1:][j*8+i]),
                    end='')
        print()
    print("};")
    print()


if __name__ == "__main__":
    import sys
    import argparse
    parser = argparse.ArgumentParser(
        description="Generate the generator polynomial for a Reed-Solomon code "
            "with the specified ecc_size.",
        allow_abbrev=False)
    parser.add_argument(
        'ecc_size',
        type=lambda x: int(x, 0),
        help="Size of the error-correcting code in bytes. The resulting "
            "polynomial will also be this size.")
    parser.add_argument(
        '-p',
        type=lambda x: int(x, 0),
        default=0x11d,
        help="The irreducible polynomial that defines the field. Defaults to "
            "0x11d")
    parser.add_argument(
        '-T', '--no-truncate',
        action='store_true',
        help="Including the leading 1 byte. This makes the resulting "
            "polynomial ecc_size+1 bytes.")
    sys.exit(main(**{k: v
        for k, v in vars(parser.parse_args()).items()
        if v is not None}))
