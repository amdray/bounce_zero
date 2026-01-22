#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
grid_watcher.py — наблюдатель буфера обмена (Windows)
Версия: matrix-dump-with-label (метка может быть строкой)
Зависимости: Pillow, pywin32
"""

import time
import hashlib
from pathlib import Path
from PIL import ImageGrab, Image

# Параметры сетки
LINE_W = 1
CELL_INNER = 7
STEP = LINE_W + CELL_INNER  # 8

POLL = 0.6
TOLERANCE = 0
DUMP_FILE = Path("glyphs.txt")

def is_black(pixel, tolerance=0):
    if isinstance(pixel, int):
        r = g = b = pixel
    else:
        if len(pixel) >= 3:
            r, g, b = pixel[:3]
        else:
            return False
    return r <= tolerance and g <= tolerance and b <= tolerance

def find_first_black(img, tolerance=0):
    w, h = img.size
    px = img.load()
    for y in range(h):
        for x in range(w):
            if is_black(px[x, y], tolerance=tolerance):
                return x, y
    return None

def run_length_right(img, x0, y0, tolerance=0):
    w, _ = img.size
    px = img.load()
    x = x0
    while x < w and is_black(px[x, y0], tolerance=tolerance):
        x += 1
    return x - x0

def run_length_down(img, x0, y0, tolerance=0):
    _, h = img.size
    px = img.load()
    y = y0
    while y < h and is_black(px[x0, y], tolerance=tolerance):
        y += 1
    return y - y0

def cells_from_length(length):
    if length <= 0:
        return 0, 0
    base = length - 1
    n = base // STEP
    rem = base % STEP
    return int(n), int(rem)

def to_ascii(matrix, on="█", off=" "):
    return "\n".join("".join(on if v else off for v in row) for row in matrix)

def to_bits_lines(matrix):
    return "\n".join("".join("1" if v else "0" for v in row) for row in matrix)

def open_image_from_file(path: Path):
    try:
        img = Image.open(path)
        return img.convert("RGB")
    except Exception:
        return None

def grab_clipboard_image():
    clip = ImageGrab.grabclipboard()
    if clip is None:
        return None
    if isinstance(clip, Image.Image):
        return clip.convert("RGB")
    if isinstance(clip, list):
        for p in clip:
            pth = Path(p)
            if pth.suffix.lower() in {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tif", ".tiff"}:
                try:
                    img = Image.open(pth)
                    return img.convert("RGB")
                except Exception:
                    pass
    return None

def img_hash(img: Image.Image) -> str:
    return hashlib.sha256(img.tobytes()).hexdigest()

def build_matrix_from_centers(img, x0, y0, n_x, n_y, tolerance=0):
    px = img.load()
    h = img.size[1]
    w = img.size[0]
    mx = [[0]*n_x for _ in range(n_y)]
    cx_off = LINE_W + (CELL_INNER // 2)
    cy_off = LINE_W + (CELL_INNER // 2)
    for r in range(n_y):
        y = y0 + r*STEP + cy_off
        if y < 0 or y >= h:
            continue
        for c in range(n_x):
            x = x0 + c*STEP + cx_off
            if x < 0 or x >= w:
                continue
            if is_black(px[x, y], tolerance=tolerance):
                mx[r][c] = 1
    return mx

def process(img):
    first = find_first_black(img, tolerance=TOLERANCE)
    if first is None:
        return {"err": f"Чёрные пиксели не найдены (tolerance={TOLERANCE})."}
    x0, y0 = first
    len_x = run_length_right(img, x0, y0, tolerance=TOLERANCE)
    len_y = run_length_down(img, x0, y0, tolerance=TOLERANCE)
    n_x, rem_x = cells_from_length(len_x)
    n_y, rem_y = cells_from_length(len_y)
    matrix = build_matrix_from_centers(img, x0, y0, n_x, n_y, tolerance=TOLERANCE)
    return {
        "x0": x0, "y0": y0,
        "len_x": len_x, "len_y": len_y,
        "n_x": n_x, "rem_x": rem_x,
        "n_y": n_y, "rem_y": rem_y,
        "matrix": matrix,
    }

def append_dump(label: str, matrix, dims, file_path: Path):
    rows, cols = dims
    ascii_art = to_ascii(matrix, on="█", off=" ")
    bits = to_bits_lines(matrix)
    header = f"// '{label}'\n"
    block = (
        header +
        "[ASCII]\n" + ascii_art + "\n" +
        "[BITS]\n" + bits + "\n" +
        "-"*60 + "\n"
    )
    with file_path.open("a", encoding="utf-8") as f:
        f.write(block)

def print_result(res):
    if "err" in res:
        print(res["err"]); return False
    print(f"Первая чёрная точка: x={res['x0']}, y={res['y0']}")
    print(f"Длина линии X: {res['len_x']} пикс.")
    print(f"Длина линии Y: {res['len_y']} пикс.")
    print(f"Ячеек по X: {res['n_x']}" + (f" (остаток {res['rem_x']} пикс.)" if res['rem_x'] else ""))
    print(f"Ячеек по Y: {res['n_y']}" + (f" (остаток {res['rem_y']} пикс.)" if res['rem_y'] else ""))
    print("Матрица (1=чёрный центр):")
    print(to_ascii(res["matrix"], on="█", off=" "))
    return True

def main():
    print("Ожидание изображения в буфере… (Ctrl+C для выхода)")
    last_h = None
    try:
        while True:
            try:
                img = grab_clipboard_image()
                if img is None:
                    time.sleep(POLL); continue
                h = img_hash(img)
                if h == last_h:
                    time.sleep(POLL); continue
                last_h = h
                print("\n" + "="*60)
                print(time.strftime("[%H:%M:%S] Получено новое изображение"))
                res = process(img)
                ok = print_result(res)
                if not ok:
                    continue
                try:
                    label = input("Введите символ/метку для этой матрицы (Enter — пропустить): ").strip()
                except EOFError:
                    label = ""
                if label:
                    append_dump(label, res["matrix"], (len(res["matrix"]), len(res["matrix"][0]) if res["matrix"] else 0), DUMP_FILE)
                    print(f"Сохранено в {DUMP_FILE.resolve()}")
                else:
                    print("Пропуск сохранения.")
            except Exception as e:
                print(f"[Ошибка] {e}")
                time.sleep(POLL)
    except KeyboardInterrupt:
        print("\nЗавершено пользователем.")
        try:
            input("Нажмите Enter для выхода...")
        except Exception:
            pass

if __name__ == "__main__":
    main()
