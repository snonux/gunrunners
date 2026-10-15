"""The cutscene speakers' sound on top of the cloned voice. The runners
get exactly their in-game treatment (fx.py)."""
import numpy as np
from scipy.signal import butter, sosfilt

import fx

RATE = fx.RATE
RUNNERS = {"dash", "rocco", "nova", "jade", "skye", "bolt"}


def _pitch(wav, sr, factor):
    """Down (factor < 1) or up in pitch and pace together, to RATE."""
    return fx.resample(fx.resample(wav, sr, int(sr / factor)), sr, RATE)


def _echoes(wav, taps):
    out = np.copy(wav)
    for secs, gain in taps:
        d = int(secs * RATE)
        out[d:] += gain * wav[:-d]
    return out


def voice(wav, sr, who):
    wav = np.asarray(wav, dtype=np.float32)
    if who in RUNNERS:
        return fx.voice(wav, sr, who)
    if who == "max":
        # A hologram: lower, ring-modulated, with a short tin-can comb.
        wav = _pitch(wav, sr, 0.9)
        t = np.arange(len(wav)) / RATE
        wav = 0.75 * wav + 0.25 * wav * np.sin(2 * np.pi * 70 * t)
        wav = _echoes(wav, [(0.003, 0.35)])
        wav = sosfilt(butter(2, 140, "highpass", fs=RATE, output="sos"), wav)
    elif who == "zero":
        # The station AI: a touch lower, in a cold metal room.
        wav = _pitch(wav, sr, 0.96)
        wav = _echoes(wav, [(0.023, 0.28), (0.037, 0.2), (0.053, 0.14), (0.079, 0.09)])
    elif who == "signal":
        # A weak radio signal: band-limited, wavering, under hiss and crackle.
        wav = fx.resample(wav, sr, RATE)
        wav = sosfilt(butter(4, [380, 3000], "bandpass", fs=RATE, output="sos"), wav)
        wav = wav / (np.max(np.abs(wav)) + 1e-9)
        t = np.arange(len(wav)) / RATE
        wav = wav * (0.8 + 0.2 * np.sin(2 * np.pi * 6.5 * t))
        rng = np.random.default_rng(len(wav))
        hiss = sosfilt(butter(2, 2500, "highpass", fs=RATE, output="sos"), rng.standard_normal(len(wav)))
        crackle = (rng.random(len(wav)) < 0.0008) * rng.standard_normal(len(wav)) * 0.6
        wav = wav + 0.05 * hiss + crackle
        wav = np.round(wav * 64) / 64
    elif who == "announcer":
        # The MaxTV voice: chest, a little push and a big room.
        wav = fx.resample(wav, sr, RATE)
        wav = fx.biquad(wav / (np.max(np.abs(wav)) + 1e-9), *fx.low_shelf(RATE, 160, 3.0))
        wav = np.tanh(wav * 1.5) / np.tanh(1.5)
        wav = _echoes(wav, [(0.09, 0.14), (0.17, 0.07)])
    elif who == "board":
        wav = _pitch(wav, sr, 0.95)
    else:
        wav = fx.resample(wav, sr, RATE)
    return fx.normalize(np.asarray(wav, dtype=np.float32))
