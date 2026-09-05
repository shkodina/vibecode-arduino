from __future__ import annotations

import re
from pathlib import Path

_MAP = {
    "а": "a",
    "б": "b",
    "в": "v",
    "г": "g",
    "д": "d",
    "е": "e",
    "ё": "yo",
    "ж": "zh",
    "з": "z",
    "и": "i",
    "й": "y",
    "к": "k",
    "л": "l",
    "м": "m",
    "н": "n",
    "о": "o",
    "п": "p",
    "р": "r",
    "с": "s",
    "т": "t",
    "у": "u",
    "ф": "f",
    "х": "h",
    "ц": "ts",
    "ч": "ch",
    "ш": "sh",
    "щ": "sch",
    "ъ": "",
    "ы": "y",
    "ь": "",
    "э": "e",
    "ю": "yu",
    "я": "ya",
}


def translit_stem(name: str) -> str:
    out = []
    for ch in name.lower().replace(" ", "-"):
        if ch in _MAP:
            out.append(_MAP[ch])
        elif ch.isalnum() or ch in "-_":
            out.append(ch)
        else:
            out.append("-")
    slug = re.sub(r"[-_]+", "-", "".join(out)).strip("-")
    return slug


def module_filename(original: str | None) -> str:
    stem = translit_stem(Path(original or "").stem)
    return f"{stem or 'sound'}.wav"
