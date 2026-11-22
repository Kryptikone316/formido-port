#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthesise the enhanced game's own sounds (no samples used; everything is generated here):

  flame.dat        a loopable flamethrower roar: shaped noise with a flutter and crackles (exactly periodic, so the loop is seamless)
  flame_start.dat  the ignition: a soft thump and a whoosh, played once when the flame starts
  tyrant_roar.dat  the Hive Tyrant's arrival: a deep growling roar that falls in pitch, with a sub-boom and a tail

All are 16-bit mono 22050 Hz WAV files (the game's own sounds are 22050 Hz mono as well).

usage: make_sounds.py <data folder>      (needs numpy and scipy)"""
import sys
import wave
import numpy as np
from scipy import signal

SR = 22050
rng = np.random.default_rng(20261121)


def save(path, x, peak=0.9):
    x = np.asarray(x, dtype=np.float64)
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    pcm = np.int16(np.round(x * 32767))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    print(f"{path}: {len(x) / SR:.2f} s, peak {np.max(np.abs(pcm)) / 32767:.2f}, first/last sample {pcm[0]}/{pcm[-1]}")


def periodic_noise(n, shape):
    """Noise whose spectrum follows shape(f); built in the frequency domain with random phases, so it repeats exactly."""
    f = np.fft.rfftfreq(n, 1.0 / SR)
    mag = shape(f)
    mag[0] = 0
    ph = rng.uniform(0, 2 * np.pi, len(f))
    return np.fft.irfft(mag * np.exp(1j * ph), n)


def flame_loop():
    n = SR * 8 // 10                    # 0.8 s; every modulation below has a whole number of cycles in it
    t = np.arange(n) / SR
    body = periodic_noise(n, lambda f: (np.minimum(f / 350.0, 1.0) ** 1.2) / (1.0 + (f / 2600.0) ** 2) ** 1.1)
    hiss = periodic_noise(n, lambda f: (f / 3500.0) / (1.0 + (f / 6500.0) ** 4) * (f > 1500))
    rumble = periodic_noise(n, lambda f: np.exp(-((f - 90.0) / 70.0) ** 2))
    body /= np.std(body)
    hiss /= np.std(hiss)
    rumble /= np.std(rumble)
    # the roar breathes: slow swell and a quick flutter (whole cycles in the loop: 0.8 s * 5 Hz = 4 cycles, and so on)
    flutter = 1 + 0.30 * np.sin(2 * np.pi * 5.0 * t + 0.7) + 0.22 * np.sin(2 * np.pi * 11.25 * t + 2.1) + 0.12 * np.sin(2 * np.pi * 26.25 * t)
    x = (body * 0.9 + hiss * 0.28) * flutter + rumble * 0.9
    # crackles: short bursts of high noise at random places (they wrap round the end, so the loop has no seam)
    for _ in range(26):
        pos = rng.integers(0, n)
        ln = int(rng.uniform(0.003, 0.012) * SR)
        burst = rng.normal(0, 1, ln) * np.exp(-np.arange(ln) / (ln / 4.0))
        b, a = signal.butter(2, 1500 / (SR / 2), "highpass")
        burst = signal.lfilter(b, a, burst) * rng.uniform(1.0, 3.2)
        idx = (pos + np.arange(ln)) % n
        x[idx] += burst
    x = np.tanh(x * 0.55)              # a little saturation glues it together
    return x


def flame_start():
    n = int(0.42 * SR)
    t = np.arange(n) / SR
    # the thump: a sine that falls from 110 to 38 Hz
    f = 38 + 72 * np.exp(-t / 0.07)
    thump = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / 0.10)
    # the whoosh: noise that swells quickly and fades into the loop's level
    nz = rng.normal(0, 1, n)
    b, a = signal.butter(2, [250 / (SR / 2), 3200 / (SR / 2)], "bandpass")
    nz = signal.lfilter(b, a, nz)
    env = (1 - np.exp(-t / 0.035)) * (0.45 + 0.55 * np.exp(-t / 0.16))
    x = thump * 1.0 + nz * env * 0.55
    x *= np.minimum(1.0, (n - np.arange(n)) / (0.05 * SR))      # fade out
    return np.tanh(x * 1.2)


def tyrant_roar():
    dur = 2.7
    n = int(dur * SR)
    t = np.arange(n) / SR
    # pitch: a sharp rise to 130 Hz, then a long fall to 52 Hz
    f0 = 52 + 80 * np.exp(-np.maximum(t - 0.12, 0) / 0.9)
    f0 = np.where(t < 0.12, 52 + 78 * (t / 0.12) ** 1.5, f0)
    f0 = f0 * (1 + 0.025 * np.sin(2 * np.pi * 6.5 * t) + 0.012 * rng.normal(0, 1, n).cumsum() / np.sqrt(np.arange(1, n + 1)) * 0.0)
    ph = np.cumsum(2 * np.pi * f0 / SR)
    # a rich source: harmonics up to 4 kHz, falling off gently, each slightly out of tune with the next
    src = np.zeros(n)
    for k in range(1, 36):
        if f0.max() * k > 4200:
            break
        src += np.sin(k * ph + rng.uniform(0, 6.28)) / (k ** 0.85)
    # growl: the voice is chopped by a fast, uneven tremolo
    trem = 0.55 + 0.45 * np.sign(np.sin(2 * np.pi * (24 + 6 * np.sin(2 * np.pi * 1.3 * t)) * t)) * 0.0 \
        + 0.45 * np.sin(2 * np.pi * (24 + 5 * np.sin(2 * np.pi * 1.7 * t)) * t)
    growl = src * np.clip(trem, 0.12, 1.0)
    # breath: noise through two formants, following the same envelope
    nz = rng.normal(0, 1, n)
    out = np.zeros(n)
    for fc, q, g in ((420, 3.0, 1.0), (900, 4.0, 0.7), (1700, 5.0, 0.3)):
        b, a = signal.iirpeak(fc / (SR / 2), q)
        out += signal.lfilter(b, a, growl * 0.8 + nz * 0.22) * g
    # envelope: swells in over 0.25 s, holds, then dies away
    env = np.minimum(1.0, t / 0.25) ** 1.5 * np.where(t < 1.5, 1.0, np.exp(-(t - 1.5) / 0.38))
    x = out * env
    # the sub-boom at the start
    boom = np.sin(2 * np.pi * np.cumsum(34 + 40 * np.exp(-t / 0.12)) / SR) * np.exp(-t / 0.35) * 2.2
    x = x / (np.std(x) + 1e-9) * 0.55 + boom
    x = np.tanh(x * 1.7)               # drive: the roar bites
    # a cavernous tail: three soft echoes
    y = x.copy()
    for d, g in ((0.061, 0.34), (0.097, 0.24), (0.151, 0.17), (0.233, 0.11)):
        k = int(d * SR)
        y[k:] += x[:-k] * g
    return y


def main():
    d = sys.argv[1]
    save(f"{d}/flame.dat", flame_loop(), 0.6)
    save(f"{d}/flame_start.dat", flame_start(), 0.7)
    save(f"{d}/tyrant_roar.dat", tyrant_roar(), 0.85)


if __name__ == "__main__":
    main()
