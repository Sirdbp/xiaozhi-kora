# Reporte técnico — Proyecto AURA-902

**Fecha:** 17 de octubre de 2026
**Placa:** Waveshare ESP32-P4-NANO (chip ESP32-P4 rev. v1.3)
**Pantalla:** GC9A01 SPI, 240 × 240, redonda
**Firmware:** Xiaozhi 2.5.3 · ESP-IDF v6.1
**Rama:** `aura902-gc9a01`

## 1. Resumen

| Subsistema | Estado |
|---|---|
| Pantalla GC9A01 con interfaz Xiaozhi (LVGL) | ✅ Funciona, orientación correcta |
| Audio ES8311 (micrófono y altavoz) | ✅ |
| Wi-Fi vía ESP32-C6 (ESP-Hosted) | ✅ |
| Palabra de activación «Hi ESP» (`wn9_hiesp`) | ✅ |
| Sensor DHT11 (temperatura/humedad) | ✅ Visible en pantalla y consultable por voz |
| Sensor táctil TTP223 | ✅ Inicia/detiene la conversación |

## 2. Conexiones

| Señal | GPIO | Observación |
|---|---|---|
| GC9A01 SCK | 47 | Dominio VDDPST_5 (requiere LDO4) |
| GC9A01 MOSI | 48 | Dominio VDDPST_5 |
| GC9A01 DC | 46 | Dominio VDDPST_5 |
| GC9A01 CS | 24 | Pin D- del USB-Serial-JTAG |
| GC9A01 RST | 5 | |
| GC9A01 VCC / GND | 3V3 / GND | Retroiluminación sin conectar (siempre encendida) |
| DHT11 DATA | 4 | Pull-up interno |
| TTP223 SIG | 6 | Activo en alto |

## 3. Problema de la pantalla negra: causa y solución

### Síntoma
Con Arduino (`Adafruit_GC9A01A`) la pantalla se ponía roja con los mismos cables. Con ESP-IDF todas
las transferencias SPI devolvían `ESP_OK`, pero la pantalla permanecía negra. Cambiar SPI2/SPI3, bajar
la frecuencia a 1 MHz o cambiar la secuencia de inicialización no tenía efecto.

### Causa principal: GPIO39–48 sin alimentación
En el ESP32-P4, los GPIO39–48 pertenecen al dominio de E/S **VDDPST_5** (el de la tarjeta SD), que en
esta placa se alimenta desde el **LDO interno canal 4**. ESP-IDF solo enciende ese LDO si algún
componente lo solicita; el `sdkconfig` únicamente reservaba los canales 1 (flash) y 2 (PSRAM).
Por tanto SCK (47), MOSI (48) y DC (46) nunca generaban 3,3 V, aunque el driver informara éxito.

**Solución:** `esp_ldo_acquire_channel()` con `chan_id = 4` y `voltage_mv = 3300` antes de
`spi_bus_initialize()`. Si falla, el firmware muestra un `ESP_LOGE` explícito.

### Causa secundaria: GPIO24 ocupado por el USB
GPIO24 es D- del USB-Serial-JTAG, activo como consola secundaria. El firmware desactiva el pad USB
(`USB_SERIAL_JTAG.conf0.usb_pad_enable = 0`) y reinicia el pin para usarlo como CS. Efecto
secundario: la consola/JTAG por USB-Serial-JTAG deja de funcionar tras el arranque; la consola UART
(COM5) no se ve afectada.

### Ajustes finales
- `DISPLAY_MIRROR_X = true` para corregir la imagen en espejo.
- Frecuencia SPI de vuelta a 20 MHz; `max_transfer_sz` igual al buffer de LVGL (20 filas).
- Se usa la secuencia de inicialización por defecto del driver `esp_lcd_gc9a01` 2.0.4.

### Lección aprendida
`ESP_OK` solo indica que la API aceptó la operación, no que las señales lleguen al pin. Ante un
periférico que «funciona» sin efecto visible, verificar alimentación del dominio de E/S y
funciones alternativas del pin (USB, JTAG, strapping) antes de cambiar protocolo o secuencias.

## 4. Funciones añadidas

