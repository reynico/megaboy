# Tuning from the GBC

The GBC can edit a small set of fuel, spark, AFR, rev limit and boost tables. Open Menu > TUNE with the bridge connected; the ECU type reported by the bridge determines which tables you get.

Before trying it, save a backup MSQ in TunerStudio. An edit goes into ECU RAM when you confirm it. Making it survive a restart takes a separate burn to flash. There is no RPM lockout for either action, and a power loss during a burn can corrupt the page.

## Using the editor

Move around with the D-pad and press A to edit a cell. The cursor is amber, then red while editing. The cell nearest the current RPM and load uses inverse colors; boost tables use TPS for load, and the other grids use MAP.

While editing, UP and DOWN change the value on the GBC only. A sends it to ECU RAM. B cancels that edit and leaves the previous value alone.

Pressing B to leave a table with RAM changes brings up three choices:

| Button | Action |
|---|---|
| A: BURN | Save the RAM changes to ECU flash. |
| B: DISCARD | Write the original values back to ECU RAM. |
| START: STAY | Return to the table. |

Discard involves writes to the ECU too, so keep the link connected while it runs.

RPM bins run across the top in hundreds of rpm. The left column shows MAP in kPa, or TPS in percent for boost tables, with the highest load at the top. The view scrolls to reach cells outside the screen, including the extra rows in the 16x16 MS2 VE table. REV LIMITS is a list: UP and DOWN select an entry, and A edits it.

## What a write checks

The bridge keeps a copy of the open table and checks each write against its allowed cells, value range and maximum step. The GBC splits a larger edit into several writes, then asks the bridge to read back the last value. A mismatch shows `VERIFY FAILED`. Intermediate steps are not read back.

Rev limits have ordering rules as well. On MS1, soft, launch and flat-shift limits must stay at or below the hard limit. On MS2, launch and flat-shift limits must stay at or below the rev limit. Both the GBC and bridge check this; an invalid change shows `LIMIT ORDER`.

For example, to move the hard limit below the current soft limit, lower the soft limit first. Discard also restores values in an order that respects those rules.

The tables below are the implemented limits, defined in [common/tune.c](../common/tune.c). The GBC link carries values as s16 in ECU units; display conversions are applied by the ROM.

## MS1/Extra

| Table | Page cmd | Size | Limits (raw) | Display |
|---|---|---|---|---|
| VE table 1 | `P 01` | 12x12 | 10-250 | % |
| Spark table 1 | `P 03` | 12x12 | 0-156 | `raw * 0.352 - 10` deg (-10 to 44.9) |
| AFR target 1 | `P 06` | 8x8 | 26-230 | `10 + raw * 0.0392` (Innovate 0-5V linear) |
| Rev limits | `P 00` | soft 2, hard 6, launch 20, flat shift 128 | 20-90 | `raw * 100` rpm |
| Boost target | `P 07` | 6x6 at 0, RPM x TPS | 90-250 | kPa (115/250 kPa sensor) |
| Boost duty | `P 07` | 6x6 at 48, RPM x TPS | 0-242 | `raw * 0.392` % |

Each button press changes an MS1 value by one raw unit, and each individual write is limited to a change of 10 raw units. MAP bins are shown as raw kPa, which is correct for the 115 and 250 kPa sensors only.

## MS2/Extra (comms332m2)

These offsets match [ecu/mainController.ini](../ecu/mainController.ini). Multi-byte values on the ECU side are big endian. The page column below refers to MS2 pages, not the `TUNE_*` ids used over the GBC link. “Max step” is the limit on one write; “Key step” is the change from one button press.

| Table | ECU page | Cells | Bins | Limits (raw) | Max step | Key step | Display |
|---|---|---|---|---|---|---|---|
| VE table 1 | `09` | 16x16 U08 at 0 | rpm U16 at 768, MAP S16 at 864 | 10-250 | 10 | 1 | % |
| Spark table 1 | `0a` | 12x12 S16 at 0 | rpm U16 at 576, MAP S16 at 624 | -100 to 500 | 30 | 5 | raw / 10 deg |
| AFR target 1 | `04` | 12x12 U08 at 48 | rpm U16 at 374, MAP S16 at 422 | 90-200 | 10 | 1 | raw / 10 AFR |
| Rev limits | `04`, `0a` | rev limit S16 `04`:656, launch `0a`:678, flat arm `0a`:685, flat shift `0a`:691 | - | 2000-9000 | 1000 | 100 | rpm |
| Boost target | `05` | 8x8 S16 at 76 | rpm U16 at 220, TPS S16 at 204 | 900-2500 | 100 | 10 | raw / 10 kPa |
| Boost duty | `05` | 8x8 U08 at 236 | rpm U16 at 316, TPS S16 at 300 | 0-100 | 10 | 1 | % |

For the MAP-based fuel layout here, MAP bins are stored as kPa x 10 and TPS bins as percent x 10.

## What the bridge sends

On MS1, reading starts with `P <page>` and `V`. The bridge reads the 189-byte page, extracts the table and sends it to the GBC. A write uses `P <page> W <offset> <value>` after the checks above. If the verify flag is set, the bridge reads the page again and compares the value. Otherwise it updates its cached value. A page read takes about 200 ms at 9600 baud, which is why the GBC only requests verification on the final step of an edit.

An MS1 burn sends `P <page> B`, then waits 200 ms.

On MS2, `r 00 <page> <offset> <count>` reads the cells and axes in chunks of at most 256 bytes. Rev limits are read individually. Writes use `w 00 <page> <offset> <count> <data>` with one or two data bytes, depending on the value type. A verified write reads that value back with `r`.

An MS2 burn sends `b 00 <page>` for every page used by the selected table. Most tables occupy one page, but the rev limit entries span `04` and `0a`, so both must be burned.
