# Kora — AURA robot on the Waveshare ESP32-P4-NANO

Kora is the AURA-902 robot (round GC9A01 face, DHT11, TTP223 touch, optional 4-leg + pan/tilt body)
with its own board identity, on the same hardware: **Waveshare ESP32-P4-NANO** (ESP32-P4 chip v1.3,
16 MB flash, 32 MB PSRAM) with an **ESP32-C6** Wi-Fi 6 / BLE co-processor over SDIO (ESP-Hosted).
Spanish UI, "Hi ESP" wake word.

Shared code comes from the AURA-902 board (`main/boards/waveshare/esp32-p4-nano`): `aura_display.h`
(100-mood animated robot face), `aura_motion.h` + `pca9685.h` (servos), `dht11_sensor.h`.

> Not for the ESP32-P4-NANO-**WIFI6-DB**: that board has an ESP32-C5 co-processor, a v3 P4 chip and
> GPIO6 wired to the C5, so it needs a different configuration.

## Wiring (same as AURA-902)

| Signal | GPIO | Notes |
|---|---|---|
| GC9A01 SCK / MOSI / DC | 47 / 48 / 46 | VDDPST_5 domain: the board enables LDO4 at 3.3 V |
| GC9A01 CS | 24 | USB-Serial-JTAG D-: the board releases the USB pad |
| GC9A01 RST | 5 | VCC 3V3, backlight unconnected |
| DHT11 DATA | 4 | internal pull-up |
| TTP223 SIG | 6 | active high |
| PCA9685 SDA / SCL | 7 / 8 | shared with ES8311, addr 0x40; servo V+ from a separate 5 V >= 3 A supply |

On-board (do not use): ES8311 I2C 7/8 + I2S 13/12/10/9/11, PA 53, ESP32-C6 SDIO 14–19, C6 reset 54,
BOOT 35, UART0 37/38.

## Build

Kora builds into its own directory so the AURA-902 `build/` and `sdkconfig` stay untouched:

```bash
idf.py -B build_kora -D SDKCONFIG=build_kora/sdkconfig "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;main/boards/kora/sdkconfig.kora" set-target esp32p4
idf.py -B build_kora -D SDKCONFIG=build_kora/sdkconfig "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;main/boards/kora/sdkconfig.kora" -p COM5 flash monitor
```

Or with the canonical script: `python scripts/build.py kora --name kora --language es-ES`.

The ESP32-C6 already runs ESP-Hosted 2.13.0 (updated during AURA-902, see its report section 7).

## Roadmap

Phase 1 (this board) → reminders/alarms/timer → tasks/habits/pomodoro → screen tools → Telegram → Tavily
search. API keys will come from a git-ignored `.env`, never from committed files.
