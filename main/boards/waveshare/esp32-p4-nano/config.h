#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>

// Audio ES8311: original Waveshare ESP32-P4-NANO pin assignment
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_13
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_10
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_12
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_11
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_9
#define AUDIO_CODEC_PA_PIN      GPIO_NUM_53
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_7
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_8
#define AUDIO_CODEC_ES8311_ADDR ES8311_CODEC_DEFAULT_ADDR

#define BOOT_BUTTON_GPIO GPIO_NUM_35

// GC9A01 240x240 SPI display - PROVISIONAL GPIOs; verify against exact board revision.
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define LCD_BIT_PER_PIXEL 16
#define DISPLAY_SPI_HOST SPI2_HOST
#define DISPLAY_SPI_MODE 0
#define DISPLAY_MOSI_PIN GPIO_NUM_48
#define DISPLAY_CLK_PIN  GPIO_NUM_47
#define DISPLAY_DC_PIN   GPIO_NUM_46
#define DISPLAY_CS_PIN   GPIO_NUM_24
#define DISPLAY_RST_PIN  GPIO_NUM_5
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR true
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

// AURA sensors: DHT11 (MCP tool) and TTP223 touch module (active high, acts like BOOT click)
#define AURA_DHT_GPIO GPIO_NUM_4
#define AURA_TOUCH_GPIO GPIO_NUM_6

// Temperature alerts (DHT11): the robot reacts when crossing these limits
#define AURA_TEMP_HOT_C  30
#define AURA_TEMP_COLD_C 16

// Robot face: falls asleep ("Dormido") after this many idle seconds
#define AURA_SLEEP_AFTER_S 120

// Body: 6 x MG90S micro servos on a PCA9685 sharing the codec I2C bus (SDA 7 / SCL 8). Servo power (V+)
// must come from a separate 5 V supply (>= 3 A) with common GND. If no PCA9685 answers, the robot has no body.
#define AURA_PCA9685_ADDR 0x40
// MG90S: 500-2500 us for ~180 deg (some batches stop at 2400 us; lower MAX if a servo buzzes at the end)
#define AURA_SERVO_PULSE_MIN_US 500
#define AURA_SERVO_PULSE_MAX_US 2500
#define AURA_SERVO_RANGE_DEG    180
#define AURA_SERVO_CH_FRONT_LEFT  0
#define AURA_SERVO_CH_FRONT_RIGHT 1
#define AURA_SERVO_CH_REAR_LEFT   2
#define AURA_SERVO_CH_REAR_RIGHT  3
#define AURA_SERVO_CH_PAN         4
#define AURA_SERVO_CH_TILT        5
// Mirror-mounted servos turn the other way; flip these so "+" means leg forward / head left / up
#define AURA_SERVO_INVERT_FRONT_LEFT  false
#define AURA_SERVO_INVERT_FRONT_RIGHT true
#define AURA_SERVO_INVERT_REAR_LEFT   false
#define AURA_SERVO_INVERT_REAR_RIGHT  true
#define AURA_SERVO_INVERT_PAN         false
#define AURA_SERVO_INVERT_TILT        false
// Mechanical limits (degrees from centre)
#define AURA_LEG_LIMIT  70
#define AURA_PAN_LIMIT  60
#define AURA_TILT_LIMIT 35
// Walking: leg swing amplitude (deg) and timing (ms); tune on the real robot
#define AURA_GAIT_AMPLITUDE 25
#define AURA_GAIT_SWING_MS  150
#define AURA_GAIT_PUSH_MS   400

#endif // _BOARD_CONFIG_H_
