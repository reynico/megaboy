# Performance

These are the results from the bench work on 2026-11-14: MS1/Extra, a Pico bridge, and the GBC ROM running in PyBoy in CGB double-speed mode. They describe that setup, so keep that in mind when comparing another ECU or real GBC hardware.

There were two places to look: how much time the Game Boy spent drawing, and how much time the bridge spent waiting. Both mattered. A slow screen update threw off the ROM's timing, while an SD card probe could stop the bridge for half a second.

## GBC draw cost

To measure drawing, I used a test copy of the ROM that counted scan lines with `sys_time` and `LY`. It kept the counters at a fixed WRAM address for the emulator to read. One frame is 154 lines.

The test changed every value on every tick, deliberately giving the renderer more work than a mostly steady dash would. These are the before and after numbers for that workload:

| Screen | Before | After |
|---|---|---|
| DASH | 475 lines (3.1 frames) | 191 (1.2) |
| BOOST | 313 (2.0) | 107 (0.7) |
| LOG | 157 (1.0) | 22 (0.15) |
| MENU, SETTINGS, TUNE | 38-56 | 12-29 |

The SM83 has no divide instruction. That made the 16-bit and 32-bit divisions in `fmt_int`, `fmt_x10` and `fmt_round_x10` surprisingly expensive for something as ordinary as printing a number. Those functions now get digits by subtracting powers of ten. A host test compares all 65,536 int16 values against `snprintf`.

The bars had the same problem: `ui_bar` used a 32-bit multiply and divide. `fmt_bar_px` now does the job with 16-bit math, within one pixel of the old result. Large text also did unnecessary work, searching the glyph table four times per character. It now looks up each character once.

Then there was the drawing itself. Every field used to be redrawn on every update, even if the value had not moved. `ui.c` now keeps a copy of the tile and attribute maps and only writes changed bytes. `ui_changed` skips formatting and drawing when both the value and color stay the same. The table editor follows the same approach: changed cells only, with the axes redrawn when scrolling.

This also exposed a timing bug. The main loop counted iterations as frames, but drawing the dash could take three frames. Sound, uptime and blinking then ran three times too slowly. The loop now measures elapsed frames with `sys_time`.

## Bridge loop (MS1, 9600 baud)

On the Pico, a test build timed each part of the loop with `time_us_64` and printed the average and maximum over USB every five seconds.

| Phase | Before | After |
|---|---|---|
| ECU poll (`R`, 39 bytes) | 42.7 ms | 42.7 ms |
| RT frame to GBC | 9.75 ms | 9 us (queued) |
| SD row write | - | 0.2 ms, max 2.7 ms |
| 1 Hz block (RTC, frames, SD sync) | 17 ms | 11-14 ms, max 19 ms |
| SD probe without a card | 518 ms every 5 s | about 1 ms |
| Rows per second | 17.2 | 21 |

Sending a realtime frame to the GBC used to hold up the bridge for 9.75 ms. Link bytes now go into a queue, and `gb_pump` sends them while the bridge is waiting for the ECU. A timestamp enforces the 150 us gap between bytes. The 9 us figure above is the time to queue the frame; sending it over the wire still takes time.

The MS1 needed a little breathing room too. Sending a command straight after its last reply byte produced eight timeouts in a minute. With a 5 ms gap between commands, the 60-second test had none.

The long SD probe came from waiting for a ready signal before CMD0. At that point the card does not drive DO yet, so a buffered module can leave MISO low. Initialization now sends CMD0 without that first ready wait and uses a 50 ms ready timeout during initialization.

After those changes, the MS1 serial link is the main limit. A 39-byte `R` reply alone takes about 41 ms at 9600 baud, before command overhead and the gap.

## SD card on core 1

Even after fixing initialization, an SD card can take too long to answer. All card work now runs on the RP2040's second core: probing, opening files, writing rows, syncing and closing. Core 0 keeps polling the ECU and handling the GBC link, so a slow or stuck card no longer holds up the dash.

Rows and start/stop commands share a 64-item queue. Keeping them in order means a stop comes after the rows already queued for that file, and those rows do not spill into the next recording.

When the queue fills, new rows are dropped and counted. Start and stop commands wait in a separate pending list on core 0 without blocking ECU polling. A stop is always accepted; if that list is full, it cancels the last start that has not been delivered yet. A start can be refused, in which case no new recording begins.

Core 1 takes at most eight queued items per pass, leaving time for the once-per-second sync. Configuration writes to the Pico's flash use `flash_safe_execute`, which pauses core 1 while flash is being written. The queue behavior has host tests:

```sh
make -C bridge/test test
```
