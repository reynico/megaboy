# Bridge to GBC link protocol

The GBC doesn't need to know which serial protocol the ECU speaks. The bridge decodes that side and sends the same small set of frames to the ROM. Commands from the GBC travel back over the same link.

The encoder and parser are in [common/link.c](../common/link.c), with tests in [test_link.c](../common/test/test_link.c). This page describes the bytes on the wire.

## Physical

The bridge supplies the clock and the GBC uses external clock mode. Each transfer is eight bits, most significant bit first. The clock idles high; the bridge sets data on the falling edge and samples it on the rising edge.

There is a 150 us gap between bytes. The GBC needs that time to read `SB` and set `SC` for the next transfer, even running at double speed.

Each byte clocked out by the bridge also clocks one byte back from the GBC. The ROM uses those reply bytes to send its own frames, or `0x00` when it has nothing queued.

## Frame

```
4D 42 <type> <len> <payload: len bytes> <sum>
```

`4D 42` marks the start of a frame. The payload can be at most 32 bytes long. To calculate `sum`, add `type`, `len` and every payload byte as an 8-bit sum, then take the bitwise NOT.

Multi-byte values are little endian, matching the GBC CPU. This differs from the MS2 serial protocol; the bridge handles that conversion.

## Bridge to GBC

Lengths below are payload lengths, excluding the frame header and checksum. In the RT payload, RPM is unsigned, the eight scaled measurements are signed 16-bit values, and engine and flags are single bytes.

| Type | Name | Len | Payload |
|---|---|---|---|
| 0x01 | RT | 20 | rpm u16, adv x10, map x10 kPa, iat x10 C, clt x10 C, tps x10 %, batt x10 V, afr x10, boost x10 psi (s16), engine u8, flags u8 |
| 0x02 | STATUS | 3 | state (0 wait for ECU, 1 ok), ecu (the type the bridge uses: 0 not set, 1 MS1, 2 MS2), MS2 poll rate in Hz |
| 0x03 | TABLE | 3 + 2n | table id, blob offset u16 (in values), n values s16 (n <= 14) |
| 0x04 | ACK | 6 | command, table id, cell, status, value s16 |
| 0x05 | TIME | 7 | clock state (0 no RTC, 1 not set, 2 ok), year (0-99, 20yy), month, day, hour, minute, second |
| 0x06 | LOG | 17 | state (0 idle, 1 recording, 2 no card, 3 card full, 4 error), file number u16, seconds u16, bytes u32, rows per second u8, dated u8 (1: file named by start time), start year, month, day, hour, minute, second |

## GBC to bridge

| Type | Name | Len | Payload |
|---|---|---|---|
| 0x10 | READ | 1 | table id. The bridge answers with TABLE frames, then ACK. |
| 0x11 | WRITE | 5 | table id, cell, value s16, flags. The bridge writes the value to the ECU. Flag bit 0 = verify: read it back and compare. Without that flag, update the cache without a readback. The bridge answers with ACK. |
| 0x12 | BURN | 1 | table id. The bridge answers with ACK. |
| 0x13 | ECU | 1 or 2 | ECU type (0 keep, 1 MS1, 2 MS2), MS2 poll rate in Hz (10 to 40, optional). The bridge saves both in flash and connects again when the type changes. STATUS shows the new values. |
| 0x14 | TIME SET | 6 | year (0-99), month, day, hour, minute, second. The bridge checks the date and writes the RTC. |
| 0x15 | LOG | 1 | 1 start, 0 stop. The bridge answers with a LOG frame. |

## Tables and connection state

For a grid, the table blob starts with cell values in row-major order: each MAP or TPS row contains the RPM columns. RPM bins come next, followed by the MAP or TPS bins. The rev limit list has values only, with no axes. Everything travels as s16 in ECU units, even when the ECU stores a value as a single byte.

The ECU type in STATUS selects the MS1 or MS2 table set. Link table ids are the `TUNE_*` ids in [common/tune.h](../common/tune.h), not the ECU's page numbers. [common/tune.c](../common/tune.c) maps those ids to pages, offsets, formats and write limits. ACK status codes are also in `common/tune.h`.

An ECU type of 0 means nothing has been selected, so the bridge sends nothing to the ECU. If the GBC has MS1 or MS2 selected and STATUS reports a different type, it sends an ECU command. It also sends that command when the reported poll rate differs from its setting.

RT frames follow the ECU replies, roughly 20 Hz for MS1. STATUS, TIME and LOG are sent once per second.
