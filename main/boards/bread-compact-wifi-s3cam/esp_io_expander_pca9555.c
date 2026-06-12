/*
 * SPDX-FileCopyrightText: 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include "esp_bit_defs.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_io_expander.h"
#include "esp_io_expander_pca9555.h"

/* I2C communication related */
#define I2C_TIMEOUT_MS          (1000)
#define I2C_CLK_SPEED           (400000)

#define IO_COUNT                (16)

/* PCA9555 Register addresses (NXP) */
#define INPUT_REG_ADDR0         (0x00)
#define INPUT_REG_ADDR1         (0x01)
#define OUTPUT_REG_ADDR0        (0x02)
#define OUTPUT_REG_ADDR1        (0x03)
#define POLARITY_REG_ADDR0      (0x04)
#define POLARITY_REG_ADDR1      (0x05)
#define CONFIG_REG_ADDR0        (0x06)
#define CONFIG_REG_ADDR1        (0x07)

/* Default register value on power-up */
#define CONFIG_REG_DEFAULT_VAL  (0xFFFF)  // All inputs by default
#define OUTPUT_REG_DEFAULT_VAL  (0xFFFF)  // All HIGH by default

/**
 * @brief Device Structure Type
 *
 */
typedef struct {
    esp_io_expander_t base;
    i2c_master_dev_handle_t i2c_handle;
    struct {
        uint16_t direction;
        uint16_t output;
    } regs;
} esp_io_expander_pca9555_t;

static char *TAG = "pca9555";

static esp_err_t read_input_reg(esp_io_expander_handle_t handle, uint32_t *value);
static esp_err_t write_output_reg(esp_io_expander_handle_t handle, uint32_t value);
static esp_err_t read_output_reg(esp_io_expander_handle_t handle, uint32_t *value);
static esp_err_t write_direction_reg(esp_io_expander_handle_t handle, uint32_t value);
static esp_err_t read_direction_reg(esp_io_expander_handle_t handle, uint32_t *value);
static esp_err_t reset(esp_io_expander_t *handle);
static esp_err_t del(esp_io_expander_t *handle);

esp_err_t esp_io_expander_new_i2c_pca9555(i2c_master_bus_handle_t i2c_bus, uint32_t dev_addr, esp_io_expander_handle_t *handle_ret)
{
    ESP_RETURN_ON_FALSE(handle_ret != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle_ret");

    // Allocate memory for driver object
    esp_io_expander_pca9555_t *pca9555 = (esp_io_expander_pca9555_t *)calloc(1, sizeof(esp_io_expander_pca9555_t));
    ESP_RETURN_ON_FALSE(pca9555 != NULL, ESP_ERR_NO_MEM, TAG, "Malloc failed");

    // Add new I2C device
    esp_err_t ret = ESP_OK;
    const i2c_device_config_t i2c_dev_cfg = {
        .device_address = dev_addr,
        .scl_speed_hz = I2C_CLK_SPEED,
    };
    ESP_GOTO_ON_ERROR(i2c_master_bus_add_device(i2c_bus, &i2c_dev_cfg, &pca9555->i2c_handle), err, TAG, "Add new I2C device failed");

    pca9555->base.config.io_count = IO_COUNT;
    pca9555->base.config.flags.dir_out_bit_zero = 1;
    pca9555->base.read_input_reg = read_input_reg;
    pca9555->base.write_output_reg = write_output_reg;
    pca9555->base.read_output_reg = read_output_reg;
    pca9555->base.write_direction_reg = write_direction_reg;
    pca9555->base.read_direction_reg = read_direction_reg;
    pca9555->base.del = del;
    pca9555->base.reset = reset;

    /* Reset configuration and register status */
    ESP_GOTO_ON_ERROR(reset(&pca9555->base), err, TAG, "Reset failed");

    *handle_ret = &pca9555->base;
    return ESP_OK;
err:
    free(pca9555);
    return ret;
}

static esp_err_t read_input_reg(esp_io_expander_handle_t handle, uint32_t *value)
{
    esp_io_expander_pca9555_t *pca9555 = (esp_io_expander_pca9555_t *)__containerof(handle, esp_io_expander_pca9555_t, base);

    uint8_t temp[2] = {0};
    // Read Input Port 0 then Input Port 1
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(pca9555->i2c_handle, (uint8_t[]){ INPUT_REG_ADDR0 }, 1, &temp[0], 1, I2C_TIMEOUT_MS), TAG, "Read input reg0 failed");
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(pca9555->i2c_handle, (uint8_t[]){ INPUT_REG_ADDR1 }, 1, &temp[1], 1, I2C_TIMEOUT_MS), TAG, "Read input reg1 failed");
    *value = (temp[1] << 8) | temp[0];
    return ESP_OK;
}

