"""Reads voices/lines.txt the way the game does (src/frontend/voices.cpp):
yields (id, text, voices) for every line that needs a recording."""

VOICES = ["dash", "rocco", "nova", "jade", "skye", "bolt"]


def read_lines(path):
    out = []
    section = kind = name = None
    count = 0
    with open(path, encoding="utf-8") as f:
        for raw in f:
            s = raw.strip()
            if not s or s.startswith("#"):
                continue
            if s.startswith("["):
                kind, name = s[1:-1].split()[:2]
                count = 0
                continue
            if "|" not in s:
                continue
            count += 1
            text = s.split("|", 1)[1].strip()
            base = name if kind == "level" else f"{kind}_{name}"
            voices = [name] if kind == "runner" else VOICES
            out.append((f"{base}_{count}", text, voices))
    return out