### DHT11 (`dht11_sensor.h`)
- Lectura por bit-bang con temporización mediante `esp_timer_get_time()` y verificación de checksum.
- Caché de 2 s entre lecturas (el DHT11 requiere ≥ 1 s) y mutex para lecturas concurrentes.
- Herramienta MCP `self.environment.get_temperature_humidity` (3 intentos): Xiaozhi responde a
  «what's the temperature?».
- Una sola lectura al arrancar para no retrasar el arranque si falta el sensor.

### Cara de robot infantil (`aura_display.h`)
- Sustituye el emoji de Xiaozhi por una cara de robot dibujada con primitivas LVGL (sin imágenes):
  fondo azul noche `#07111F`, trazo azul `#2F7DFF`, ojos rojos `#FF3B30` en los estados de enojo.
- **Colores:** el panel GC9A01 es BGR (`rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR`). Con RGB el
  rojo y el azul se intercambiaban y la cara se veía café.
- **100 estados de ánimo** (hoja de diseño AURA-902), construidos por piezas: 12 tipos de ojo
  (arco, cuadrado con pupila, > <, línea, triste, enojado rojo, decidido, corazón, estrella,
  punto…), 15 bocas y extras (Zzz, lágrimas, gota de sudor, rubor, destellos, cejas).
- **Animaciones** según el estado: rebote (alegría), respiración (calma/tristeza), sacudida
  (enojo), temblor (nervios/miedo), pulso (corazones/estrellas), mirar alrededor (curiosidad),
  Zzz flotando (sueño) y lágrimas cayendo (llanto); parpadeo cada 3,5 s en ojos abiertos.
  8 fps y solo se redibuja la zona de la cabeza para no saturar el SPI.
- Las 21 emociones del servidor se asignan a estados de la hoja (p. ej. `laughing` → 83 Riendo,
  `crying` → 84 Llorando, `loving` → 11 Enamorado, `thinking` → 14 Pensativo).
- **Boca al hablar:** mientras Xiaozhi está en estado *speaking*, la boca alterna cada 250 ms
  entre la de la emoción y una boca abierta.
- **Saludo y sueño:** «Feliz» 5 s al terminar el arranque; «Dormido» tras `AURA_SLEEP_AFTER_S`
  (120 s, `config.h`) en reposo; despierta al salir del estado idle. Verificado por log.
- Herramienta MCP `self.face.set_mood` (número o nombre): «pon cara de enamorado». La cara pedida
  se mantiene 10 s aunque el servidor envíe otras emociones.
- La cara está en la capa del emoji: barra de estado, avisos y subtítulos quedan por encima.
  Se fuerza el tema oscuro para que los textos se lean sobre el fondo.
- Texto de la interfaz reducido de 30 a 20 px (`BUILTIN_TEXT_FONT` en `main/CMakeLists.txt`).

### Alertas de temperatura
- Umbrales en `config.h`: `AURA_TEMP_HOT_C` (30 °C) y `AURA_TEMP_COLD_C` (16 °C), con 1 °C de histéresis.
- Al cruzarlos: aviso «¡Hace calor! / ¡Hace frío!», cara «Cansado 2» (lengua fuera) o «Asustado»
  (temblando) durante 10 s, y el indicador de abajo en rojo o celeste mientras dure la alerta.

### Indicador en pantalla (`aura_display.h`)
- `AuraDisplay` extiende `SpiLcdDisplay` y añade una etiqueta en `lv_layer_top()`, abajo al centro
  (por ejemplo `24°C  54%`), actualizada cada 10 s por la tarea `aura_env`.

### Sensor táctil TTP223
- `Button` activo en alto en GPIO6: un toque alterna la conversación, igual que BOOT, pero nunca
  entra en modo de configuración Wi-Fi durante el arranque.
- Pulsación larga (1,5 s): cambia el volumen del altavoz 30 → 60 → 90 → 30 y lo muestra en pantalla.

### Idioma
- Interfaz y avisos de voz en español: `CONFIG_LANGUAGE_ES_ES=y` (antes `en-US`). Como la palabra de
  activación, vive en `sdkconfig`; con `scripts/build.py` usar `--language es-ES`.
- El idioma en que responde el asistente se configura en la consola del servidor de Xiaozhi.

