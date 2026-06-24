#include "wifi_board.h"
#include "codecs/es8374_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include <driver/gpio.h>
#include "esp32_camera.h"
#include "mpu6050_app.h"
#include "sht30_app.h"
#include "i2c_scanner.h"
#include "esp_io_expander_pca9555.h"

#include <vector>

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>

// PCA9555 test pin - P13 (Port 1, Pin 3)
// PCA9555的8位写地址是0x40，转换为7位地址：0x40 >> 1 = 0x20
// PCA9555有16个引脚，编号0-15，P00-P07对应0-7，P10-P17对应8-15
// P13 = Port 1, Pin 3 = 引脚编号 11 (P10=8, P11=9, P12=10, P13=11)
#define PCA9555_TEST_PIN 11  // P13对应的引脚编号是11
#define PCA9555_I2C_ADDRESS (0x40 >> 1)  // 7位地址

#if defined(LCD_TYPE_ILI9341_SERIAL)
#include "esp_lcd_ili9341.h"
#endif

#if defined(LCD_TYPE_GC9A01_SERIAL)
#include "esp_lcd_gc9a01.h"
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb1, (uint8_t[]){0x80}, 1, 0},
    {0xb2, (uint8_t[]){0x27}, 1, 0},
    {0xb3, (uint8_t[]){0x13}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x05}, 1, 0},
    {0xac, (uint8_t[]){0xc8}, 1, 0},
    {0xab, (uint8_t[]){0x0f}, 1, 0},
    {0x3a, (uint8_t[]){0x05}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x08}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xea, (uint8_t[]){0x02}, 1, 0},
    {0xe8, (uint8_t[]){0x2A}, 1, 0},
    {0xe9, (uint8_t[]){0x47}, 1, 0},
    {0xe7, (uint8_t[]){0x5f}, 1, 0},
    {0xc6, (uint8_t[]){0x21}, 1, 0},
    {0xc7, (uint8_t[]){0x15}, 1, 0},
    {0xf0,
    (uint8_t[]){0x1D, 0x38, 0x09, 0x4D, 0x92, 0x2F, 0x35, 0x52, 0x1E, 0x0C,
                0x04, 0x12, 0x14, 0x1f},
    14, 0},
    {0xf1,
    (uint8_t[]){0x16, 0x40, 0x1C, 0x54, 0xA9, 0x2D, 0x2E, 0x56, 0x10, 0x0D,
                0x0C, 0x1A, 0x14, 0x1E},
    14, 0},
    {0xf4, (uint8_t[]){0x00, 0x00, 0xFF}, 3, 0},
    {0xba, (uint8_t[]){0xFF, 0xFF}, 2, 0},
};
#endif
 
#define TAG "CompactWifiBoardS3Cam"

class CompactWifiBoardS3Cam : public WifiBoard {
private: 

    Button boot_button_;
    LcdDisplay* display_;
    Esp32Camera* camera_;
    i2c_master_bus_handle_t i2c_bus_;
    Mpu6050App* mpu6050_app_;
    Sht30App* sht30_app_;
    esp_io_expander_handle_t pca9555_expander_ = NULL;

    void InitializePca9555() {
        ESP_LOGI(TAG, "Initializing PCA9555 IO expander at address 0x%02X...", PCA9555_I2C_ADDRESS);

        esp_err_t ret = esp_io_expander_new_i2c_pca9555(i2c_bus_, PCA9555_I2C_ADDRESS, &pca9555_expander_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "PCA9555 create failed: %s", esp_err_to_name(ret));
            return;
        }

