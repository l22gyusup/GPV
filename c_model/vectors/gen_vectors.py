#!/usr/bin/env python3
#------------------------------------------------------------------------------
# File        : gen_vectors.py
# Description : Generate CSV test vectors for the 16-point FFT reference model.
#               Uses numpy.fft as the golden source of truth.
# Author      : Gyusup LEE <gyu2910@waric.co.kr>
# Created     : 2026-08-20
# Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
#------------------------------------------------------------------------------

import argparse
import csv
import datetime
import os
import sys
import time

import numpy as np


FFT_POINTS = 16


def golden_fft(x):
    return np.fft.fft(x)


def case_impulse():
    x = np.zeros(FFT_POINTS, dtype=complex)
    x[0] = 1.0 + 0.0j
    return x


def case_dc():
    return np.ones(FFT_POINTS, dtype=complex)


def case_tone(bin_k):
    n = np.arange(FFT_POINTS)
    return np.exp(1j * 2.0 * np.pi * bin_k * n / FFT_POINTS)


def case_random(rng, scale=0.9):
    # Q2.14 fixed input range is roughly [-2, 2). Keep well within [-1, 1] to
    # leave headroom for the float tests to share the same vectors.
    re = rng.uniform(-scale, scale, FFT_POINTS)
    im = rng.uniform(-scale, scale, FFT_POINTS)
    return re + 1j * im


def case_two_tone(bin_a, bin_b, amp_a=0.5, amp_b=0.5):
    return amp_a * case_tone(bin_a) + amp_b * case_tone(bin_b)


def build_cases():
    cases = []

    cases.append(("impulse", case_impulse()))
    cases.append(("dc", case_dc()))

    for k in range(FFT_POINTS):
        cases.append((f"tone_bin{k}", case_tone(k)))

    two_tone_pairs = [(1, 5), (2, 7), (3, 8), (4, 12), (0, 8)]
    for a, b in two_tone_pairs:
        cases.append((f"two_tone_{a}_{b}", case_two_tone(a, b)))

    rng = np.random.default_rng(seed=20260820)
    for i in range(64):
        cases.append((f"random_{i:03d}", case_random(rng)))

    return cases


def build_random_cases(num_cases, seed):
    rng = np.random.default_rng(seed=seed)
    return [(f"random_{i:06d}", case_random(rng)) for i in range(num_cases)]


def write_csv(path, cases):
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow([
            "test_id", "test_name", "sample_idx",
            "in_re", "in_im", "exp_out_re", "exp_out_im",
        ])
        for tid, (name, x) in enumerate(cases):
            y = golden_fft(x)
            for n in range(FFT_POINTS):
                w.writerow([
                    tid, name, n,
                    f"{x[n].real:.10f}", f"{x[n].imag:.10f}",
                    f"{y[n].real:.10f}", f"{y[n].imag:.10f}",
                ])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--regress", action="store_true",
                        help="Emit a random regression set instead of the "
                             "curated fixed vectors.")
    parser.add_argument("--num-cases", type=int, default=1000,
                        help="Random case count (regress mode only)")
    parser.add_argument("--seed", type=int, default=None,
                        help="RNG seed (regress mode; default: current epoch)")
    parser.add_argument("--out", default=None,
                        help="Output path (default: curated → "
                             "vectors/fft16_vectors.csv; regress → "
                             "vectors/fft16_random_<timestamp>.csv)")
    args = parser.parse_args()

    out_dir = os.path.dirname(os.path.abspath(__file__))

    if args.regress:
        seed = args.seed if args.seed is not None else int(time.time())
        cases = build_random_cases(args.num_cases, seed)
        if args.out is None:
            stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            out_path = os.path.join(out_dir, f"fft16_random_{stamp}.csv")
        else:
            out_path = args.out
        write_csv(out_path, cases)
        print(f"Wrote {len(cases)} random cases "
              f"({len(cases) * FFT_POINTS} rows, seed={seed}) to {out_path}")
    else:
        out_path = (args.out if args.out is not None
                    else os.path.join(out_dir, "fft16_vectors.csv"))
        cases = build_cases()
        write_csv(out_path, cases)
        print(f"Wrote {len(cases)} test cases "
              f"({len(cases) * FFT_POINTS} rows) to {out_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
