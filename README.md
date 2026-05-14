# Phoebe

Going open source — feel free to compile and play around if you're interested.

- Demo video: [bilibili.com/video/BV1qCBBYkEXS](https://www.bilibili.com/video/BV1qCBBYkEXS)
- Hardware: [oshwhub.com/eedadada/phoebe-4-real](https://oshwhub.com/eedadada/phoebe-4-real)

---

- New Mooncake project framework
- JS runtime with UI and HAL API bindings — usable for watch faces or apps
- A more ergonomic LVGL C++ binding, with transition animation support

## Desktop build

Fetch dependencies (submodules):

```bash
git submodule update --init --recursive
```

If you cloned without `--recurse-submodules`, the command above pulls in everything under `dependencies/`.

Build:

```bash
mkdir build && cd build
```

```bash
cmake .. && make
```

Run:

```bash
cd desktop
```

```bash
./app_desktop_build
```

Use WASD to navigate.

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
