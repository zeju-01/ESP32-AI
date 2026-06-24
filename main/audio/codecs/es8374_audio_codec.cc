#include "es8374_audio_codec.h"

#include <esp_log.h>

#define TAG "Es8374AudioCodec"

Es8374AudioCodec::Es8374AudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
    gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
    gpio_num_t pa_pin, uint8_t es8374_addr, bool use_mclk) {
    (void)use_mclk; // 参数保留以兼容其他codec构造函数签名，ES8374始终使用MCLK
    duplex_ = true; // 是否双工
    input_reference_ = false; // 是否使用参考输入，实现回声消除
    input_channels_ = 1; // 输入通道数
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    input_gain_ = 15;  // DMIC模式下PGA(0x22)被旁路，此值映射到DMIC_SCALE+ADC音量(0x25)

    pa_pin_ = pa_pin;
    CreateDuplexChannels(mclk, bclk, ws, dout, din);

    // Do initialize of related interface: data_if, ctrl_if and gpio_if
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);

    // Output
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = i2c_port,
        .addr = es8374_addr,
        .bus_handle = i2c_master_handle,
    };
    ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(ctrl_if_ != NULL);

    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);

    es8374_codec_cfg_t es8374_cfg = {};
    es8374_cfg.ctrl_if = ctrl_if_;
    es8374_cfg.gpio_if = gpio_if_;
    es8374_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8374_cfg.pa_pin = pa_pin;
    es8374_cfg.digital_mic = true;  // 使用SPH0641LU4H-1数字麦克风，通过ES8374 DMIC接口输入
    codec_if_ = es8374_codec_new(&es8374_cfg);
    assert(codec_if_ != NULL);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if_,
        .data_if = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(output_dev_ != NULL);
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    input_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(input_dev_ != NULL);
    esp_codec_set_disable_when_closed(output_dev_, false);
    esp_codec_set_disable_when_closed(input_dev_, false);
    ESP_LOGI(TAG, "Es8374AudioCodec initialized");
}

Es8374AudioCodec::~Es8374AudioCodec() {
    ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    esp_codec_dev_delete(output_dev_);
    ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    esp_codec_dev_delete(input_dev_);

    audio_codec_delete_codec_if(codec_if_);
    audio_codec_delete_ctrl_if(ctrl_if_);
    audio_codec_delete_gpio_if(gpio_if_);
    audio_codec_delete_data_if(data_if_);
}

void Es8374AudioCodec::CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
    assert(input_sample_rate_ == output_sample_rate_);

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
			#ifdef   I2S_HW_VERSION_2    
				.ext_clk_freq_hz = 0,
			#endif
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            #ifdef   I2S_HW_VERSION_2   
                .left_align = true,
                .big_endian = false,
                .bit_order_lsb = false
            #endif
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = dout,
            .din = din,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    ESP_LOGI(TAG, "Duplex channels created");
}

void Es8374AudioCodec::SetOutputVolume(int volume) {
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
    AudioCodec::SetOutputVolume(volume);
}

