#!/usr/bin/env python3

import itertools as it
import functools as ft
import operator as op


# step an LFSR
def step(l, s):
    return ft.reduce(
        op.__xor__,
        [l_ & s_ for l_, s_ in zip(l[1:], s)],
        0)

# some helpers for printing things
def sequence(l):
    return " ".join("%01x" % l_ for l_ in l)

def recurrence(l):
    if not any(l):
        return "0"

    return " + ".join([
        "s_i" if i == 0 else "s_i-%d" % i
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
        "|   " if any(l) else "    ",
        "." if l else "",
        "".join("-|--." if l_ else "----."
            for l_ in l),
    ])
    yield "".join([
        "'-> " if any(l) else "0-> ",
        "|" if l else "",
        "".join(" %01x  |" % bits[len(bits)-len(l)+i]
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



def main(bits):
    # reparse to accept bits with and without spaces
    bits_ = []
    for b in bits:
        for c in b:
            if c == '1':
                bits_.append(1)
            elif c == '0':
                bits_.append(0)
    bits = bits_

    print("Solving: %s" % sequence(bits))
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

    while n < len(bits):
        # calculate the discrepancy d
        next = step(l, bits[len(bits)-n:])
        d = next ^ bits[len(bits)-(n+1)]

        for i, line in enumerate(lfsr(l, bits)):
            if i == 3:
                print("L%d = " % n, end='')
            else:
                print(" "*len("L%d = " % n), end='')

            print(line, end='')

            if i == 3:
                print("Output:   %s" % (
                    sequence([next] + bits[len(bits)-n:])),
                    end='')
            elif i == 4:
                print("Expected: %s" % (
                    sequence(bits[len(bits)-(n+1):])),
                    end='')
            elif i == 5:
                print("      d = %s" % d, end='')

            print()
        print()

        # no discrepancy? keep going
        if d == 0:
            # let C'(i) = C(i-1)
            c = [0] + c

            print("|L%d| = |L%d| = %d" % (n+1, n, e))
            print("L%d(i) = L%d(i) = %s" % (n+1, n, recurrence(l)))
            print("C%d(i) = C%d(i-1) = %s" % (n+1, n, recurrence(c)))
            print()

        # found discrepancy?
        else:
            # fits in current LFSR?
            if n < 2*e:
                l, c = (
                    # let L'(i) = L(i) + C(i-1)
                    [l_ ^ c_
                        for l_, c_ in it.zip_longest(
                            l, [0] + c, fillvalue=0)],
                    # let C'(i) = C(i-1)
                    [0] + c
                )

                print("|L%d| = |L%d| = %d" % (n+1, n, e))
                print("L%d(i) = L%d(i) + C%d(i-1) = %s" % (
                    n+1, n, n, recurrence(l)))
                print("C%d(i) = C%d(i-1) = %s" % (n+1, n, recurrence(c)))
                print()

            # need a bigger LFSR?
            else:
                e, l, c = (
                    # let |L'| = n+1-|L|
                    n+1-e,
                    # let L'(i) = L(i) + C(i-1)
                    [l_ ^ c_
                        for l_, c_ in it.zip_longest(
                            l, [0] + c, fillvalue=0)],
                    # let C'(i) = s_i + L(i)
                    [1] + l[1:]
                )

                print("|L%d| = %d+1-|L%d| = %d" % (n+1, n, n, e))
                print("L%d(i) = L%d(i) + C%d(i-1) = %s" % (
                    n+1, n, n, recurrence(l)))
                print("C%d(i) = L%d(i) = %s" % (n+1, n, recurrence(c)))
                print()

        n += 1

    # print final LFSR
    for i, line in enumerate(it.islice(lfsr(l, bits), 5)):
        if i == 3:
            print("L%d = " % n, end='')
        else:
            print(" "*len("L%d = " % n), end='')

        print(line, end='')

        if i == 3:
            print("Output:   %s" % sequence(bits), end='')
        elif i == 4:
            print("Expected: %s" % sequence(bits), end='')

        print()
    print()


if __name__ == "__main__":
    import sys
    import argparse
    parser = argparse.ArgumentParser(
        description="Find the minimal LFSR for a sequence of bits using "
            "the Berlekamp-Massey algorithm.",
        allow_abbrev=False)
    parser.add_argument(
        'bits',
        nargs='*',
        help="Bit sequence to find an LFSR from.")
    sys.exit(main(**{k: v
        for k, v in vars(parser.parse_args()).items()
        if v is not None}))
