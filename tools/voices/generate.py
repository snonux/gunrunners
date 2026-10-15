#!/usr/bin/env python3
"""Records voices/lines.txt in the runners' voices.

Needs (not part of the build): torch, chatterbox-tts, faster-whisper,
soundfile, numpy, ffmpeg. Every line is cloned with Chatterbox Turbo from
the runner's reference clip (refs/<voice>.wav, an excited read made with
Chatterbox at high exaggeration from a Kokoro-82M voice), checked with
faster-whisper and re-rolled when the words don't match, then given the
runner's sound (fx.py) and written as Ogg Vorbis.

  generate.py LINES.TXT REFS_DIR OUT_DIR [--voices a,b] [--only PREFIX]
              [--part K/N]
Files that already exist are kept, so a stopped run picks up where it was.
"""
import argparse, json, os, re, subprocess, sys, tempfile

import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lines import read_lines
import fx

TRIES = 4


def words(s):
    s = s.lower().replace("'", "")
    return re.findall(r"[a-z]+", s)


def match(want, got):
    """Share of the wanted words heard, in order (a longest common subsequence)."""
    a, b = words(want), words(got)
    if not a:
        return 0.0
    dp = [[0] * (len(b) + 1) for _ in range(len(a) + 1)]
    for i in range(len(a)):
        for j in range(len(b)):
            dp[i + 1][j + 1] = dp[i][j] + 1 if a[i] == b[j] else max(dp[i][j + 1], dp[i + 1][j])
    extra = max(0, len(b) - len(a))  # babble after the line
    return (dp[-1][-1] - 0.5 * extra) / len(a)


def shouted(text):
    """What the model reads: every sentence ends in an exclamation mark, so the
    delivery stays at a shout (the subtitles keep lines.txt's punctuation)."""
    return re.sub(r"[.]+(\s|$)", r"!\1", text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("lines")
    ap.add_argument("refs")
    ap.add_argument("out")
    ap.add_argument("--voices", default="")
    ap.add_argument("--only", default="")
    ap.add_argument("--part", default="1/1")
    ap.add_argument("--threads", type=int, default=4)
    ap.add_argument("--max", type=int, default=0, help="stop after this many clips (bounds memory; rerun to go on)")
    args = ap.parse_args()

    import torch
    torch.set_num_threads(args.threads)
    from chatterbox.tts_turbo import ChatterboxTurboTTS
    from faster_whisper import WhisperModel

    k, n = map(int, args.part.split("/"))
    want = set(filter(None, args.voices.split(",")))
    jobs = []
    for idx, (lid, text, voices) in enumerate(read_lines(args.lines)):
        if args.only and not lid.startswith(args.only):
            continue
        for v in voices:
            if want and v not in want:
                continue
            jobs.append((lid, text, v))
    jobs = [j for i, j in enumerate(jobs) if i % n == k - 1]
    jobs = [j for j in jobs if not os.path.exists(os.path.join(args.out, j[2], j[0] + ".ogg"))]
    print(f"{len(jobs)} clips to record", flush=True)
    if args.max:
        jobs = jobs[: args.max]
    if not jobs:
        return

    tts = ChatterboxTurboTTS.from_pretrained(device="cpu")
    asr = WhisperModel("base.en", device="cpu", compute_type="int8", cpu_threads=args.threads)
    conds = {}
    log = open(os.path.join(args.out, f"log_{k}of{n}.jsonl"), "a")
    for lid, text, voice in jobs:
        if voice not in conds:
            tts.prepare_conditionals(os.path.join(args.refs, voice + ".wav"))
            conds[voice] = tts.conds
        tts.conds = conds[voice]
        best = None
        for attempt in range(TRIES):
            wav = tts.generate(shouted(text)).squeeze(0).numpy()
            wav = fx.trim(wav, tts.sr)
            with tempfile.NamedTemporaryFile(suffix=".wav") as tmp:
                sf.write(tmp.name, wav, tts.sr)
                segs, _ = asr.transcribe(tmp.name, language="en", beam_size=1)
                heard = " ".join(s.text for s in segs).strip()
            score = match(text, heard)
            # Too long for the words is a model ramble even if they match.
            secs = len(wav) / tts.sr
            if secs > 1.2 + 0.55 * len(words(text)):
                score -= 0.5
            if best is None or score > best[0]:
                best = (score, wav, heard, attempt)
            if score >= 0.9:
                break
        score, wav, heard, attempt = best
        out = fx.voice(wav, tts.sr, voice)
        dest = os.path.join(args.out, voice, lid + ".ogg")
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        fx.write_ogg(out, fx.RATE, dest)
        log.write(json.dumps({"id": lid, "voice": voice, "text": text, "heard": heard, "score": round(score, 2),
                              "tries": attempt + 1, "secs": round(len(out) / fx.RATE, 2)}) + "\n")
        log.flush()
        print(f"{voice} {lid} {score:.2f} {heard}", flush=True)


if __name__ == "__main__":
    main()
