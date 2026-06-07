# Phoebe — Claude Meter & apps for the Freenove ESP32 Mini TV

A multi-app firmware for the **Freenove "ESP32 Mini TV" (FNK0112)** — a little
desktop cube with a 240×240 IPS screen and a single capacitive touch button.
It runs a Claude Code usage meter, watch faces, weather, and a carousel of small
apps you flip through with the touch pad.

> **Built on [Phoebe](https://oshwhub.com/eedadada/phoebe-4-real) by
> Forairaaaaa (eedadada).** Phoebe is an open-source LVGL + Mooncake watch
> firmware with a clean app/HAL framework, a JS runtime, and a C++ LVGL binding.
> **This repository reuses that project** and adds a new `esp32_minitv` hardware
> platform plus the apps documented below. Full credit for the underlying
> framework and original hardware goes to the original author —
> demo: [bilibili.com/video/BV1qCBBYkEXS](https://www.bilibili.com/video/BV1qCBBYkEXS).
>
> Claude usage data comes from the sister project
> [claude-usage-exporter](https://github.com/lahirunirmalx/claude-usage-exporter)
> — a tiny local HTTP server that exposes Claude Code's `/usage` data.

---

## Hardware needed

| Item | Notes |
| ---- | ----- |
| **Freenove ESP32 Mini TV (FNK0112)** | The cube — classic ESP32, **240×240 ST7789 SPI** display, capacitive touch pad. **[Buy on AliExpress](https://www.aliexpress.com/item/1005012027955795.html)** |
| USB‑C cable | Power + flashing (the board's CH340 enumerates as `/dev/ttyUSB0`). |
| 2.4 GHz WiFi | For the Claude meter, weather, currency, uptime, etc. |

No soldering or extra parts — the display and touch are built into the board.
Confirmed pinout (baked into the firmware): SCLK 14, MOSI 13, DC 2, CS 15,
backlight 19 *(active‑low)*, panel‑VDD enable 21 *(active‑low)*, touch pad
**T9 / GPIO32**.

## Build & flash

Toolchain: **ESP‑IDF v4.4** (Arduino is pulled in as a managed component).

```bash
git submodule update --init --recursive   # dependencies/

cd platforms/esp32_minitv
./flash.sh                 # build + flash /dev/ttyUSB0 + serial monitor
./flash.sh --no-monitor    # build + flash only
SERIAL_PORT=/dev/ttyUSB1 ./flash.sh
```

`flash.sh` sources `export.sh` and re‑adds the classic `xtensa-esp32-elf`
toolchain to PATH if it's missing. There is also a standalone
`platforms/tft_touch_test/` harness used to bring up the display + touch.

## Using it

The screen is **dark by default** (power saving). Everything is driven by the
one touch pad:

- **Tap** — wake / cycle to the next app.
- **Double‑tap** — pin the current app (no sleep, stops cycling) until you tap.
- **Long‑press (≥3 s)** — open the WiFi/settings **captive portal**.
- **5 min idle** — display turns off; a tap or a notification wakes it.

Apps in the cycle:

**clock** (watch faces) → **Claude meter** (5h/7d rings) → **weather** →
**pomodoro** → **world clock** → **next meeting** → **currency** (FX→LKR) →
**AQI** → **3‑day forecast** → **sun & moon** → **network ping** → **uptime** →
**pet** → **screensaver**.

The backlight also **pulses** on a Claude fetch / error / limit‑reached as an
ambient notification while the screen is otherwise dark.

## First‑boot setup (captive portal)

A fresh board has no settings stored, so **long‑press** the pad to open the
portal:

1. Join WiFi **`Phoebe-Setup`** (password `12345678`).
2. Browse to `http://192.168.4.1`.
3. Set: WiFi, timezone, watch face, widgets, **Claude base + bearer**, a
   **Sri Lanka weather city**, an optional **calendar `.ics` URL** (next
   meeting), and up to **5 uptime URLs**.
4. Save — the device reboots configured.

Settings persist in NVS across re‑flashes. All data sources are free and
key‑less apart from your own endpoints:

| App | Source |
| --- | ------ |
| Weather / forecast / sun & moon / AQI | [Open‑Meteo](https://open-meteo.com) (no key) |
| Currency | `open.er-api.com` (no key) |
| Next meeting | your secret iCal `.ics` URL (streamed line‑by‑line) |
| Claude meter | [claude-usage-exporter](https://github.com/lahirunirmalx/claude-usage-exporter) (`<base>/usage`, bearer token) |
| Network / uptime | HTTP status/latency to public endpoints / your URLs |

The Claude meter colours: green (<70%), orange (70–90%), red (≥90%).

## Watch faces

The clock app picks a face from the `watchFace` setting. Faces:

- **`analog`** *(default)* — full‑screen analog clock with hour/minute/second
  hands and a dial ring; date at the bottom.
- **`digital`** — large `HH:MM` with `:SS` below and the date at the bottom.
- **`animated`** — large `HH:MM` behind a continuously rotating accent arc.
- **`seg7`** — 7‑segment LCD look: four red digits + a blinking colon; unlit
  segments stay visible as dim red, like a real LCD.
- **`vfd`** — VFD‑style 5×7 dot‑matrix in phosphor green on dark teal.

## Architecture notes

- **Dual‑core:** the UI (LVGL/Mooncake/touch) runs on APP_CPU; all networking
  (Claude + weather + the extras) runs on PRO_CPU via a single serialized fetch
  thread, so only one TLS connection is open at a time.
- **Memory:** mbedTLS dynamic buffers are enabled, large feeds are streamed
  (never buffered whole), and uptime/ping use a status‑only request that never
  downloads the page body — all to keep TLS off the heap‑exhaustion cliff.
- **Single touch:** the GPIO32 pad drives an LVGL pointer indev plus tap /
  double‑tap / long‑press gesture detection.

## Desktop simulator (development)

The shared app layer also builds as a desktop simulator (SDL + libcurl) for
working on UI without hardware:

```bash
sudo apt install libsdl2-dev libcurl4-openssl-dev cmake build-essential
mkdir build && cd build && cmake .. && make
./desktop/app_desktop_build          # run from the repo root
```

On desktop, settings live in a gitignored `system_config.json` (copy
`system_config.example.json`); WiFi is simulated and time comes from the host.

## Credits

- **Original Phoebe firmware & framework:** Forairaaaaa (eedadada) —
  [hardware](https://oshwhub.com/eedadada/phoebe-4-real) ·
  [demo](https://www.bilibili.com/video/BV1qCBBYkEXS). This project would not
  exist without it.
- **Claude usage data:**
  [claude-usage-exporter](https://github.com/lahirunirmalx/claude-usage-exporter).
