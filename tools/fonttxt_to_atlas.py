#!/usr/bin/env python3
"""Generate T4 font atlas C/H from new font txt format."""
import math
import re
from pathlib import Path

PAGE_SIZE = 512  # PSP-safe power-of-two


def parse_header_kv(line):
    parts = line.split()
    kv = {}
    for p in parts[1:]:
        if '=' in p:
            k, v = p.split('=', 1)
            kv[k.strip()] = v.strip()
    return kv


def parse_codepoint(token):
    if not token.startswith('U+'):
        raise ValueError(f"Bad codepoint token: {token}")
    return int(token[2:], 16)


def parse_font_txt(path):
    lines = Path(path).read_text(encoding='utf-8').splitlines()
    header = {}
    glyphs = {}

    i = 0
    while i < len(lines):
        raw = lines[i]
        line = raw.strip()
        i += 1
        if not line or line.startswith('#'):
            continue
        if line.startswith('font '):
            header.update(parse_header_kv(line))
            continue
        if line.startswith('range '):
            m = re.match(r"range\s+U\+([0-9A-Fa-f]+)\.\.U\+([0-9A-Fa-f]+)", line)
            if not m:
                raise SystemExit(f"Bad range line: {line}")
            header['range_start'] = int(m.group(1), 16)
            header['range_end'] = int(m.group(2), 16)
            continue
        if line.startswith('default '):
            parts = line.split()
            if len(parts) != 2:
                raise SystemExit(f"Bad default line: {line}")
            header['default'] = parse_codepoint(parts[1])
            continue
        if line.startswith('glyph '):
            parts = line.split()
            if len(parts) < 2:
                raise SystemExit(f"Bad glyph line: {line}")
            cp = parse_codepoint(parts[1])
            width = None
            for p in parts[2:]:
                if p.startswith('width='):
                    width = int(p.split('=', 1)[1])
            bitmap = []
            while i < len(lines):
                l = lines[i].strip()
                i += 1
                if not l or l.startswith('#'):
                    continue
                if l == 'end':
                    break
                l = l.replace('.', '0').replace('#', '1')
                if not re.match(r'^[01]+$', l):
                    raise SystemExit(f"Bad bitmap line: {l}")
                bitmap.append(l)
            if cp in glyphs:
                raise SystemExit(f"Duplicate glyph U+{cp:04X} in {path}")
            glyphs[cp] = {'width': width, 'bitmap': bitmap}
            continue
        raise SystemExit(f"Unknown line: {line}")

    # Validate header
    if 'height' not in header:
        raise SystemExit(f"Missing height in {path}")
    height = int(header['height'])
    range_start = int(header.get('range_start', 0))
    range_end = int(header.get('range_end', 0))
    default_cp = int(header.get('default', 0x20))

    # Validate glyphs
    for cp, g in glyphs.items():
        if len(g['bitmap']) != height:
            raise SystemExit(f"U+{cp:04X}: expected {height} rows, got {len(g['bitmap'])}")
        # If width not set, compute from max line length
        if g['width'] is None:
            g['width'] = max(len(row) for row in g['bitmap']) if g['bitmap'] else 0
        # Ensure line length matches width
        for row in g['bitmap']:
            if len(row) != g['width']:
                raise SystemExit(f"U+{cp:04X}: line length {len(row)} != width {g['width']}")

    return {
        'name': header.get('name', Path(path).stem),
        'height': height,
        'range_start': range_start,
        'range_end': range_end,
        'default_cp': default_cp,
        'glyphs': glyphs,
    }


def pack_pixel_t4(buf, page_w, x, y, value):
    idx = y * page_w + x
    b = idx >> 1
    if idx & 1:
        # high nibble
        buf[b] = (buf[b] & 0x0F) | ((value & 0x0F) << 4)
    else:
        # low nibble
        buf[b] = (buf[b] & 0xF0) | (value & 0x0F)


def emit_c_array(data, indent='    '):
    lines = []
    line = indent
    for i, b in enumerate(data):
        token = f"0x{b:02X}, "
        if len(line) + len(token) > 100:
            lines.append(line.rstrip())
            line = indent + token
        else:
            line += token
    if line.strip():
        lines.append(line.rstrip())
    return '\n'.join(lines)


