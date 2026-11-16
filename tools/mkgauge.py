#!/usr/bin/env python3
import importlib.util
import math
import sys
from pathlib import Path

CX, CY = 80, 64
R_ARC = 46
SWEEP = 240.0
START = 210.0
RPM_MAX = 8000
COL0, COL1 = 3, 17
ROW0, ROW1 = 2, 11
NEEDLE_STEPS = RPM_MAX // 64 + 1
NEEDLE_R = range(10, 43, 4)


def load_glyphs() -> dict:
    path = Path(__file__).with_name("mkfont.py")
    spec = importlib.util.spec_from_file_location("mkfont", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.GLYPHS


def angle(rpm: float) -> float:
    return math.radians(START - SWEEP * rpm / RPM_MAX)


def point(rpm: float, r: float) -> tuple[float, float]:
    a = angle(rpm)
    return CX + r * math.cos(a), CY - r * math.sin(a)


def render() -> tuple[list[list[int]], list[list[float]]]:
    w, h = 160, 144
    px = [[0] * w for _ in range(h)]
    rpm_at = [[-1.0] * w for _ in range(h)]

    def plot(x: float, y: float, rpm: float) -> None:
        ix, iy = int(round(x)), int(round(y))
        if 0 <= ix < w and 0 <= iy < h:
            px[iy][ix] = 1
            rpm_at[iy][ix] = rpm

    steps = 2000
    for i in range(steps + 1):
        rpm = RPM_MAX * i / steps
        for r in (R_ARC, R_ARC + 1):
            plot(*point(rpm, r), rpm)
    for rpm in range(0, RPM_MAX + 1, 500):
        major = rpm % 1000 == 0
        r0 = R_ARC - (9 if major else 5)
        for k in range(int((R_ARC + 1 - r0) * 4) + 1):
            r = r0 + k / 4
            plot(*point(rpm, r), rpm)
            if major:
                plot(*point(rpm + 25, r), rpm)
    glyphs = load_glyphs()
    for n in range(RPM_MAX // 1000 + 1):
        rpm = n * 1000
        x, y = point(rpm, R_ARC - 17)
        g = glyphs[str(n)]
        x0, y0 = int(round(x)) - 3, int(round(y)) - 3
        for gy, row in enumerate(g):
            for gx, c in enumerate(row):
                if c == "#":
                    plot(x0 + gx, y0 + gy, rpm)
    return px, rpm_at


def tile_bytes(px: list[list[int]], tx: int, ty: int) -> bytes:
    out = []
    for y in range(8):
        b = 0
        for x in range(8):
            if px[ty * 8 + y][tx * 8 + x]:
                b |= 0x80 >> x
        out += [b, b]
    return bytes(out)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: mkgauge.py <out.c>", file=sys.stderr)
        return 1
    px, rpm_at = render()
    for y in range(ROW0 * 8):
        if any(px[y]):
            raise SystemExit("dial above its tile area")
    tiles: list[bytes] = []
    index: dict[bytes, int] = {}
    tmap = []
    trpm = []
    for ty in range(ROW0, ROW1):
        for tx in range(COL0, COL1):
            data = tile_bytes(px, tx, ty)
            if not any(data):
                tmap.append(0xFF)
                trpm.append(0xFF)
                continue
            if data not in index:
                index[data] = len(tiles)
                tiles.append(data)
            tmap.append(index[data])
            vals = [rpm_at[ty * 8 + y][tx * 8 + x] for y in range(8) for x in range(8) if rpm_at[ty * 8 + y][tx * 8 + x] >= 0]
            trpm.append(int(round(max(vals) / 100)))
    needle = []
    for i in range(NEEDLE_STEPS):
        rpm = min(i * 64, RPM_MAX)
        for r in NEEDLE_R:
            x, y = point(rpm, r)
            needle += [int(round(x - CX)), int(round(y - CY))]

    def arr(vals, per=16, fmt="0x{:02x}"):
        return ",\n    ".join(", ".join(fmt.format(v) for v in vals[i:i + per]) for i in range(0, len(vals), per))

    tile_data = [b for t in tiles for b in t]
    lines = [
        '#include "gauge_gfx.h"',
        "",
        f"#if GAUGE_TILE_COUNT != {len(tiles)}",
        "#error gauge_gfx.h does not match gauge_gfx.c",
        "#endif",
        "",
        f"const uint8_t gauge_tiles[GAUGE_TILE_COUNT * 16] = {{\n    {arr(tile_data)},\n}};",
        "",
        f"const uint8_t gauge_map[GAUGE_W * GAUGE_H] = {{\n    {arr(tmap, COL1 - COL0)},\n}};",
        "",
        f"const uint8_t gauge_rpm100[GAUGE_W * GAUGE_H] = {{\n    {arr(trpm, COL1 - COL0, '{:3d}')},\n}};",
        "",
        f"const int8_t gauge_needle[GAUGE_STEPS * GAUGE_DOTS * 2] = {{\n    {arr(needle, 18, '{:3d}')},\n}};",
        "",
    ]
    Path(sys.argv[1]).write_text("\n".join(lines))
    hdr = Path(sys.argv[1]).with_name("gauge_gfx.h")
    hdr.write_text(
        "#pragma once\n\n#include <stdint.h>\n\n"
        f"#define GAUGE_X {COL0}u\n#define GAUGE_Y {ROW0}u\n"
        f"#define GAUGE_W {COL1 - COL0}u\n#define GAUGE_H {ROW1 - ROW0}u\n"
        f"#define GAUGE_TILE_COUNT {len(tiles)}u\n"
        f"#define GAUGE_STEPS {NEEDLE_STEPS}u\n#define GAUGE_DOTS {len(NEEDLE_R)}u\n"
        f"#define GAUGE_CX {CX}u\n#define GAUGE_CY {CY}u\n"
        f"#define GAUGE_RPM_MAX {RPM_MAX}u\n#define GAUGE_NONE 0xffu\n\n"
        "extern const uint8_t gauge_tiles[GAUGE_TILE_COUNT * 16];\n"
        "extern const uint8_t gauge_map[GAUGE_W * GAUGE_H];\n"
        "extern const uint8_t gauge_rpm100[GAUGE_W * GAUGE_H];\n"
        "extern const int8_t gauge_needle[GAUGE_STEPS * GAUGE_DOTS * 2];\n"
    )
    print(f"{len(tiles)} tiles", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
