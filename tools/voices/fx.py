"""Each runner's sound on top of the cloned voice, trimming and encoding."""
import os, subprocess, tempfile

import numpy as np
import soundfile as sf

RATE = 24000  # what the game gets (it resamples to 48 kHz)


def trim(wav, sr, floor_db=-42.0, pad=0.06):
    """Cuts the silence before and after the line."""
    if len(wav) == 0:
        return wav
    hop = int(sr * 0.01)
    frames = [np.sqrt(np.mean(wav[i:i + hop] ** 2) + 1e-12) for i in range(0, len(wav), hop)]
    db = 20 * np.log10(np.array(frames) + 1e-9)
    loud = np.where(db > floor_db)[0]
    if len(loud) == 0:
        return wav
    a = max(0, loud[0] * hop - int(pad * sr))
    b = min(len(wav), (loud[-1] + 1) * hop + int(pad * sr))
    return wav[a:b]


def resample(wav, sr, to):
    if sr == to:
        return wav
    n = int(round(len(wav) * to / sr))
    return np.interp(np.linspace(0, len(wav) - 1, n), np.arange(len(wav)), wav).astype(np.float32)


def biquad(wav, b, a):
    y = np.zeros_like(wav)
    x1 = x2 = y1 = y2 = 0.0
    b0, b1, b2 = b
    _, a1, a2 = a
    for i, x in enumerate(wav):
        v = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, x, y1, v
        y[i] = v
    return y


def low_shelf(sr, f0, gain_db):
    A = 10 ** (gain_db / 40)
    w = 2 * np.pi * f0 / sr
    al = np.sin(w) / 2 * np.sqrt(2)
    c = np.cos(w)
    b0 = A * ((A + 1) - (A - 1) * c + 2 * np.sqrt(A) * al)
    b1 = 2 * A * ((A - 1) - (A + 1) * c)
    b2 = A * ((A + 1) - (A - 1) * c - 2 * np.sqrt(A) * al)
    a0 = (A + 1) + (A - 1) * c + 2 * np.sqrt(A) * al
    a1 = -2 * ((A - 1) + (A + 1) * c)
    a2 = (A + 1) + (A - 1) * c - 2 * np.sqrt(A) * al
    return (b0 / a0, b1 / a0, b2 / a0), (1.0, a1 / a0, a2 / a0)


def normalize(wav, peak=0.89, rms=0.16):
    r = np.sqrt(np.mean(wav ** 2) + 1e-12)
    wav = wav * min(rms / r, peak / (np.max(np.abs(wav)) + 1e-9))
    return np.clip(wav, -0.99, 0.99).astype(np.float32)


def voice(wav, sr, who):
    wav = np.asarray(wav, dtype=np.float32)
    if who == "rocco":
        # The trailer voice: down 8% (pitch and pace), chest and gravel.
        wav = resample(wav, sr, int(sr / 0.92))
        wav = resample(wav, sr, RATE) if sr != RATE else wav
        wav = biquad(wav / (np.max(np.abs(wav)) + 1e-9), *low_shelf(RATE, 180, 4.0))
        wav = np.tanh(wav * 1.8) / np.tanh(1.8)
    elif who == "bolt":
        wav = resample(wav, sr, RATE)
        t = np.arange(len(wav)) / RATE
        # Ring modulation for the metal, a short comb for the tin can, a
        # little bit-crush for the circuits.
        ring = wav * np.sin(2 * np.pi * 55 * t)
        wav = 0.6 * wav + 0.4 * ring
        d = int(0.0045 * RATE)
        comb = np.copy(wav)
        comb[d:] += 0.5 * wav[:-d]
        wav = np.round(comb * 128) / 128
    else:
        wav = resample(wav, sr, RATE)
    return normalize(wav)


def write_ogg(wav, sr, dest, quality="5", rate=None, bitrate=None):
    """Ogg Vorbis: the masters at high quality, the game's banks small."""
    with tempfile.NamedTemporaryFile(suffix=".wav") as tmp:
        sf.write(tmp.name, wav, sr, subtype="PCM_16")
        enc = ["-b:a", bitrate] if bitrate else ["-q:a", quality]
        subprocess.run(["ffmpeg", "-nostdin", "-loglevel", "error", "-y", "-i", tmp.name, "-ac", "1", "-ar",
                        str(rate or sr), "-c:a", "libvorbis", *enc, dest], check=True)


def read_ogg(path):
    """Mono floats at RATE."""
    with tempfile.NamedTemporaryFile(suffix=".wav") as tmp:
        subprocess.run(["ffmpeg", "-nostdin", "-loglevel", "error", "-y", "-i", path, "-ac", "1", "-ar", str(RATE),
                        tmp.name], check=True)
        wav, _ = sf.read(tmp.name, dtype="float32")
    return wav
