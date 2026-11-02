#ifndef __PCA9685_H__
#define __PCA9685_H__

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Minimal PCA9685 16-channel PWM driver for hobby servos (50 Hz). Unlike I2cDevice it never aborts
// on I2C errors: a loose cable or a servo brown-out must not reboot the whole robot.
class Pca9685 {
private:
    static constexpr const char* kTag = "Pca9685";
    static constexpr uint8_t kRegMode1 = 0x00;
    static constexpr uint8_t kRegMode2 = 0x01;
    static constexpr uint8_t kRegLed0OnL = 0x06;
    static constexpr uint8_t kRegAllLedOffH = 0xFD;
    static constexpr uint8_t kRegPrescale = 0xFE;
    static constexpr uint32_t kOscillatorHz = 25000000;
    static constexpr uint32_t kPwmHz = 50;  // 20 ms servo period

    i2c_master_dev_handle_t dev_ = nullptr;

    esp_err_t Write(const uint8_t* data, size_t length) {
        esp_err_t err = i2c_master_transmit(dev_, data, length, 50);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "I2C write failed: %s", esp_err_to_name(err));
        }
        return err;
    }

    esp_err_t WriteReg(uint8_t reg, uint8_t value) {
        uint8_t data[2] = {reg, value};
        return Write(data, sizeof(data));
    }

public:
    // Returns false if no PCA9685 answers at `address` (servos are optional hardware).
    bool Init(i2c_master_bus_handle_t bus, uint8_t address) {
        if (i2c_master_probe(bus, address, 50) != ESP_OK) {
            ESP_LOGW(kTag, "No PCA9685 at 0x%02x, servos disabled", address);
            return false;
        }
        i2c_device_config_t cfg = {};
        cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        cfg.device_address = address;
        cfg.scl_speed_hz = 400 * 1000;
        if (i2c_master_bus_add_device(bus, &cfg, &dev_) != ESP_OK) {
            return false;
        }

        uint8_t prescale = (uint8_t)(kOscillatorHz / (4096 * kPwmHz) - 1);  // 121 -> ~50 Hz
        bool ok = WriteReg(kRegMode1, 0x10) == ESP_OK          // sleep (required to set prescale)
                  && WriteReg(kRegPrescale, prescale) == ESP_OK
                  && WriteReg(kRegMode2, 0x04) == ESP_OK       // totem-pole outputs
                  && WriteReg(kRegMode1, 0x20) == ESP_OK;      // wake, register auto-increment
        vTaskDelay(pdMS_TO_TICKS(2));
        ok = ok && AllOff();
        ESP_LOGI(kTag, "PCA9685 at 0x%02x %s (prescale %u)", address, ok ? "ready" : "init failed", prescale);
        return ok;
    }

    // Pulse width in microseconds (0 = output off, servo relaxed).
    bool SetPulseUs(uint8_t channel, uint32_t pulse_us) {
        uint8_t data[5] = {(uint8_t)(kRegLed0OnL + 4 * channel), 0, 0, 0, 0};
        if (pulse_us == 0) {
            data[4] = 0x10;  // LEDn_OFF_H full-off bit
        } else {
            uint32_t ticks = pulse_us * 4096 / (1000000 / kPwmHz);
            data[3] = ticks & 0xFF;
            data[4] = (ticks >> 8) & 0x0F;
        }
        return Write(data, sizeof(data)) == ESP_OK;
    }

    bool AllOff() { return WriteReg(kRegAllLedOffH, 0x10) == ESP_OK; }
};

#endif // __PCA9685_H__
