#ifndef SHT30_APP_H
#define SHT30_APP_H

#include "sht30.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class Sht30App {
public:
    Sht30App(i2c_master_bus_handle_t i2c_bus);
    ~Sht30App();

    bool Initialize();
    void Update();

    float GetTemperature();
    float GetHumidity();
    bool IsDataValid();
    void PrintData();

private:
    Sht30* sht30_;
    TickType_t last_read_time_;     // 上次读取时间
    float current_temperature_;     // 当前温度
    float current_humidity_;        // 当前湿度
    bool is_valid_;                 // 数据是否有效
};

#endif // SHT30_APP_H
