# Waveshare ESP32-P4-NANO — AURA-902

[ESP32-P4-NANO](https://www.waveshare.com/esp32-p4-nano.htm) with a round 240 × 240 **GC9A01** SPI display,
the on-board **ES8311** audio codec and a **DHT11** temperature/humidity sensor.
Wi-Fi goes through the on-board ESP32-C6 (ESP-Hosted).

## Wiring

| Signal        | GPIO | Notes |
|---------------|------|-------|
| GC9A01 SCK    | 47   | VDDPST_5 domain, needs LDO4 (see below) |
| GC9A01 MOSI   | 48   | VDDPST_5 domain |
| GC9A01 DC     | 46   | VDDPST_5 domain |
| GC9A01 CS     | 24   | USB-Serial-JTAG D- (see below) |
| GC9A01 RST    | 5    | |
| GC9A01 VCC / GND | 3V3 / GND | backlight pin left unconnected (always on) |
| DHT11 DATA    | 4    | internal pull-up enabled |
| TTP223 SIG    | 6    | active high; touch toggles the chat like BOOT, long press (1.5 s) cycles volume 30/60/90 |
| PCA9685 SDA/SCL | 7 / 8 | shares the codec I2C bus, addr 0x40; 6 x MG90S (ch 0-3 legs FL/FR/RL/RR, 4 pan, 5 tilt); servo V+ from a separate 5 V >= 3 A supply |

## ESP32-P4 pitfalls handled in `esp32-p4-nano.cc`

- **GPIO39–48 are unpowered by default.** They belong to the VDDPST_5 (SD) IO domain, fed by on-chip
  LDO channel 4. ESP-IDF only enables it when something acquires it, so the board enables LDO4 at
  3.3 V before starting SPI. Without it every SPI transfer returns `ESP_OK` but the panel stays black.
- **GPIO24 is USB-Serial-JTAG D-.** The board disables the USB pad so the pin can drive CS.
  As a consequence the USB-Serial-JTAG console/JTAG stops working after boot; the UART console is unaffected.

## Features

- Xiaozhi UI on the GC9A01 (mirrored on X) with a kid-friendly robot face instead of emoji:
  100 moods built from parts (eyes, mouth, extras) with per-mood animations, navy background, blue strokes, red eyes when angry (`aura_display.h`). MCP tool `self.face.set_mood` shows any of them on request. Panel colour order is BGR.
  UI text font reduced to 20 px in `main/CMakeLists.txt` for the 240 px screen.
- MCP tool `self.environment.get_temperature_humidity` reads the DHT11
  (ask e.g. "what's the temperature?"), and a badge at the bottom of the screen shows it (refreshed every 10 s, `aura_display.h`).
- Wake word **"Hi ESP"** (`wn9_hiesp`), or press the BOOT button.
- The robot's mouth moves while the assistant speaks.
- Optional body (`aura_motion.h`): walking, gestures and head moves by voice (`self.robot.*` MCP tools), small
  reactions to the face, smooth ramps and relaxed servos at rest; per-servo trims in NVS. Inert without a PCA9685.
- Temperature alerts: above `AURA_TEMP_HOT_C` / below `AURA_TEMP_COLD_C` (config.h) the robot shows a
  notification and a matching face, and the badge turns red / light blue.

## Build

With `scripts/build.py` (Spanish UI and prompts; selects the wake word automatically: Hi ESP for non-Chinese languages):

```bash
python scripts/build.py esp32-p4-nano --name esp32-p4-nano-aura902-gc9a01 --language es-ES
```

With plain `idf.py`, a fresh `sdkconfig` starts from `sdkconfig.aura902` (Spanish, Hi ESP, ESP-Hosted
mempool in PSRAM — required, otherwise ESP-Hosted asserts at boot); the root `CMakeLists.txt` adds it to
`SDKCONFIG_DEFAULTS`. On a fresh clone run `idf.py set-target esp32p4` first.

```bash
idf.py build
idf.py -p COM5 flash monitor
```
