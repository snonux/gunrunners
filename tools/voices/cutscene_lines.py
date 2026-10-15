"""Reads the spoken lines out of cutscenes/*.txt the way the game does
(src/frontend/cutscene.cpp, cutscene_voices.cpp): every `say` and `voice`
cue, with the clip id the game looks up."""
import glob, os, re

# Speaker -> voice, as CutsceneVoices::voiceOf. The runners use the voices
# of their in-game lines (refs from generate.py's REFS_DIR).
VOICES = {
    "DASH": "dash", "ROCCO": "rocco", "NOVA": "nova", "MAX": "max", "MAXINE": "maxine", "ZERO": "zero",
    "LANCE": "lance", "S": "signal", "NARRATOR": "narrator", "BOARD": "board", "ANNOUNCER": "announcer",
}

# What the voice actually reads where the subtitle's spelling would trip
# the speech model (keyed by the subtitle text).
SPOKEN = {
    "...How far up?": "How far up?",
    "...Of course he did.": "Of course he did.",
    "Mwa-ha-ha. Ratings!": "Mwa ha ha ha! Ratings!",
    "...can... hear... me...": "Can... hear... me?",
    "The east district. Four sectors, one grid, and the package is-": "The east district. Four sectors, one grid, and the package is",
    "Hey! Get your hands-": "Hey! Get your hands",
    "Wheee! Okay, that was fun.": "Whee! Okay, that was fun!",
    "AND THAT'S A WRAP ON SEASON ONE OF... GUNRUNNERS!": "And that's a wrap on season one of... Gunrunners!",
    "Cut to commercial! CUT TO COMMERCIAL!": "Cut to commercial! Cut to commercial!",
    "Tonight, YOU are the monster!": "Tonight, you are the monster!",
    "Is that... a queen? On a throne made of eggs?": "Is that a queen? On a throne made of eggs?",
    "...and we're back!": "And we're back!",
}


def spoken(text):
    return SPOKEN.get(text, text).replace("MaxTV", "Max TV")


def fnv1a(s):
    h = 2166136261
    for b in s.encode("utf-8"):
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def clip_id(cutscene, speaker, text):
    return f"{cutscene}_{fnv1a(speaker + '|' + text):08x}"


CUE = re.compile(r'^\s*cue\s+\d+\s+(say|voice)=(\S+)\s*(.*)$')


def read_cutscenes(cutscene_dir):
    """[(id, cutscene, speaker, voice, text, spoken)] in file order, each id once."""
    out, seen = [], set()
    for path in sorted(glob.glob(os.path.join(cutscene_dir, "*.txt"))):
        name = None
        for raw in open(path, encoding="utf-8"):
            s = raw.strip()
            if s.startswith("cutscene "):
                name = s.split()[1]
                continue
            m = CUE.match(raw)
            if not m or not name:
                continue
            speaker, rest = m.group(2), m.group(3)
            q1, q2 = rest.find('"'), rest.rfind('"')
            if q1 < 0 or q2 <= q1:
                continue
            text = rest[q1 + 1:q2]
            voice = VOICES.get(speaker)
            lid = clip_id(name, speaker, text)
            if not voice or lid in seen:
                continue
            seen.add(lid)
            out.append((lid, name, speaker, voice, text, spoken(text)))
    return out
