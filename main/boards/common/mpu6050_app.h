#ifndef MPU6050_APP_H
#define MPU6050_APP_H

#include <driver/i2c_master.h>
#include "mpu6050.h"

class Mpu6050App {
public:
    Mpu6050App(i2c_master_bus_handle_t i2c_bus);
    ~Mpu6050App();

    bool Initialize();
    void Update();
    void PrintData();

    float GetPitch() const { return mpu6050_->GetPitch(); }
    float GetRoll() const { return mpu6050_->GetRoll(); }
    float GetYaw() const { return mpu6050_->GetYaw(); }

    float GetAccelX() const { return mpu6050_->GetAccelX(); }
    float GetAccelY() const { return mpu6050_->GetAccelY(); }
    float GetAccelZ() const { return mpu6050_->GetAccelZ(); }

    bool IsTiltedLeft() const;
    bool IsTiltedRight() const;
    bool IsTiltedForward() const;
    bool IsTiltedBackward() const;
    bool IsShaken() const;

    void ResetAngles();

private:
    i2c_master_bus_handle_t i2c_bus_;
    Mpu6050* mpu6050_;

    float last_accel_x_;
    float last_accel_y_;
    float last_accel_z_;
    uint32_t shake_count_;
    uint32_t last_update_time_;
};

#endif // MPU6050_APP_H