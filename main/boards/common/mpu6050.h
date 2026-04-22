#ifndef MPU6050_H
#define MPU6050_H

#include <driver/i2c_master.h>
#include "i2c_device.h"

class Mpu6050 : public I2cDevice {
public:
    Mpu6050(i2c_master_bus_handle_t i2c_bus, uint8_t addr = 0x68);

    bool Initialize();
    bool TestConnection();

    void ReadAccel(int16_t& x, int16_t& y, int16_t& z);
    void ReadGyro(int16_t& x, int16_t& y, int16_t& z);
    void ReadTemp(int16_t& temp);

    void ReadAll(int16_t& ax, int16_t& ay, int16_t& az,
                 int16_t& gx, int16_t& gy, int16_t& gz,
                 int16_t& temp);

    void SetAccelRange(uint8_t range);
    void SetGyroRange(uint8_t range);
    void SetSleep(bool enable);

    float GetAccelX() const { return accel_x_; }
    float GetAccelY() const { return accel_y_; }
    float GetAccelZ() const { return accel_z_; }
    float GetGyroX() const { return gyro_x_; }
    float GetGyroY() const { return gyro_y_; }
    float GetGyroZ() const { return gyro_z_; }
    float GetTemp() const { return temp_; }

    float GetPitch() const { return pitch_; }
    float GetRoll() const { return roll_; }
    float GetYaw() const { return yaw_; }

    void Update();

    enum AccelRange {
        ACCEL_RANGE_2G  = 0,
        ACCEL_RANGE_4G  = 1,
        ACCEL_RANGE_8G  = 2,
        ACCEL_RANGE_16G = 3
    };

    enum GyroRange {
        GYRO_RANGE_250  = 0,
        GYRO_RANGE_500  = 1,
        GYRO_RANGE_1000 = 2,
        GYRO_RANGE_2000 = 3
    };

    enum Registers {
        SELF_TEST_X       = 0x0D,
        SELF_TEST_Y       = 0x0E,
        SELF_TEST_Z       = 0x0F,
        SELF_TEST_A       = 0x10,
        SMPLRT_DIV        = 0x19,
        CONFIG            = 0x1A,
        GYRO_CONFIG       = 0x1B,
        ACCEL_CONFIG      = 0x1C,
        ACCEL_CONFIG2     = 0x1D,
        FIFO_EN           = 0x23,
        I2C_MST_CTRL      = 0x24,
        I2C_SLV0_ADDR     = 0x25,
        I2C_SLV0_REG      = 0x26,
        I2C_SLV0_CTRL     = 0x27,
        I2C_SLV1_ADDR     = 0x28,
        I2C_SLV1_REG      = 0x29,
        I2C_SLV1_CTRL     = 0x2A,
        I2C_SLV2_ADDR     = 0x2B,
        I2C_SLV2_REG      = 0x2C,
        I2C_SLV2_CTRL     = 0x2D,
        I2C_SLV3_ADDR     = 0x2E,
        I2C_SLV3_REG      = 0x2F,
        I2C_SLV3_CTRL     = 0x30,
        I2C_SLV4_ADDR     = 0x31,
        I2C_SLV4_REG      = 0x32,
        I2C_SLV4_DO       = 0x33,
        I2C_SLV4_CTRL     = 0x34,
        I2C_SLV4_DI       = 0x35,
        I2C_MST_STATUS    = 0x36,
        INT_PIN_CFG       = 0x37,
        INT_ENABLE        = 0x38,
        INT_STATUS        = 0x3A,
        ACCEL_OUT         = 0x3B,
        TEMP_OUT          = 0x41,
        GYRO_OUT          = 0x43,
        FIFO_COUNTH       = 0x72,
        FIFO_COUNTL       = 0x73,
        FIFO_R_W          = 0x74,
        WHO_AM_I          = 0x75,
        XA_OFFSET_H       = 0x77,
        XA_OFFSET_L       = 0x78,
        YA_OFFSET_H       = 0x7A,
        YA_OFFSET_L       = 0x7B,
        ZA_OFFSET_H       = 0x7D,
        ZA_OFFSET_L       = 0x7E,
        XG_OFFSET_H       = 0x13,
        XG_OFFSET_L       = 0x14,
        YG_OFFSET_H       = 0x15,
        YG_OFFSET_L       = 0x16,
        ZG_OFFSET_H       = 0x17,
        ZG_OFFSET_L       = 0x18,
        PWR_MGMT_1        = 0x6B,
        PWR_MGMT_2        = 0x6C
    };

private:
    float accel_x_;
    float accel_y_;
    float accel_z_;
    float gyro_x_;
    float gyro_y_;
    float gyro_z_;
    float temp_;

    float pitch_;
    float roll_;
    float yaw_;

    float accel_scale_;
    float gyro_scale_;

    uint8_t accel_range_;
    uint8_t gyro_range_;

    float complementaryFilter(float gyroAngle, float accelAngle, float alpha);
    void ComputeAngles();
};

#endif // MPU6050_H