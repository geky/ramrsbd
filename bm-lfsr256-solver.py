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


# step an LFSR
def step(l, s):
    return ft.reduce(
        op.__xor__,
        [gf_mul(l_, s_) for l_, s_ in zip(l[1:], s)],
        0)

# generate a random LFSR
def randomlfsr(n):
    import random
    # generate a random LFSR
    l = [0] + [random.getrandbits(8) for _ in range(n)]
    # generate a random initial state
    s = [random.getrandbits(8) for _ in range(n)]

    # run the lfsr for an additional n steps
    for _ in range(n):
        s = [step(l, s)] + s

    return s


# some helpers for printing things
def sequence(l):
    return " ".join("%02x" % l_ for l_ in l)

def recurrence(l):
    if not any(l):
        return "0"

    return " + ".join([
        "%02x s_%s" % (
            b,
            "i" if i == 0 else "i-%d" % i)
        for i, b in enumerate(l)
        if b])

def lfsr(l, bits):
    l = l[1:]
    wire = max(
        (i if l_ else 0 for i, l_ in enumerate(l)),
        default=0)
    yield "".join([
        ".----" if any(l) else "     ",
        "".join("-.   " if l_ and i == wire
                else "     " if i >= wire
                else " + <-" if l_
                else "-----"
            for i, l_ in enumerate(l)),
    ])
    yield "".join([
        "|    " if any(l) else "     ",
        "".join(" |   " if l_ and i == wire
                else " ^   " if l_
                else "     "
            for i, l_ in enumerate(l)),
    ])
    yield "".join([
        "|    " if any(l) else "     ",
        "".join("*%02x  " % l_ if l_ else "     "
            for l_ in l),
    ])
    yield "".join([
        "|    " if any(l) else "     ",
        "".join(" ^   " if l_
                else "     "
            for i, l_ in enumerate(l)),
    ])
    yield "".join([
        "|   " if any(l) else "    ",
        "." if l else "",
        "".join("-|--." if l_ else "----."
            for l_ in l),
    ])
    yield "".join([
        "'-> " if any(l) else "0-> ",
        "|" if l else "",
        "".join(" %02x |" % bits[len(bits)-len(l)+i]
            for i, _ in enumerate(l)),
        "-> " if l else "",
    ])
    yield "".join([
        "    ",
        "'" if l else "",
        "".join("----'" for _ in l),
        "   " if l else "",
    ])
    yield "".join([
        "    ",
        " " if l else "",
        "".join("     " for _ in l),
        "   " if l else "",
    ])



def main(bytes, *,
        p=None,
        random=None):
    # first build our GF_POW/GF_LOG tables based on p
    build_gf_tables(p)

    # generate random bytes?
    if random is not None:
        s = randomlfsr(random)

    # parse as a sequence of bytes
    else:
        s = [int(b, 16) for b in bytes]

    print("Solving: %s" % sequence(s))
    print()

    # guess an initial LFSR
    n = 0
    e = 0
    l = [0]
    c = [1]

    print("|L%d| = %d" % (n, e))
    print("L%d(i) = %s" % (n, recurrence(l)))
    print("C%d(i) = %s" % (n, recurrence(c)))
    print()

    while n < len(s):
        # calculate the discrepancy d
        next = step(l, s[len(s)-n:])
        d = next ^ s[len(s)-(n+1)]

        for i, line in enumerate(lfsr(l, s)):
            if i == 5:
                print("L%d = " % n, end='')
            else:
                print(" "*len("L%d = " % n), end='')

            print(line, end='')

            if i == 5:
                print("Output:   %s" % (
                    sequence([next] + s[len(s)-n:])),
                    end='')
            elif i == 6:
                print("Expected: %s" % (
                    sequence(s[len(s)-(n+1):])),
                    end='')
            elif i == 7:
                print("      d = %02x" % d, end='')

            print()
        print()

        # no discrepancy? keep going
        if d == 0:
            e_ = e
            l_ = l
            # let C'(i) = C(i-1)
            c_ = [0] + c

            print("|L%d| = |L%d| = %d" % (n+1, n, e_))
            print("L%d(i) = L%d(i) = %s" % (n+1, n, recurrence(l_)))
            print("C%d(i) = C%d(i-1) = %s" % (n+1, n, recurrence(c_)))
            print()

        # found discrepancy?
        else:
            # fits in current LFSR?
            if n < 2*e:
                e_ = e
                # let L'(i) = L(i) + d C(i-1)
                l_ = [l_ ^ gf_mul(c_, d)
                    for l_, c_ in it.zip_longest(
                        l, [0] + c, fillvalue=0)]
                # let C'(i) = C(i-1)
                c_ = [0] + c

                print("|L%d| = |L%d| = %d" % (n+1, n, e_))
                print("L%d(i) = L%d(i) + d C%d(i-1) = %s" % (
                    n+1, n, n, recurrence(l_)))
                print("C%d(i) = C%d(i-1) = %s" % (
                    n+1, n, recurrence(c_)))
                print()

            # need a bigger LFSR?
            else:
                # let |L'| = n+1-|L|
                e_ = n+1-e
                # let L'(i) = L(i) + d C(i-1)
                l_ = [l_ ^ gf_mul(c_, d)
                    for l_, c_ in it.zip_longest(
                        l, [0] + c, fillvalue=0)]
                # let C'(i) = d^-1 (s_i + L(i))
                c_ = [gf_div(c_, d) for c_ in [1] + l[1:]]

                print("|L%d| = %d+1-|L%d| = %d" % (n+1, n, n, e_))
                print("L%d(i) = L%d(i) + d C%d(i-1) = %s" % (
                    n+1, n, n, recurrence(l_)))
                print("C%d(i) = d^-1 L%d(i) = %s" % (
                    n+1, n, recurrence(c_)))
                print()

        e = e_
        l = l_
        c = c_
        n += 1

    # print final LFSR
    for i, line in enumerate(it.islice(lfsr(l, s), 7)):
        if i == 5:
            print("L%d = " % n, end='')
        else:
            print(" "*len("L%d = " % n), end='')

        print(line, end='')

        if i == 5:
            print("Output:   %s" % sequence(s), end='')
        elif i == 6:
            print("Expected: %s" % sequence(s), end='')

        print()
    print()


if __name__ == "__main__":
    import sys
    import argparse
    parser = argparse.ArgumentParser(
        description="Find the minimal LFSR for a sequence of GF(256) bytes "
            "using the Berlekamp-Massey algorithm.",
        allow_abbrev=False)
    parser.add_argument(
        'bytes',
        nargs='*',
        help="Byte sequence to find an LFSR from.")
    parser.add_argument(
        '-p',
        type=lambda x: int(x, 0),
        default=0x11d,
        help="The irreducible polynomial that defines the field. Defaults to "
            "0x11d.")
    parser.add_argument(
        '-r', '--random',
        type=lambda x: int(x, 0),
        help="Use a byte sequence generated by a random LFSR of this size.")
    sys.exit(main(**{k: v
        for k, v in vars(parser.parse_args()).items()
        if v is not None}))
