# Host tests

Tests that run the firmware on your PC, no ESP32 needed.

```sh
python test/run_tests.py                     # button test + full simulation
python test/run_tests.py --baseline HEAD~1   # also compare against another revision
```

## Requirements

- Python 3.8+
- A C/C++ compiler: gcc or clang on your `PATH`, or `pip install ziglang`
  (easiest on Windows). `CC`, `CXX` and `AR` are honoured if set.
- U8g2: taken from your Arduino libraries folder, `--u8g2 PATH` or `U8G2_DIR`.
  If none is found, U8g2 2.36.19 is downloaded into `test/.build/`.

The first run builds U8g2 (about 30 s); later runs take a few seconds.

## What is tested

**`button_test.cpp`** compiles the real `Button.ino` against a mocked clock and
pin. It replays single, double and triple clicks, long press and click+hold,
with and without contact bounce, at `loop()` periods of 1, 14 and 20 ms.

**`sim/`** runs the whole sketch against the real U8g2 C library with mocked
Arduino, FFat and Preferences. Only the I2C transport is replaced: every byte
sent to the display is decoded by a model of the SH1106's RAM. A scripted
session opens every screen, plays videos, pauses, restarts, spins the slot
machine, triggers a jackpot and resets the history. It checks that:

- every video frame is shown, in order, including after pause and restart
- nothing is sent while a video is paused or a placeholder screen is idle
- after every `loop()` the panel shows exactly what is in U8g2's buffer
  (catches partial updates that miss a changed area)
- no I2C write overflows the ESP32's 128-byte Wire buffer

It also saves key screens and I2C metrics. `--baseline REF` runs the same
session on another git revision and reports every screen that differs,
pixel by pixel, plus the metrics side by side. The run fails if any screen
differs, so expect that when a change is meant to look different.

`--show-screens` prints a few screens as ASCII art.

## Limits

- Timing is modelled, not measured. Each I2C transaction costs its bytes at
  the configured bus clock plus a fixed overhead (`--txn-us`, default 50 µs).
  CPU time is not modelled.
- FFat, NVS and the button are mocks, so this does not replace testing on the
  real device (display quirks, flash speed, power).

`SIM_STOCK_CAD=1` and `SIM_CLOCK=<Hz>` switch the simulated display back to
U8g2's stock I2C transport or another bus clock, to measure each optimisation
on its own.
