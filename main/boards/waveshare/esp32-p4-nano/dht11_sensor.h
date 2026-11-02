#ifndef __DHT11_SENSOR_H__
#define __DHT11_SENSOR_H__

#include "mcp_server.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>
#include <mutex>
#include <string>

// DHT11 single-wire temperature/humidity sensor (bit-banged) exposed to Xiaozhi as an MCP tool.
class Dht11Sensor {
private:
    static constexpr const char* kTag = "Dht11Sensor";
    static constexpr int64_t kMinReadIntervalUs = 2 * 1000 * 1000;  // DHT11 needs >= 1 s between reads

    gpio_num_t gpio_num_;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    std::mutex read_mutex_;  // MCP tool and display task may read concurrently
    int64_t last_read_us_ = 0;
    bool last_ok_ = false;
    int temperature_ = 0;
    int humidity_ = 0;

    // Waits while the line stays at `level`; returns microseconds elapsed or -1 on timeout.
    int WaitWhile(int level, int timeout_us) {
        int64_t start = esp_timer_get_time();
        int elapsed = 0;
        while (gpio_get_level(gpio_num_) == level) {
            elapsed = (int)(esp_timer_get_time() - start);
            if (elapsed > timeout_us) {
                return -1;
            }
        }
        return elapsed;
    }

    bool ReadRaw(uint8_t data[5]) {
        int idle_level = gpio_get_level(gpio_num_);

        // Start signal: hold the line low >= 18 ms, then release it.
        gpio_set_level(gpio_num_, 0);
        vTaskDelay(pdMS_TO_TICKS(20));

        int failed_stage = 0;  // 0 = ok, 1..3 = response phase, 4+ = bit index + 4
        portENTER_CRITICAL(&mux_);
        gpio_set_level(gpio_num_, 1);

        // Sensor response: pulls low after 20-40 us, ~80 us low, ~80 us high.
        if (WaitWhile(1, 200) < 0) {
            failed_stage = 1;
        } else if (WaitWhile(0, 120) < 0) {
            failed_stage = 2;
        } else if (WaitWhile(1, 120) < 0) {
            failed_stage = 3;
        }
        for (int i = 0; failed_stage == 0 && i < 40; ++i) {
            // Each bit: ~50 us low, then high for ~27 us (0) or ~70 us (1).
            int high_us = -1;
            if (WaitWhile(0, 100) < 0 || (high_us = WaitWhile(1, 120)) < 0) {
                failed_stage = 4 + i;
                break;
            }
            data[i / 8] = (data[i / 8] << 1) | (high_us > 45 ? 1 : 0);
        }
        portEXIT_CRITICAL(&mux_);
        gpio_set_level(gpio_num_, 1);

        if (failed_stage != 0) {
            ESP_LOGW(kTag, "Timeout on GPIO%d at stage %d (idle level %d)", gpio_num_, failed_stage, idle_level);
            return false;
        }
        if (((data[0] + data[1] + data[2] + data[3]) & 0xFF) != data[4]) {
            ESP_LOGW(kTag, "Checksum error: %02x %02x %02x %02x %02x", data[0], data[1], data[2], data[3], data[4]);
            return false;
        }
        return true;
    }

public:
    explicit Dht11Sensor(gpio_num_t gpio_num) : gpio_num_(gpio_num) {
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << gpio_num_),
            .mode = GPIO_MODE_INPUT_OUTPUT_OD,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config));
        gpio_set_level(gpio_num_, 1);

        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.environment.get_temperature_humidity",
            "Read the current room temperature (Celsius) and relative humidity (%) from the DHT11 sensor. "
            "Use this when the user asks about temperature, heat, cold or humidity.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                int temperature = 0;
                int humidity = 0;
                if (!Read(temperature, humidity)) {
                    return "{\"success\": false, \"message\": \"DHT11 sensor not responding\"}";
                }
                char json[96];
                snprintf(json, sizeof(json), "{\"success\": true, \"temperature_c\": %d, \"humidity_percent\": %d}",
                         temperature, humidity);
                return std::string(json);
            });
    }

    // Returns the last valid reading if called again within 2 s.
    bool Read(int& temperature, int& humidity, int attempts = 3) {
        std::lock_guard<std::mutex> guard(read_mutex_);
        int64_t now = esp_timer_get_time();
        if (last_read_us_ == 0 || now - last_read_us_ >= kMinReadIntervalUs) {
            uint8_t data[5] = {};
            bool ok = false;
            for (int attempt = 0; attempt < attempts && !ok; ++attempt) {
                if (attempt > 0) {
                    vTaskDelay(pdMS_TO_TICKS(1100));
                }
                ok = ReadRaw(data);
            }
            last_read_us_ = esp_timer_get_time();
            last_ok_ = ok;
            if (ok) {
                int new_humidity = data[0];
                int new_temperature = (data[3] & 0x80) ? -data[2] : data[2];
                if (new_temperature != temperature_ || new_humidity != humidity_) {
                    ESP_LOGI(kTag, "Temperature %d C, humidity %d %%", new_temperature, new_humidity);
                }
                temperature_ = new_temperature;
                humidity_ = new_humidity;
            }
        }
        temperature = temperature_;
        humidity = humidity_;
        return last_ok_;
    }
};

#endif // __DHT11_SENSOR_H__