void Es8374AudioCodec::EnableInput(bool enable) {
    std::lock_guard<std::mutex> lock(data_if_mutex_);
    if (enable == input_enabled_) {
        return;
    }
    if (enable) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)input_sample_rate_,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256, // 必须与CreateDuplexChannels一致，否则esp_codec_dev_open内部set_drv_fs会将MCLK降为128*fs导致ES8374 ADC时钟错误
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(input_dev_, input_gain_));

        // 全量 dump ES8374 寄存器 0x00-0x7F（与 MaixPy 参考值对比）
        ESP_LOGI(TAG, "=== ES8374 Full Register Dump (0x00-0x7F) ===");
        int reg_val = 0;
        for (int r = 0x00; r <= 0x7F; r++) {
            ctrl_if_->read_reg(ctrl_if_, r, 1, &reg_val, 1);
            if (r % 16 == 0) {
                printf("  %02x:", r);
            }
            printf(" %02x", reg_val & 0xFF);
            if (r % 16 == 15) {
                printf("\n");
            }
        }
        printf("\n");
        ESP_LOGI(TAG, "=== End Register Dump ===");
        ESP_LOGI(TAG, "Expect: 0x21=0x24(DMIC+ADC on) 0x6D=0x60(DMIC_CLK) 0x24=0x8B(dmic_src=11+6dB boost) 0x25=0x12(-9dB) 0x14=0x8A(ADC DOUT enable)");

        // esp_codec_dev默认将I2S RX配置为MONO+左声道(slot_mask=0x1)
        // DMIC数据在左声道，保持默认左声道配置
        // 如需切换右声道，将 I2S_STD_SLOT_LEFT 改为 I2S_STD_SLOT_RIGHT
        i2s_channel_disable(rx_handle_);
        i2s_std_slot_config_t slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            #ifdef I2S_HW_VERSION_2
                .left_align = true,
                .big_endian = false,
                .bit_order_lsb = false,
            #endif
        };
        i2s_channel_reconfig_std_slot(rx_handle_, &slot_cfg);
        i2s_channel_enable(rx_handle_);
        ESP_LOGI(TAG, "I2S RX configured to LEFT channel for DMIC data");

        // 最终验证：I2S重配置后再次读取关键寄存器，确认状态未变
        int v00 = 0, v10 = 0, v14 = 0, v21 = 0, v24 = 0, v25 = 0, v6d = 0, v6f = 0;
        ctrl_if_->read_reg(ctrl_if_, 0x00, 1, &v00, 1);
        ctrl_if_->read_reg(ctrl_if_, 0x10, 1, &v10, 1);
        ctrl_if_->read_reg(ctrl_if_, 0x14, 1, &v14, 1);
        ctrl_if_->read_reg(ctrl_if_, 0x21, 1, &v21, 1);
        ctrl_if_->read_reg(ctrl_if_, 0x24, 1, &v24, 1);
        ctrl_if_->read_reg(ctrl_if_, 0x25, 1, &v25, 1);
        ctrl_if_->read_reg(ctrl_if_, 0x6D, 1, &v6d, 1);
        ctrl_if_->read_reg(ctrl_if_, 0x6F, 1, &v6f, 1);
        ESP_LOGI(TAG, "Post-init verify: 0x00=%02x 0x10=%02x 0x14=%02x 0x21=%02x 0x24=%02x 0x25=%02x 0x6D=%02x 0x6F=%02x",
                 v00 & 0xFF, v10 & 0xFF, v14 & 0xFF, v21 & 0xFF, v24 & 0xFF, v25 & 0xFF, v6d & 0xFF, v6f & 0xFF);
        ESP_LOGI(TAG, "Expected:         0x00=80 0x10=0c 0x14=8a 0x21=24 0x24=8b 0x25=12 0x6D=60 0x6F=00");
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    }
    AudioCodec::EnableInput(enable);
}

void Es8374AudioCodec::EnableOutput(bool enable) {
    std::lock_guard<std::mutex> lock(data_if_mutex_);
    if (enable == output_enabled_) {
        return;
    }
    if (enable) {
        // Play 16bit 1 channel
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256, // 必须与CreateDuplexChannels一致
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_));
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 1);
        }
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 0);
        }
    }
    AudioCodec::EnableOutput(enable);
}

int Es8374AudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_) {
        esp_err_t ret = esp_codec_dev_read(input_dev_, (void*)dest, samples * sizeof(int16_t));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "I2S read failed: %s", esp_err_to_name(ret));
            memset(dest, 0, samples * sizeof(int16_t));
        } else {
            // 尖峰抑制hack已移除：0x24 dmic_src修复后ADC读取的是DMIC数据而非模拟输入垃圾噪声
            // 原hack会误杀正常大声语音信号(abs>20000即置零，约占满量程61%)

            // 诊断：每250次读取（约2.5秒）打印前16个样本值
            static int read_count = 0;
            read_count++;
            if (read_count % 250 == 0) {
                ESP_LOGI(TAG, "Raw: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                    dest[0], dest[1], dest[2], dest[3], dest[4], dest[5], dest[6], dest[7],
                    dest[8], dest[9], dest[10], dest[11], dest[12], dest[13], dest[14], dest[15]);
            }
        }
    }
    return samples;
}

int Es8374AudioCodec::Write(const int16_t* data, int samples) {
    if (output_enabled_) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(output_dev_, (void*)data, samples * sizeof(int16_t)));
    }
    return samples;
}