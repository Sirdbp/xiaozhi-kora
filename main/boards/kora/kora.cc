#include "sdkconfig.h"

#include "wifi_board.h"

#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "display/lcd_display.h"
#include "button.h"
#include "assets/lang_config.h"
#include "config.h"
#include "dht11_sensor.h"
#include "aura_display.h"
#include "aura_motion.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "soc/usb_serial_jtag_struct.h"
#include "esp_ldo_regulator.h"

#include <cstddef>
#include <cstdint>

#define TAG "Kora"

// Kora: AURA robot (GC9A01 face, DHT11, TTP223, PCA9685 body) on the Waveshare ESP32-P4-NANO (ESP32-C6).
class KoraBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    Button boot_button_;
    Button touch_button_;
    Dht11Sensor* dht11_ = nullptr;
    AuraDisplay* display__ = nullptr;
    AuraMotion motion_;

    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeLCD() {
        ESP_LOGI(TAG, "Initializing GC9A01 SPI display");

        // GPIO24 is USB-Serial-JTAG D- on ESP32-P4. While the USB pad is enabled
        // (secondary USJ console is on), the USB PHY owns the pin and the SPI CS
        // signal never reaches the panel. Release the pad before using GPIO24 as CS.
        // Side effect: the USB-Serial-JTAG console/JTAG stops working after this point.
        USB_SERIAL_JTAG.conf0.usb_pad_enable = 0;
        gpio_reset_pin(DISPLAY_CS_PIN);

        // GPIO39..48 (SCK 47, MOSI 48, DC 46) live in the VDDPST_5 (SD) IO domain,
        // which on ESP32-P4 boards is fed by on-chip LDO channel 4. ESP-IDF leaves
        // it off unless something acquires it, so those pins never drive 3.3 V.
        static esp_ldo_channel_handle_t sd_io_ldo = nullptr;
        esp_ldo_channel_config_t ldo_cfg = {};
        ldo_cfg.chan_id = 4;
        ldo_cfg.voltage_mv = 3300;
        esp_err_t err = esp_ldo_acquire_channel(&ldo_cfg, &sd_io_ldo);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable LDO4 for GPIO39-48: %s (display will stay black)", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(10));

        spi_bus_config_t bus_cfg = {};
        bus_cfg.mosi_io_num = DISPLAY_MOSI_PIN;
        bus_cfg.miso_io_num = GPIO_NUM_NC;
        bus_cfg.sclk_io_num = DISPLAY_CLK_PIN;
        bus_cfg.quadwp_io_num = GPIO_NUM_NC;
        bus_cfg.quadhd_io_num = GPIO_NUM_NC;
        bus_cfg.max_transfer_sz = DISPLAY_WIDTH * 20 * sizeof(uint16_t);  // LVGL flushes 20 rows

        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_cfg = {};
        io_cfg.cs_gpio_num = DISPLAY_CS_PIN;
        io_cfg.dc_gpio_num = DISPLAY_DC_PIN;
        io_cfg.spi_mode = DISPLAY_SPI_MODE;
        io_cfg.pclk_hz = 20 * 1000 * 1000;
        io_cfg.trans_queue_depth = 10;
        io_cfg.lcd_cmd_bits = 8;
        io_cfg.lcd_param_bits = 8;

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_cfg, &panel_io));

        esp_lcd_panel_dev_config_t panel_cfg = {};
        panel_cfg.reset_gpio_num = DISPLAY_RST_PIN;
        panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;  // panel is BGR: with RGB, blue showed as brown
        panel_cfg.bits_per_pixel = LCD_BIT_PER_PIXEL;

        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_cfg, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        ESP_LOGI(TAG, "GC9A01 initialized (%d x %d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);

        display__ = new AuraDisplay(
            panel_io,
            panel,
            DISPLAY_WIDTH,
            DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X,
            DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X,
            DISPLAY_MIRROR_Y,
            DISPLAY_SWAP_XY
        );
    }

    // Refreshes the temperature/humidity badge every 10 s. When the temperature crosses the
    // hot/cold limits (1 °C hysteresis) the robot reacts with a face and a notification.
    static void EnvironmentTask(void* arg) {
        auto* board = static_cast<KoraBoard*>(arg);
        int alert = 0;  // 1 hot, -1 cold
        while (true) {
            if (board->display__->IsSetupUICalled()) {
                int temperature = 0, humidity = 0;
                bool ok = board->dht11_->Read(temperature, humidity, 1);
                if (ok) {
                    int new_alert = alert;
                    if (temperature >= AURA_TEMP_HOT_C) {
                        new_alert = 1;
                    } else if (temperature <= AURA_TEMP_COLD_C) {
                        new_alert = -1;
                    } else if ((alert == 1 && temperature <= AURA_TEMP_HOT_C - 1) ||
                               (alert == -1 && temperature >= AURA_TEMP_COLD_C + 1)) {
                        new_alert = 0;
                    }
                    if (new_alert != alert && new_alert != 0) {
                        char message[48];
                        snprintf(message, sizeof(message), new_alert > 0 ? "¡Hace calor! %d°C" : "¡Hace frío! %d°C",
                                 temperature);
                        board->display__->ShowNotification(message, 5000);
                        board->display__->SetMoodByName(new_alert > 0 ? "Cansado 2" : "Asustado");
                        ESP_LOGW(TAG, "Temperature alert: %s (%d C)", new_alert > 0 ? "hot" : "cold", temperature);
                    }
                    alert = new_alert;
                }
                board->display__->SetEnvironment(temperature, humidity, ok, ok ? alert : 0);
            }
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        // TTP223 touch module: same as a BOOT click, but never enters Wi-Fi config
        // mode so an accidental touch during startup is harmless.
        touch_button_.OnClick([]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() != kDeviceStateStarting) {
                app.ToggleChatState();
            }
        });

        // Long press (1.5 s) cycles the speaker volume 30 -> 60 -> 90 -> 30.
        touch_button_.OnLongPress([this]() {
            auto codec = GetAudioCodec();
            int volume = 30;
            for (int step : {30, 60, 90}) {
                if (step > codec->output_volume()) {
                    volume = step;
                    break;
                }
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
    }

public:
    KoraBoard() : boot_button_(BOOT_BUTTON_GPIO), touch_button_(AURA_TOUCH_GPIO, true, 1500) {
        InitializeCodecI2c();
        InitializeLCD();
        InitializeButtons();
        dht11_ = new Dht11Sensor(AURA_DHT_GPIO);
        int temperature = 0, humidity = 0;
        if (!dht11_->Read(temperature, humidity, 1)) {  // single try so a missing sensor does not delay boot
            ESP_LOGW(TAG, "DHT11 not responding on GPIO%d", AURA_DHT_GPIO);
        }
        xTaskCreate(EnvironmentTask, "aura_env", 4096, this, 2, nullptr);
        InitializeFaceTools();
        // Optional body: 4 legs + pan/tilt head on a PCA9685 (same I2C bus as the codec)
        if (motion_.Init(codec_i2c_bus_)) {
            display__->OnMoodChanged([this](int mood) { motion_.ReactToMood(mood); });
        }
    }

    // Lets the assistant (or a child asking it) show any of the 100 robot faces.
    void InitializeFaceTools() {
        McpServer::GetInstance().AddTool("self.face.set_mood",
            "Show one of the robot's 100 facial expressions on the screen. Use it when the user asks "
            "for a face or mood (e.g. \"pon cara de enamorado\"), or to act out a feeling. "
            "`mood` is the number or the Spanish name. Available: " + AuraDisplay::MoodList(),
            PropertyList({Property("mood", kPropertyTypeString)}),
            [this](const PropertyList& properties) -> ReturnValue {
                int number = display__->SetMoodByName(properties["mood"].value<std::string>());
                if (number == 0) {
                    return "{\"success\": false, \"message\": \"Unknown mood\"}";
                }
                return std::string("{\"success\": true, \"mood\": \"") + AuraDisplay::MoodName(number) + "\"}";
            });
    }

    AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, I2C_NUM_1,
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR
        );
        return &audio_codec;
    }

    Display* GetDisplay() override {
        return display__;
    }

    Camera* GetCamera() override {
        return nullptr;
    }

    Backlight* GetBacklight() override {
        return nullptr;
    }
};

DECLARE_BOARD(KoraBoard);
