#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

from PIL import Image
from pyboy import PyBoy

SCALE = 3
BOOT_FRAMES = 340


def main() -> int:
    ap = argparse.ArgumentParser(description="Capture the dash screen for the README")
    ap.add_argument("--rom", type=Path, default=Path("../gbc/build/megaboy.gbc"))
    ap.add_argument("--out", type=Path, default=Path("../docs/img/dash.png"))
    args = ap.parse_args()

    pb = PyBoy(str(args.rom), window="null", cgb=True, sound_emulated=False)
    pb.set_emulation_speed(0)
    pb.tick(BOOT_FRAMES, True)
    img = pb.screen.image.convert("RGB")
    pb.stop(save=False)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    img.resize((img.width * SCALE, img.height * SCALE), Image.NEAREST).save(args.out, optimize=True)
    print(f"wrote {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
