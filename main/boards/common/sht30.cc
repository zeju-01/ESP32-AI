#include "sht30.h"
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "SHT30";

Sht30::Sht30(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr),
                                                              temperature_(0.0),
                                                              humidity_(0.0),
                                                              read_fail_count_(0),
                                                              last_successful_read_(0) {
}

bool Sht30::Initialize() {
    ESP_LOGI(TAG, "Initializing SHT30");
    if (!TestConnection()) {
        ESP_LOGE(TAG, "SHT30 not found - check wiring");
        return false;
    }

    ESP_LOGI(TAG, "SHT30 found, performing soft reset");
    SoftReset();
    ESP_LOGI(TAG, "SHT30 initialized successfully");
    return true;
}

bool Sht30::TestConnection() {
    ESP_LOGI(TAG, "Testing SHT30 connection");
    // 增加重试机制
    for (int i = 0; i < 3; i++) {
        if (SendCommand(Commands::READ_STATUS)) {
            ESP_LOGI(TAG, "Test connection result: success");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGE(TAG, "Test connection result: failed");
    return false;
}

bool Sht30::SendCommand(uint16_t command) {
    uint8_t cmd[2] = {
        static_cast<uint8_t>(command >> 8),
        static_cast<uint8_t>(command & 0xFF)
    };

    esp_err_t ret = i2c_master_transmit(i2c_device_, cmd, 2, 100);
    return ret == ESP_OK;
}

bool Sht30::ReadRawData(uint16_t& temp_raw, uint16_t& hum_raw) {
    uint8_t data[6];
    
    esp_err_t ret = i2c_master_receive(i2c_device_, data, 6, 100);
    if (ret != ESP_OK) {
        return false;
    }

    temp_raw = (data[0] << 8) | data[1];
    hum_raw = (data[3] << 8) | data[4];

    return true;
}

bool Sht30::ReadTemperatureHumidity(float& temperature, float& humidity) {
    // 发送测量命令
    if (!SendCommand(Commands::MEASURE_HIGH_PREcision)) {
        ESP_LOGD(TAG, "Failed to send measurement command");
        return false;
    }

    // 增加延时等待传感器完成测量（SHT30高精度模式需要约15ms）
    vTaskDelay(pdMS_TO_TICKS(20));

    // 尝试读取数据，最多重试3次
    for (int retry = 0; retry < 3; retry++) {
        uint16_t temp_raw, hum_raw;
        if (ReadRawData(temp_raw, hum_raw)) {
            // 转换数据
            temperature = -45.0f + 175.0f * static_cast<float>(temp_raw) / 65535.0f;
            humidity = 100.0f * static_cast<float>(hum_raw) / 65535.0f;

            // 更新内部状态
            temperature_ = temperature;
            humidity_ = humidity;
            read_fail_count_ = 0;
            last_successful_read_ = xTaskGetTickCount();

            ESP_LOGD(TAG, "Temperature: %.2f C, Humidity: %.2f %%", temperature, humidity);
            return true;
        }

        // 如果读取失败，等待一段时间再重试
        if (retry < 2) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    // 读取失败，增加失败计数
    read_fail_count_++;
    
    // 只有在连续失败多次时才输出错误日志
    if (read_fail_count_ % 10 == 1) {
        ESP_LOGW(TAG, "Failed to read data (fail count: %d), using last valid values", read_fail_count_);
    }

    // 返回上一次成功的数据
    temperature = temperature_;
    humidity = humidity_;
    return false;
}

void Sht30::SoftReset() {
    SendCommand(Commands::SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void Sht30::Heater(bool enable) {
    if (enable) {
        SendCommand(Commands::HEATER_ENABLE);
    } else {
        SendCommand(Commands::HEATER_DISABLE);
    }
}

void Sht30::Update() {
    float temp, hum;
    ReadTemperatureHumidity(temp, hum);
}
