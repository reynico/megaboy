#!/usr/bin/env python3
import argparse
import re
import sys
from pathlib import Path

from PIL import Image
from pyboy import PyBoy

SCALE = 3
SHEET_SCALE = 2
BOOT_FRAMES = 340
SHEET_BOOT_FRAMES = 345
PAGE_FRAMES = 60
GAP = 8
COLS = 3
BG = (24, 26, 30)

RT = {
    "rpm": 0,
    "adv": 2,
    "map": 4,
    "iat": 6,
    "clt": 8,
    "tps": 10,
    "batt": 12,
    "afr": 14,
    "boost": 16,
}

CRUISE = {
    "rpm": 4200,
    "adv": 280,
    "map": 1450,
    "iat": 380,
    "clt": 880,
    "tps": 620,
    "batt": 138,
    "afr": 128,
    "boost": 65,
}

PAGES = ["dash", "track", "gauge", "boost", "log"]


def app_addr(noi: Path) -> int:
    m = re.search(r"^DEF _app (0x[0-9A-Fa-f]+)$", noi.read_text(), re.M)
    if not m:
        raise SystemExit(f"_app not found in {noi}")
    return int(m.group(1), 16)


def poke(pb: PyBoy, app: int, values: dict) -> None:
    for name, v in values.items():
        v &= 0xFFFF
        pb.memory[app + RT[name]] = v & 0xFF
        pb.memory[app + RT[name] + 1] = v >> 8


def grab(pb: PyBoy) -> Image.Image:
    return pb.screen.image.convert("RGB")


def sheet(rom: Path, noi: Path) -> Image.Image:
    app = app_addr(noi)
    pb = PyBoy(str(rom), window="null", cgb=True, sound_emulated=False)
    pb.set_emulation_speed(0)
    pb.tick(SHEET_BOOT_FRAMES, True)
    poke(pb, app, CRUISE)
    shots = []
    for i, _ in enumerate(PAGES):
        if i:
            pb.button("right")
        pb.tick(PAGE_FRAMES, True)
        shots.append(grab(pb))
    pb.button("left")
    pb.tick(PAGE_FRAMES, True)
    poke(pb, app, {"rpm": 6200, "boost": 215, "map": 2490})
    pb.tick(PAGE_FRAMES, True)
    shots.append(grab(pb))
    pb.stop(save=False)

    w, h = shots[0].size
    rows = (len(shots) + COLS - 1) // COLS
    out = Image.new("RGB", (COLS * w + (COLS + 1) * GAP, rows * h + (rows + 1) * GAP), BG)
    for i, s in enumerate(shots):
        out.paste(s, (GAP + (i % COLS) * (w + GAP), GAP + (i // COLS) * (h + GAP)))
    return out.resize((out.width * SHEET_SCALE, out.height * SHEET_SCALE), Image.NEAREST)


def dash(rom: Path) -> Image.Image:
    pb = PyBoy(str(rom), window="null", cgb=True, sound_emulated=False)
    pb.set_emulation_speed(0)
    pb.tick(BOOT_FRAMES, True)
    img = grab(pb)
    pb.stop(save=False)
    return img.resize((img.width * SCALE, img.height * SCALE), Image.NEAREST)


def main() -> int:
    ap = argparse.ArgumentParser(description="Capture GBC screens from the ROM")
    ap.add_argument("--rom", type=Path, default=Path("../gbc/build/megaboy.gbc"))
    ap.add_argument("--out", type=Path)
    ap.add_argument("--sheet", action="store_true", help="all pages and one alert with sample values")
    ap.add_argument("--noi", type=Path, help="symbol file, default: ROM path with .noi")
    args = ap.parse_args()

    if args.sheet:
        img = sheet(args.rom, args.noi or args.rom.with_suffix(".noi"))
        out = args.out or Path("../gbc/build/screens.png")
    else:
        img = dash(args.rom)
        out = args.out or Path("../docs/img/dash.png")

    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out, optimize=True)
    print(f"wrote {out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
