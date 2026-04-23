#include <esp_log.h>
#include <driver/i2c_master.h>

#define TAG "I2cScanner"

void ScanI2cBus(i2c_master_bus_handle_t i2c_bus) {
    ESP_LOGI(TAG, "Scanning I2C bus...");
    
    for (uint8_t addr = 0; addr < 128; addr++) {
        i2c_device_config_t i2c_device_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100 * 1000,
            .scl_wait_us = 0,
            .flags = {
                .disable_ack_check = 0,
            },
        };
        
        i2c_master_dev_handle_t i2c_device;
        esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &i2c_device_cfg, &i2c_device);
        if (ret == ESP_OK) {
            // 尝试读取一个字节，看看设备是否响应
            uint8_t dummy = 0;
            ret = i2c_master_transmit_receive(i2c_device, &dummy, 1, &dummy, 1, 100);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Found device at address 0x%02X", addr);
            } else if (ret == ESP_ERR_INVALID_RESPONSE) {
                // 有些设备可能不响应读操作，但会响应地址
                ESP_LOGI(TAG, "Found device at address 0x%02X (no response to read)", addr);
            }
            i2c_master_bus_rm_device(i2c_device);
        }
    }
    
    ESP_LOGI(TAG, "I2C bus scan completed");
}
