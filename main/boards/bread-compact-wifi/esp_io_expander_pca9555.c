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
    ESP_RETURN_ON_ERROR(i2c_master_transmit(pca9555->i2c_handle, data0, sizeof(data0), I2C_TIMEOUT_MS), TAG, "Write output reg0 failed");
    ESP_RETURN_ON_ERROR(i2c_master_transmit(pca9555->i2c_handle, data1, sizeof(data1), I2C_TIMEOUT_MS), TAG, "Write output reg1 failed");
    pca9555->regs.output = value;
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
    ESP_RETURN_ON_ERROR(i2c_master_transmit(pca9555->i2c_handle, data0, sizeof(data0), I2C_TIMEOUT_MS), TAG, "Write config reg0 failed");
    ESP_RETURN_ON_ERROR(i2c_master_transmit(pca9555->i2c_handle, data1, sizeof(data1), I2C_TIMEOUT_MS), TAG, "Write config reg1 failed");
    pca9555->regs.direction = value;
    return ESP_OK;
}

static esp_err_t read_direction_reg(esp_io_expander_handle_t handle, uint32_t *value)
{
    esp_io_expander_pca9555_t *pca9555 = (esp_io_expander_pca9555_t *)__containerof(handle, esp_io_expander_pca9555_t, base);

    *value = pca9555->regs.direction;
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