#include "mpu6050_app.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <math.h>

#define TAG "Mpu6050App"

#define TILT_THRESHOLD 30.0f
#define SHAKE_THRESHOLD 2.5f
#define SHAKE_COUNT_THRESHOLD 3

Mpu6050App::Mpu6050App(i2c_master_bus_handle_t i2c_bus)
    : i2c_bus_(i2c_bus),
      mpu6050_(nullptr),
      last_accel_x_(0),
      last_accel_y_(0),
      last_accel_z_(0),
      shake_count_(0),
      last_update_time_(0) {
}

Mpu6050App::~Mpu6050App() {
    if (mpu6050_) {
        delete mpu6050_;
    }
}

bool Mpu6050App::Initialize() {
    // 首先尝试地址0x68（AD0接GND）
    mpu6050_ = new Mpu6050(i2c_bus_, 0x68);
    if (!mpu6050_->Initialize()) {
        ESP_LOGW(TAG, "Failed to initialize MPU6050 at address 0x68, trying 0x69...");
        delete mpu6050_;
        mpu6050_ = nullptr;
        
        // 再尝试地址0x69（AD0接VCC）
        mpu6050_ = new Mpu6050(i2c_bus_, 0x69);
        if (!mpu6050_->Initialize()) {
            ESP_LOGE(TAG, "Failed to initialize MPU6050 at both addresses!");
            ESP_LOGE(TAG, "Please check:");
            ESP_LOGE(TAG, "  1. MPU6050 AD0 pin connection (try both GND and VCC)");
            ESP_LOGE(TAG, "  2. MPU6050 SDA/SCL pins are connected to GPIO 4/5");
            ESP_LOGE(TAG, "  3. MPU6050 module is not damaged");
            delete mpu6050_;
            mpu6050_ = nullptr;
            return false;
        }
    }
    ESP_LOGI(TAG, "MPU6050 App initialized successfully");
    return true;
}

void Mpu6050App::Update() {
    if (!mpu6050_) {
        return;
    }

    mpu6050_->Update();

    float ax = mpu6050_->GetAccelX();
    float ay = mpu6050_->GetAccelY();
    float az = mpu6050_->GetAccelZ();

    float delta_x = ax - last_accel_x_;
    float delta_y = ay - last_accel_y_;
    float delta_z = az - last_accel_z_;

    float acceleration = sqrtf(delta_x * delta_x + delta_y * delta_y + delta_z * delta_z);

    if (acceleration > SHAKE_THRESHOLD) {
        shake_count_++;
    } else {
        if (shake_count_ > 0) {
            shake_count_--;
        }
    }

    last_accel_x_ = ax;
    last_accel_y_ = ay;
    last_accel_z_ = az;
    last_update_time_ = esp_timer_get_time() / 1000;
}

void Mpu6050App::PrintData() {
    if (!mpu6050_) {
        return;
    }

    ESP_LOGI(TAG, "Accel: X=%.2f Y=%.2f Z=%.2f | Gyro: X=%.2f Y=%.2f Z=%.2f | Temp=%.1f",
             mpu6050_->GetAccelX(), mpu6050_->GetAccelY(), mpu6050_->GetAccelZ(),
             mpu6050_->GetGyroX(), mpu6050_->GetGyroY(), mpu6050_->GetGyroZ(),
             mpu6050_->GetTemp());
    ESP_LOGI(TAG, "Angles: Pitch=%.1f Roll=%.1f Yaw=%.1f",
             mpu6050_->GetPitch(), mpu6050_->GetRoll(), mpu6050_->GetYaw());
}

bool Mpu6050App::IsTiltedLeft() const {
    if (!mpu6050_) return false;
    return mpu6050_->GetRoll() < -TILT_THRESHOLD;
}

bool Mpu6050App::IsTiltedRight() const {
    if (!mpu6050_) return false;
    return mpu6050_->GetRoll() > TILT_THRESHOLD;
}

bool Mpu6050App::IsTiltedForward() const {
    if (!mpu6050_) return false;
    return mpu6050_->GetPitch() > TILT_THRESHOLD;
}

bool Mpu6050App::IsTiltedBackward() const {
    if (!mpu6050_) return false;
    return mpu6050_->GetPitch() < -TILT_THRESHOLD;
}

bool Mpu6050App::IsShaken() const {
    return shake_count_ >= SHAKE_COUNT_THRESHOLD;
}

void Mpu6050App::ResetAngles() {
    if (mpu6050_) {
        mpu6050_->Update();
    }
    last_accel_x_ = 0;
    last_accel_y_ = 0;
    last_accel_z_ = 0;
    shake_count_ = 0;
}