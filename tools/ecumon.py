#!/usr/bin/env python3
import argparse
import struct
import sys
import time
import zlib

import serial

import ms1_tables

SIG = b"MS2Extra comms332m2"
MS1_SIGS = (b"MSnS-extra format ", b"MS1/Extra format ")
MS1_SIG_LEN = 32
MS1_RT_LEN = 39
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


def f40_to_c(raw: int) -> float:
    return (raw - 40 - 32) / 1.8


def ms1_rpm(d: bytes, cyl: int, two_stroke: bool) -> int:
    itime = d[37] << 16 | d[22] << 8 | d[23]
    if itime == 0:
        return 0
    rpm = round(60000000 * (1 if two_stroke else 2) / (itime * cyl))
    return rpm if rpm > 20 else 0


def decode_ms1(d: bytes, kpa: tuple, cyl: int = 4, two_stroke: bool = False) -> dict:
    baro = kpa[d[3]]
    mp = kpa[d[4]]
    return {
        "rpm": ms1_rpm(d, cyl, two_stroke),
        "adv": d[24] * 0.352 - 10,
        "boost": (mp - baro) * 0.145038,
        "map": float(mp),
        "clt": f40_to_c(ms1_tables.THERM[d[6]]),
        "iat": f40_to_c(ms1_tables.MAT[d[5]]),
        "tps": float(ms1_tables.TPS[d[7]]),
        "batt": d[8] * 30 / 255,
        "afr": 10 + d[9] * 0.039216,
    }


def ms1_cmd(port: serial.Serial, cmd: bytes, n: int) -> bytes:
    port.reset_input_buffer()
    port.write(cmd)
    return read_exact(port, n)


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


def show(v: dict) -> None:
    print(
        f"rpm {v['rpm']:5d}  boost {v['boost']:5.1f} psi  adv {v['adv']:5.1f}  "
        f"clt {v['clt']:5.1f} C  iat {v['iat']:5.1f} C  map {v['map']:5.1f}  "
        f"tps {v['tps']:5.1f}  batt {v['batt']:4.1f}  afr {v['afr']:4.1f}",
        flush=True,
    )


def open_ms2(port: serial.Serial):
    sig = transact(port, req_read(TABLE_SIG, 0, 20)).rstrip(b"\x00")
    print(f"signature: {sig.decode(errors='replace')}")
    if not sig.startswith(SIG):
        raise ValueError(f"unexpected signature, expected {SIG.decode()}")
    return lambda: decode(transact(port, req_read(TABLE_RT, 0, RT_LEN)))


def open_ms1(port: serial.Serial, kpa: tuple, cyl: int, two_stroke: bool):
    sig = ms1_cmd(port, b"S", MS1_SIG_LEN)
    print(f"signature: {sig.decode(errors='replace')}")
    if not sig.startswith(MS1_SIGS):
        raise ValueError("unexpected signature, expected MS1/Extra")
    return lambda: decode_ms1(ms1_cmd(port, b"R", MS1_RT_LEN), kpa, cyl, two_stroke)


def main() -> int:
    ap = argparse.ArgumentParser(description="Read-only MS1/Extra and MS2/Extra realtime monitor")
    ap.add_argument("port")
    ap.add_argument("--ecu", choices=("ms1", "ms2"), required=True)
    ap.add_argument("--baud", type=int)
    ap.add_argument("--map-sensor", choices=("4250", "4115"), default="4250", help="ms1 only")
    ap.add_argument("--cyl", type=int, default=4, choices=range(1, 17), metavar="1-16", help="ms1 only")
    ap.add_argument("--two-stroke", action="store_true", help="ms1 only")
    ap.add_argument("--hz", type=float, default=10.0)
    args = ap.parse_args()

    baud = args.baud or (9600 if args.ecu == "ms1" else 115200)
    with serial.Serial(args.port, baud, timeout=0.5) as port:
        time.sleep(0.2)
        try:
            if args.ecu == "ms1":
                kpa = ms1_tables.KPA4250 if args.map_sensor == "4250" else ms1_tables.KPA4115
                poll = open_ms1(port, kpa, args.cyl, args.two_stroke)
            else:
                poll = open_ms2(port)
        except (TimeoutError, ValueError) as e:
            print(f"error: {e}", file=sys.stderr)
            return 1

        period = 1.0 / args.hz
        while True:
            t0 = time.monotonic()
            try:
                show(poll())
            except (TimeoutError, ValueError) as e:
                print(f"error: {e}", file=sys.stderr)
            time.sleep(max(0.0, period - (time.monotonic() - t0)))


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
