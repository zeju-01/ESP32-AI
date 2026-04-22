#ifndef SHT30_H
#define SHT30_H

#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "i2c_device.h"

class Sht30 : public I2cDevice {
public:
    Sht30(i2c_master_bus_handle_t i2c_bus, uint8_t addr = 0x44);

    bool Initialize();
    bool TestConnection();

    bool ReadTemperatureHumidity(float& temperature, float& humidity);
    float GetTemperature() const { return temperature_; }
    float GetHumidity() const { return humidity_; }

    void SoftReset();
    void Heater(bool enable);

    void Update();

    enum Commands {
        MEASURE_HIGH_PREcision = 0x2400,
        MEASURE_MEDIUM_PREcision = 0x240B,
        MEASURE_LOW_PREcision = 0x2416,
        READ_STATUS = 0xF32D,
        CLEAR_STATUS = 0x3041,
        HEATER_ENABLE = 0x306D,
        HEATER_DISABLE = 0x3066,
        SOFT_RESET = 0x30A2
    };

private:
    float temperature_;
    float humidity_;
    uint32_t read_fail_count_;      // 连续失败计数
    TickType_t last_successful_read_; // 上次成功读取的时间

    bool SendCommand(uint16_t command);
    bool ReadRawData(uint16_t& temp_raw, uint16_t& hum_raw);
};

#endif // SHT30_H
