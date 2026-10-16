"""Download official club-site logo files and append 40x40 OLED bitmaps.

The script keeps the existing header format (1-bit MSB-first, 5 bytes per row).
Run from the project root with:
  uv run --with pillow --with requests python tools/add_missing_team_logos.py
"""
from __future__ import annotations

from io import BytesIO
from pathlib import Path

import requests
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "include" / "team_logos.h"
LOGOS = {
    "MBL": "https://bkboleslav.esports.cz/files/logos/MBL_t.png",
    "SPA": "https://hokejkv.esports.cz/files/logos/hc_sparta_logo_TISK_2022_23.png",
    "CEB": "https://hokejkv.esports.cz/files/logos/motor_ceskebudejovice_logo_TISK_2022_23.png",
    "KVA": "https://bkboleslav.esports.cz/files/logos/kva_svetle.png",
}
W = H = 40
HEADERS = {"User-Agent": "Litvinov-OLED-logo-builder/1.0"}


def bitmap_bytes(url: str) -> list[int]:
    response = requests.get(url, headers=HEADERS, timeout=30)
    response.raise_for_status()
    source = Image.open(BytesIO(response.content)).convert("RGBA")
    alpha = source.getchannel("A")
    bbox = alpha.getbbox()
    if bbox is None:
        raise RuntimeError(f"Logo at {url} has no visible pixels")
    source = source.crop(bbox)
    alpha = source.getchannel("A")
    rgb = Image.new("RGB", source.size, "white")
    rgb.paste(source, mask=alpha)
    gray = ImageOps.grayscale(rgb)

    # Preserve dark/coloured strokes; alpha also preserves white club marks such
    # as the KVA mark. A tiny dilation makes 40px OLED artwork legible.
    mask = gray.point(lambda px: 255 if px < 224 else 0)
    if mask.getbbox() is None:
        mask = alpha.point(lambda px: 255 if px > 40 else 0)
    canvas = Image.new("1", (W, H), 0)
    mask.thumbnail((38, 38), Image.Resampling.LANCZOS)
    x = (W - mask.width) // 2
    y = (H - mask.height) // 2
    canvas.paste(mask.convert("1"), (x, y))

    packed: list[int] = []
    for y in range(H):
        byte = 0
        for x in range(W):
            if canvas.getpixel((x, y)):
                byte |= 1 << (7 - (x % 8))
            if x % 8 == 7:
                packed.append(byte)
                byte = 0
    if len(packed) != W * H // 8:
        raise AssertionError(f"Expected 200 bytes, got {len(packed)}")
    return packed


def cpp_array(code: str, values: list[int]) -> str:
    lines = [f"static const uint8_t LOGO_{code}[] PROGMEM = {{"]
    for index in range(0, len(values), 8):
        lines.append("  " + ", ".join(f"0x{value:02X}" for value in values[index:index + 8]) + ",")
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    contents = HEADER.read_text(encoding="utf-8")
    for code, url in LOGOS.items():
        if f"LOGO_{code}[]" in contents:
            raise RuntimeError(f"LOGO_{code} already exists; refusing to duplicate it")
        values = bitmap_bytes(url)
        contents += "\n\n" + cpp_array(code, values) + "\n"
        print(f"Generated {code}: {len(values)} bytes from {url}")
    HEADER.write_text(contents, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
