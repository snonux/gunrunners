#!/usr/bin/env python3
"""Reference voices for the cutscenes' speakers who aren't runners.

  cutscene_refs.py OUT_DIR [voice ...]

As refs.py: a Kokoro-82M read of a neutral text, then Chatterbox (the full
model) re-reads a text in the character's manner twice; the two takes are
OUT_DIR/<voice>.wav, what cutscene_generate.py clones every line from.
The runners' refs come from refs.py (the livelier v2 set), so a runner
sounds the same in cutscenes and in play.
"""
import os, sys

import numpy as np
import soundfile as sf

NEUTRAL = ("Listen up, because I am only going to say this once. We go in fast, we hit hard, "
           "and we get out before anybody knows what happened.")

# voice: (Kokoro voice, speed, language, exaggeration, cfg weight, text in the character's manner)
VOICES = {
    # MAX, the client: a cool, clipped hologram (fx: pitched down, ring modulated).
    "max": ("af_heart", 0.95, "a", 0.75, 0.3,
            "Your next package is waiting. The pay is excellent. The questions are not. Move fast, runners, "
            "and do not stop for anything!"),
    # Maxine Vale, the director, in her own voice: urgent, sure of herself.
    "maxine": ("af_heart", 1.0, "a", 1.0, 0.25,
               "Listen to me! The cameras are rolling, the whole world is watching, and you are going to give "
               "them a show they will never forget! Go, go, go!"),
    # ZERO, the station AI: calm, polite and sad (fx: a cold metal room).
    "zero": ("bm_lewis", 0.92, "b", 0.45, 0.45,
             "Good morning. The station is operating within normal parameters. Please remain calm. "
             "I am so very glad you are here."),
    # Lance Marquee, the host with the chrome teeth.
    "lance": ("am_adam", 1.0, "a", 1.25, 0.2,
              "Good evening ladies and gentlemen, and welcome to the biggest show on television! "
              "Give it up for our contestants! The ratings are through the roof tonight, folks!"),
    # S, a voice in the static (fx: a weak radio signal).
    "signal": ("af_river", 0.95, "a", 0.6, 0.35,
               "Can you hear me? Don't be afraid. Follow the signal. It will lead you home. Hurry!"),
    # The nature documentary's narrator: hushed wonder.
    "narrator": ("bm_george", 0.95, "b", 0.7, 0.3,
                 "And here, in the warm light of dawn, we find a truly remarkable creature. "
                 "Look at it. Simply magnificent. Absolutely extraordinary!"),
    # The network board, in the post-credits scene.
    "board": ("am_santa", 0.95, "a", 1.0, 0.25,
              "Gentlemen, the numbers are in, and they are spectacular! Get me the network on the phone, right now!"),
    # MaxTV's announcer: the big voice on the cards and idents.
    "announcer": ("am_fenrir", 0.95, "a", 1.2, 0.2,
                  "Coming up next on MaxTV! The most explosive hour of television ever made! "
                  "Stay tuned, don't touch that dial!"),
}


def main():
    out = sys.argv[1]
    names = sys.argv[2:] or list(VOICES)
    os.makedirs(out, exist_ok=True)
    from kokoro import KPipeline
    import torch, perth
    from chatterbox.tts import ChatterboxTTS
    torch.set_num_threads(4)
    pipes = {}
    tts = ChatterboxTTS.from_pretrained(device="cpu")
    for name in names:
        kv, speed, lang, ex, cfg, text = VOICES[name]
        p = pipes.setdefault(lang, KPipeline(lang_code=lang))
        base = np.concatenate([a for _, _, a in p(NEUTRAL, voice=kv, speed=speed)])
        neutral = os.path.join(out, name + "_kokoro.wav")
        sf.write(neutral, base, 24000)
        takes = []
        for _ in range(2):
            wav = tts.generate(text, audio_prompt_path=neutral, exaggeration=ex, cfg_weight=cfg)
            takes.append(wav.squeeze(0).numpy())
            takes.append(np.zeros(int(0.3 * tts.sr), dtype=np.float32))
        sf.write(os.path.join(out, name + ".wav"), np.concatenate(takes), tts.sr)
        print(name, flush=True)


if __name__ == "__main__":
    main()
