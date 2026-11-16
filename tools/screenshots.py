#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from pyboy import PyBoy

SCALE = 3
BODY = (92, 54, 140)
BODY_EDGE = (66, 36, 104)
BEZEL = (52, 52, 62)
DPAD = (34, 34, 40)
BTN = (176, 40, 72)
PILL = (70, 70, 80)
LABEL = (220, 214, 236)


def font(size: int):
    return ImageFont.load_default(size=size)


def frame(screen: Image.Image) -> Image.Image:
    sw, sh = screen.width * SCALE, screen.height * SCALE
    bx, by = 50, 50
    pad, pad_b = 40, 70
    body_w = sw + 2 * pad + 2 * bx
    body_h = by + sh + pad + pad_b + 420
    img = Image.new("RGBA", (body_w, body_h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, body_w - 1, body_h - 1), radius=48, fill=BODY, outline=BODY_EDGE, width=6)
    bezel = (bx, by, body_w - bx, by + sh + pad + pad_b)
    d.rounded_rectangle(bezel, radius=24, fill=BEZEL)
    d.ellipse((bx + 14, by + sh // 2 - 8, bx + 30, by + sh // 2 + 8), fill=(220, 40, 40))
    d.text((bx + 22, by + sh // 2 + 16), "PWR", fill=LABEL, font=font(14), anchor="mt")
    img.paste(screen.resize((sw, sh), Image.NEAREST), (bx + pad, by + pad))
    d.text((body_w // 2, by + sh + pad + pad_b // 2), "MEGABOY", fill=LABEL, font=font(30), anchor="mm")

    cy = bezel[3] + 170
    cx = bx + 120
    arm, w = 70, 46
    d.rounded_rectangle((cx - arm, cy - w // 2, cx + arm, cy + w // 2), radius=8, fill=DPAD)
    d.rounded_rectangle((cx - w // 2, cy - arm, cx + w // 2, cy + arm), radius=8, fill=DPAD)

    r = 34
    ax, ay = body_w - bx - 70, cy - 30
    bx2, by2 = ax - 100, cy + 20
    for x, y, t in ((ax, ay, "A"), (bx2, by2, "B")):
        d.ellipse((x - r, y - r, x + r, y + r), fill=BTN)
        d.text((x, y + r + 18), t, fill=LABEL, font=font(22), anchor="mm")

    py = cy + 150
    for x, t in ((body_w // 2 - 70, "SELECT"), (body_w // 2 + 70, "START")):
        d.rounded_rectangle((x - 38, py - 10, x + 38, py + 10), radius=10, fill=PILL)
        d.text((x, py + 30), t, fill=LABEL, font=font(18), anchor="mm")
    return img


class Driver:
    def __init__(self, rom: Path):
        self.pb = PyBoy(str(rom), window="null", cgb=True, sound_emulated=False)
        self.pb.set_emulation_speed(0)

    def wait(self, frames: int) -> None:
        self.pb.tick(frames, True)

    def press(self, button: str, times: int = 1) -> None:
        for _ in range(times):
            self.pb.button(button, 6)
            self.wait(20)

    def shot(self) -> Image.Image:
        return self.pb.screen.image.convert("RGB")

    def stop(self) -> None:
        self.pb.stop(save=False)


def capture(rom: Path) -> dict[str, Image.Image]:
    g = Driver(rom)
    shots = {}
    g.wait(300)
    shots["dash"] = g.shot()
    g.press("right")
    g.wait(120)
    shots["gauge"] = g.shot()
    g.press("right")
    g.wait(120)
    shots["boost"] = g.shot()
    g.press("right")
    g.wait(60)
    shots["log"] = g.shot()
    g.press("start")
    shots["menu"] = g.shot()
    g.press("down")
    g.press("a")
    shots["settings"] = g.shot()
    g.press("b")
    g.press("down")
    g.press("a")
    shots["tune"] = g.shot()
    g.press("b")
    g.press("down")
    g.press("a")
    g.wait(60)
    shots["about"] = g.shot()
    g.press("b")
    g.press("up", 2)
    g.press("a")
    g.press("a")
    g.press("down", 20)
    g.press("a")
    g.wait(60)
    shots["warning"] = g.shot()
    g.stop()
    return shots


def gallery(images: list[Image.Image], cols: int, gap: int = 24) -> Image.Image:
    w, h = images[0].size
    rows = (len(images) + cols - 1) // cols
    out = Image.new("RGBA", (cols * w + (cols - 1) * gap, rows * h + (rows - 1) * gap), (0, 0, 0, 0))
    for i, im in enumerate(images):
        out.paste(im, ((i % cols) * (w + gap), (i // cols) * (h + gap)), im)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="Capture framed ROM screenshots for the README")
    ap.add_argument("--rom", type=Path, default=Path("../gbc/build/megaboy.gbc"))
    ap.add_argument("--out", type=Path, default=Path("../docs/img"))
    args = ap.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    shots = capture(args.rom)
    framed = {}
    for name, im in shots.items():
        framed[name] = frame(im)
        framed[name].save(args.out / f"{name}.png", optimize=True)
    order = ["dash", "gauge", "boost", "log", "warning", "menu", "settings", "tune", "about"]
    g = gallery([framed[n] for n in order], 5)
    g = g.resize((g.width // 3, g.height // 3), Image.LANCZOS)
    g.save(args.out / "gallery.png", optimize=True)
    print(f"wrote {len(framed) + 1} images to {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
