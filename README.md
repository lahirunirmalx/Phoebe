# Phoebe

Going open source — feel free to compile and play around if you're interested.

- Demo video: [bilibili.com/video/BV1qCBBYkEXS](https://www.bilibili.com/video/BV1qCBBYkEXS)
- Hardware: [oshwhub.com/eedadada/phoebe-4-real](https://oshwhub.com/eedadada/phoebe-4-real)

---

- New Mooncake project framework
- JS runtime with UI and HAL API bindings — usable for watch faces or apps
- A more ergonomic LVGL C++ binding, with transition animation support
- `AppClaudeMeter` — Claude API usage meter (5h / 7d bars) with an
  integrated analog clock face. Click toggles between clock and meter
  views. Polls `<base>/usage` every 5 minutes via the HAL HTTP client
  (libcurl on desktop, Arduino HTTPClient on ESP32). See **Configuration**
  below.

## Desktop build

Fetch dependencies (submodules):

```bash
git submodule update --init --recursive
```

If you cloned without `--recurse-submodules`, the command above pulls in everything under `dependencies/`.

Install the desktop build dependencies (Ubuntu/Debian):

```bash
sudo apt install libsdl2-dev libcurl4-openssl-dev cmake build-essential
```

Build:

```bash
mkdir build && cd build
cmake .. && make
```

Run from the repo root (so the runtime config files are picked up from
the working directory):

```bash
./build/desktop/app_desktop_build
```

Click anywhere in the LVGL window to toggle between the clock view and
the Claude meter view.

## Configuration

Two runtime configs live next to the binary's working directory and are
**gitignored** because they contain secrets. Templates are provided.

### WiFi credentials (`wifi_config.json`)

```bash
cp wifi_config.example.json wifi_config.json
$EDITOR wifi_config.json
```

Or seed them via env vars on first run; the desktop impl will write the
JSON for you:

```bash
PHOEBE_WIFI_SSID=mywifi PHOEBE_WIFI_PASSWORD=secret \
  ./build/desktop/app_desktop_build
```

On desktop the WiFi manager is a simulation — the OS already has WiFi,
so `connect()` just records the credentials. On ESP32 it actually drives
the radio (Arduino `WiFi.begin()`) with credentials persisted to NVS
namespace `wifi`.

### Claude API endpoint (`claude_config.json`)

```bash
cp claude_config.example.json claude_config.json
$EDITOR claude_config.json
```

Or seed via env vars:

```bash
PHOEBE_CLAUDE_BASE=http://127.0.0.1:7878 \
PHOEBE_CLAUDE_BEARER=sk-... \
  ./build/desktop/app_desktop_build
```

Schema is intentionally identical to the M5Cardputer-UserDemo NVS layout
(`claude` namespace, keys `base` / `bearer`), so the same `flash_nvs.sh`
CSV used on the Cardputer works unchanged on the ESP32 build of phoebe.

The HTTP fetcher hits `<base>/usage` with `Authorization: Bearer <token>`
and parses `five_hour.utilization` / `seven_day.utilization` out of the
JSON response. Bar colors: green (<70%), orange (70–90%), red (≥90%).

---

## Ember's Trace

*A fire that once burned fiercely in the depths of the soul, now reduced to a faint flicker — whispering of vows left unfulfilled.*

- **Type:** Unfinished weapon
- **Attack:** 0 → ∞ (depends on the wielder's resolve)
- **Durability:** 3/99 (slowly crumbling under weariness)
- **Weight:** 5.5
- Stat requirements:
  - Strength: 12
  - Faith: 8
  - Endurance: no upper limit

### Skill: Will of the Forsaken

Consume all remaining conviction and patience to reforge this unfinished blade into a weapon of one's own — its form and stats shaped by the wielder. *(Note: the path is long and lonely; failure means returning to the void.)*

> *"It never took shape, yet that faint light still waits for the fearless to temper it."*
