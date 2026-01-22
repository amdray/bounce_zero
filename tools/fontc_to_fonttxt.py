#!/usr/bin/env python3
"""Generate new human-friendly font txt from src/font*.c tables."""
import re
from pathlib import Path

MAP_RANGE_END = 0x0451

# Mapping from table index to Unicode codepoint

def index_to_codepoint(idx):
    if idx <= 127:
        return idx
    if idx == 134:
        return 0x0401  # Ё
    if idx == 167:
        return 0x0451  # ё
    if 128 <= idx <= 133:
        return 0x0410 + (idx - 128)  # А-Е
    if 135 <= idx <= 160:
        return 0x0416 + (idx - 135)  # Ж-Я
    if 161 <= idx <= 166:
        return 0x0430 + (idx - 161)  # а-е
    if 168 <= idx <= 193:
        return 0x0436 + (idx - 168)  # ж-я
    return None


def parse_font_c(path):
    src = Path(path).read_text(encoding='utf-8')
    entries = []
    for m in re.finditer(r"\{\{([^}]*)\},\s*(\d+)\}\,", src):
        rows = [int(x.strip(), 16) for x in m.group(1).split(',')]
        width = int(m.group(2))
        entries.append((rows, width))
    return entries


def bits_to_lines(rows, width, bit_width):
    lines = []
    for row in rows:
        line = []
        for col in range(width):
            bit = 1 << (bit_width - 1 - col)
            line.append('1' if (row & bit) else '0')
        lines.append(''.join(line))
    return lines


def write_font_txt(out_path, name, height, entries, bit_width):
    # Map entries to codepoints
    glyphs = {}
    for idx, (rows, width) in enumerate(entries[:194]):
        cp = index_to_codepoint(idx)
        if cp is None:
            continue
        glyphs[cp] = (rows, width)

    out = []
    out.append(f"font name={name} height={height}")
    out.append(f"range U+0000..U+{MAP_RANGE_END:04X}")
    out.append("default U+0020")
    out.append("")

    for cp in sorted(glyphs.keys()):
        rows, width = glyphs[cp]
        # Ensure height matches
        if len(rows) != height:
            raise SystemExit(f"{name}: height mismatch for U+{cp:04X}")
        out.append(f"glyph U+{cp:04X} width={width}")
        out.extend(bits_to_lines(rows, width, bit_width))
        out.append("end")
        out.append("")

    Path(out_path).write_text('\n'.join(out), encoding='utf-8')


def main():
    write_font_txt('fonts/font9.txt', 'font9', 9, parse_font_c('src/font9.c'), 16)
    write_font_txt('fonts/font12.txt', 'font12', 12, parse_font_c('src/font12.c'), 16)
    write_font_txt('fonts/font23.txt', 'font23', 23, parse_font_c('src/font23.c'), 32)


if __name__ == '__main__':
    main()
