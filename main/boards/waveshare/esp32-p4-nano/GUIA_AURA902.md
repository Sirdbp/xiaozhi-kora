# AURA-902 — Guía del proyecto

Asistente de voz infantil basado en **Xiaozhi 2.5.3** sobre una **Waveshare ESP32-P4-NANO**, con
pantalla redonda **GC9A01** (240 × 240), cara de robot animada, sensor de temperatura **DHT11** y
sensor táctil **TTP223**.

- **Firmware:** Xiaozhi 2.5.3 · ESP-IDF v6.1
- **Rama:** `aura902-gc9a01`
- **Chip:** ESP32-P4 rev. v1.3 · coprocesador Wi-Fi ESP32-C6 (ESP-Hosted 2.13.0)
- **Fecha:** 17 de octubre de 2026

---

## 1. Qué hace

| Función | Cómo se usa |
|---|---|
| Conversación por voz en español | Decir **«Hi ESP»** (*jai i-es-pi*) o tocar el sensor táctil |
| Cara de robot animada | Cambia sola según la emoción de la respuesta; 100 estados |
| Pedir una cara | «Hi ESP, pon cara de enamorado» o «pon la cara número 44» |
| Temperatura y humedad | Siempre visibles abajo en la pantalla; «¿qué temperatura hace?» |
| Alertas de temperatura | Si hace calor o frío, el robot avisa y pone cara de calor o de frío |
| Volumen | Mantener el dedo 1,5 s en el sensor táctil: 30 → 60 → 90 → 30 |
| Boca al hablar | La boca se mueve mientras el robot habla |
| Saludo al encender | Pone cara «Feliz» 5 s al terminar de arrancar |
| Se duerme solo | Tras 2 min sin uso pone «Dormido» (Zzz); despierta al hablarle o tocarlo |

---

## 2. Conexiones

| Señal | GPIO | Nota |
|---|---|---|
| GC9A01 SCK | 47 | Dominio VDDPST_5: requiere LDO4 encendido |
| GC9A01 MOSI | 48 | Dominio VDDPST_5 |
| GC9A01 DC | 46 | Dominio VDDPST_5 |
| GC9A01 CS | 24 | Es D- del USB-Serial-JTAG: el firmware lo libera |
| GC9A01 RST | 5 | |
| GC9A01 VCC / GND | 3V3 / GND | Retroiluminación sin conectar (siempre encendida) |
| DHT11 DATA | 4 | Pull-up interno |
| TTP223 SIG | 6 | Activo en alto |
| ES8311 (audio) | I2C 7/8, I2S 9–13, PA 53 | Original de la placa |
| ESP32-C6 (Wi-Fi) | SDIO 14–19, reset 54 | Original de la placa |

---

## 3. Uso con los niños

**Hablar con el robot**
1. Decir «Hi ESP», o tocar una vez el sensor táctil.
2. Hablar cuando la pantalla indique que escucha.
3. Tocar otra vez el sensor para terminar la conversación.

**Cosas que se le pueden pedir**
- «¿Qué temperatura hace?» / «¿Hay mucha humedad?»
- «Pon cara de sorpresa» / «Pon cara de dormido» / «Ponte triste» / «Cara número 11»
- La cara pedida se mantiene 10 segundos y luego vuelve a seguir la conversación.

**Sensor táctil**
- Toque corto: empezar o terminar la conversación.
- Mantener 1,5 s: cambiar el volumen (aparece «Volumen 30/60/90» en pantalla).

**Alertas** (umbrales en `config.h`)
- ≥ 30 °C: «¡Hace calor!», cara con la lengua fuera e indicador en rojo.
- ≤ 16 °C: «¡Hace frío!», cara asustada que tiembla e indicador en celeste.

**Sueño:** tras `AURA_SLEEP_AFTER_S` (120 s) en reposo se duerme; se despierta con «Hi ESP» o un toque.

> El idioma en que **responde** el asistente se configura en la consola del servidor de Xiaozhi,
> no en el firmware. El firmware solo pone en español la pantalla y los avisos locales.

---

## 4. La cara de robot

- Dibujada con primitivas de LVGL (sin imágenes): fondo azul noche `#07111F`, trazo azul
  `#2F7DFF`, ojos rojos `#FF3B30` cuando está enojado.
- Cada estado combina **ojos** (12 tipos), **boca** (15 tipos), **extras** y una **animación**.
- Parpadeo automático cada 3,5 s en las caras con ojos abiertos.
- Animaciones a 8 fps; solo se redibuja la zona de la cabeza para no saturar el bus SPI.
- Para modificar una cara: editar su línea en la tabla `kMoods` de `aura_display.h`.

