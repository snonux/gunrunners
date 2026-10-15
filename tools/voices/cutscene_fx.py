"""The cutscene speakers' sound on top of the cloned voice. The runners
get exactly their in-game treatment (fx.py)."""
import numpy as np
from scipy.signal import butter, sosfilt

import fx

RATE = fx.RATE
RUNNERS = {"dash", "rocco", "nova", "jade", "skye", "bolt"}
# Paul (2026-10-15): "almost screamed". Everyone shouts but ZERO (a calm AI)
# and S (a whisper in the static); as the runners' in-game lines.
CALM = {"zero", "signal"}


def shouted(text, who):
    """What the speech model reads: a line that ends in a full stop ends
    in "!" instead, so it comes out shouted."""
    if who in CALM:
        return text
    out = []
    for part in text.split(" "):
        out.append(part[:-1] + "!" if part.endswith(".") and not part.endswith("..") else part)
    return " ".join(out)


def _peak(sr, f0, gain_db, q=1.0):
    A = 10 ** (gain_db / 40)
    w = 2 * np.pi * f0 / sr
    al = np.sin(w) / (2 * q)
    c = np.cos(w)
    a0 = 1 + al / A
    return ((1 + al * A) / a0, -2 * c / a0, (1 - al * A) / a0), (1.0, -2 * c / a0, (1 - al / A) / a0)


def shout(wav, who):
    """The runners' shout stage: a touch faster and higher (not Rocco or the
    pitched voices), less boom, more bite, driven, and louder."""
    if who in RUNNERS - {"rocco"} or who in {"lance", "maxine", "narrator"}:
        wav = fx.resample(wav, RATE, int(RATE / 1.04))
    wav = fx.biquad(wav, *fx.low_shelf(RATE, 200, -3.0))
    wav = fx.biquad(wav, *_peak(RATE, 2800, 5.0))
    wav = wav / (np.max(np.abs(wav)) + 1e-9)
    wav = np.tanh(wav * 2.4) / np.tanh(2.4)
    return fx.normalize(np.asarray(wav, dtype=np.float32), rms=0.19)


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
        return shout(fx.voice(wav, sr, who), who)
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
    wav = fx.normalize(np.asarray(wav, dtype=np.float32))
    return wav if who in CALM else shout(wav, who)