        // 配置IO方向: 0=输出, 1=输入
        pca9555_set_pin_direction_direct(pca9555_expander_, 0, 0);   // P00 按键1 (输入)
        pca9555_set_pin_direction_direct(pca9555_expander_, 1, 1);   // P01 马达 (输出)
        pca9555_set_pin_direction_direct(pca9555_expander_, 5, 1);   // P05 显示屏CS (输出)
        pca9555_set_pin_direction_direct(pca9555_expander_, 6, 1);   // P06 显示屏RST (输出)
        pca9555_set_pin_direction_direct(pca9555_expander_, 7, 1);   // P07 背光控制 (输出)
        pca9555_set_pin_direction_direct(pca9555_expander_, 8, 1);   // P10 红外发射 (输出)
        pca9555_set_pin_direction_direct(pca9555_expander_, 9, 0);   // P11 红外接收 (输入)
        pca9555_set_pin_direction_direct(pca9555_expander_, 11, 1);  // P13 LED (输出)
        pca9555_set_pin_direction_direct(pca9555_expander_, 13, 0);  // P15 编码开关左 (输入)
        pca9555_set_pin_direction_direct(pca9555_expander_, 14, 0);  // P16 编码开关右 (输入)
        pca9555_set_pin_direction_direct(pca9555_expander_, 15, 0);  // P17 按键2 (输入)

        // 设置初始电平
        pca9555_set_pin_level_direct(pca9555_expander_, 5, 1);  // CS HIGH(未选中)
        pca9555_set_pin_level_direct(pca9555_expander_, 6, 1);  // RST HIGH
        pca9555_set_pin_level_direct(pca9555_expander_, 7, 0);  // 背光ON(低电平有效)
        pca9555_set_pin_level_direct(pca9555_expander_, 1, 0);  // 马达关闭
        pca9555_set_pin_level_direct(pca9555_expander_, 8, 0);  // 红外发射关闭

        // 硬件复位显示屏（与Korvo-2的InitializeTca9554一致）
        vTaskDelay(pdMS_TO_TICKS(300));
        pca9555_set_pin_level_direct(pca9555_expander_, 6, 0); // RST LOW
        vTaskDelay(pdMS_TO_TICKS(300));
        pca9555_set_pin_level_direct(pca9555_expander_, 6, 1); // RST HIGH

