#!/usr/bin/env python3
"""Packs the cutscenes' recorded lines into the game's banks.

  cutscene_pack.py CUTSCENES_DIR MASTERS_DIR OUT_DIR

As pack.py: every voice becomes one Ogg Vorbis stream OUT_DIR/<voice>.ogg
(mono, 22.05 kHz, ~20 kbit/s) and an index OUT_DIR/<voice>.txt of
`<id> <first sample> <samples>`. Only lines the cutscenes still say are
packed, so an edited or removed line's old clip drops out.
"""
import os, sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cutscene_lines import read_cutscenes, VOICES
import fx

BANK_RATE = 22050
GAP = 0.15


def main():
    cutscenes, masters, out = sys.argv[1:4]
    entries = read_cutscenes(cutscenes)
    os.makedirs(out, exist_ok=True)
    missing = 0
    for voice in sorted(set(VOICES.values())):
        parts, index, at = [], [], 0
        gap = np.zeros(int(GAP * BANK_RATE), dtype=np.float32)
        for lid, _, _, v, _, _ in entries:
            if v != voice:
                continue
            path = os.path.join(masters, voice, lid + ".ogg")
            if not os.path.exists(path):
                missing += 1
                continue
            wav = fx.resample(fx.read_ogg(path), fx.RATE, BANK_RATE)
            index.append(f"{lid} {at} {len(wav)}")
            parts += [wav, gap]
            at += len(wav) + len(gap)
        for ext in ("ogg", "txt"):
            stale = os.path.join(out, f"{voice}.{ext}")
            if not parts and os.path.exists(stale):
                os.remove(stale)
        if not parts:
            continue
        fx.write_ogg(np.concatenate(parts), BANK_RATE, os.path.join(out, voice + ".ogg"), bitrate="20k")
        with open(os.path.join(out, voice + ".txt"), "w") as f:
            f.write(f"# {voice}: <id> <first sample> <samples> at {BANK_RATE} Hz in {voice}.ogg "
                    "(tools/voices/cutscene_pack.py)\n")
            f.write("\n".join(index) + "\n")
        print(voice, len(index), "clips", round(at / BANK_RATE), "s", flush=True)
    if missing:
        print(missing, "clips not recorded yet", flush=True)


if __name__ == "__main__":
    main()
