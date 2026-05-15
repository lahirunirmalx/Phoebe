# Phoebe

Going open source — feel free to compile and play around if you're interested.

- Demo video: [bilibili.com/video/BV1qCBBYkEXS](https://www.bilibili.com/video/BV1qCBBYkEXS)
- Hardware: [oshwhub.com/eedadada/phoebe-4-real](https://oshwhub.com/eedadada/phoebe-4-real)
- Sister project (data source for `AppClaudeMeter`):
  [github.com/lahirunirmalx/claude-usage-exporter](https://github.com/lahirunirmalx/claude-usage-exporter)
  — tiny local HTTP server + Prometheus exporter for Claude Code's
  `/usage` data. Set its base URL + bearer in `system_config.json`; see
  **Configuration**.

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

Runtime configs live next to the binary's working directory and are
**gitignored** because they contain secrets. Committed templates show
the schema.

### App preferences + Claude endpoint (`system_config.json`)

```bash
cp system_config.example.json system_config.json
$EDITOR system_config.json
```

Fields:

| Key              | Purpose                                              |
| ---------------- | ---------------------------------------------------- |
| `mute`           | global mute                                          |
| `hapticFeedback` | enable haptic motor (ESP32)                          |
| `watchFace`      | watch-face id (legacy)                               |
| `widgetA`/`B`    | watch-face widget slots                              |
| `claudeBase`     | usage server base URL, e.g. `http://127.0.0.1:7878`  |
| `claudeBearer`   | API token sent as `Authorization: Bearer <token>`    |

Or seed the Claude fields via env vars on first run; the desktop impl
will write them into `system_config.json` for you:

```bash
PHOEBE_CLAUDE_BASE=http://127.0.0.1:7878 \
PHOEBE_CLAUDE_BEARER=sk-... \
  ./build/desktop/app_desktop_build
```

The HTTP fetcher hits `<claudeBase>/usage` with `Authorization: Bearer
<claudeBearer>` and parses `five_hour.utilization` /
`seven_day.utilization` out of the JSON response. Bar colors:
green (<70%), orange (70–90%), red (≥90%).

The expected endpoint shape is provided by
[claude-usage-exporter](https://github.com/lahirunirmalx/claude-usage-exporter)
(run it locally and point `claudeBase` at e.g. `http://127.0.0.1:7878`).
Any service returning the same JSON shape will work just as well.

### WiFi credentials (`wifi_config.json`)

```bash
cp wifi_config.example.json wifi_config.json
$EDITOR wifi_config.json
```

Or seed via env vars on first run:

```bash
PHOEBE_WIFI_SSID=mywifi PHOEBE_WIFI_PASSWORD=secret \
  ./build/desktop/app_desktop_build
```

On desktop the WiFi manager is a simulation — the OS already has WiFi,
so `connect()` just records the credentials. On ESP32 it actually drives
the radio (Arduino `WiFi.begin()`) with credentials persisted to NVS
namespace `wifi`.

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