### Emociones del servidor → cara

El servidor de Xiaozhi envía 21 emociones; cada una se muestra con un estado del catálogo:

| Emoción | Cara | Emoción | Cara | Emoción | Cara |
|---|---|---|---|---|---|
| neutral | 4 Sereno | happy | 1 Feliz | laughing | 83 Riendo |
| funny | 10 Divertido | sad | 6 Triste | angry | 7 Enojado |
| crying | 84 Llorando | loving | 11 Enamorado | embarrassed | 86 Avergonzado |
| surprised | 2 Sorprendido | shocked | 15 Asombrado | thinking | 14 Pensativo |
| winking | 16 Guiño | cool | 52 Confidente | relaxed | 24 Relajado |
| delicious | 5 Travieso | kissy | 54 Cariñoso | confident | 67 Seguro |
| sleepy | 9 Dormido | silly | 53 Cómplice | confused | 17 Confundido |

Los iconos de estado (conectando, etc.) y cualquier nombre desconocido muestran **4 Sereno**.

### Catálogo de los 100 estados

*(Generado desde `kMoods` en `aura_display.h`. Las caras con ojos cuadrados, redondos,
decididos, corazón o estrella además parpadean.)*

| # | Estado | Ojos | Boca | Extras | Animación |
|---|---|---|---|---|---|
| 1 | Feliz | ^ cerrado | sonrisa D | — | rebote |
| 2 | Sorprendido | cuadrado | o abierta | — | — |
| 3 | Emocionado | > < | sonrisa D | — | rebote |
| 4 | Sereno | — cerrado | sonrisa suave | — | respira |
| 5 | Travieso | — cerrado / cuadrado | lengua fuera | — | — |
| 6 | Triste | caído | triste | — | respira |
| 7 | Enojado | enojado (rojo) | triste | — | sacudida |
| 8 | Curioso | cuadrado | o pequeña | — | mira alrededor |
| 9 | Dormido | — cerrado | punto | Zzz | Zzz flotando |
| 10 | Divertido | > < | sonrisa enorme | — | rebote |
| 11 | Enamorado | corazón | sonrisa D | — | late |
| 12 | Dudoso | — cerrado / cuadrado | media sonrisa | — | — |
| 13 | Preocupado | cuadrado | o pequeña | cejas preocupadas | tiembla |
| 14 | Pensativo | decidido | ondulada | — | respira |
| 15 | Asombrado | cuadrado | punto | — | — |
| 16 | Guiño | > < / cuadrado | media sonrisa | — | — |
| 17 | Confundido | cuadrado | ondulada | — | mira alrededor |
| 18 | Aliviado | ^ cerrado | sonrisa D | — | respira |
| 19 | Concentrado | decidido | recta | — | — |
| 20 | Entusiasmado | cuadrado | sonrisa D | destellos | rebote |
| 21 | Indeciso | cuadrado / — cerrado | triste pequeña | — | mira alrededor |
| 22 | Agradecido | ^ cerrado | sonrisa D | — | respira |
| 23 | Nervioso | cuadrado | sonrisa pequeña | gota de sudor | tiembla |
| 24 | Relajado | ◡ relajado / — cerrado | sonrisa pequeña | — | respira |
| 25 | Cansado | — cerrado / cuadrado | triste pequeña | — | respira |
| 26 | Motivado | ^ cerrado | sonrisa D | — | rebote |
| 27 | Decepcionado | caído | triste | — | respira |
| 28 | Orgulloso | > < | sonrisa D | destellos | rebote |
| 29 | Vergonzoso | cuadrado | recta corta | rubor | — |
| 30 | Tímido | — cerrado | sonrisa pequeña | rubor | respira |
| 31 | Impaciente | > < | punto | — | sacudida |
| 32 | Tranquilo | — cerrado | sonrisa suave | — | respira |
| 33 | Alegre | ^ cerrado | sonrisa D | — | rebote |
| 34 | Aburrido | — cerrado | recta | — | respira |
| 35 | Interesado | cuadrado | sonrisa pequeña | — | mira alrededor |
| 36 | Intrigado | cuadrado | o abierta | — | — |
| 37 | Pensativo 2 | cuadrado | triste pequeña | — | mira alrededor |
| 38 | Sospechoso | decidido / cuadrado | recta | gota de sudor | mira alrededor |
| 39 | Asustado | cuadrado | abierta triste | cejas preocupadas | tiembla |
| 40 | Valiente | decidido | sonrisa D | gota de sudor | rebote |
| 41 | Satisfecho | cuadrado | sonrisa pequeña | — | — |
| 42 | Frustrado | enojado (rojo) | triste | — | sacudida |
| 43 | Incrédulo | cuadrado | sonrisa suave | — | — |
| 44 | Admirado | estrella | o abierta | — | late |
| 45 | Celoso | estrella | recta corta | — | late |
| 46 | Determinado | > < | sonrisa D | — | rebote |
| 47 | Alerta | cuadrado | sonrisa suave | — | mira alrededor |
| 48 | Distraído | cuadrado | ondulada | — | mira alrededor |
| 49 | Apático | — cerrado | recta corta | — | — |
| 50 | Optimista | ^ cerrado | sonrisa D | destellos | rebote |
| 51 | Pesimista | — cerrado | triste | — | respira |
| 52 | Confidente | — cerrado / cuadrado | media sonrisa | rubor | — |
| 53 | Cómplice | ^ cerrado / > < | sonrisa D | rubor | rebote |
| 54 | Cariñoso | corazón | sonrisa pequeña | — | late |
| 55 | Romántico | corazón | sonrisa D | rubor | late |
| 56 | Ilusionado | > < | sonrisa D | rubor | rebote |
| 57 | Melancólico | redondo | triste pequeña | — | respira |
| 58 | Enojado 2 | enojado (rojo) | abierta triste | — | sacudida |
| 59 | Tranquilo 2 | — cerrado | recta corta | — | respira |
| 60 | Genial | > < | sonrisa enorme | destellos | rebote |
| 61 | Sorprendido 2 | cuadrado | sonrisa pequeña | — | — |
| 62 | Ofendido | enojado (rojo) | ondulada | gota de sudor | sacudida |
| 63 | Complacido | ^ cerrado | sonrisa D | — | respira |
| 64 | Pensando | — cerrado / cuadrado | punto | — | mira alrededor |
| 65 | Con sueño | ◡ relajado / – corto | sonrisa pequeña | Zzz | Zzz flotando |
| 66 | Recuperado | cuadrado | media sonrisa | — | — |
| 67 | Seguro | — cerrado | sonrisa suave | — | respira |
| 68 | Inseguro | cuadrado | sonrisa pequeña | gota de sudor | mira alrededor |
| 69 | Expectante | cuadrado | o pequeña | — | — |
| 70 | Encantado | ^ cerrado | sonrisa D | rubor | rebote |
| 71 | Pasivo | — cerrado | recta | — | — |
| 72 | Activo | decidido | sonrisa enorme | gota de sudor | rebote |
| 73 | Indignado | enojado (rojo) | abierta triste | — | sacudida |
| 74 | Dolido | — cerrado | o pequeña | cejas preocupadas | respira |
| 75 | Esperanzado | ^ cerrado | sonrisa D | — | respira |
| 76 | Negativo | — cerrado | triste pequeña | — | — |
| 77 | Positivo | — cerrado | sonrisa D | — | rebote |
| 78 | Extrañado | cuadrado | sonrisa pequeña | — | mira alrededor |
| 79 | Entristecido | — cerrado | triste pequeña | lágrimas | lágrimas caen |
| 80 | Emocionado 2 | > < | sonrisa enorme | destellos | rebote |
| 81 | Asombrado 2 | cuadrado | o abierta | — | — |
| 82 | Sereno 2 | — cerrado | sonrisa suave | — | respira |
| 83 | Riendo | ^ cerrado | sonrisa enorme | — | rebote |
| 84 | Llorando | redondo | abierta triste | lágrimas | lágrimas caen |
| 85 | Conmovido | cuadrado | sonrisa pequeña | lágrimas | lágrimas caen |
| 86 | Avergonzado | cuadrado | sonrisa pequeña | rubor | — |
| 87 | Concentrado 2 | decidido | recta corta | — | — |
| 88 | Atento | cuadrado | sonrisa pequeña | — | mira alrededor |
| 89 | Inspirado | cuadrado | sonrisa suave | destellos | late |
| 90 | Determinado 2 | decidido | sonrisa D | — | rebote |
| 91 | Divertido 2 | > < | sonrisa D | — | rebote |
| 92 | Triste 2 | — cerrado | triste pequeña | cejas preocupadas | respira |
| 93 | Desconfiado | decidido | triste pequeña | — | mira alrededor |
| 94 | Intrigado 2 | cuadrado | sonrisa suave | gota de sudor | — |
| 95 | Cansado 2 | — cerrado | lengua fuera | — | respira |
| 96 | Apasionado | corazón | sonrisa pequeña | destellos | late |
| 97 | Calmado | — cerrado | recta | — | respira |
| 98 | Impresionado | cuadrado | o abierta | — | — |
| 99 | Feliz 2 | ^ cerrado | sonrisa D | — | rebote |
| 100 | Celebrando | redondo | sonrisa enorme | destellos | rebote |

