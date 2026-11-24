# MS2/Extra protocol

This is the MS2/Extra layout used by the bridge. It comes from [mainController.ini](../ecu/mainController.ini), with signature `MS2Extra comms332m2` and `CAN_COMMANDS` enabled. Check the matching INI before adapting it to another firmware version.

## Link

The serial link runs at 115200 baud, 8N1. Requests and responses use the `msEnvelope_1.0` envelope:

```text
[len u16 BE][payload][crc32 u32 BE]
```

The CRC32 covers the payload. Response payloads begin with a status byte, followed by the requested data. Multi-byte fields are big endian, and the bridge reads or writes at most 256 bytes per request.

## Commands (payload)

| Purpose | Bytes |
|---|---|
| Signature | `r 00 0f 0000 0014` (20 bytes, expect `MS2Extra comms332m2`) |
| Version text | `r 00 0e 0000 003c` (60 bytes) |
| Realtime block | `r 00 07 <offset u16> <count u16>`, full block 209 bytes |
| Page read | `r 00 <page> <offset u16> <count u16>` |
| Page write | `w 00 <page> <offset u16> <count u16> <data>` |
| Burn | `b 00 <page>` |

The `00` in each command is the CAN id of the main ECU. The INI lists pages `04 05 0a 08 09 0b 0c`, each 1024 bytes long. The [tuning guide](tuning.md) maps the editable tables to those pages.

## Realtime channels used by the dash

| Channel | Offset | Type | Conversion |
|---|---|---|---|
| rpm | 6 | U16 | raw |
| advance | 8 | S16 | raw / 10 deg |
| engine | 11 | U08 | status bits |
| barometer | 16 | S16 | raw / 10 kPa |
| map | 18 | S16 | raw / 10 kPa |
| mat (IAT) | 20 | S16 | (raw - 320) / 18 C |
| coolant | 22 | S16 | (raw - 320) / 18 C |
| tps | 24 | S16 | raw / 10 % |
| batteryVoltage | 26 | S16 | raw / 10 V |
| afr1 | 28 | S16 | raw / 10 AFR |

Watch the temperature units here. The ECU sends degrees F x 10 even when TunerStudio is set to Celsius. `CELSIUS` only changes how the INI converts the value. The formulas above give degrees C; the bridge keeps tenths of a degree for the GBC link.

For raw MAP and barometer values, boost in psi is `(map - barometer) / 10 * 0.145038`. Negative values indicate vacuum.

A 30-byte read starting at offset 0 covers all of these channels in one request.
