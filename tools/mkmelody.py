#!/usr/bin/env python3
import argparse
import struct
import sys
import wave
from collections import defaultdict
from pathlib import Path

FPS = 59.73
MAX_FRAMES = 255
MAX_VOICES = 2
GB_MIN_HZ = 64.0


def varlen(d: bytes, i: int) -> tuple[int, int]:
    v = 0
    while True:
        b = d[i]
        i += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80:
            return v, i


def parse(path: Path) -> tuple[int, list[tuple[int, int]], list[tuple[int, int, int, int]]]:
    d = path.read_bytes()
    if d[:4] != b"MThd":
        raise ValueError("not a midi file")
    hlen = int.from_bytes(d[4:8], "big")
    ntrk = int.from_bytes(d[10:12], "big")
    div = int.from_bytes(d[12:14], "big")
    if div & 0x8000:
        raise ValueError("smpte time division not supported")
    tempos = []
    notes = []
    i = 8 + hlen
    for _ in range(ntrk):
        if d[i : i + 4] != b"MTrk":
            raise ValueError("bad track header")
        tlen = int.from_bytes(d[i + 4 : i + 8], "big")
        j = i + 8
        end = j + tlen
        tick = 0
        status = 0
        while j < end:
            delta, j = varlen(d, j)
            tick += delta
            b = d[j]
            if b & 0x80:
                status = b
                j += 1
            if status == 0xFF:
                mtype = d[j]
                mlen, j = varlen(d, j + 1)
                if mtype == 0x51:
                    tempos.append((tick, int.from_bytes(d[j : j + mlen], "big")))
                j += mlen
                status = 0
            elif status in (0xF0, 0xF7):
                slen, j = varlen(d, j)
                j += slen
                status = 0
            else:
                kind = status & 0xF0
                ch = status & 0x0F
                if kind in (0xC0, 0xD0):
                    j += 1
                else:
                    a, b2 = d[j], d[j + 1]
                    j += 2
                    if kind == 0x90 and b2 > 0:
                        notes.append((tick, ch, a, 1))
                    elif kind == 0x80 or (kind == 0x90 and b2 == 0):
                        notes.append((tick, ch, a, 0))
        i = end
    tempos.sort()
    notes.sort(key=lambda n: (n[0], n[3]))
    return div, tempos, notes


def tick_to_s(div: int, tempos: list[tuple[int, int]]):
    pts = [(0, 0.0, 500000)]
    for t, us in tempos:
        t0, s0, us0 = pts[-1]
        pts.append((t, s0 + (t - t0) * us0 / 1e6 / div, us))

    def conv(tick: int) -> float:
        k = max(i for i, p in enumerate(pts) if p[0] <= tick)
        t0, s0, us0 = pts[k]
        return s0 + (tick - t0) * us0 / 1e6 / div

    return conv


def gb_reg(note: int) -> int:
    hz = 440.0 * 2 ** ((note - 69) / 12)
    while hz < GB_MIN_HZ:
        hz *= 2
    return max(0, min(2047, round(2048 - 131072 / hz)))


def melody(notes, ch: int, conv, transpose: int, seconds: float, start: float):
    on = {}
    events = []
    last_t = 0.0
    for tick, c, n, is_on in notes:
        if c != ch:
            continue
        t = conv(tick)
        if t > seconds:
            break
        events.append((last_t, t, max(on) if on else None))
        last_t = t
        if is_on:
            on[n] = on.get(n, 0) + 1
        elif n in on:
            on[n] -= 1
            if not on[n]:
                del on[n]
    out = []
    err = 0.0
    for t0, t1, n in events:
        if t1 <= start:
            continue
        t0 = max(t0, start)
        exact = (t1 - t0) * FPS + err
        frames = round(exact)
        err = exact - frames
        reg = 0 if n is None else gb_reg(n + transpose)
        while frames > 0:
            f = min(frames, MAX_FRAMES)
            if out and out[-1][0] == reg and out[-1][1] + f <= MAX_FRAMES:
                out[-1] = (reg, out[-1][1] + f)
            else:
                out.append((reg, f))
            frames -= f
    return out


def render(out: list[tuple[int, int]], rate: int, amp: int) -> list[int]:
    samples = []
    phase = 0.0
    for reg, n in out:
        count = round(n / FPS * rate)
        hz = 131072 / (2048 - reg) if reg else 0.0
        for _ in range(count):
            if hz:
                phase = (phase + hz / rate) % 1.0
                samples.append(amp if phase < 0.5 else -amp)
            else:
                samples.append(0)
    return samples


def write_wav(path: Path, voices: list[list[tuple[int, int]]], rate: int = 44100) -> None:
    tracks = [render(v, rate, 6000 if i == 0 else 3500) for i, v in enumerate(voices)]
    n = max(len(t) for t in tracks)
    mix = [sum(t[i] for t in tracks if i < len(t)) for i in range(n)]
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(struct.pack(f"<{n}h", *mix))


def main() -> int:
    ap = argparse.ArgumentParser(description="Convert MIDI channels to GBC note tables")
    ap.add_argument("midi", type=Path)
    ap.add_argument("--channel", type=int, action="append", help="0-15, up to 2 times; omit to list channels")
    ap.add_argument("--transpose", type=int, default=0)
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--out", type=Path, default=Path("gbc/src/melody.c"))
    ap.add_argument("--wav", type=Path, help="write a preview wav instead of the c table")
    args = ap.parse_args()

    div, tempos, notes = parse(args.midi)
    conv = tick_to_s(div, tempos)

    if args.channel is None:
        stats = defaultdict(list)
        for tick, ch, n, is_on in notes:
            if is_on:
                stats[ch].append((conv(tick), n))
        for ch in sorted(stats):
            v = stats[ch]
            print(f"ch {ch:2d}: {len(v):5d} notes, {min(n for _, n in v)}-{max(n for _, n in v)}, "
                  f"first {v[0][0]:6.1f} s, last {v[-1][0]:6.1f} s")
        return 0

    if len(args.channel) > MAX_VOICES:
        ap.error(f"at most {MAX_VOICES} channels")
    first = [conv(t) for t, c, _, on in notes if on and c in args.channel]
    if not first:
        ap.error("no notes in the selected channels")
    start = min(first)
    voices = [melody(notes, ch, conv, args.transpose, args.seconds, start) for ch in args.channel]
    secs = max(sum(f for _, f in v) for v in voices) / FPS
    if args.wav:
        write_wav(args.wav, voices)
        print(f"{args.wav}: {secs:.1f} s", file=sys.stderr)
        return 0

    pitches = sorted({r for v in voices for r, _ in v})
    index = {r: i for i, r in enumerate(pitches)}
    parts = ['#include "melody.h"\n']
    parts.append(f"const uint16_t melody_pitch[] = {{{', '.join(f'{r}u' for r in pitches)}}};\n")
    for i, v in enumerate(voices):
        rows = [", ".join(f"{{{index[r]}u, {f}u}}" for r, f in v[k : k + 8]) for k in range(0, len(v), 8)]
        body = ",\n    ".join(rows)
        parts.append(f"static const melody_note_t v{i}[] = {{\n    {body},\n}};\n")
    entries = ", ".join(f"{{v{i}, {len(v)}u}}" for i, v in enumerate(voices))
    parts.append(f"const melody_voice_t melody_voices[] = {{{entries}}};\n")
    parts.append(f"const uint8_t melody_voice_count = {len(voices)}u;\n")
    args.out.write_text("\n".join(parts))
    print(f"{[len(v) for v in voices]} notes, {secs:.1f} s", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
