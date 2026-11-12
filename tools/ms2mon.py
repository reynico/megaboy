#!/usr/bin/env python3
import argparse
import struct
import sys
import time
import zlib

import serial

SIG = b"MS2Extra comms332m2"
TABLE_RT = 0x07
TABLE_SIG = 0x0F
RT_LEN = 30


def frame(payload: bytes) -> bytes:
    return struct.pack(">H", len(payload)) + payload + struct.pack(">I", zlib.crc32(payload))


def req_read(table: int, off: int, cnt: int) -> bytes:
    return frame(struct.pack(">cBBHH", b"r", 0, table, off, cnt))


def read_exact(port: serial.Serial, n: int) -> bytes:
    data = port.read(n)
    if len(data) != n:
        raise TimeoutError(f"wanted {n} bytes, got {len(data)}")
    return data


def transact(port: serial.Serial, req: bytes) -> bytes:
    port.reset_input_buffer()
    port.write(req)
    (length,) = struct.unpack(">H", read_exact(port, 2))
    payload = read_exact(port, length)
    (crc,) = struct.unpack(">I", read_exact(port, 4))
    if crc != zlib.crc32(payload):
        raise ValueError("bad crc")
    if payload[0] >= 0x80:
        raise ValueError(f"ecu error 0x{payload[0]:02x}")
    return payload[1:]


def f10_to_c(raw: int) -> float:
    return (raw - 320) / 18.0


def decode(d: bytes) -> dict:
    rpm, adv = struct.unpack_from(">Hh", d, 6)
    baro, mp, mat, clt, tps, batt, afr = struct.unpack_from(">7h", d, 16)
    return {
        "rpm": rpm,
        "adv": adv / 10,
        "boost": (mp - baro) / 10 * 0.145038,
        "map": mp / 10,
        "clt": f10_to_c(clt),
        "iat": f10_to_c(mat),
        "tps": tps / 10,
        "batt": batt / 10,
        "afr": afr / 10,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description="Read-only MS2/Extra realtime monitor")
    ap.add_argument("port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--hz", type=float, default=10.0)
    args = ap.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.5) as port:
        time.sleep(0.2)
        sig = transact(port, req_read(TABLE_SIG, 0, 20)).rstrip(b"\x00")
        print(f"signature: {sig.decode(errors='replace')}")
        if not sig.startswith(SIG):
            print(f"unexpected signature, expected {SIG.decode()}", file=sys.stderr)
            return 1

        period = 1.0 / args.hz
        while True:
            t0 = time.monotonic()
            try:
                v = decode(transact(port, req_read(TABLE_RT, 0, RT_LEN)))
                print(
                    f"rpm {v['rpm']:5d}  boost {v['boost']:5.1f} psi  adv {v['adv']:5.1f}  "
                    f"clt {v['clt']:5.1f} C  iat {v['iat']:5.1f} C  map {v['map']:5.1f}  "
                    f"tps {v['tps']:5.1f}  batt {v['batt']:4.1f}  afr {v['afr']:4.1f}",
                    flush=True,
                )
            except (TimeoutError, ValueError) as e:
                print(f"error: {e}", file=sys.stderr)
            time.sleep(max(0.0, period - (time.monotonic() - t0)))


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
