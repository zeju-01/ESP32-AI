#define _USE_MATH_DEFINES
#include "mpu6050.h"
#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <math.h>

#define TAG "Mpu6050"

Mpu6050::Mpu6050(i2c_master_bus_handle_t i2c_bus, uint8_t addr)
    : I2cDevice(i2c_bus, addr),
      accel_x_(0), accel_y_(0), accel_z_(0),
      gyro_x_(0), gyro_y_(0), gyro_z_(0), temp_(0),
      pitch_(0), roll_(0), yaw_(0),
      accel_scale_(16384.0f), gyro_scale_(131.0f),
      accel_range_(ACCEL_RANGE_2G), gyro_range_(GYRO_RANGE_250) {
}

bool Mpu6050::Initialize() {
    if (!TestConnection()) {
        ESP_LOGE(TAG, "MPU6050 connection failed!");
        return false;
    }

    SetSleep(false);

    esp_err_t ret = WriteReg(PWR_MGMT_1, 0x01);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write PWR_MGMT_1 register: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = WriteReg(CONFIG, 0x03);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write CONFIG register: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = WriteReg(ACCEL_CONFIG, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write ACCEL_CONFIG register: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = WriteReg(GYRO_CONFIG, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write GYRO_CONFIG register: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = WriteReg(SMPLRT_DIV, 0x07);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write SMPLRT_DIV register: %s", esp_err_to_name(ret));
        return false;
    }

    SetAccelRange(ACCEL_RANGE_2G);
    SetGyroRange(GYRO_RANGE_250);

    ESP_LOGI(TAG, "MPU6050 initialized successfully");
    return true;
}

bool Mpu6050::TestConnection() {
    uint8_t who = 0;
    esp_err_t ret = ReadReg(WHO_AM_I, who);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WHO_AM_I register: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "MPU6050 WHO_AM_I = 0x%02X (expected 0x68)", who);
    // 即使WHO_AM_I不匹配，也继续尝试初始化
    // return (who == 0x68);
    return true;
}

void Mpu6050::SetSleep(bool enable) {
    uint8_t pwr_mgmt = 0;
    esp_err_t ret = ReadReg(PWR_MGMT_1, pwr_mgmt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read PWR_MGMT_1 register: %s", esp_err_to_name(ret));
        return;
    }
    
    if (enable) {
        pwr_mgmt |= 0x40;
    } else {
        pwr_mgmt &= ~0x40;
    }
    
    ret = WriteReg(PWR_MGMT_1, pwr_mgmt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write PWR_MGMT_1 register: %s", esp_err_to_name(ret));
    }
}

void Mpu6050::SetAccelRange(uint8_t range) {
    accel_range_ = range;
    esp_err_t ret = WriteReg(ACCEL_CONFIG, range << 3);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write ACCEL_CONFIG register: %s", esp_err_to_name(ret));
    }

    switch (range) {
        case ACCEL_RANGE_2G:
            accel_scale_ = 16384.0f;
            break;
        case ACCEL_RANGE_4G:
            accel_scale_ = 8192.0f;
            break;
        case ACCEL_RANGE_8G:
            accel_scale_ = 4096.0f;
            break;
        case ACCEL_RANGE_16G:
            accel_scale_ = 2048.0f;
            break;
    }
}

void Mpu6050::SetGyroRange(uint8_t range) {
    gyro_range_ = range;
    esp_err_t ret = WriteReg(GYRO_CONFIG, range << 3);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write GYRO_CONFIG register: %s", esp_err_to_name(ret));
    }

    switch (range) {
        case GYRO_RANGE_250:
            gyro_scale_ = 131.0f;
            break;
        case GYRO_RANGE_500:
            gyro_scale_ = 65.5f;
            break;
        case GYRO_RANGE_1000:
            gyro_scale_ = 32.8f;
            break;
        case GYRO_RANGE_2000:
            gyro_scale_ = 16.4f;
            break;
    }
}

void Mpu6050::ReadAccel(int16_t& x, int16_t& y, int16_t& z) {
    uint8_t buffer[6] = {0};
    esp_err_t ret = ReadRegs(ACCEL_OUT, buffer, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read accelerometer data: %s", esp_err_to_name(ret));
        x = y = z = 0;
        return;
    }

    x = (int16_t)((buffer[0] << 8) | buffer[1]);
    y = (int16_t)((buffer[2] << 8) | buffer[3]);
    z = (int16_t)((buffer[4] << 8) | buffer[5]);
}

void Mpu6050::ReadGyro(int16_t& x, int16_t& y, int16_t& z) {
    uint8_t buffer[6] = {0};
    esp_err_t ret = ReadRegs(GYRO_OUT, buffer, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read gyroscope data: %s", esp_err_to_name(ret));
        x = y = z = 0;
        return;
    }

    x = (int16_t)((buffer[0] << 8) | buffer[1]);
    y = (int16_t)((buffer[2] << 8) | buffer[3]);
    z = (int16_t)((buffer[4] << 8) | buffer[5]);
}

void Mpu6050::ReadTemp(int16_t& temp) {
    uint8_t buffer[2] = {0};
    esp_err_t ret = ReadRegs(TEMP_OUT, buffer, 2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature data: %s", esp_err_to_name(ret));
        temp = 0;
        return;
    }
    temp = (int16_t)((buffer[0] << 8) | buffer[1]);
}

void Mpu6050::ReadAll(int16_t& ax, int16_t& ay, int16_t& az,
                      int16_t& gx, int16_t& gy, int16_t& gz,
                      int16_t& temperature) {
    ReadAccel(ax, ay, az);
    ReadGyro(gx, gy, gz);
    ReadTemp(temperature);
}

float Mpu6050::complementaryFilter(float gyroAngle, float accelAngle, float alpha) {
    return alpha * gyroAngle + (1.0f - alpha) * accelAngle;
}

void Mpu6050::ComputeAngles() {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;

    ReadAccel(ax, ay, az);
    ReadGyro(gx, gy, gz);

    float accel_angle_pitch = atan2f((float)ay, sqrtf((float)ax * (float)ax + (float)az * (float)az)) * 180.0f / M_PI;
    float accel_angle_roll = atan2f((float)ax, sqrtf((float)ay * (float)ay + (float)az * (float)az)) * 180.0f / M_PI;

    float gyro_x = gx / gyro_scale_;
    float gyro_y = gy / gyro_scale_;
    float gyro_z = gz / gyro_scale_;

    static float last_pitch = 0;
    static float last_roll = 0;
    static uint64_t last_time = 0;

    uint64_t current_time = esp_timer_get_time();
    float dt = 0;
    if (last_time > 0) {
        dt = (current_time - last_time) / 1000000.0f;
    }
    last_time = current_time;

    pitch_ = complementaryFilter(pitch_ + gyro_x * dt, accel_angle_pitch, 0.98f);
    roll_ = complementaryFilter(roll_ + gyro_y * dt, accel_angle_roll, 0.98f);
    yaw_ += gyro_z * dt;

    accel_x_ = (float)ax / accel_scale_;
    accel_y_ = (float)ay / accel_scale_;
    accel_z_ = (float)az / accel_scale_;
    gyro_x_ = (float)gx / gyro_scale_;
    gyro_y_ = (float)gy / gyro_scale_;
    gyro_z_ = (float)gz / gyro_scale_;

    int16_t raw_temp;
    ReadTemp(raw_temp);
    temp_ = (raw_temp / 340.0f) + 36.53f;
}

void Mpu6050::Update() {
    ComputeAngles();
}