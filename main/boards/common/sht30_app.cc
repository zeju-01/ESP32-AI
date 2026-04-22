#include "sht30_app.h"
#include <esp_log.h>

static const char* TAG = "SHT30_APP";

// 读取间隔：2秒
#define READ_INTERVAL_MS 2000

Sht30App::Sht30App(i2c_master_bus_handle_t i2c_bus) {
    sht30_ = new Sht30(i2c_bus);
    last_read_time_ = 0;
    current_temperature_ = 0.0f;
    current_humidity_ = 0.0f;
    is_valid_ = false;
}

Sht30App::~Sht30App() {
    if (sht30_) {
        delete sht30_;
    }
}

bool Sht30App::Initialize() {
    if (!sht30_) {
        ESP_LOGE(TAG, "SHT30 not initialized");
        return false;
    }

    bool result = sht30_->Initialize();
    if (result) {
        // 初始化成功后立即读取一次数据
        float temp, hum;
        if (sht30_->ReadTemperatureHumidity(temp, hum)) {
            current_temperature_ = temp;
            current_humidity_ = hum;
            is_valid_ = true;
            ESP_LOGI(TAG, "Initial reading - Temperature: %.2f C, Humidity: %.2f %%", temp, hum);
        }
    }
    return result;
}

void Sht30App::Update() {
    if (!sht30_) {
        return;
    }

    // 检查是否到达读取间隔
    TickType_t current_time = xTaskGetTickCount();
    if ((current_time - last_read_time_) < pdMS_TO_TICKS(READ_INTERVAL_MS)) {
        return; // 还未到达读取间隔，跳过
    }

    last_read_time_ = current_time;

    float temp, hum;
    bool success = sht30_->ReadTemperatureHumidity(temp, hum);
    
    if (success) {
        // 读取成功，更新数据
        current_temperature_ = temp;
        current_humidity_ = hum;
        is_valid_ = true;
        
        // 每10次成功读取输出一次日志
        static int success_count = 0;
        success_count++;
        if (success_count % 10 == 1) {
            ESP_LOGI(TAG, "Temperature: %.2f C, Humidity: %.2f %%", temp, hum);
        }
    } else {
        // 读取失败，但保持上一次的数据
        // 只有在有有效数据的情况下才输出警告
        if (is_valid_) {
            ESP_LOGD(TAG, "Using cached data - Temperature: %.2f C, Humidity: %.2f %%", 
                     current_temperature_, current_humidity_);
        }
    }
}

float Sht30App::GetTemperature() {
    return current_temperature_;
}

float Sht30App::GetHumidity() {
    return current_humidity_;
}

bool Sht30App::IsDataValid() {
    return is_valid_;
}

void Sht30App::PrintData() {
    if (is_valid_) {
        ESP_LOGI(TAG, "Temperature: %.2f C, Humidity: %.2f %%", 
                 current_temperature_, current_humidity_);
    } else {
        ESP_LOGW(TAG, "No valid data available");
    }
}
