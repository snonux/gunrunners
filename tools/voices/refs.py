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
EXCITED = ("Oh man, look at that! We are so getting paid for this one. Come on, move it, let's go, let's go! "
           "Nothing is stopping us today!")
# Rocco is the trailer voice: deep and deadpan rather than shouty.
DEADPAN = ("Listen up. I came here to grab the prototype and kick some butt. And I am all out of prototypes. "
           "Let's rock.")

# voice: (Kokoro voice, speed, language, exaggeration, cfg weight)
VOICES = {
    "dash": ("am_puck", 1.0, "a", 0.9, 0.3),
    "rocco": ("am_onyx", 0.86, "a", 0.85, 0.25),
    "nova": ("af_nova", 1.0, "a", 0.9, 0.3),
    "jade": ("bf_isabella", 1.0, "b", 0.95, 0.3),
    "skye": ("af_sky", 1.05, "a", 0.9, 0.3),
    "bolt": ("am_michael", 0.95, "a", 0.6, 0.4),
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