static esp_err_t write_output_reg(esp_io_expander_handle_t handle, uint32_t value)
{
    esp_io_expander_pca9555_t *pca9555 = (esp_io_expander_pca9555_t *)__containerof(handle, esp_io_expander_pca9555_t, base);
    value &= 0xFFFF;

    uint8_t data0[] = {OUTPUT_REG_ADDR0, (uint8_t)(value & 0xFF)};
    uint8_t data1[] = {OUTPUT_REG_ADDR1, (uint8_t)((value >> 8) & 0xFF)};
    ESP_LOGD(TAG, "Writing output reg: 0x%04X (port0=0x%02X, port1=0x%02X)", value, data0[1], data1[1]);
    ESP_RETURN_ON_ERROR(i2c_master_transmit(pca9555->i2c_handle, data0, sizeof(data0), I2C_TIMEOUT_MS), TAG, "Write output reg0 failed");
    ESP_RETURN_ON_ERROR(i2c_master_transmit(pca9555->i2c_handle, data1, sizeof(data1), I2C_TIMEOUT_MS), TAG, "Write output reg1 failed");
    pca9555->regs.output = value;
    
    // Small delay to ensure PCA9555 has processed the write
    esp_rom_delay_us(100);
    return ESP_OK;
}

static esp_err_t read_output_reg(esp_io_expander_handle_t handle, uint32_t *value)
{
    esp_io_expander_pca9555_t *pca9555 = (esp_io_expander_pca9555_t *)__containerof(handle, esp_io_expander_pca9555_t, base);

    *value = pca9555->regs.output;
    return ESP_OK;
}

static esp_err_t write_direction_reg(esp_io_expander_handle_t handle, uint32_t value)
{
    esp_io_expander_pca9555_t *pca9555 = (esp_io_expander_pca9555_t *)__containerof(handle, esp_io_expander_pca9555_t, base);
    value &= 0xFFFF;

    uint8_t data0[] = {CONFIG_REG_ADDR0, (uint8_t)(value & 0xFF)};
    uint8_t data1[] = {CONFIG_REG_ADDR1, (uint8_t)((value >> 8) & 0xFF)};
    ESP_LOGD(TAG, "Writing direction reg: 0x%04X (port0=0x%02X, port1=0x%02X)", value, data0[1], data1[1]);
    ESP_RETURN_ON_ERROR(i2c_master_transmit(pca9555->i2c_handle, data0, sizeof(data0), I2C_TIMEOUT_MS), TAG, "Write config reg0 failed");
    ESP_RETURN_ON_ERROR(i2c_master_transmit(pca9555->i2c_handle, data1, sizeof(data1), I2C_TIMEOUT_MS), TAG, "Write config reg1 failed");
    pca9555->regs.direction = value;
    
    // Small delay to ensure PCA9555 has processed the write
    esp_rom_delay_us(100);
    return ESP_OK;
}

static esp_err_t read_direction_reg(esp_io_expander_handle_t handle, uint32_t *value)
{
    esp_io_expander_pca9555_t *pca9555 = (esp_io_expander_pca9555_t *)__containerof(handle, esp_io_expander_pca9555_t, base);

    // Return cached value to avoid repeated I2C reads which can fail
    // when I2C bus is busy with other devices
    *value = pca9555->regs.direction;
    ESP_LOGD(TAG, "read_direction_reg returning cached value: 0x%04X", *value);
    return ESP_OK;
}

static esp_err_t reset(esp_io_expander_t *handle)
{
    ESP_RETURN_ON_ERROR(write_direction_reg(handle, CONFIG_REG_DEFAULT_VAL), TAG, "Write config reg failed");
    ESP_RETURN_ON_ERROR(write_output_reg(handle, OUTPUT_REG_DEFAULT_VAL), TAG, "Write output reg failed");
    return ESP_OK;
}