---

## 4b. Cuerpo: patas y cabeza (servos)

**Hardware:** 6 micro-servos **MG90S** en una placa **PCA9685** (I2C, dirección 0x40) que comparte el bus del
códec (SDA GPIO 7, SCL GPIO 8). Sin PCA9685 el robot funciona igual, sin cuerpo.

| PCA9685 | Conexión |
|---|---|
| VCC | 3V3 de la P4-NANO |
| GND | GND de la P4-NANO **y** de la fuente de servos (común) |
| SDA / SCL | GPIO 7 / GPIO 8 |
| V+ (bornera) | Fuente aparte de **5 V, ≥ 3 A** (+ condensador 470–1000 µF) |
| Canal 0 / 1 | Pata delantera izquierda / derecha |
| Canal 2 / 3 | Pata trasera izquierda / derecha |
| Canal 4 / 5 | Cabeza: giro (pan) / inclinación (tilt) |

**Qué hace**
- Por voz: «camina hacia adelante», «gira a la izquierda», «siéntate», «túmbate», «saluda»,
  «baila», «haz una reverencia», «estírate», «mira arriba», «para».
- Reacciona a la cara: feliz → mueve la cabeza; emocionado/riendo → baila; triste → baja la cabeza;
  enojado → niega; sorprendido → mira arriba; curioso → mira a un lado; enamorado → asiente;
  dormido → se tumba (y se levanta al despertar). Como mucho una reacción cada 4 s.
