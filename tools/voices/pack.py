#!/usr/bin/env python3
"""Packs the recorded lines into the game's voice banks.

  pack.py LINES.TXT MASTERS_DIR OUT_DIR

MASTERS_DIR/<voice>/<id>.ogg are generate.py's takes. Every voice becomes
one Ogg Vorbis stream, OUT_DIR/<voice>.ogg (mono, 22.05 kHz, ~20 kbit/s:
one file, so the codec's headers are paid once, not per line), and an
index OUT_DIR/<voice>.txt with a line per clip: `<id> <first sample>
<samples>` at the bank's rate. The game seeks straight to a clip.
"""
import os, sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lines import read_lines, VOICES
import fx

BANK_RATE = 22050
GAP = 0.15  # seconds of silence between clips, so no clip bleeds into the next


def main():
    lines, masters, out = sys.argv[1:4]
    entries = read_lines(lines)
    missing = 0
    for voice in VOICES:
        parts, index, at = [], [], 0
        gap = np.zeros(int(GAP * BANK_RATE), dtype=np.float32)
        for lid, _, voices in entries:
            if voice not in voices:
                continue
            path = os.path.join(masters, voice, lid + ".ogg")
            if not os.path.exists(path):
                missing += 1
                continue
            wav = fx.resample(fx.read_ogg(path), fx.RATE, BANK_RATE)
            index.append(f"{lid} {at} {len(wav)}")
            parts += [wav, gap]
            at += len(wav) + len(gap)
        if not parts:
            continue
        fx.write_ogg(np.concatenate(parts), BANK_RATE, os.path.join(out, voice + ".ogg"), bitrate="20k")
        with open(os.path.join(out, voice + ".txt"), "w") as f:
            f.write(f"# {voice}: <id> <first sample> <samples> at {BANK_RATE} Hz in {voice}.ogg (tools/voices/pack.py)\n")
            f.write("\n".join(index) + "\n")
        print(voice, len(index), "clips", round(at / BANK_RATE), "s", flush=True)
    if missing:
        print(missing, "clips not recorded yet", flush=True)


if __name__ == "__main__":
    main()