static esp_err_t del(esp_io_expander_t *handle)
{
    esp_io_expander_pca9555_t *pca9555 = (esp_io_expander_pca9555_t *)__containerof(handle, esp_io_expander_pca9555_t, base);

    ESP_RETURN_ON_ERROR(i2c_master_bus_rm_device(pca9555->i2c_handle), TAG, "Remove I2C device failed");
    free(pca9555);
    return ESP_OK;
}

/**
 * @brief Directly set level of a PCA9555 pin using existing expander handle
 *
 * @param[in] handle     PCA9555 expander handle
 * @param[in] pin_num    Pin number (0-15, P00-P17)
 * @param[in] level      Level to set (0 or 1)
 *
 * @return
 *      - ESP_OK: Success, otherwise returns ESP_ERR_xxx
 */
esp_err_t pca9555_set_pin_level_direct(esp_io_expander_handle_t handle, uint8_t pin_num, uint8_t level)
{
    ESP_RETURN_ON_FALSE(pin_num < 16, ESP_ERR_INVALID_ARG, TAG, "Pin number must be 0-15");
    
    esp_io_expander_pca9555_t *pca9555 = (esp_io_expander_pca9555_t *)__containerof(handle, esp_io_expander_pca9555_t, base);
    
    // Determine which register to write
    uint8_t reg_addr;
    uint8_t bit_mask;
    
    if (pin_num < 8) {
        reg_addr = OUTPUT_REG_ADDR0;
        bit_mask = (1 << pin_num);
    } else {
        reg_addr = OUTPUT_REG_ADDR1;
        bit_mask = (1 << (pin_num - 8));
    }
    
    // Read current output register value
    uint8_t current_value = 0;
    esp_err_t ret = i2c_master_transmit_receive(pca9555->i2c_handle, &reg_addr, 1, &current_value, 1, I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read output register");
        return ret;
    }
    
    // Modify the bit
    if (level) {
        current_value |= bit_mask;
    } else {
        current_value &= ~bit_mask;
    }
    
    // Write back
    uint8_t data[2] = {reg_addr, current_value};
    ret = i2c_master_transmit(pca9555->i2c_handle, data, sizeof(data), I2C_TIMEOUT_MS);
    
    if (ret == ESP_OK) {
        // Update cache
        if (pin_num < 8) {
            pca9555->regs.output = (pca9555->regs.output & 0xFF00) | current_value;
        } else {
            pca9555->regs.output = (pca9555->regs.output & 0x00FF) | ((uint16_t)current_value << 8);
        }
        ESP_LOGD(TAG, "Set pin %d level to %d", pin_num, level);
    } else {
        ESP_LOGE(TAG, "Failed to set pin %d level", pin_num);
    }
    
    return ret;
}

/**
 * @brief Directly set direction of a PCA9555 pin using existing expander handle
 *
 * @param[in] handle     PCA9555 expander handle
 * @param[in] pin_num    Pin number (0-15, P00-P17)
 * @param[in] is_output  1=output, 0=input
 *
 * @return
 *      - ESP_OK: Success, otherwise returns ESP_ERR_xxx
 */
esp_err_t pca9555_set_pin_direction_direct(esp_io_expander_handle_t handle, uint8_t pin_num, uint8_t is_output)
{
    ESP_RETURN_ON_FALSE(pin_num < 16, ESP_ERR_INVALID_ARG, TAG, "Pin number must be 0-15");
    
    esp_io_expander_pca9555_t *pca9555 = (esp_io_expander_pca9555_t *)__containerof(handle, esp_io_expander_pca9555_t, base);
    
    // Determine which register to write
    uint8_t reg_addr;
    uint8_t bit_mask;
    
    if (pin_num < 8) {
        reg_addr = CONFIG_REG_ADDR0;
        bit_mask = (1 << pin_num);
    } else {
        reg_addr = CONFIG_REG_ADDR1;
        bit_mask = (1 << (pin_num - 8));
    }
    
    // Read current config register value
    uint8_t current_value = 0;
    esp_err_t ret = i2c_master_transmit_receive(pca9555->i2c_handle, &reg_addr, 1, &current_value, 1, I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read config register");
        return ret;
    }
    
    // Modify the bit (0=output, 1=input)
    if (is_output) {
        current_value &= ~bit_mask;  // Clear bit for output
    } else {
        current_value |= bit_mask;   // Set bit for input
    }
    
    // Write back
    uint8_t data[2] = {reg_addr, current_value};
    ret = i2c_master_transmit(pca9555->i2c_handle, data, sizeof(data), I2C_TIMEOUT_MS);
    
    if (ret == ESP_OK) {
        // Update cache
        if (pin_num < 8) {
            pca9555->regs.direction = (pca9555->regs.direction & 0xFF00) | current_value;
        } else {
            pca9555->regs.direction = (pca9555->regs.direction & 0x00FF) | ((uint16_t)current_value << 8);
        }
        ESP_LOGD(TAG, "Set pin %d direction to %s", pin_num, is_output ? "output" : "input");
    } else {
        ESP_LOGE(TAG, "Failed to set pin %d direction", pin_num);
    }
    
    return ret;
}