def generate_atlas(font, out_c, out_h):
    name = font['name']
    height = font['height']
    start = font['range_start']
    end = font['range_end']
    default_cp = font['default_cp']
    glyphs = font['glyphs']

    max_w = max((g['width'] for g in glyphs.values()), default=1)
    cell_w = max_w
    cell_h = height

    cols = PAGE_SIZE // cell_w
    rows = PAGE_SIZE // cell_h
    if cols <= 0 or rows <= 0:
        raise SystemExit(f"Cell {cell_w}x{cell_h} doesn't fit into {PAGE_SIZE}x{PAGE_SIZE}")
    cells_per_page = cols * rows

    total_cells = end - start + 1
    page_count = math.ceil(total_cells / cells_per_page)

    # Build pages
    pages = [bytearray((PAGE_SIZE * PAGE_SIZE) // 2) for _ in range(page_count)]
    glyph_table = []

    for cp in range(start, end + 1):
        idx = cp - start
        page = idx // cells_per_page
        cell = idx % cells_per_page
        col = cell % cols
        row = cell // cols
        x0 = col * cell_w
        y0 = row * cell_h

        g = glyphs.get(cp)
        if g is None:
            glyph_table.append((0, 0, 0, 0, 0))
            continue

        width = g['width']
        # Draw glyph
        for y, row_bits in enumerate(g['bitmap']):
            for x, ch in enumerate(row_bits):
                if ch == '1':
                    pack_pixel_t4(pages[page], PAGE_SIZE, x0 + x, y0 + y, 1)

        glyph_table.append((x0, y0, width, height, page))

    # Write header
    h_lines = []
    h_lines.append('#ifndef FONT_ATLAS_' + name.upper() + '_H')
    h_lines.append('#define FONT_ATLAS_' + name.upper() + '_H')
    h_lines.append('')
    h_lines.append('#include "font_atlas.h"')
    h_lines.append('')
    h_lines.append('const FontAtlas* ' + name + '_get_atlas(void);')
    h_lines.append('')
    h_lines.append('#endif')
    Path(out_h).write_text('\n'.join(h_lines), encoding='utf-8')

    # Write C
    c = []
    c.append('#include "' + Path(out_h).name + '"')
    c.append('')
    for i, data in enumerate(pages):
        c.append(f'static const unsigned char {name}_atlas_page{i}[] __attribute__((aligned(16))) = {{')
        c.append(emit_c_array(data))
        c.append('};')
        c.append('')

    c.append(f'static const texture_t {name}_pages[] = {{')
    for i in range(page_count):
        c.append(f'    {{ (void*){name}_atlas_page{i}, {PAGE_SIZE}, {PAGE_SIZE}, {PAGE_SIZE}, {PAGE_SIZE}, GU_PSM_T4, 0 }},')
    c.append('};')
    c.append('')

    c.append(f'static const FontGlyph {name}_glyphs[{len(glyph_table)}] = {{')
    for x, y, w, h, page in glyph_table:
        c.append(f'    {{ {x}, {y}, {w}, {h}, {page} }},')
    c.append('};')
    c.append('')

    c.append(f'static const FontAtlas {name}_atlas = {{')
    c.append(f'    {name}_pages, {page_count}, {PAGE_SIZE}, {PAGE_SIZE}, {cell_h},')
    c.append(f'    {start}, {end}, {default_cp}, {name}_glyphs')
    c.append('};')
    c.append('')

    c.append(f'const FontAtlas* {name}_get_atlas(void) {{')
    c.append(f'    return &{name}_atlas;')
    c.append('}')
    c.append('')

    Path(out_c).write_text('\n'.join(c), encoding='utf-8')


def main():
    fonts = [
        ('fonts/font9.txt', 'src/font9_atlas.c', 'src/font9_atlas.h'),
        ('fonts/font12.txt', 'src/font12_atlas.c', 'src/font12_atlas.h'),
        ('fonts/font23.txt', 'src/font23_atlas.c', 'src/font23_atlas.h'),
    ]
    for src, out_c, out_h in fonts:
        font = parse_font_txt(src)
        generate_atlas(font, out_c, out_h)


if __name__ == '__main__':
    main()
