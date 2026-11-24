# MS1/Extra protocol

These are the MS1/Extra commands and offsets used by megaboy. I kept them here so there is no need to go through the whole TunerStudio INI just to find a sensor value. The layout comes from `MSnS-extra format 029q` and `MS1/Extra format 029y3`; both use the same realtime block.

## Link

The link runs at 9600 baud, 8N1. Each request starts with a command byte, followed by any arguments. There is no envelope or CRC, so the bridge has to know the reply length in advance and time out if it does not receive enough bytes.

The bridge leaves a 5 ms gap between commands. On the bench, sending a command immediately after the last reply byte caused dropped commands; the [performance notes](performance.md) have the measurements.

## Commands

| Purpose | Bytes | Response |
|---|---|---|
| Signature | `S` | 32 bytes, starts with `MSnS-extra format ` or `MS1/Extra format ` |
| Realtime | `R` | 39 bytes |
| Page select | `P <page>` | none |
| Page read | `V` | 189 bytes |
| Write byte | `W <offset> <value>` | none |
| Burn | `B` | none |

Startup and monitoring use `S`, `P 01`, `V` and `R`. Selecting a page with `P` does not change the tune. The Pico bridge also uses `W` and `B` for the [GBC table editor](tuning.md).

## Realtime offsets (U08)

| Offset | Name | Conversion |
|---|---|---|
| 2 | engine | bits |
| 3 | baroADC | by MAP type: 115 `kpafactor4115.inc`, 250 `kpafactor4250.inc`, 300 `(adc + 1.53) * 1.213675`, 400 `(adc + 2.147) * 1.6197783`, kPa |
| 4 | mapADC | same as baro |
| 5 | matADC | `matfactor.inc`, degF + 40 |
| 6 | cltADC | `thermfactor.inc`, degF + 40 |
| 7 | tpsADC | `throttlefactor.inc`, % |
| 8 | batADC | `adc / 255 * 30` V |
| 9 | egoADC | Innovate 0-5V linear: `10 + adc * 0.039216` AFR |
| 13 | rpm100 | `* 100` rpm, 100 rpm steps. Not used. |
| 22 | iTime (U16) | low 16 bits of the time between ignition events, us |
| 24 | advance | `adc * 0.352 - 10` deg |
| 37 | iTimeX | high 8 bits of iTime |

The coarse `rpm100` value moves in 100 rpm steps, so megaboy uses `RpmHiRes`, like the TunerStudio gauge:

```text
60000000 * (2 - twoStroke) / (iTimeX << 16 | iTime) / nCylinders
```

Values below 21 rpm become zero. The cylinder count comes from the tune.

## Config (page 1, offset 182)

At startup, `P 01` followed by `V` reads the 189-byte page. Byte 182 tells the decoder which MAP conversion, stroke count and cylinder count to use.

| Bits | Name |
|---|---|
| 0-1 | mapType1: 115, 250, 300, 400 kPa |
| 2 | twoStroke1 |
| 4-7 | nCylinders1 - 1 |

## Conversion tables

[mkms1tables.py](../tools/mkms1tables.py) generates the C and Python lookup tables from TunerStudio's inc files. If your project uses custom sensor tables from EasyTherm, pass that project's inc folder when regenerating them:

```
python3 -I tools/mkms1tables.py --inc ~/TunerStudioProjects/your-project/inc
```

## Write safety

Without a CRC, a damaged write can change the tune without an error from the ECU. The GBC asks the bridge to read back the final value of each edit. Intermediate steps are written to the ECU and recorded in the bridge's cache without a readback: reading a whole page after every step would add about 200 ms per step at 9600 baud.

That leaves intermediate writes unchecked. Burning is a separate action in the editor. See the [tuning guide](tuning.md) for the write limits and discard behavior.