        // 启动PCA9555按键轮询任务（替代GPIO 0按键，因为GPIO 0被SPI MOSI占用）
        xTaskCreatePinnedToCore(Pca9555ButtonPollTask, "PCA9555_BtnPoll", 2048, this, 5, NULL, 0);
    }

    /**
     * @brief PCA9555 P00 (KEY1) 按键轮询任务
     *
     * 由于GPIO 0被SPI MOSI占用，使用PCA9555的P00引脚作为启动按键输入。
     * 通过轮询PCA9555输入寄存器检测按键按下事件。
     */
    static void Pca9555ButtonPollTask(void* arg) {
        CompactWifiBoardS3Cam* board = (CompactWifiBoardS3Cam*)arg;
        bool last_state = true;  // 按键未按下时为高电平（上拉）
        uint8_t debounce_count = 0;
        const uint8_t DEBOUNCE_THRESHOLD = 3;  // 消抖阈值
        const int LONG_PRESS_MS = 3000;  // 长按阈值3秒
        int press_duration = 0;  // 按下持续时间(毫秒)
        bool long_press_triggered = false;

        ESP_LOGI(TAG, "PCA9555 button poll task started, monitoring P00 (KEY1)");

        if (board->pca9555_expander_ == NULL) {
            ESP_LOGE(TAG, "PCA9555 expander handle is NULL! Button poll task exiting.");
            vTaskDelete(NULL);
            return;
        }

        while (true) {
            // 使用esp_io_expander框架API读取P00引脚电平
            uint32_t level = 0;
            esp_err_t ret = esp_io_expander_get_level(board->pca9555_expander_, 0x01, &level);

            if (ret != ESP_OK) {
                ESP_LOGD(TAG, "Failed to read PCA9555 P00 level: %s", esp_err_to_name(ret));
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }

            // P00对应bit 0，低电平表示按键按下
            bool current_state = (level & 0x01) != 0;

            if (current_state != last_state) {
                debounce_count++;
                if (debounce_count >= DEBOUNCE_THRESHOLD) {
                    debounce_count = 0;
                    if (last_state && !current_state) {
                        // 检测到下降沿（按键按下）
                        ESP_LOGI(TAG, "PCA9555 P00 (KEY1) button pressed");
                        press_duration = 0;
                        long_press_triggered = false;
                    } else if (!last_state && current_state) {
                        // 检测到上升沿（按键释放）
                        if (!long_press_triggered) {
                            // 短按动作
                            auto& app = Application::GetInstance();
                            if (app.GetDeviceState() == kDeviceStateStarting) {
                                board->EnterWifiConfigMode();
                            } else {
                                app.ToggleChatState();
                            }
                        }
                    }
                    last_state = current_state;
                }
            } else {
                debounce_count = 0;
            }

            // 长按检测：按键持续按下时累计时间
            if (!last_state && !long_press_triggered) {
                press_duration += 20;  // 20ms轮询间隔
                if (press_duration >= LONG_PRESS_MS) {
                    long_press_triggered = true;
                    auto& app = Application::GetInstance();
                    auto state = app.GetDeviceState();
                    ESP_LOGI(TAG, "PCA9555 P00 (KEY1) long press detected, state=%d", state);
                    // 长按3秒：在Idle/Listening/Speaking状态下重新配网
                    if (state == kDeviceStateIdle || state == kDeviceStateListening ||
                        state == kDeviceStateSpeaking || state == kDeviceStateStarting) {
                        ESP_LOGI(TAG, "Entering WiFi config mode via long press");
                        board->EnterWifiConfigMode();
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(20));  // 20ms轮询间隔
        }
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        if (pca9555_expander_ == nullptr) {
            ESP_LOGE(TAG, "PCA9555 not initialized, cannot initialize LCD display");
            return;
        }

        // 安装SPI面板IO
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = 6;  // 虚拟CS（实际CS由PCA9555 P05控制）
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;  // 40MHz
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 安装LCD驱动
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;

#ifdef LCD_TYPE_GC9A01_SERIAL
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds,
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
        };
        panel_config.vendor_config = &gc9107_vendor_config;
#endif

#if defined(LCD_TYPE_ILI9341_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_GC9A01_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif

        // 初始化序列（与Korvo-2一致）
        esp_lcd_panel_reset(panel);
        pca9555_set_pin_level_direct(pca9555_expander_, 5, 0); // CS LOW
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));

        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);

        // 开启背光（低电平有效）
        pca9555_set_pin_level_direct(pca9555_expander_, 7, 0);
        ESP_LOGI(TAG, "LCD display initialized successfully");
    }

    void InitializeCamera() {
        camera_config_t config = {};
        config.pin_d0 = CAMERA_PIN_D0;
        config.pin_d1 = CAMERA_PIN_D1;
        config.pin_d2 = CAMERA_PIN_D2;
        config.pin_d3 = CAMERA_PIN_D3;
        config.pin_d4 = CAMERA_PIN_D4;
        config.pin_d5 = CAMERA_PIN_D5;
        config.pin_d6 = CAMERA_PIN_D6;
        config.pin_d7 = CAMERA_PIN_D7;
        config.pin_xclk = CAMERA_PIN_XCLK;
        config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC;
        config.pin_href = CAMERA_PIN_HREF;
        // 设置pin_sccb_sda为-1，让摄像头驱动使用已初始化的I2C端口
        // 这样摄像头SCCB会复用主I2C总线（I2C_NUM_0），与PCA9555、ES8374等设备共享
        config.pin_sccb_sda = -1;
        config.pin_sccb_scl = -1;
        config.sccb_i2c_port = I2C_NUM_0;  // 使用主I2C端口
        config.pin_pwdn = CAMERA_PIN_PWDN;
        config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        camera_ = new Esp32Camera(config);
        camera_->SetHMirror(false);
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
    }

    void InitializeI2c() {
        // 使用自定义I2C总线（GPIO 17=SDA, GPIO 18=SCL）
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = GPIO_NUM_17,
            .scl_io_num = GPIO_NUM_18,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        esp_err_t ret = i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
            return;
        }

        // 扫描I2C总线，查看总线上的设备
        ScanI2cBus(i2c_bus_);

        // 初始化MPU6050加速度陀螺仪传感器（地址位接VCC，地址为0x69）
        mpu6050_app_ = new Mpu6050App(i2c_bus_);
        if (mpu6050_app_->Initialize()) {
            ESP_LOGI(TAG, "MPU6050 initialized successfully");
        } else {
            ESP_LOGE(TAG, "Failed to initialize MPU6050, continuing without it");
            delete mpu6050_app_;
            mpu6050_app_ = nullptr;
        }

        // 初始化SHT30温湿度传感器
        sht30_app_ = new Sht30App(i2c_bus_);
        if (sht30_app_->Initialize()) {
            ESP_LOGI(TAG, "SHT30 initialized successfully");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SHT30, continuing without it");
            delete sht30_app_;
            sht30_app_ = nullptr;
        }

        // 初始化PCA9555 IO扩展芯片
        InitializePca9555();
    }