### Configuración persistente (`sdkconfig.aura902`)
- `sdkconfig` está ignorado por git. Para que una compilación desde cero con `idf.py` quede igual,
  el `CMakeLists.txt` raíz añade `main/boards/waveshare/esp32-p4-nano/sdkconfig.aura902` a
  `SDKCONFIG_DEFAULTS` (solo si no viene definido; `scripts/build.py` pasa el suyo).
- Contiene: placa P4-NANO, chip < v3, español, «Hi ESP» y `CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM=y`.
- Verificado: borrando `sdkconfig`, `idf.py reconfigure` genera una configuración idéntica a la buena.
- **Imprescindible:** sin `MEMPOOL_PREFER_SPIRAM` el pool SDIO de ESP-Hosted no cabe en RAM interna
  y el equipo entra en bucle de reinicios (`assert failed: sdio_mempool_create`).
- En un clon nuevo, ejecutar antes `idf.py set-target esp32p4`.

### Palabra de activación
- `CONFIG_USE_AFE_WAKE_WORD=y` y `CONFIG_SR_WN_WN9_HIESP=y`. Con `scripts/build.py` se selecciona
  automáticamente («Hi ESP» para en-US). Con `idf.py` directo vive solo en `sdkconfig` (ignorado
  por git); ver `README.md` para restaurarlo.

## 5. Archivos de la placa

| Archivo | Contenido |
|---|---|
| `esp32-p4-nano.cc` | Placa: LDO4, USB pad, SPI/GC9A01, botones, DHT11, tarea de pantalla |
| `config.h` | Pines y parámetros |
| `config.json` | Build `esp32-p4-nano-aura902-gc9a01` (chip < v3, sin cámara) |
| `dht11_sensor.h` | Driver DHT11 + herramienta MCP |
| `aura_display.h` | Pantalla con indicador de temperatura/humedad |
| `README.md` | Cableado, particularidades del P4 y compilación |

Se eliminó `lcd_init_cmds.h` (pantalla DSI anterior, sin uso).

## 6. Backup

`C:\Users\DBP\Desktop\AURA902_backups\AURA902_2026-10-17.zip`: carpeta de la placa con los backups
intermedios, `sdkconfig`, `dependencies.lock`, diff sin commit y binarios para reflashear con
`python -m esptool --chip esp32p4 -p COM5 -b 460800 write-flash "@flash_args"`.
(Este backup es anterior al indicador en pantalla y al sensor táctil.)
`AURA902_2026-10-17_final.zip` contiene el estado con el indicador y el sensor táctil.
`AURA902_2026-10-17_caras.zip` es el más reciente: caras de robot, español, colores BGR, alertas,
`sdkconfig.aura902`, ambos `CMakeLists.txt` y el firmware 2.13.0 del C6 (`c6_firmware`).

## 7. Actualización del ESP32-C6 (ESP-Hosted)

El C6 traía un firmware antiguo: informaba versión 0.0.0 y no respondía a `GetCoprocessorFwVersion`.
El log de Xiaozhi avisaba `Version mismatch: Host [2.13.0] > Co-proc [0.0.0]`.

- Se compiló el firmware *slave* de ESP-Hosted 2.13.0 para `esp32c6` (SDIO) desde una copia del
  componente en `C:\Users\DBP\Desktop\aura902_c6\esp_hosted\slave`.
- Se actualizó por el propio enlace SDIO con el ejemplo `host_performs_slave_ota` (método
  *partition*), con los mismos pines SDIO que Xiaozhi (CLK 18, CMD 19, D0–D3 14–17, reset 54)
  y la opción de chip anterior a v3.
- Resultado: `Slave firmware version: 2.13.0`. Después se reflasheó Xiaozhi: desaparecen los avisos
  de versión y el Wi-Fi conecta en unos 7 s.
- Si en el futuro se actualiza `espressif/esp_hosted` en Xiaozhi, repetir el proceso con la nueva versión.

## 8. Pendientes

1. **Commit** en `aura902-gc9a01`: configurar antes `git config user.name` / `user.email`.
2. Opcional: mover CS fuera de GPIO24 para conservar el USB-Serial-JTAG (requiere recablear).

## 9. Comandos

Desde **IDF v6.1 PowerShell** (en un PowerShell normal `idf.py` no existe):

```powershell
cd C:\Users\DBP\Desktop\xiaozhi-esp32
idf.py build
idf.py -p COM5 flash monitor
```
