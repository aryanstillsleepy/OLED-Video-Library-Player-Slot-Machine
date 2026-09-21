# OLED Video Library Player & Slot Machine

ESP32 firmware for a 128×64 SH1106 OLED that uses a single push button to drive:

- **Video library + player**: plays `.bin` videos stored on the FFat partition.
- **Slot machine**: three-reel slot game with a pity system, jackpot animation and
  persistent stats (saved to NVS).
- Placeholder screens for Music, Messages and Settings.

## Repository layout

```
.
├── README.md
├── OLED_Device/                 Arduino sketch folder (open OLED_Device.ino)
│   ├── OLED_Device.ino          setup(), loop(), app state machine, fast I2C transport
│   ├── Config.h                 pins, I2C clock, button timing, video constants
│   ├── Button.ino               debounced single/double/triple click, long press, click+hold
│   ├── Home.ino                 home menu + placeholder screens
│   ├── Videos.ino               video library + player
│   ├── Slot.ino                 slot machine game
│   └── src/SlotAssets/          bitmaps used by the slot machine
│       ├── slot_machine_blank.h   128×64 machine artwork
│       ├── jackpot_win.h          128×64 jackpot screen
│       └── *_sprite.h             16×16 reel symbols
└── test/                        host tests: run the firmware on a PC (see test/README.md)
    ├── run_tests.py             builds and runs everything
    ├── button_test.cpp          button gesture timing
    └── sim/                     full-sketch simulator with an SH1106 display model
```

The Arduino IDE requires the main `.ino` file to live in a folder with the same
name, which is why the sketch is in `OLED_Device/`. Assets live under `src/` so
every Arduino IDE / arduino-cli version copies them into the build.

## Hardware

| Part        | Connection                                   |
|-------------|----------------------------------------------|
| ESP32 board | any ESP32 with an FFat partition             |
| SH1106 OLED | I2C: SDA → GPIO 8, SCL → GPIO 9              |
| Push button | GPIO 4 → GND (uses the internal pull-up)     |

Pins and timings can be changed in `OLED_Device/Config.h`.

The display runs its I2C bus at **800 kHz** (`OLED_I2C_CLOCK_HZ`). That is
above the SH1106's rated 400 kHz but works on most modules and roughly halves
the time per frame. If the picture glitches, set it back to `400000`.

## Performance notes

- Video frames are stored in the display's native page layout, so they are read
  straight into U8g2's frame buffer with no per-pixel conversion.
- Only the 8×8 tiles that changed since the previous frame are sent over I2C.
  Static areas of a video cost nothing, and a frame that changes everywhere
  costs the same as a full refresh.
- A custom U8g2 I2C transport uses ESP32's 128-byte Wire buffer instead of
  U8g2's AVR-sized 24-byte chunks: 24 I2C transactions per full frame instead
  of 64.
- The slot machine artwork is pre-rendered once, and only the reel area is sent
  while the reels spin.

## Building

1. Install the **ESP32 Arduino core** (Espressif) and the **U8g2** library.
2. Open `OLED_Device/OLED_Device.ino`.
3. Pick your board and a **partition scheme that includes FFat/FATFS**.
4. Upload.

With arduino-cli (example for an ESP32-S3):

```sh
arduino-cli compile --fqbn esp32:esp32:esp32s3 OLED_Device
```

## Tests

```sh
python test/run_tests.py
```

Runs the button logic and the whole sketch on your PC against the real U8g2
library and a model of the SH1106 display. It checks every screen, video
playback and the I2C traffic. Needs gcc/clang or `pip install ziglang`; see
[test/README.md](test/README.md).

## Controls

| Screen           | Click             | Double-click   | Triple-click     | Long press       | Click + hold |
|------------------|-------------------|----------------|------------------|------------------|--------------|
| Home             | next item         | open item      | –                | –                | –            |
| Video library    | next video        | play           | –                | back to Home     | –            |
| Video player     | pause / resume    | restart        | –                | back to library  | –            |
| Slot machine     | –                 | spin           | open history     | back to Home     | forced 777 (not counted in stats) |
| Slot history     | –                 | –              | reset prompt     | back to machine  | –            |
| Reset prompt     | toggle YES / NO   | confirm        | –                | back to history  | –            |
| Music / Messages / Settings | –      | –              | –                | back to Home     | –            |

## Video file format

Videos are raw `.bin` files in the root of the FFat partition. All integers are
little-endian.

| Offset | Size | Field                              |
|-------:|-----:|------------------------------------|
| 0      | 4    | magic `"OLED"`                     |
| 4      | 2    | width (must be 128)                |
| 6      | 2    | height (must be 64)                |
| 8      | 4    | frames per second × 1000           |
| 12     | 4    | frame count                        |
| 16     | 4    | frame size (must be 1024)          |
| 20     | …    | `frame count × 1024` bytes of frames |

Each frame uses the native SSD1306/SH1106 page layout: 8 pages of 128 bytes,
each byte is a vertical strip of 8 pixels with the least significant bit at the
top, and a set bit is a lit pixel. The file size must be exactly
`20 + frame count × 1024` bytes or the player rejects it.

To get videos onto the board, put the `.bin` files in `OLED_Device/data/` and
upload them with an FFat/FATFS filesystem uploader for your IDE.