esp_err_t pca9555_set_pin_level(i2c_master_bus_handle_t i2c_bus, uint32_t dev_addr, uint8_t pin_num, uint8_t level)
{
    ESP_RETURN_ON_FALSE(pin_num < 16, ESP_ERR_INVALID_ARG, TAG, "Pin number must be 0-15");
    
    // Create temporary I2C device handle
    const i2c_device_config_t i2c_dev_cfg = {
        .device_address = dev_addr,
        .scl_speed_hz = I2C_CLK_SPEED,
    };
    
    i2c_master_dev_handle_t dev_handle = NULL;
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &i2c_dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device");
        return ret;
    }
    
    // Determine which register to write
    uint8_t reg_addr;
    uint8_t bit_mask;
    
    if (pin_num < 8) {
        reg_addr = OUTPUT_REG_ADDR0;
        bit_mask = (1 << pin_num);
    } else {
        reg_addr = OUTPUT_REG_ADDR1;
        bit_mask = (1 << (pin_num - 8));
    }
    
    // Read current output register value
    uint8_t current_value = 0;
    ret = i2c_master_transmit_receive(dev_handle, &reg_addr, 1, &current_value, 1, I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read output register");
        i2c_master_bus_rm_device(dev_handle);
        return ret;
    }
    
    // Modify the bit
    if (level) {
        current_value |= bit_mask;
    } else {
        current_value &= ~bit_mask;
    }
    
    // Write back
    uint8_t data[2] = {reg_addr, current_value};
    ret = i2c_master_transmit(dev_handle, data, sizeof(data), I2C_TIMEOUT_MS);
    
    i2c_master_bus_rm_device(dev_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Set pin %d level to %d", pin_num, level);
    } else {
        ESP_LOGE(TAG, "Failed to set pin %d level", pin_num);
    }
    
    return ret;
}

esp_err_t pca9555_set_pin_direction(i2c_master_bus_handle_t i2c_bus, uint32_t dev_addr, uint8_t pin_num, uint8_t is_output)
{
    ESP_RETURN_ON_FALSE(pin_num < 16, ESP_ERR_INVALID_ARG, TAG, "Pin number must be 0-15");
    
    // Create temporary I2C device handle
    const i2c_device_config_t i2c_dev_cfg = {
        .device_address = dev_addr,
        .scl_speed_hz = I2C_CLK_SPEED,
    };
    
    i2c_master_dev_handle_t dev_handle = NULL;
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &i2c_dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device");
        return ret;
    }
    
    // Determine which register to write
    uint8_t reg_addr;
    uint8_t bit_mask;
    
    if (pin_num < 8) {
        reg_addr = CONFIG_REG_ADDR0;
        bit_mask = (1 << pin_num);
    } else {
        reg_addr = CONFIG_REG_ADDR1;
        bit_mask = (1 << (pin_num - 8));
    }
    
    // Read current config register value
    uint8_t current_value = 0;
    ret = i2c_master_transmit_receive(dev_handle, &reg_addr, 1, &current_value, 1, I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read config register");
        i2c_master_bus_rm_device(dev_handle);
        return ret;
    }
    
    // Modify the bit (0=output, 1=input)
    if (is_output) {
        current_value &= ~bit_mask;  // Clear bit for output
    } else {
        current_value |= bit_mask;   // Set bit for input
    }
    
    // Write back
    uint8_t data[2] = {reg_addr, current_value};
    ret = i2c_master_transmit(dev_handle, data, sizeof(data), I2C_TIMEOUT_MS);
    
    i2c_master_bus_rm_device(dev_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Set pin %d direction to %s", pin_num, is_output ? "output" : "input");
    } else {
        ESP_LOGE(TAG, "Failed to set pin %d direction", pin_num);
    }
    
    return ret;
}