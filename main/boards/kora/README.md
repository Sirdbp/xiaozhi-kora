# Kora — AURA robot on the Waveshare ESP32-P4-NANO-WIFI6-DB

Kora is the AURA-902 robot (round GC9A01 face, DHT11, TTP223 touch, optional 4-leg + pan/tilt body)
on the **Waveshare ESP32-P4-NANO-WIFI6-DB**: ESP32-P4 (chip v3) with an **ESP32-C5** Wi-Fi 6 /
BLE co-processor over SDIO (ESP-Hosted). Spanish UI, "Hi ESP" wake word.

Shared code comes from the AURA-902 board (`main/boards/waveshare/esp32-p4-nano`): `aura_display.h`
(100-mood animated robot face), `aura_motion.h` + `pca9685.h` (servos), `dht11_sensor.h`.

## Wiring

| Signal | GPIO | Notes |
|---|---|---|
| GC9A01 SCK / MOSI / DC | 47 / 48 / 46 | VDDPST_5 domain: the board enables LDO4 at 3.3 V |
| GC9A01 CS | 24 | USB-Serial-JTAG D-: the board releases the USB pad |
| GC9A01 RST | 5 | VCC 3V3, backlight unconnected |
| DHT11 DATA | 4 | internal pull-up |
| TTP223 SIG | **20** | not 6 as on AURA-902: GPIO6 goes to the ESP32-C5 (IO2) on this board |
| PCA9685 SDA / SCL | 7 / 8 | shared with ES8311, addr 0x40; servo V+ from a separate 5 V >= 3 A supply |

On-board (do not use): ES8311 I2C 7/8 + I2S 13/12/10/9/11, PA 53, ESP32-C5 SDIO 14–19, C5 reset 54,
C5 IO2 6, microSD 39–45, Ethernet 28–31/34/35/49–52, BOOT 35, UART0 37/38.

## Build

Kora builds into its own directory so the AURA-902 `build/` and `sdkconfig` stay untouched:

```bash
idf.py -B build_kora -D SDKCONFIG=build_kora/sdkconfig "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;main/boards/kora/sdkconfig.kora" set-target esp32p4
idf.py -B build_kora -D SDKCONFIG=build_kora/sdkconfig "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;main/boards/kora/sdkconfig.kora" -p COMx flash monitor
```

Or with the canonical script: `python scripts/build.py kora --name kora --language es-ES`.

## ESP32-C5 co-processor

The C5 firmware must match ESP-Hosted 2.13.0 used by Xiaozhi. If the log shows
`Version mismatch: Host [2.13.0] > Co-proc [...]`, build the ESP-Hosted slave for `esp32c5` and update
it over SDIO with `host_performs_slave_ota` (same procedure as the C6 update in the AURA-902 report).
The board ships with `esp32-c5-slave-260623.bin` (Waveshare factory image).

## Roadmap

Phase 1 (this board) → reminders/alarms/timer → tasks/habits/pomodoro → screen tools → Telegram → Tavily
search. API keys will come from a git-ignored `.env`, never from committed files.