- Movimientos suaves (rampas de 20 ms) y servos **relajados** tras 3 s quieto (sin temblor ni calor).
- Caminar: paso de reptación (una pata avanza sola, luego empujan las cuatro). Ajustar amplitud y
  tiempos en `config.h` (`AURA_GAIT_*`) sobre el robot real.

**Calibración**
1. Con los servos sin brazo, decir «calibra el servo 0 con 0»: el servo va a su centro (1500 µs).
2. Montar el brazo de la pata/cabeza lo más recto posible.
3. Ajuste fino por voz: «calibra el servo 2 con 5» (−30 a 30 grados). Se guarda en NVS.
4. Si un servo gira al revés, cambiar su `AURA_SERVO_INVERT_*` en `config.h`.
5. Si un servo zumba o se fuerza en un extremo, bajar `AURA_SERVO_PULSE_MAX_US` (p. ej. a 2400) o subir
   `AURA_SERVO_PULSE_MIN_US`; algunos lotes de MG90S no llegan a 2500 µs.

> El MG90S tiene ~1,8 kg·cm: suficiente para un robot pequeño como el de referencia, pero conviene
> que el chasis sea ligero (PLA fino, pocas piezas metálicas) y evitar que las patas soporten golpes.

Servos: 0 delantera izq., 1 delantera der., 2 trasera izq., 3 trasera der., 4 giro cabeza, 5 inclinación.

| Herramienta MCP | Qué hace |
|---|---|
| `self.robot.gesture` | stand, sit, lie, bow, nod, shake_head, look_left/right/up/down, wave, dance, stretch, wiggle |
| `self.robot.walk` | forward, backward, left, right · 1–10 pasos |
| `self.robot.head` | pan −60…60, tilt −35…35 |
| `self.robot.stop` | Detiene todo |
| `self.robot.calibrate` | Centro (trim) de un servo, guardado en NVS |

## 5. Archivos del proyecto

| Archivo | Contenido |
|---|---|
| `esp32-p4-nano.cc` | Placa: LDO4, pad USB, SPI/GC9A01, botones, DHT11, alertas, herramienta de caras |
| `config.h` | Pines, umbrales de temperatura y parámetros de pantalla |
| `aura_display.h` | Cara de robot (100 estados, animaciones, boca al hablar) e indicador de temperatura |
| `dht11_sensor.h` | Driver del DHT11 y herramienta MCP de temperatura |
| `aura_motion.h` | Patas y cabeza: gestos, caminar, reacciones a la cara, herramientas `self.robot.*` |
| `pca9685.h` | Driver PCA9685 (no aborta ante errores I2C) |
| `sdkconfig.aura902` | Configuración por defecto (español, «Hi ESP», PSRAM para ESP-Hosted) |
| `config.json` | Build `esp32-p4-nano-aura902-gc9a01` para `scripts/build.py` |
| `README.md` | Resumen técnico en inglés |
| `Reporte_AURA902_GC9A01_ESP-IDF_6.1.md` | Reporte técnico detallado (causas y soluciones) |
| `CMakeLists.txt` (raíz) | Añade `sdkconfig.aura902` a `SDKCONFIG_DEFAULTS` |
| `main/CMakeLists.txt` | Letra de la interfaz a 20 px para esta placa |

