# Hardware

These are my wiring notes for the Pico bridge. Most of it is ordinary UART, SPI and I2C wiring, but the modules have a few details worth checking before soldering everything together. Start with the MAX3232 pin labels, the SD module's regulator and the voltage on the ECU's DB9 supply pin.

## Parts

| Part | Choice |
|---|---|
| Flash cart | Game Flash GB Pro+ (OS V4 clone) |
| MCU | RP2040 board, Pico pinout, USB-C |
| RS232 transceiver | MAX3232 mini module (no DB9) |
| DB9 | Male DB9 |
| Level shifter | 4 channel BSS138 module |
| SD | TF SPI module |
| Link cable | GBC link cable, cut |
| RTC (optional) | DS3231 module |

## Pin map

| Function | GPIO | Pico pin |
|---|---|---|
| MS2 UART0 TX | GP0 | 1 |
| MS2 UART0 RX | GP1 | 2 |
| SD SPI0 SCK | GP2 | 4 |
| SD SPI0 MOSI | GP3 | 5 |
| SD SPI0 MISO | GP4 | 6 |
| SD CS | GP5 | 7 |
| RTC I2C1 SDA (optional) | GP6 | 9 |
| RTC I2C1 SCL (optional) | GP7 | 10 |
| GB link SCK | GP26 | 31 |
| GB link out (to GBC SIN) | GP27 | 32 |
| GB link in (from GBC SOUT) | GP28 | 34 |
| 3.3V out | 3V3 | 36 |
| External power in | VSYS | 39 |
| GND | GND | 38 |

## MS2 serial (MAX3232 mini)

The MAX3232 sits between the ECU's RS232 port and the Pico's UART. This pin map is for the small module without a DB9 connector:

```
Pico 3V3  -> +            DB9 pin 2 (MS2 TX) -> R1IN  (chip pin 13)
Pico GND  -> -            DB9 pin 3 (MS2 RX) <- T1OUT (chip pin 14)
GP0 (TX)  -> T1IN  (chip pin 11)
GP1 (RX)  <- R1OUT (chip pin 12)
                          DB9 pin 5 (GND)    -> -
```

Power this module from 3.3V. At 5V, its R1OUT can put 5V onto GP1, which the RP2040 cannot tolerate. Add 10 uF and 0.1 uF across the supply near the module.

Some boards label the TTL pins from the other device's point of view, so a pin marked TXD can be an input. Follow the chip pins in the diagram. If your module has its own female DB9, check its wiring: a DCE module has the same arrangement as the ECU and needs pins 2 and 3 crossed between them.

There is also an annoying startup problem. During reset, the RP2040 pulls GP0 low. The MAX3232 turns that into a BREAK on the serial line. If the ECU boots at the same time, it can stay in its serial monitor. On MS2, the fuel pump won't prime and the engine won't start, even though the wiring looks right.

Add a 10 kOhm pull-up from GP0 to 3.3V. The firmware also drives GP0 high as its first step and leaves the ECU alone for the first 3 seconds, but the resistor covers the time before the firmware runs.

Before connecting the ECU, you can test the serial path by shorting pins 2 and 3 on the bridge-side DB9. During the MS1 signature request, the bridge should receive its own command and log `got 1 of 32` instead of `got 0 of 32`. Remove the short afterward. On the powered MS2, TX should idle at roughly -5 to -10V relative to DB9 pin 5; check that with a meter before soldering the cable.

## GBC link (BSS138)

```
LV  <- Pico 3V3           HV  <- link pin 1 (GBC VCC)
GND <- Pico GND           GND <- link pin 6 (GND)
LV1 <- GP26               HV1 -> link pin 5 (SCK)
LV2 <- GP27               HV2 -> link pin 3 (GBC SIN)
LV3 <- GP28               HV3 <- link pin 2 (GBC SOUT)
```

The high side gets its supply from the GBC, rather than an always-on 5V rail. That keeps the level shifter's pull-ups from feeding power into the GBC while it is off. Measure link pin 1 with the GBC on; if it is 3.3V, you do not need the shifter.

Once the link works, select MS1 or MS2 under SETTINGS > ECU. The bridge saves that choice in flash and sends nothing to the ECU until a type is selected. SETTINGS > ECU HZ controls the MS2 poll rate, from 10 to 40 Hz with a default of 20. MS1 runs as fast as its 9600-baud link allows, about 21 Hz in the bench setup.

## SD card

```
SD CS   <- GP5 (pin 7)
SD SCK  <- GP2 (pin 4)
SD MOSI <- GP3 (pin 5)
SD MISO -> GP4 (pin 6)
SD GND  -- GND
```

Check which SD module you have before connecting VCC:

| Module | Supply |
|---|---|
| AMS1117 regulator and 74LVC125 buffer | At least 4.5V at VCC. USB VBUS, pin 40, works on the bench. |
| No regulator | 3.3V only. Feeding 5V here can destroy the card. |

For the car wiring, the AMS1117 module can run from 3V3 after removing the regulator and bridging its input pad to its output pad. Keep the SPI wires at 10 cm or less; the bus runs at 8 MHz after initialization.

Modules with a 74HC125 buffer have another catch: the input can float before the card enters SPI mode, leaving MISO stuck low. Add a 10 kOhm pull-up on the card side of the buffer, from microSD pin 7 (DAT0) to 3.3V.

Logs are tab-separated MegaLogViewer files, recorded at the ECU poll rate. With a set RTC, filenames use the start time, for example `2026-11-14_09.05.07.msl`. Otherwise they use `LOGnnnn.MSL`. The bridge syncs once per second, so a power cut can lose roughly the last second of buffered data.

Press SELECT on a gauge page or A on the LOG page to start and stop recording. AUTO LOG can start a recording when the engine starts.

## RTC (optional, DS3231)

```
Pico 3V3 -> RTC VCC
Pico GND -> RTC GND
GP6      -> RTC SDA
GP7      -> RTC SCL
```

The DS3231 uses I2C address `0x68`. Set it from SETTINGS > CLOCK: left and right select a field, up and down change it, and A sends the date and time. The bridge checks the date, writes it to the RTC and clears the oscillator stop flag.

`NO RTC` means the bridge cannot read the module. `UNSET` means the clock needs setting after a loss of RTC power. Without a usable RTC, logging still works with numbered filenames. Files get a placeholder date of 2026-01-01 and no capture date line.

Check the battery circuit too. Many DS3231 modules include a charging circuit. Remove its charging diode or resistor if you fit a non-rechargeable CR2032; the charging arrangement is intended for an LIR2032.

## Power

The bench MS1 supplies 5V on DB9 pin 9, and this wiring uses it to power the bridge. Do not assume another ECU is wired the same way: measure the MS2 before connecting it and confirm the supply can deliver 150 mA. This is separate from the sensor 5V reference.

```
DB9 pin 9  -> polyfuse 200 mA -> 2-5 ohm -> + 470 uF -> Schottky diode -> VSYS (pin 39)
DB9 pin 5  -----------------------------------------------------------> GND
```

The 470 uF capacitor helps with the current peaks during SD writes; the series resistor limits the capacitor's inrush current. The Schottky diode prevents USB power from flowing backward into the ECU through VSYS.

Using USB and DB9 power together also depends on the Pico board having its VBUS-to-VSYS diode. Check the board, especially if it is a clone. If either that diode or the external Schottky is missing, do not connect both supplies at once. Without the external diode, USB can feed the ECU's 5V rail even with the ECU switched off.
