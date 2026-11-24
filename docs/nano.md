# Nano test bridge (MS1/Extra only)

This is the small test bridge from before the Pico version. A Nano reads an MS1/Extra and sends the data to the GBC, enough to check the serial path and get the display going. There is no MS2 support or SD logging here. For the full setup, use the [Pico bridge](hardware.md).

## Wiring

```
D10  <- MAX232 R1OUT (pin 12)
D11  -> MAX232 T1IN  (pin 11)
D2   -> 1k -> link pin 5 (SCK)
D3   -> 1k -> link pin 3 (GBC SIN)
D4   <- link pin 2 (GBC SOUT)
5V   -> MAX232 VCC (pin 16)
GND  -> MAX232 GND (pin 15), DB9 pin 5, link pin 6
```

On the RS232 side, connect DB9 pin 2 (MS1 TX) to MAX232 R1IN, chip pin 13. Connect MAX232 T1OUT, chip pin 14, to DB9 pin 3 (MS1 RX). This setup uses a 5V MAX232; the Pico setup uses a MAX3232 at 3.3V.

Measure link pin 1 before connecting the Nano. If the link is at 3.3V, put a BSS138 level shifter between them, with HV on the Nano's 5V supply and LV on link pin 1. Do not feed the Nano's 5V outputs straight into a 3.3V link.

## Build and flash

The sketch uses the code in `common/`. For the Arduino IDE, link that directory into your Arduino libraries folder and restart the IDE. With this repository at `~/Documents/megasquirtboy`, the command is:

```
ln -s ~/Documents/megasquirtboy/common ~/Documents/Arduino/libraries/megaboy_common
```

With `arduino-cli`, run these from the repository root. Replace the serial port with the one assigned to your Nano:

```
arduino-cli compile --fqbn arduino:avr:nano --library common nano/megaboy_nano
arduino-cli upload --fqbn arduino:avr:nano -p /dev/cu.usbserial-XXXX nano/megaboy_nano
```

If a clone refuses to upload, check whether it has the old bootloader. For that version, use `--fqbn arduino:avr:nano:cpu=atmega328old` in both commands.

## Debug output

```
arduino-cli monitor -p /dev/cu.usbserial-XXXX -c baudrate=115200
```

The monitor prints the decoded readings in the same format as `ecumon`, so you can compare them with the GBC while testing.

## Test

1. From the repository root, build the ROM with `make -C gbc` and copy `gbc/build/megaboy.gbc` to the flash cart.
2. Connect the MAX232, the MS1 and the link. Reset the Nano and start the ROM. Compare the GBC with the serial monitor.
3. Remove the link cable. After 2 s, the header shows `NO LINK`.
4. Disconnect the MS1. After 1 s, the header shows `NO ECU`.

If something is missing, Menu > About shows the DATA, LINK and ECU states separately. That helps narrow it down to the ECU side or the GBC link.
