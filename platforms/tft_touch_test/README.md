# TFT + touch pin-discovery test — Freenove ESP32 Mini TV (FNK0112)

Standalone ESP-IDF app (separate from the phoebe app layer) that confirmed the
**240×240 ST7789V SPI display** and the **ESP32 capacitive touch pad T9 (GPIO32)**
on the **Freenove FNK0112 "ESP32 Mini TV"** board. Both are verified working.

Target: classic **ESP32 WROOM-32** (CH340 → `/dev/ttyUSB0`), ESP-IDF v4.4.

## Confirmed pinout  ✅

Pins are baked into [`main/st7789.cpp`](main/st7789.cpp) / [`main/touch_t9.cpp`](main/touch_t9.cpp).
This is an **all-in-one board** (screen routed to the ESP32 on-PCB), so there is
nothing to rewire — the mapping is fixed by the hardware. Sourced from the
board's ESPHome config and verified on-device.

| Signal            | GPIO | Notes                                                        |
|-------------------|-----:|--------------------------------------------------------------|
| SPI SCLK          | 14   | HSPI                                                         |
| SPI MOSI          | 13   | HSPI                                                         |
| DC                | 2    | data/command                                                |
| CS                | 15   | (board may also tie CS to GND)                              |
| RESET             | —    | **not wired** → software reset (`SWRESET 0x01`) only        |
| Backlight (BL)    | 19   | **active-LOW** (drive low = on)                             |
| Panel VDD enable  | 21   | **active-LOW** — must be driven LOW or the panel is unpowered |
| Touch             | 32   | capacitive pad T9; idle ≈ 1400, **drops** when touched      |
| MISO              | —    | unused (write-only display)                                 |

**Display config:** SPI mode 0, **20 MHz** (the board "does not work at 80 MHz"),
240×240, offset 0/0, **colors inverted** (`INVON 0x21`).

> The two active-low enables (VDD on GPIO21, BL on GPIO19) were the cause of the
> initial dead-black screen — nothing lights up until GPIO21 is pulled low.

## Build / flash / monitor

```bash
./flash.sh                 # build + flash /dev/ttyUSB0 + serial monitor
./flash.sh --no-monitor    # build + flash only
SERIAL_PORT=/dev/ttyUSB1 ./flash.sh
```
Exit the monitor with **Ctrl-]**.

> Toolchain note: `export.sh` on this machine does not add the classic
> `xtensa-esp32-elf` toolchain to PATH (only the s3 one). `flash.sh` re-adds it
> automatically; if building by hand, prepend
> `~/.espressif/tools/xtensa-esp32-elf/*/xtensa-esp32-elf/bin` to PATH.

## What it does

1. **Display:** color sweep on boot — RED → GREEN → BLUE → WHITE → BLACK
   (~0.7 s each), then solid BLUE.
2. **Touch:** screen turns GREEN while the pad is touched, BLUE on release.
   The raw touch value is streamed over serial (~2.5 Hz); touch is detected as a
   ±12 % deviation from the measured idle baseline (direction-agnostic).

## Carrying the result into phoebe

These pins are the answer for running phoebe on this board. **Note:** phoebe's
`platforms/esp32_idf_test` currently drives a 128×64 **SSD1306 over I²C**, so
switching to this 240×240 ST7789 SPI panel is **more than a pin change**:

1. `HAL_SCREEN_WIDTH/HEIGHT` → `240`/`240` and the SPI + control pins added to
   [`../esp32_idf_test/main/hal/hal_config.h`](../esp32_idf_test/main/hal/hal_config.h).
2. An **ST7789 SPI backend** in the display HAL component (the current one talks
   I²C to the SSD1306). The init + `set_window`/fill logic here ports directly
   into the LVGL `flush_cb`. Don't forget the GPIO21 VDD-enable + GPIO19
   backlight (both active-low).
3. LVGL draw-buffer sizing bumped for the larger panel.
