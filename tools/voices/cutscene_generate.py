#!/usr/bin/env python3
"""Records the cutscenes' spoken lines (every `say` and `voice` cue in
cutscenes/*.txt) in their speakers' voices.

As generate.py: each line is cloned with Chatterbox Turbo from
REFS_DIR/<voice>.wav (the runners' refs from refs.py, the others' from
cutscene_refs.py), checked with faster-whisper and re-rolled when the
words don't match, given the speaker's sound (cutscene_fx.py) and written
as OUT_DIR/<voice>/<id>.ogg.

  cutscene_generate.py CUTSCENES_DIR REFS_DIR OUT_DIR [--voices a,b]
                       [--only PREFIX] [--part K/N] [--threads N]
Files that already exist are kept; delete one to record it again.
"""
import argparse, json, os, sys, tempfile

import soundfile as sf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cutscene_lines import read_cutscenes
from generate import match, words
import cutscene_fx
import fx

TRIES = 5


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cutscenes")
    ap.add_argument("refs")
    ap.add_argument("out")
    ap.add_argument("--voices", default="")
    ap.add_argument("--only", default="")
    ap.add_argument("--part", default="1/1")
    ap.add_argument("--threads", type=int, default=2)
    args = ap.parse_args()

    import torch
    torch.set_num_threads(args.threads)
    from chatterbox.tts_turbo import ChatterboxTurboTTS
    from faster_whisper import WhisperModel

    k, n = map(int, args.part.split("/"))
    want = set(filter(None, args.voices.split(",")))
    jobs = [j for j in read_cutscenes(args.cutscenes)
            if (not want or j[3] in want) and (not args.only or j[0].startswith(args.only))]
    jobs = [j for i, j in enumerate(jobs) if i % n == k - 1]
    jobs = [j for j in jobs if not os.path.exists(os.path.join(args.out, j[3], j[0] + ".ogg"))]
    print(f"{len(jobs)} clips to record", flush=True)
    if not jobs:
        return

    tts = ChatterboxTurboTTS.from_pretrained(device="cpu")
    asr = WhisperModel("base.en", device="cpu", compute_type="int8", cpu_threads=args.threads)
    conds = {}
    log = open(os.path.join(args.out, f"log_{k}of{n}.jsonl"), "a")
    for lid, name, speaker, voice, text, spoken in jobs:
        if voice not in conds:
            tts.prepare_conditionals(os.path.join(args.refs, voice + ".wav"))
            conds[voice] = tts.conds
        tts.conds = conds[voice]
        best = None
        for attempt in range(TRIES):
            wav = tts.generate(spoken).squeeze(0).numpy()
            wav = fx.trim(wav, tts.sr)
            with tempfile.NamedTemporaryFile(suffix=".wav") as tmp:
                sf.write(tmp.name, wav, tts.sr)
                segs, _ = asr.transcribe(tmp.name, language="en", beam_size=1)
                heard = " ".join(s.text for s in segs).strip()
            score = match(spoken, heard)
            secs = len(wav) / tts.sr
            if secs > 1.2 + 0.55 * len(words(spoken)):
                score -= 0.5
            if best is None or score > best[0]:
                best = (score, wav, heard, attempt)
            if score >= 0.95:
                break
        score, wav, heard, attempt = best
        out = cutscene_fx.voice(wav, tts.sr, voice)
        dest = os.path.join(args.out, voice, lid + ".ogg")
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        fx.write_ogg(out, fx.RATE, dest)
        log.write(json.dumps({"id": lid, "voice": voice, "text": spoken, "heard": heard, "score": round(score, 2),
                              "tries": attempt + 1, "secs": round(len(out) / fx.RATE, 2)}) + "\n")
        log.flush()
        print(f"{voice} {lid} {score:.2f} {heard}", flush=True)


if __name__ == "__main__":
    main()
