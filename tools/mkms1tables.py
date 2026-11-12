#!/usr/bin/env python3
import argparse
import re
import sys
from pathlib import Path

DEFAULT_INC = "/Applications/TunerStudio_MS.app/Contents/Java/inc"
TABLES = {
    "therm": "thermfactor.inc",
    "mat": "matfactor.inc",
    "kpa4250": "kpafactor4250.inc",
    "kpa4115": "kpafactor4115.inc",
    "tps": "throttlefactor.inc",
}
DB = re.compile(r"^\s*DB\s+(\d+)T", re.IGNORECASE)


def parse(path: Path) -> list[int]:
    vals = []
    for line in path.read_text(encoding="latin-1").splitlines():
        m = DB.match(line)
        if m:
            vals.append(int(m.group(1)))
    if len(vals) != 256:
        raise ValueError(f"{path}: expected 256 values, got {len(vals)}")
    return vals


def find(dirs: list[Path], name: str) -> Path:
    for d in reversed(dirs):
        p = d / name
        if p.is_file():
            return p
    raise FileNotFoundError(name)


def c_array(name: str, vals: list[int]) -> str:
    rows = [", ".join(f"{v:4d}" for v in vals[i : i + 16]) for i in range(0, 256, 16)]
    body = ",\n    ".join(rows)
    return f"const uint16_t ms1_{name}[256] = {{\n    {body},\n}};\n"


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate MS1/Extra conversion tables from TunerStudio inc files")
    ap.add_argument("--inc", action="append", type=Path, default=[], help="extra inc dir, later wins")
    ap.add_argument("--c", type=Path, default=Path("bridge/src/ms1_tables.c"))
    ap.add_argument("--py", type=Path, default=Path("tools/ms1_tables.py"))
    args = ap.parse_args()

    dirs = [Path(DEFAULT_INC), *args.inc]
    data = {}
    for key, fname in TABLES.items():
        src = find(dirs, fname)
        data[key] = parse(src)
        print(f"{key}: {src}", file=sys.stderr)

    args.c.write_text(
        '#include "ms1_tables.h"\n\n' + "\n".join(c_array(k, v) for k, v in data.items())
    )
    args.py.write_text("".join(f"{k.upper()} = {tuple(v)!r}\n" for k, v in data.items()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
