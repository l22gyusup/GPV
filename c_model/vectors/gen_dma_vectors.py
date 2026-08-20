#!/usr/bin/env python3
#------------------------------------------------------------------------------
# File        : gen_dma_vectors.py
# Description : Generate CSV test vectors for the fft_dma reference model.
#               Uses numpy.fft as the golden source of truth for MODE_FFT
#               and the spec's counter pattern for MODE_WRITE_ONLY.
# Author      : Gyusup LEE <gyu2910@waric.co.kr>
# Created     : 2026-08-20
# Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
#------------------------------------------------------------------------------

import csv
import os
import sys

import numpy as np


MODE_FFT        = 0
MODE_READ_ONLY  = 1
MODE_WRITE_ONLY = 2

FFT_POINTS   = 16
SAMPLE_BYTES = 4


def impulse(n):
    x = np.zeros(n, dtype=complex)
    x[0] = 1.0 + 0.0j
    return x


def dc(n):
    return np.ones(n, dtype=complex)


def tone(n, k):
    idx = np.arange(n)
    return np.exp(1j * 2.0 * np.pi * k * idx / n)


def rand_data(n, rng, scale=0.9):
    re = rng.uniform(-scale, scale, n)
    im = rng.uniform(-scale, scale, n)
    return re + 1j * im


def batch_fft(x, num_ffts):
    out = np.zeros_like(x)
    for k in range(num_ffts):
        chunk = x[k * FFT_POINTS : (k + 1) * FFT_POINTS]
        out[k * FFT_POINTS : (k + 1) * FFT_POINTS] = np.fft.fft(chunk)
    return out


def counter_pattern(n_samples):
    x = np.zeros(n_samples, dtype=complex)
    for n in range(n_samples):
        re_raw = n & 0xFFFF
        im_raw = (n >> 16) & 0xFFFF
        if re_raw >= 0x8000:
            re_raw -= 0x10000
        if im_raw >= 0x8000:
            im_raw -= 0x10000
        x[n] = complex(re_raw / 16384.0, im_raw / 16384.0)
    return x


def build_scenarios():
    scenarios = []
    tid = 0
    rng = np.random.default_rng(seed=20260820)

    fft_cases = [
        (1,  "fft_impulse_1",    impulse(FFT_POINTS)),
        (1,  "fft_dc_1",         dc(FFT_POINTS)),
        (1,  "fft_tone1_1",      tone(FFT_POINTS, 1)),
        (2,  "fft_batch2",       np.concatenate([impulse(FFT_POINTS),
                                                 dc(FFT_POINTS)])),
        (3,  "fft_batch3",       np.concatenate([tone(FFT_POINTS, 1),
                                                 tone(FFT_POINTS, 8),
                                                 dc(FFT_POINTS)])),
        (5,  "fft_random5",      rand_data(FFT_POINTS * 5,  rng)),
        (8,  "fft_random8",      rand_data(FFT_POINTS * 8,  rng)),
        (10, "fft_random10",     rand_data(FFT_POINTS * 10, rng)),
        (1,  "fft_random1_addr", rand_data(FFT_POINTS,      rng)),
        (4,  "fft_random4_addr", rand_data(FFT_POINTS * 4,  rng)),
    ]
    for nf, name, x in fft_cases:
        src, dst = (0x800, 0x2400) if "addr" in name else (0x100, 0x1000)
        exp = batch_fft(x, nf)
        scenarios.append({
            "id": tid, "name": name, "mode": MODE_FFT,
            "num_ffts": nf, "src": src, "dst": dst,
            "in": x, "exp": exp,
        })
        tid += 1

    ro_cases = [
        (1,  "readonly_1"),
        (2,  "readonly_2"),
        (5,  "readonly_5"),
        (8,  "readonly_8"),
        (10, "readonly_10_addr"),
    ]
    for nf, name in ro_cases:
        src, dst = (0x800, 0x2400) if "addr" in name else (0x100, 0x1000)
        x = rand_data(FFT_POINTS * nf, rng)
        exp = np.zeros(FFT_POINTS * nf, dtype=complex)
        scenarios.append({
            "id": tid, "name": name, "mode": MODE_READ_ONLY,
            "num_ffts": nf, "src": src, "dst": dst,
            "in": x, "exp": exp,
        })
        tid += 1

    wo_cases = [
        (1,  "writeonly_1"),
        (2,  "writeonly_2"),
        (5,  "writeonly_5"),
        (8,  "writeonly_8"),
        (10, "writeonly_10"),
    ]
    for nf, name in wo_cases:
        src, dst = 0x100, 0x1000
        n_samples = FFT_POINTS * nf
        x = np.zeros(n_samples, dtype=complex)
        exp = counter_pattern(n_samples)
        scenarios.append({
            "id": tid, "name": name, "mode": MODE_WRITE_ONLY,
            "num_ffts": nf, "src": src, "dst": dst,
            "in": x, "exp": exp,
        })
        tid += 1

    return scenarios


def write_csv(path, scenarios):
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow([
            "test_id", "test_name", "mode",
            "num_ffts", "src_addr", "dst_addr",
            "sample_idx", "in_re", "in_im", "exp_re", "exp_im",
        ])
        for s in scenarios:
            n_samples = FFT_POINTS * s["num_ffts"]
            for n in range(n_samples):
                w.writerow([
                    s["id"], s["name"], s["mode"],
                    s["num_ffts"], s["src"], s["dst"],
                    n,
                    f"{s['in'][n].real:.10f}",  f"{s['in'][n].imag:.10f}",
                    f"{s['exp'][n].real:.10f}", f"{s['exp'][n].imag:.10f}",
                ])


def main():
    out_dir  = os.path.dirname(os.path.abspath(__file__))
    out_path = os.path.join(out_dir, "fft_dma_vectors.csv")
    scenarios = build_scenarios()
    total_rows = sum(FFT_POINTS * s["num_ffts"] for s in scenarios)
    write_csv(out_path, scenarios)
    print(f"Wrote {len(scenarios)} scenarios ({total_rows} rows) to {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