public:
    CompactWifiBoardS3Cam() :
        boot_button_(BOOT_BUTTON_GPIO),
        i2c_bus_(nullptr),
        mpu6050_app_(nullptr),
        sht30_app_(nullptr) {
        InitializeI2c();           // I2C + PCA9555（含硬件复位）
        InitializeCamera();
        InitializeSpi();           // SPI在复位之后初始化
        InitializeLcdDisplay();
        InitializeButtons();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        // PCA9555使用7位地址0x40，ES8374使用7位地址0x20，不会冲突
        static Es8374AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8374_ADDR, true);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        // 背光由PCA9555的P07控制（低电平开启），使用直接I2C操作避免esp_io_expander_set_level的内部映射错误
        if (pca9555_expander_) {
            pca9555_set_pin_level_direct(pca9555_expander_, 7, 0); // 背光开启(低电平有效)
            ESP_LOGI(TAG, "Backlight P07 set to level 0 (active low) via direct I2C");
        }
        return nullptr;  // 背光由PCA9555直接控制，不使用PwmBacklight
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }

    void UpdateSensors() {
        auto display = GetDisplay();
        if (mpu6050_app_) {
            mpu6050_app_->Update();
            mpu6050_app_->PrintData();
            
            if (display) {
                ESP_LOGI(TAG, "Calling display->SetMpu6050Data with Pitch=%.1f, Roll=%.1f, Yaw=%.1f",
                         mpu6050_app_->GetPitch(), mpu6050_app_->GetRoll(), mpu6050_app_->GetYaw());
                display->SetMpu6050Data(mpu6050_app_->GetPitch(), mpu6050_app_->GetRoll(), mpu6050_app_->GetYaw());
            } else {
                ESP_LOGW(TAG, "display is nullptr in UpdateSensors!");
            }
        }
        if (sht30_app_) {
            sht30_app_->Update();
            sht30_app_->PrintData();
            
            if (display) {
                display->SetTemperatureHumidity(sht30_app_->GetTemperature(), sht30_app_->GetHumidity());
            }
        }
    }

    void UpdateSensorsSafe() {
        // Safe update method that checks all pointers before access
        auto display = GetDisplay();
        if (mpu6050_app_ != nullptr) {
            mpu6050_app_->Update();
            mpu6050_app_->PrintData();
            
            if (display != nullptr) {
                display->SetMpu6050Data(mpu6050_app_->GetPitch(), mpu6050_app_->GetRoll(), mpu6050_app_->GetYaw());
            }
        }
        if (sht30_app_ != nullptr) {
            sht30_app_->Update();
            sht30_app_->PrintData();
            
            if (display != nullptr) {
                display->SetTemperatureHumidity(sht30_app_->GetTemperature(), sht30_app_->GetHumidity());
            }
        }
    }

    Mpu6050App* GetMpu6050App() {
        return mpu6050_app_;
    }

    Sht30App* GetSht30App() {
        return sht30_app_;
    }
};

DECLARE_BOARD(CompactWifiBoardS3Cam);