### Herramientas MCP (las usa el asistente)

| Herramienta | Qué hace |
|---|---|
| `self.environment.get_temperature_humidity` | Lee temperatura y humedad del DHT11 |
| `self.face.set_mood` | Muestra uno de los 100 estados (por número o nombre) durante 10 s |

---

## 6. Compilar y flashear

Usar siempre **IDF v6.1 PowerShell** (en un PowerShell normal `idf.py` no existe):

```powershell
cd C:\Users\DBP\Desktop\xiaozhi-esp32
idf.py build
idf.py -p COM5 flash monitor
```

- En un clon nuevo, primero: `idf.py set-target esp32p4`.
- Si se borra `sdkconfig`, se regenera igual a partir de `sdkconfig.aura902`.
- Alternativa: `python scripts/build.py esp32-p4-nano --name esp32-p4-nano-aura902-gc9a01 --language es-ES`

**Reflashear sin compilar** (desde la carpeta `firmware` de un backup):

```powershell
python -m esptool --chip esp32p4 -p COM5 -b 460800 write-flash "@flash_args"
```

---

## 7. Solución de problemas

| Síntoma | Causa | Solución |
|---|---|---|
| Pantalla negra aunque todo da `ESP_OK` | LDO4 apagado: GPIO39–48 sin tensión | El firmware lo enciende; si el log dice `Failed to enable LDO4`, revisar |
| Colores raros (azul se ve café) | Panel BGR configurado como RGB | `rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR` |
| Imagen en espejo | Orientación del panel | `DISPLAY_MIRROR_X true` en `config.h` |
| No escucha la palabra de activación | Wake word desactivada en `sdkconfig` | Debe tener `CONFIG_USE_AFE_WAKE_WORD=y` y `CONFIG_SR_WN_WN9_HIESP=y` |
| Bucle de reinicios `assert failed: sdio_mempool_create` | Falta `CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM=y` | Está en `sdkconfig.aura902`; si se restaura un `sdkconfig`, ejecutar `idf.py reconfigure` |
| DHT11 `Timeout ... stage 1` | El sensor no responde | Revisar cableado y alimentación (VCC, DATA a GPIO4, GND) |
| `Version mismatch ... Co-proc [0.0.0]` | Firmware viejo en el ESP32-C6 | Actualizar el C6 por SDIO (ver reporte, sección 7) |
| `idf.py` no se reconoce | PowerShell sin entorno ESP-IDF | Abrir «IDF v6.1 PowerShell» |
| La consola USB-JTAG se corta al arrancar | GPIO24 (CS) es D- del USB | Es normal; usar la consola UART (COM5) |

---

## 8. Backups

En `C:\Users\DBP\Desktop\AURA902_backups\`:

| Archivo | Contenido |
|---|---|
| `AURA902_2026-10-17_caras.zip` | **El más reciente**: todo lo anterior, `sdkconfig.aura902`, ambos `CMakeLists.txt`, binarios y firmware del C6 |
| `AURA902_2026-10-17_final.zip` | Indicador de temperatura y sensor táctil (sin caras ni español) |
| `AURA902_2026-10-17.zip` | Primer backup, con los archivos intermedios de diagnóstico |

Firmware del ESP32-C6 y herramientas de actualización: `C:\Users\DBP\Desktop\aura902_c6\`.

---

## 9. Pendiente

1. **Commit** en la rama `aura902-gc9a01` (configurar antes `git config user.name` / `user.email`):
   ```bash
   git add CMakeLists.txt main/CMakeLists.txt main/boards/waveshare/esp32-p4-nano
   git commit -m "AURA-902: GC9A01, cara de robot animada, DHT11, alertas, táctil, español"
   ```
2. Probar en persona las alertas de temperatura (por ejemplo, bajando `AURA_TEMP_HOT_C` a 24).
3. Opcional: mover CS fuera de GPIO24 para conservar la consola USB-Serial-JTAG (requiere recablear).
