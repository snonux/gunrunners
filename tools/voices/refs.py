#!/usr/bin/env python3
"""The runners' reference voices for generate.py.

  refs.py OUT_DIR [voice ...]

A Kokoro-82M read of a neutral text per runner, then Chatterbox (the full
model, at high exaggeration) re-reads an excited text in that voice twice;
the two takes together are OUT_DIR/<voice>.wav, what Chatterbox Turbo
clones every line from.
"""
import os, sys

import numpy as np
import soundfile as sf

NEUTRAL = ("Listen up, because I am only going to say this once. We go in fast, we hit hard, "
           "and we get out before anybody knows what happened. Grab the prototype, find the exit, "
           "and do not stop running!")
# Paul (2026-10-15) wanted the lines almost screamed: shouted text, very high
# exaggeration and low cfg.
EXCITED = ("WOW! LOOK AT THAT!! THIS IS AMAZING!!! We are SO getting paid for this one!! COME ON, MOVE IT! "
           "GO, GO, GO!!! NOTHING is stopping us TODAY!!!")
# Rocco is the trailer voice: deep and deadpan rather than shouty.
# Rocco shouts too, but deep: much past exaggeration 1.25 his voice jumps an
# octave and loses the Duke sound.
DEADPAN = ("LISTEN UP!! I came here to grab the prototype and KICK SOME BUTT!! And I am ALL OUT of prototypes!!! "
           "LET'S ROCK!!!")

# voice: (Kokoro voice, speed, language, exaggeration, cfg weight)
VOICES = {
    "dash": ("am_puck", 1.0, "a", 1.7, 0.12),
    "rocco": ("am_onyx", 0.86, "a", 1.25, 0.3),
    "nova": ("af_nova", 1.0, "a", 1.7, 0.12),
    "jade": ("bf_isabella", 1.0, "b", 1.7, 0.12),
    "skye": ("af_sky", 1.05, "a", 1.7, 0.12),
    "bolt": ("am_michael", 0.95, "a", 1.3, 0.2),
}


def main():
    out = sys.argv[1]
    names = sys.argv[2:] or list(VOICES)
    os.makedirs(out, exist_ok=True)
    from kokoro import KPipeline
    import torch, perth
    from chatterbox.tts import ChatterboxTTS
    pipes = {}
    tts = ChatterboxTTS.from_pretrained(device="cuda" if torch.cuda.is_available() else "cpu")
    for name in names:
        kv, speed, lang, ex, cfg = VOICES[name]
        p = pipes.setdefault(lang, KPipeline(lang_code=lang))
        base = np.concatenate([a for _, _, a in p(NEUTRAL, voice=kv, speed=speed)])
        neutral = os.path.join(out, name + "_kokoro.wav")
        sf.write(neutral, base, 24000)
        takes = []
        for _ in range(2):
            wav = tts.generate(DEADPAN if name == "rocco" else EXCITED, audio_prompt_path=neutral,
                               exaggeration=ex, cfg_weight=cfg)
            takes.append(wav.squeeze(0).numpy())
            takes.append(np.zeros(int(0.3 * tts.sr), dtype=np.float32))
        sf.write(os.path.join(out, name + ".wav"), np.concatenate(takes), tts.sr)
        print(name, flush=True)


if __name__ == "__main__":
    main()
