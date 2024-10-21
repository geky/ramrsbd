#!/usr/bin/env python3


def main(p, g, *, pow=False, log=False):
    if not pow and not log:
        pow = True
        log = True

    assert g == 2

    # generate the pow table via repeated multiplications by a generator
    # (assumed to be 2), keep track of the inverse mapping which is the
    # log table
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

    # print the pow table
    if pow:
        print("// power table, RAMRSBD_GF_POW[x] = g^x")
        print("static const uint8_t RAMRSBD_GF_POW[256] = {")
        for j in range(256//8):
            print("    ", end='')
            for i in range(8):
                print("%s0x%02x," % (
                    " " if i != 0 else "",
                    pow_table[j*8+i]),
                    end='')
            print()
        print("};")
        print()

    # print the log table
    if log:
        print("// log table, RAMRSBD_GF_LOG[x] = log_g x")
        print("static const uint8_t RAMRSBD_GF_LOG[256] = {")
        for j in range(256//8):
            print("    ", end='')
            for i in range(8):
                print("%s0x%02x," % (
                    " " if i != 0 else "",
                    log_table.get(j*8+i, 0xff)),
                    end='')
            print()
        print("};")
        print()


if __name__ == "__main__":
    import sys
    import argparse
    parser = argparse.ArgumentParser(
        description="Generate GF(256) pow/log tables.",
        allow_abbrev=False)
    parser.add_argument(
        'p',
        nargs='?',
        type=lambda x: int(x, 0),
        default=0x11d,
        help="The irreducible polynomial that defines the field. Defaults to "
            "0x11d, the lexicographically smallest 9-bit irreducible "
            "polynomial where 2 is a generator.")
    parser.add_argument(
        'g',
        nargs='?',
        type=lambda x: int(x, 0),
        default=0x02,
        help="A generator element in the field, must be 2. Defaults to 2.")
    parser.add_argument(
        '--pow',
        action='store_true',
        help="Generate the GF_POW table. Defaults to generating both.")
    parser.add_argument(
        '--log',
        action='store_true',
        help="Generate the GF_LOG table. Defaults to generating both.")
    sys.exit(main(**{k: v
        for k, v in vars(parser.parse_args()).items()
        if v is not None}))
