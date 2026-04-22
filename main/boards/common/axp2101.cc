#include "axp2101.h"
#include "board.h"
#include "display.h"

#include <esp_log.h>
#include <esp_err.h>

#define TAG "Axp2101"

Axp2101::Axp2101(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
}

int Axp2101::GetBatteryCurrentDirection() {
    uint8_t value = 0;
    esp_err_t ret = ReadReg(0x01, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read battery current direction: %s", esp_err_to_name(ret));
        return 0;
    }
    return (value & 0b01100000) >> 5;
}

bool Axp2101::IsCharging() {
    return GetBatteryCurrentDirection() == 1;
}

bool Axp2101::IsDischarging() {
    return GetBatteryCurrentDirection() == 2;
}

bool Axp2101::IsChargingDone() {
    uint8_t value = 0;
    esp_err_t ret = ReadReg(0x01, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read charging status: %s", esp_err_to_name(ret));
        return false;
    }
    return (value & 0b00000111) == 0b00000100;
}

int Axp2101::GetBatteryLevel() {
    uint8_t value = 0;
    esp_err_t ret = ReadReg(0xA4, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read battery level: %s", esp_err_to_name(ret));
        return 0;
    }
    return value;
}

float Axp2101::GetTemperature() {
    uint8_t value = 0;
    esp_err_t ret = ReadReg(0xA5, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature: %s", esp_err_to_name(ret));
        return 0.0f;
    }
    return value;
}

void Axp2101::PowerOff() {
    uint8_t value = 0;
    esp_err_t ret = ReadReg(0x10, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read power control register: %s", esp_err_to_name(ret));
        return;
    }
    value = value | 0x01;
    ret = WriteReg(0x10, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write power control register: %s", esp_err_to_name(ret));
    }
}
