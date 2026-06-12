/*
 * SPDX-FileCopyrightText: 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "esp_io_expander.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a PCA9555 IO expander object
 *
 * @param[in]  i2c_bus    I2C bus handle. Obtained from `i2c_new_master_bus()`
 * @param[in]  dev_addr   I2C device address of chip.
 * @param[out] handle_ret Handle to created IO expander object
 *
 * @return
 *      - ESP_OK: Success, otherwise returns ESP_ERR_xxx
 */
esp_err_t esp_io_expander_new_i2c_pca9555(i2c_master_bus_handle_t i2c_bus, uint32_t dev_addr, esp_io_expander_handle_t *handle_ret);

/**
 * @brief Directly set level of a PCA9555 pin (bypasses esp_io_expander framework)
 *
 * @param[in] i2c_bus    I2C bus handle
 * @param[in] dev_addr   PCA9555 I2C address (7-bit)
 * @param[in] pin_num    Pin number (0-15, P00-P17)
 * @param[in] level      Level to set (0 or 1)
 *
 * @return
 *      - ESP_OK: Success, otherwise returns ESP_ERR_xxx
 */
esp_err_t pca9555_set_pin_level(i2c_master_bus_handle_t i2c_bus, uint32_t dev_addr, uint8_t pin_num, uint8_t level);

/**
 * @brief Directly set direction of a PCA9555 pin (bypasses esp_io_expander framework)
 *
 * @param[in] i2c_bus    I2C bus handle
 * @param[in] dev_addr   PCA9555 I2C address (7-bit)
 * @param[in] pin_num    Pin number (0-15, P00-P17)
 * @param[in] is_output  1=output, 0=input
 *
 * @return
 *      - ESP_OK: Success, otherwise returns ESP_ERR_xxx
 */
esp_err_t pca9555_set_pin_direction(i2c_master_bus_handle_t i2c_bus, uint32_t dev_addr, uint8_t pin_num, uint8_t is_output);

/**
 * @brief I2C address of the PCA9555 (7-bit address)
 * 
 * PCA9555的7位地址范围是0x20-0x27，取决于A0-A2引脚的连接：
 * - A0=0, A1=0, A2=0: 0x20
 * - A0=1, A1=0, A2=0: 0x21
 * - ...以此类推
 * 
 * 如果用户提供的是8位地址（如0x40），需要右移1位转换为7位地址（0x20）
 */
#define ESP_IO_EXPANDER_I2C_PCA9555_ADDRESS_020    0x20  // A0=0, A1=0, A2=0
#define ESP_IO_EXPANDER_I2C_PCA9555_ADDRESS_021    0x21  // A0=1, A1=0, A2=0
#define ESP_IO_EXPANDER_I2C_PCA9555_ADDRESS_022    0x22  // A0=0, A1=1, A2=0
#define ESP_IO_EXPANDER_I2C_PCA9555_ADDRESS_023    0x23  // A0=1, A1=1, A2=0
#define ESP_IO_EXPANDER_I2C_PCA9555_ADDRESS_024    0x24  // A0=0, A1=0, A2=1
#define ESP_IO_EXPANDER_I2C_PCA9555_ADDRESS_025    0x25  // A0=1, A1=0, A2=1
#define ESP_IO_EXPANDER_I2C_PCA9555_ADDRESS_026    0x26  // A0=0, A1=1, A2=1
#define ESP_IO_EXPANDER_I2C_PCA9555_ADDRESS_027    0x27  // A0=1, A1=1, A2=1

#ifdef __cplusplus
}
#endif